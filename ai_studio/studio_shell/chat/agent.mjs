// Chat agent spawn seam (T0242 increment 2). ONE codex session per chat CONVERSATION
// (design R3): the FIRST turn spawns a fresh `codex exec`; every LATER turn spawns
// `codex exec resume <sessionId>` — the model's own session IS the conversation memory
// (no history replay, no resident daemon). A short-lived process per MESSAGE either way,
// so cancelling one message only ever kills that one process; the session survives on
// disk in codex's own session store regardless (R3).
//
// Pure builders (prompt/argv text — no spawn, directly unit-tested) + the injectable
// `transport` seam `runChatTurn` calls, defaulting to `runCodexTransport` below. Tests
// ALWAYS inject a fake transport — codex NEVER spawns in the suite (same contract as
// tools/dual_plate_generate.mjs's `generatePlate`/tools/prompt_assist.mjs's `expandPrompt`).
//
// ---- LIVE-VERIFIED invocation shapes (2026-07-03, three real `codex exec` calls on this
// box — see the task's probe, not kept in the repo) ----
//
// Session id capture: `--json` prints ONE JSONL event stream to stdout; its FIRST line is
// always `{"type":"thread.started","thread_id":"<uuid>"}` — that `thread_id` IS the
// session id `codex exec resume <id>` takes. (`~/.codex/sessions/` also holds session
// files as a fallback path, but the `--json` stream is simpler and was verified first, so
// it is the one this module uses — see extractSessionId below.) Verified continuity
// (MANGO test): turn 1 `codex exec --json --output-last-message f1 "Remember the secret
// word MANGO. Reply with just: OK"` -> thread_id `019f2837-e164-7bd2-a0a4-0b434fa86fcf`,
// f1 = "OK"; turn 2 `codex exec resume 019f2837-e164-7bd2-a0a4-0b434fa86fcf --json
// --output-last-message f2 "What secret word did I ask you to remember? Reply with just
// the word."` -> f2 = "MANGO" — the resumed session actually remembers turn 1. Exact argv
// used (both real, both exit 0):
//   node <CODEX_JS> exec --skip-git-repo-check --json --output-last-message <file> "<prompt>"
//   node <CODEX_JS> exec resume <sessionId> --skip-git-repo-check --json --output-last-message <file> "<message>"
// A third live call confirmed `--dangerously-bypass-approvals-and-sandbox -C <repoRoot>`
// combined with the above runs a REAL shell command with no approval prompt (`node
// --version` -> its stdout landed verbatim in --output-last-message) — this agent needs
// exactly that (see buildFirstTurnCommand/buildResumeCommand below).
//
// As with prompt_assist.mjs/dual_plate_generate.mjs: the reply text is ALWAYS read from
// --output-last-message's FILE, never raw stdout — stdout carries hook-warning/skill-
// budget-noise `item.completed` events around the real answer (verified live, same as
// prompt_assist.mjs's own note).
import { spawn } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { join, resolve } from "node:path";
// The npm .cmd-shim workaround is ALREADY solved once, in prompt_assist.mjs — reused
// verbatim rather than re-derived (execFile/spawn can neither resolve the extensionless
// "codex" shim nor run a bare .cmd without shell:true; the shim's own target script is the
// spawnable path). See that module's doc for the live verification.
import { CODEX_JS } from "../../assets/canvas/tools/prompt_assist.mjs";

// Repo root from THIS file's own location (mirrors cli.mjs's `repoRoot` — a fixed real
// path regardless of any CANVAS_PROJECTS_ROOT test override), used as codex's `-C`
// working root so `node ai_studio/assets/canvas/cli.mjs ...` (a repo-relative path in the
// prompt) resolves the same way it does from an interactive terminal at the repo root.
// ai_studio/studio_shell/chat/agent.mjs -> up 3 -> repo root (same depth as cli.mjs).
export const REPO_ROOT = resolve(fileURLToPath(new URL("../../..", import.meta.url)));

// Generous: a chat turn can itself invoke a minutes-long canvas op (alpha-dual-generate
// spawns ITS OWN codex image generation — README.md "Long ops (minutes)"), so this ceiling
// sits above that op's own worst case, not just a text reply.
const CODEX_TIMEOUT_MS = 900_000;

function privateStoreId(context) {
  const storeId = context && context.storeId ? String(context.storeId) : "";
  return storeId && storeId !== "studio" ? storeId : "";
}

// ---- driving contract (pure text; private contexts add a store-scope reminder) -----------

