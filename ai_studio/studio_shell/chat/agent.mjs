import { runAppServerTransport } from "./app_server.mjs";

function privateStoreId(context) {
  const storeId = context?.storeId ? String(context.storeId) : "";
  return storeId && storeId !== "studio" ? storeId : "";
}

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

export function buildContextDigestText(context) {
  if (!context) throw new Error("buildContextDigestText requires context");
  const storeId = privateStoreId(context);
  const lines = [
    `Project: "${context.title}" (id: ${context.projectId})`,
    ...(storeId ? [`Canvas store: ${storeId} (private; pass --store ${storeId} on every canvas CLI command)`] : []),
    `Elements: ${context.counts.elements}, Groups: ${context.counts.groups}`,
    `Current history head: ${context.head}`,
  ];
  if (context.selection?.length) {
    lines.push(`Selection (${context.selection.length}):`);
    for (const item of context.selection) lines.push(`  - ${item.ref}`);
  } else {
    lines.push("Selection: (none — act on the project as a whole, or ask the lead what to select)");
  }
  return lines.join("\n");
}

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

export function buildResumeMessage({ context, message } = {}) {
  if (!context) throw new Error("buildResumeMessage requires context");
  if (!message) throw new Error("buildResumeMessage requires message");
  const refs = (context.selection || []).map((item) => item.ref).join("; ");
  const storeId = privateStoreId(context);
  const storeScope = storeId ? `; canvas store: ${storeId} (pass --store ${storeId})` : "";
  return `current head: ${context.head}${storeScope}; selection: ${refs || "(none)"}\n${String(message)}`;
}

export function checkDeniedVerbs(text) {
  const flags = [];
  if (/cli\.mjs\s+delete\b/i.test(String(text || "")) || /\bdelete(?:d)?\s+(?:the\s+)?project\b/i.test(String(text || ""))) {
    flags.push('possible project-deletion verb ("delete") mentioned in the reply — verify no project was deleted');
  }
  return flags;
}

export async function runChatTurn({
  context,
  message,
  sessionId,
  transport,
  onChild,
  onProgress,
  requestPermission,
} = {}) {
  if (!context) throw new Error("runChatTurn requires context");
  if (!message) throw new Error("runChatTurn requires message");
  const run = transport || runAppServerTransport;
  const isFirstTurn = !sessionId;
  const result = await run({
    context,
    prompt: isFirstTurn ? buildFirstTurnPrompt({ context, message }) : undefined,
    message: isFirstTurn ? undefined : buildResumeMessage({ context, message }),
    sessionId: sessionId || null,
    onChild,
    onProgress,
    requestPermission,
  });
  if (!result || typeof result.text !== "string" || !result.sessionId) {
    throw new Error("runChatTurn: transport must return { text, sessionId }");
  }
  return { text: result.text, sessionId: result.sessionId, flags: checkDeniedVerbs(result.text) };
}
