// Chat agent seam tests (T0242 increment 2). Only PURE builders + runChatTurn WITH A FAKE
// TRANSPORT are exercised here — codex NEVER spawns in this suite (the T0238/T0239
// contract, extended to chat; see agent.mjs's own module doc for the LIVE-verified
// session-id-capture + resume shapes, checked by hand on this box, not re-checked here).
// Run: node --test ai_studio/studio_shell/chat/tests/agent.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import {
  buildContextDigestText,
  buildDrivingContract,
  buildFirstTurnCommand,
  buildFirstTurnPrompt,
  buildResumeCommand,
  buildResumeMessage,
  checkDeniedVerbs,
  extractSessionId,
  REPO_ROOT,
  runChatTurn,
} from "../agent.mjs";
import { CODEX_JS } from "../../../assets/canvas/tools/prompt_assist.mjs";

const CONTEXT = {
  schema: "ai_studio.studio_shell.chat_context.v1",
  projectId: "proj-1",
  title: "Fixture Screen",
  selection: [
    { ref: 'canvas://proj-1/element/el_1 — project "Fixture Screen", element "Hero"', id: "el_1", kind: "element", type: "image", name: "Hero", w: 10, h: 10, groupId: null },
  ],
  counts: { elements: 2, groups: 1 },
  head: 7,
};

const EMPTY_CONTEXT = { ...CONTEXT, selection: [] };
const PRIVATE_CONTEXT = {
  ...CONTEXT,
  storeId: "game:secret-game",
  visibility: "private",
  qualifiedId: "game:secret-game:proj-1",
  gameId: "secret-game",
  selection: [
    { ...CONTEXT.selection[0], ref: "canvas://game/secret-game/proj-1/element/el_1" },
  ],
};

// ---- buildDrivingContract ------------------------------------------------------------