// The system/driving contract every turn's FIRST-turn prompt carries (SKILL.md's own
// wording, inlined here so the contract holds even if codex never auto-loads the skill —
// design doc section 2, point 3). States, in order: the ONE allowed mutation path (tool
// parity), the "run bare to discover verbs" law, the T0234 history-navigation guard
// verbatim, and the R2 permission line (undoable = allowed, delete/outside-store = denied).
export function buildDrivingContract(context) {
  const storeId = privateStoreId(context);
  const lines = [
    "You are the chat agent for AI Studio's canvas page. You act on the project ONLY through its CLI:",
    "  node ai_studio/assets/canvas/cli.mjs <verb> <projectId> [flags]",
    "Run it with NO arguments to see the full, current, self-documenting verb list — never guess a verb and never hand-edit project.json; the CLI is the ONLY path onto the project (tool parity — the page itself goes through the identical ops.mjs surface, so this is the same rule, not a weaker one).",
    storeId
      ? `For the full project state beyond the selection summary below, run: node ai_studio/assets/canvas/cli.mjs show <projectId> --store ${storeId}`
      : "For the full project state beyond the selection summary below, run: node ai_studio/assets/canvas/cli.mjs show <projectId>",
    "",
    "HISTORY NAVIGATION GUARD (T0234): before ANY undo, redo, or history-jump call, run",
    storeId
      ? `  node ai_studio/assets/canvas/cli.mjs history-list <projectId> --store ${storeId}`
      : "  node ai_studio/assets/canvas/cli.mjs history-list <projectId>",
    'and read its "head: N" line RIGHT BEFORE that call, then pass --expect-head N. Never reuse a head value read earlier in this conversation — the project may be live in the page at the same time. Never call history-jump at all unless the lead explicitly asked to time-travel.',
    "",
    "PERMISSIONS: you may perform ANY journaled, undoable canvas operation the lead asks for (add/patch/move/align/slice/alpha/generate/undo/redo/history-jump — everything the journal can restore). You must REFUSE, and say why, any request to:",
    "  - delete the project (the CLI's `delete <id>` verb — a .trash move that happens OUTSIDE the journal, not undoable), or",
    "  - read or write any file outside this project's own store directory.",
    "These are the only two denials — everything else recoverable via the journal is allowed.",
  ];
  if (storeId) {
    lines.push(
      "",
      `CANVAS STORE SCOPE: this project lives in private store ${storeId}. Pass --store ${storeId} on every canvas CLI command, including show, history-list, undo, redo, history-jump, and every mutation. Do not run bare project-id commands for this project.`,
    );
  }
  return lines.join("\n");
}

// The bounded context digest, rendered as plain text for the prompt (context.mjs's own
// `buildChatContext` output — this module never builds the digest itself, only prints it).
export function buildContextDigestText(context) {
  if (!context) throw new Error("buildContextDigestText requires context");
  const storeId = privateStoreId(context);
  const lines = [
    `Project: "${context.title}" (id: ${context.projectId})`,
    ...(storeId ? [`Canvas store: ${storeId} (private; pass --store ${storeId} on every canvas CLI command)`] : []),
    `Elements: ${context.counts.elements}, Groups: ${context.counts.groups}`,
    `Current history head: ${context.head}`,
  ];
  if (context.selection && context.selection.length) {
    lines.push(`Selection (${context.selection.length}):`);
    for (const item of context.selection) lines.push(`  - ${item.ref}`);
  } else {
    lines.push("Selection: (none — act on the project as a whole, or ask the lead what to select)");
  }
  return lines.join("\n");
}

// FIRST turn of a conversation: contract + digest + refs travel in full (design doc
// section 2, point 3 — "robust even if codex does not auto-load the skill").
export function buildFirstTurnPrompt({ context, message } = {}) {
  if (!context) throw new Error("buildFirstTurnPrompt requires context");
  if (!message) throw new Error("buildFirstTurnPrompt requires message");
  return [
    buildDrivingContract(context),
    "",
    "---- CONTEXT DIGEST (selection summary — NOT the full project) ----",
    buildContextDigestText(context),
    "---- END CONTEXT DIGEST ----",
    "",
    "User request:",
    String(message),
  ].join("\n");
}

