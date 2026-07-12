import test from "node:test";
import assert from "node:assert/strict";
import {
  buildContextDigestText,
  buildDrivingContract,
  buildFirstTurnPrompt,
  buildResumeMessage,
  checkDeniedVerbs,
  runChatTurn,
} from "../agent.mjs";

const CONTEXT = {
  projectId: "proj-1",
  title: "Fixture Screen",
  selection: [{ ref: "canvas://proj-1/element/el_1" }],
  counts: { elements: 2, groups: 1 },
  head: 7,
};

test("driving contract keeps CLI parity, live history guard, and permission boundary", () => {
  const text = buildDrivingContract(CONTEXT);
  assert.match(text, /node ai_studio\/assets\/canvas\/cli\.mjs/);
  assert.match(text, /NO arguments/);
  assert.match(text, /HISTORY NAVIGATION GUARD \(T0234\)/);
  assert.match(text, /--expect-head N/);
  assert.match(text, /journaled, undoable/);
  assert.match(text, /delete the project/);
});

test("private contract and digest require the selected store on every command", () => {
  const context = { ...CONTEXT, storeId: "game:secret" };
  assert.match(buildDrivingContract(context), /Pass --store game:secret on every canvas CLI command/);
  assert.match(buildContextDigestText(context), /Canvas store: game:secret/);
});

test("context digest contains bounded counts, head, and selected refs", () => {
  const text = buildContextDigestText(CONTEXT);
  assert.match(text, /Fixture Screen/);
  assert.match(text, /Elements: 2, Groups: 1/);
  assert.match(text, /Current history head: 7/);
  assert.match(text, /canvas:\/\/proj-1\/element\/el_1/);
  assert.match(buildContextDigestText({ ...CONTEXT, selection: [] }), /Selection: \(none/);
});

test("first prompt carries contract, digest, and request in order", () => {
  const text = buildFirstTurnPrompt({ context: CONTEXT, message: "make it blue" });
  assert.ok(text.indexOf("ONLY through its CLI") < text.indexOf("CONTEXT DIGEST"));
  assert.ok(text.indexOf("CONTEXT DIGEST") < text.indexOf("make it blue"));
});

test("resume message is compact and preserves current head, selection, and private scope", () => {
  const text = buildResumeMessage({ context: { ...CONTEXT, storeId: "game:secret" }, message: "again" });
  assert.match(text, /^current head: 7; canvas store: game:secret/);
  assert.match(text, /canvas:\/\/proj-1\/element\/el_1/);
  assert.match(text, /again$/);
  assert.doesNotMatch(text, /HISTORY NAVIGATION GUARD/);
});

test("prompt builders reject missing required values", () => {
  assert.throws(() => buildContextDigestText(), /requires context/);
  assert.throws(() => buildFirstTurnPrompt({ message: "x" }), /requires context/);
  assert.throws(() => buildFirstTurnPrompt({ context: CONTEXT }), /requires message/);
  assert.throws(() => buildResumeMessage({ context: CONTEXT }), /requires message/);
});

test("denied-verb tripwire is stable and clean replies return an empty array", () => {
  assert.equal(checkDeniedVerbs("I ran cli.mjs delete proj-1").length, 1);
  assert.equal(checkDeniedVerbs("I refused to delete the project").length, 1);
  assert.deepEqual(checkDeniedVerbs("added image el_9"), []);
});

test("runChatTurn selects full first prompt and compact resumed message", async () => {
  const calls = [];
  const transport = async (call) => {
    calls.push(call);
    return { text: "done", sessionId: call.sessionId || "thread-new" };
  };
  const first = await runChatTurn({ context: CONTEXT, message: "one", transport });
  await runChatTurn({ context: CONTEXT, message: "two", sessionId: first.sessionId, transport });
  assert.match(calls[0].prompt, /User request:\none$/);
  assert.equal(calls[0].message, undefined);
  assert.equal(calls[1].prompt, undefined);
  assert.match(calls[1].message, /^current head: 7;/);
});

test("runChatTurn forwards child, progress, and exact permission callbacks", async () => {
  const onChild = () => {};
  const onProgress = () => {};
  const requestPermission = async () => {};
  let seen;
  await runChatTurn({
    context: CONTEXT,
    message: "x",
    onChild,
    onProgress,
    requestPermission,
    transport: async (call) => { seen = call; return { text: "ok", sessionId: "t" }; },
  });
  assert.equal(seen.onChild, onChild);
  assert.equal(seen.onProgress, onProgress);
  assert.equal(seen.requestPermission, requestPermission);
});

test("runChatTurn validates input and transport result loudly", async () => {
  await assert.rejects(runChatTurn({ message: "x", transport: async () => ({}) }), /requires context/);
  await assert.rejects(runChatTurn({ context: CONTEXT, transport: async () => ({}) }), /requires message/);
  await assert.rejects(
    runChatTurn({ context: CONTEXT, message: "x", transport: async () => ({ text: "x" }) }),
    /transport must return/,
  );
});