test("buildDrivingContract: states the CLI path, the run-bare-for-verbs law, T0234, and the R2 permission line", () => {
  const contract = buildDrivingContract();
  assert.match(contract, /node ai_studio\/assets\/canvas\/cli\.mjs/);
  assert.match(contract, /NO arguments/i);
  assert.match(contract, /T0234/);
  assert.match(contract, /--expect-head/);
  assert.match(contract, /history-list/);
  assert.match(contract, /delete the project/i);
  assert.match(contract, /outside this project's own store directory/i);
  assert.match(contract, /journaled, undoable canvas operation/i);
});

test("buildDrivingContract: private contexts require --store on every canvas CLI command", () => {
  const contract = buildDrivingContract(PRIVATE_CONTEXT);
  assert.match(contract, /--store game:secret-game/);
  assert.match(contract, /every canvas CLI command/i);
});

// ---- buildContextDigestText -----------------------------------------------------------

test("buildContextDigestText: embeds title, counts, head, and every selection ref", () => {
  const text = buildContextDigestText(CONTEXT);
  assert.match(text, /Fixture Screen/);
  assert.match(text, /proj-1/);
  assert.match(text, /Elements: 2, Groups: 1/);
  assert.match(text, /Current history head: 7/);
  assert.match(text, /canvas:\/\/proj-1\/element\/el_1/);
});

test("buildContextDigestText: an empty selection says so instead of an empty list", () => {
  const text = buildContextDigestText(EMPTY_CONTEXT);
  assert.match(text, /Selection: \(none/);
});

test("buildContextDigestText: private contexts print the store scope command hint", () => {
  const text = buildContextDigestText(PRIVATE_CONTEXT);
  assert.match(text, /Canvas store: game:secret-game/);
  assert.match(text, /--store game:secret-game/);
  assert.match(text, /canvas:\/\/game\/secret-game\/proj-1\/element\/el_1/);
});

// ---- buildFirstTurnPrompt / buildResumeMessage -----------------------------------------

test("buildFirstTurnPrompt: contract + digest + refs + the user message, in that order", () => {
  const prompt = buildFirstTurnPrompt({ context: CONTEXT, message: "сделай чёрную версию" });
  const contractIdx = prompt.indexOf("node ai_studio/assets/canvas/cli.mjs");
  const digestIdx = prompt.indexOf("Current history head: 7");
  const refIdx = prompt.indexOf("canvas://proj-1/element/el_1");
  const messageIdx = prompt.indexOf("сделай чёрную версию");
  assert.ok(contractIdx >= 0 && digestIdx > contractIdx && refIdx > contractIdx && messageIdx > digestIdx);
});

test("buildFirstTurnPrompt requires context and message", () => {
  assert.throws(() => buildFirstTurnPrompt({ message: "x" }), /requires context/);
  assert.throws(() => buildFirstTurnPrompt({ context: CONTEXT }), /requires message/);
});

test("buildResumeMessage: a COMPACT head+selection line, not the full contract/digest, plus the message", () => {
  const resumeMsg = buildResumeMessage({ context: CONTEXT, message: "теперь дуал путь" });
  assert.match(resumeMsg, /^current head: 7; selection: canvas:\/\/proj-1\/element\/el_1/);
  assert.match(resumeMsg, /теперь дуал путь$/);
  assert.doesNotMatch(resumeMsg, /node ai_studio\/assets\/canvas\/cli\.mjs/, "the resume message must NOT repeat the full driving contract");
  assert.doesNotMatch(resumeMsg, /T0234/, "the resume message must NOT repeat the full digest block");
});

test("buildResumeMessage: no selection reads as (none)", () => {
  const resumeMsg = buildResumeMessage({ context: EMPTY_CONTEXT, message: "hi" });
  assert.match(resumeMsg, /selection: \(none\)/);
});

test("buildResumeMessage: private resumes keep the selected store in the compact line", () => {
  const resumeMsg = buildResumeMessage({ context: PRIVATE_CONTEXT, message: "continue" });
  assert.match(resumeMsg, /^current head: 7; canvas store: game:secret-game/);
  assert.match(resumeMsg, /--store game:secret-game/);
  assert.match(resumeMsg, /selection: canvas:\/\/game\/secret-game\/proj-1\/element\/el_1/);
});

// ---- pure argv builders (no spawn) -----------------------------------------------------

test("buildFirstTurnCommand: node <codex.js> exec with bypass-sandbox + -C repoRoot + --json + --output-last-message + the prompt as a plain positional", () => {
  const { command, args } = buildFirstTurnCommand({ prompt: "full prompt text", outputPath: "C:/tmp/last.txt" });
  assert.equal(command, process.execPath);
  assert.deepEqual(args, [
    CODEX_JS,
    "exec",
    "--skip-git-repo-check",
    "-C",
    REPO_ROOT,
    "--dangerously-bypass-approvals-and-sandbox",
    "--json",
    "--output-last-message",
    "C:/tmp/last.txt",
    "full prompt text",
  ]);
});

test("buildFirstTurnCommand requires prompt/outputPath", () => {
  assert.throws(() => buildFirstTurnCommand({ outputPath: "o" }), /requires prompt/);
  assert.throws(() => buildFirstTurnCommand({ prompt: "p" }), /requires outputPath/);
});

test("buildResumeCommand: -C sits at the EXEC level, before the resume subcommand (2026-07-05 codex CLI dropped -C from resume's options — live 'unexpected argument' incident)", () => {
  const { command, args } = buildResumeCommand({ sessionId: "sess-abc", message: "теперь дуал путь", outputPath: "C:/tmp/last2.txt" });
  assert.equal(command, process.execPath);
  assert.deepEqual(args, [
    CODEX_JS,
    "exec",
    "-C",
    REPO_ROOT,
    "resume",
    "sess-abc",
    "--skip-git-repo-check",
    "--dangerously-bypass-approvals-and-sandbox",
    "--json",
    "--output-last-message",
    "C:/tmp/last2.txt",
    "теперь дуал путь",
  ]);
});

test("buildResumeCommand requires sessionId/message/outputPath", () => {
  assert.throws(() => buildResumeCommand({ message: "m", outputPath: "o" }), /requires sessionId/);
  assert.throws(() => buildResumeCommand({ sessionId: "s", outputPath: "o" }), /requires message/);
  assert.throws(() => buildResumeCommand({ sessionId: "s", message: "m" }), /requires outputPath/);
});

// ---- extractSessionId -------------------------------------------------------------------

test("extractSessionId: reads thread_id off the FIRST thread.started event (live-verified shape)", () => {
  const jsonl = [
    '{"type":"thread.started","thread_id":"019f2837-e164-7bd2-a0a4-0b434fa86fcf"}',
    '{"type":"item.completed","item":{"id":"item_0","type":"agent_message","text":"OK"}}',
    '{"type":"turn.completed","usage":{}}',
  ].join("\n");
  assert.equal(extractSessionId(jsonl), "019f2837-e164-7bd2-a0a4-0b434fa86fcf");
});

test("extractSessionId: tolerates torn/partial trailing lines and unrelated event types", () => {
  const jsonl = '{"type":"thread.started","thread_id":"abc"}\n{"type":"item.completed"\n';
  assert.equal(extractSessionId(jsonl), "abc");
});

test("extractSessionId: no thread.started event -> null", () => {
  assert.equal(extractSessionId('{"type":"turn.completed"}'), null);
  assert.equal(extractSessionId(""), null);
});

// ---- checkDeniedVerbs (R2 post-check) ----------------------------------------------------

test("checkDeniedVerbs: flags a cli.mjs delete mention", () => {
  const flags = checkDeniedVerbs('I ran `node ai_studio/assets/canvas/cli.mjs delete proj-1` as requested.');
  assert.equal(flags.length, 1);
  assert.match(flags[0], /project-deletion/);
});

test("checkDeniedVerbs: flags a refusal that names project deletion", () => {
  const flags = checkDeniedVerbs("I refused to delete the project — that is not undoable.");
  assert.equal(flags.length, 1);
});

test("checkDeniedVerbs: a clean reply about an undoable op has no flags", () => {
  assert.deepEqual(checkDeniedVerbs("Applied alpha-dual-generate to el_1 — new element el_9 created (seq 8)."), []);
});

test("checkDeniedVerbs: never undefined, always an array", () => {
  assert.deepEqual(checkDeniedVerbs(""), []);
  assert.deepEqual(checkDeniedVerbs(undefined), []);
});

// ---- runChatTurn (fake transport — codex never spawns here) -----------------------------

test("runChatTurn: first turn (sessionId null) routes prompt/undefined-message/sessionId:null to the transport", async () => {
  const calls = [];
  const fakeTransport = async (call) => {
    calls.push(call);
    return { text: "done", sessionId: "sess-new" };
  };
  const result = await runChatTurn({ context: CONTEXT, message: "сделай чёрную версию", sessionId: null, transport: fakeTransport });
  assert.equal(calls.length, 1);
  assert.equal(calls[0].sessionId, null);
  assert.equal(calls[0].message, undefined);
  assert.match(calls[0].prompt, /сделай чёрную версию/);
  assert.match(calls[0].prompt, /node ai_studio\/assets\/canvas\/cli\.mjs/); // full contract on turn 1
  assert.deepEqual(result, { text: "done", sessionId: "sess-new", flags: [] });
});

test("runChatTurn: resume turn (sessionId set) routes message/undefined-prompt/sessionId to the transport", async () => {
  const calls = [];
  const fakeTransport = async (call) => {
    calls.push(call);
    return { text: "done again", sessionId: "sess-existing" };
  };
  await runChatTurn({ context: CONTEXT, message: "теперь дуал путь", sessionId: "sess-existing", transport: fakeTransport });
  assert.equal(calls.length, 1);
  assert.equal(calls[0].sessionId, "sess-existing");
  assert.equal(calls[0].prompt, undefined);
  assert.match(calls[0].message, /^current head: 7;/);
  assert.match(calls[0].message, /теперь дуал путь$/);
});

test("runChatTurn: forwards onChild to the transport call unchanged", async () => {
  const seenOnChild = [];
  const marker = () => {};
  const fakeTransport = async (call) => {
    seenOnChild.push(call.onChild);
    return { text: "ok", sessionId: "s1" };
  };
  await runChatTurn({ context: CONTEXT, message: "hi", sessionId: null, transport: fakeTransport, onChild: marker });
  assert.equal(seenOnChild[0], marker);
});

test("runChatTurn: sessionId passthrough — the transport's returned sessionId is what comes back", async () => {
  const fakeTransport = async () => ({ text: "x", sessionId: "sess-777" });
  const result = await runChatTurn({ context: CONTEXT, message: "hi", sessionId: "sess-666", transport: fakeTransport });
  assert.equal(result.sessionId, "sess-777");
});

test("runChatTurn: post-check flags surface from the transport's reply text", async () => {
  const fakeTransport = async () => ({ text: "ran cli.mjs delete proj-1", sessionId: "s1" });
  const result = await runChatTurn({ context: CONTEXT, message: "delete everything", sessionId: null, transport: fakeTransport });
  assert.equal(result.flags.length, 1);
});

test("runChatTurn: a malformed transport response (missing sessionId) throws loudly", async () => {
  const fakeTransport = async () => ({ text: "x" });
  await assert.rejects(
    () => runChatTurn({ context: CONTEXT, message: "hi", sessionId: null, transport: fakeTransport }),
    /transport must return/,
  );
});

test("runChatTurn requires context and message", async () => {
  await assert.rejects(() => runChatTurn({ message: "x", transport: async () => ({}) }), /requires context/);
  await assert.rejects(() => runChatTurn({ context: CONTEXT, transport: async () => ({}) }), /requires message/);
});