// LATER turns: the contract/digest already live in the resumed codex session (R3) — only a
// COMPACT "what changed since last time you looked" line travels, plus the new message
// verbatim (design doc section 2: "later turns append a compact 'current head: N;
// selection: ...' line instead of the full digest").
export function buildResumeMessage({ context, message } = {}) {
  if (!context) throw new Error("buildResumeMessage requires context");
  if (!message) throw new Error("buildResumeMessage requires message");
  const refs = (context.selection || []).map((item) => item.ref).join("; ");
  const storeId = privateStoreId(context);
  const storeScope = storeId ? `; canvas store: ${storeId} (pass --store ${storeId})` : "";
  return `current head: ${context.head}${storeScope}; selection: ${refs || "(none)"}\n${String(message)}`;
}

// ---- pure argv builders (no spawn) -------------------------------------------------------

// FIRST turn: the prompt is a plain positional PROMPT argument (no `-i` attached, so no
// variadic-swallow footgun — see prompt_assist.mjs's module doc for that hazard). Needs
// --dangerously-bypass-approvals-and-sandbox (R2): this agent's whole job is to run shell
// commands (the canvas CLI) — a plain `codex exec` sandbox/approval-gates exactly that, and
// the canvas projects root can live OUTSIDE the repo workspace (YandexDisk — SKILL.md),
// where the default sandbox denies writes outright. Accepted under the same trust boundary
// as the lead's own terminal: localhost, single-user (R2's own honest statement — there is
// no hard sandbox in v1).
export function buildFirstTurnCommand({ prompt, outputPath } = {}) {
  if (!prompt) throw new Error("buildFirstTurnCommand requires prompt");
  if (!outputPath) throw new Error("buildFirstTurnCommand requires outputPath");
  return {
    command: process.execPath,
    args: [
      CODEX_JS,
      "exec",
      "--skip-git-repo-check",
      "-C",
      REPO_ROOT,
      "--dangerously-bypass-approvals-and-sandbox",
      "--json",
      "--output-last-message",
      outputPath,
      prompt,
    ],
  };
}

// LATER turns: `exec resume <sessionId>` — the compact message (buildResumeMessage) as the
// trailing PROMPT positional (resume's own usage: `resume [OPTIONS] [SESSION_ID] [PROMPT]`).
// ARGV ORDER MATTERS (live incident 2026-07-05, «codex exec exited 2: unexpected argument
// '-C'»): a codex CLI update removed `-C/--cd` from the resume SUBCOMMAND's options — it is
// an `exec`-level flag now, so it must sit BETWEEN `exec` and `resume`. The other flags
// (--skip-git-repo-check / --dangerously-bypass… / --json / --output-last-message) are still
// accepted by resume itself (verified against `codex exec resume --help` + a live parse test
// with a fake session id: parse passes, only "no rollout found" remains).
export function buildResumeCommand({ sessionId, message, outputPath } = {}) {
  if (!sessionId) throw new Error("buildResumeCommand requires sessionId");
  if (!message) throw new Error("buildResumeCommand requires message");
  if (!outputPath) throw new Error("buildResumeCommand requires outputPath");
  return {
    command: process.execPath,
    args: [
      CODEX_JS,
      "exec",
      "-C",
      REPO_ROOT,
      "resume",
      sessionId,
      "--skip-git-repo-check",
      "--dangerously-bypass-approvals-and-sandbox",
      "--json",
      "--output-last-message",
      outputPath,
      message,
    ],
  };
}

// Pure parse of the --json event stream for the FIRST `thread.started` event's `thread_id`
// (live-verified above). Tolerant of torn/partial lines (a killed process's last buffered
// line) and of any other event type/order — it only ever looks for this one field.
export function extractSessionId(jsonlText) {
  for (const line of String(jsonlText || "").split("\n")) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    let event;
    try {
      event = JSON.parse(trimmed);
    } catch {
      continue; // torn line — keep scanning, never throw on stream noise
    }
    if (event && event.type === "thread.started" && event.thread_id) return String(event.thread_id);
  }
  return null;
}

// ---- default transport (the one un-unit-tested edge — same stance as generatePlate) -----

// The DEFAULT transport runChatTurn falls back to. `onChild`, when given, is called
// SYNCHRONOUSLY right after spawn with the live ChildProcess — the seam chat/api.mjs's
// cancel route uses to track/kill the CURRENT turn's process (T0242 increment 3) without
// this module needing to know anything about HTTP or per-project bookkeeping.
export async function runCodexTransport({ prompt, message, sessionId, onChild } = {}) {
  const workDir = mkdtempSync(join(tmpdir(), "canvas-chat-"));
  try {
    const outputPath = join(workDir, "last.txt");
    const { command, args } = sessionId
      ? buildResumeCommand({ sessionId, message, outputPath })
      : buildFirstTurnCommand({ prompt, outputPath });
    // stdin is never used by these argv shapes (the prompt/message travel as a plain
    // positional, unlike prompt_assist.mjs's vision path) — "ignore" so there is no pipe
    // for codex to stall reading from (the same stall prompt_assist.mjs's module doc warns
    // about, sidestepped here by never opening the pipe at all).
    const child = spawn(command, args, { stdio: ["ignore", "pipe", "pipe"] });
    if (typeof onChild === "function") onChild(child);
    let stdout = "";
    let stderr = "";
    child.stdout.on("data", (chunk) => {
      stdout += chunk;
    });
    child.stderr.on("data", (chunk) => {
      stderr += chunk;
    });
    const timer = setTimeout(() => child.kill("SIGTERM"), CODEX_TIMEOUT_MS);
    const { code, signal } = await new Promise((resolveExit, rejectExit) => {
      child.on("error", rejectExit);
      child.on("close", (exitCode, exitSignal) => resolveExit({ code: exitCode, signal: exitSignal }));
    });
    clearTimeout(timer);
    // Whatever session id DID reach stdout before exit/kill — `thread.started` is the
    // stream's first line, so even a cancelled turn usually has it. Attached to a thrown
    // Error too (not just the success path) so a caller (chat/api.mjs) can still persist
    // the session on a cancelled-but-already-started turn (R3: "the session survives").
    const capturedSessionId = extractSessionId(stdout) || sessionId || null;
    if (signal && code === null) {
      const err = new Error(`codex exec was killed (signal ${signal}) — timed out after ${CODEX_TIMEOUT_MS}ms or was cancelled`);
      err.sessionId = capturedSessionId;
      throw err;
    }
    if (code !== 0) {
      const err = new Error(`codex exec exited ${code}: ${stderr.slice(-2000) || "(no stderr)"}`);
      err.sessionId = capturedSessionId;
      throw err;
    }
    if (!capturedSessionId) throw new Error("runCodexTransport: no session id captured from codex exec --json output");
    let text = "";
    try {
      text = readFileSync(outputPath, "utf8").trim();
    } catch {
      text = ""; // codex exited 0 but wrote no last-message file — loud below, no silent fallback
    }
    if (!text) throw new Error("runCodexTransport: codex returned an empty result (--output-last-message file was empty)");
    return { text, sessionId: capturedSessionId };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// ---- R2 post-check: denied-verb tripwire -------------------------------------------------

// NOT a sandbox (codex exec has full shell access under this trust model, R2) — a loud,
// honest tripwire over the turn's own reply text: the driving contract tells the agent to
// name what it ran, and a refusal explanation typically quotes the denied verb too, so this
// catches both "it ran delete" and "it explained refusing delete" — either way the panel
// gets a visible flag, never a silent pass-through. Returns [] (never undefined) on a clean
// reply, so callers can always do `flags.length` without a null check.
export function checkDeniedVerbs(text) {
  const haystack = String(text || "");
  const flags = [];
  if (/cli\.mjs\s+delete\b/i.test(haystack) || /\bdelete(?:d)?\s+(?:the\s+)?project\b/i.test(haystack)) {
    flags.push('possible project-deletion verb ("delete") mentioned in the reply — verify no project was deleted');
  }
  return flags;
}

// ---- the orchestrator ---------------------------------------------------------------------

// One chat turn. `sessionId` null/undefined means "first turn of this conversation" (spawns
// a fresh session); a truthy `sessionId` means "resume" (spawns `codex exec resume`, sending
// only the compact resume message — the contract/digest already live in that session).
// `transport` is the injectable seam (default runCodexTransport); `onChild`, when given, is
// forwarded to the transport unchanged (see runCodexTransport's own doc). Returns
// { text, sessionId, flags } — flags is the R2 post-check result, always an array.
export async function runChatTurn({ context, message, sessionId, transport, onChild } = {}) {
  if (!context) throw new Error("runChatTurn requires context");
  if (!message) throw new Error("runChatTurn requires message");
  const run = transport || runCodexTransport;
  const isFirstTurn = !sessionId;
  const result = await run({
    context,
    prompt: isFirstTurn ? buildFirstTurnPrompt({ context, message }) : undefined,
    message: isFirstTurn ? undefined : buildResumeMessage({ context, message }),
    sessionId: sessionId || null,
    onChild,
  });
  if (!result || typeof result.text !== "string" || !result.sessionId) {
    throw new Error("runChatTurn: transport must return { text, sessionId }");
  }
  return { text: result.text, sessionId: result.sessionId, flags: checkDeniedVerbs(result.text) };
}
