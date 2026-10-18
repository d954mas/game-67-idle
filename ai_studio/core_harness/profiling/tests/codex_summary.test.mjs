import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import { syncBuiltinESMExports } from "node:module";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { parseCodexTranscript, readCodexSessionCwd } from "../codex_transcript.mjs";

function fixture(t, contents) {
  const dir = fs.mkdtempSync(join(tmpdir(), "codex-summary-"));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  const path = join(dir, "rollout.jsonl");
  fs.writeFileSync(path, contents);
  return path;
}

test("cwd discovery reads the metadata prefix and releases its descriptor", (t) => {
  const contents = `${JSON.stringify({ type: "session_meta", payload: { cwd: "C:/studio/пример" } })}\n${"x".repeat(4 * 1024 * 1024)}`;
  const file = fixture(t, contents);
  const originalRead = fs.readSync;
  let bytesRead = 0;
  fs.readSync = (...args) => { const count = originalRead(...args); bytesRead += count; return count; };
  syncBuiltinESMExports();
  try {
    assert.equal(readCodexSessionCwd(file), "C:/studio/пример");
    assert.ok(bytesRead < Buffer.byteLength(contents) / 10);
  } finally {
    fs.readSync = originalRead;
    syncBuiltinESMExports();
  }
  fs.unlinkSync(file);
});

test("cwd discovery does not recover identity from later records", (t) => {
  const file = fixture(t, `invalid\n${JSON.stringify({ type: "session_meta", payload: { cwd: "wrong" } })}\n`);
  assert.equal(readCodexSessionCwd(file), "");
});

test("transcript parsing also computes user-turn spans", (t) => {
  const event = (minute, payload) => ({ type: "response_item", timestamp: `2026-10-18T10:${minute}:00.000Z`, payload });
  const file = fixture(t, [
    { type: "session_meta", payload: { id: "fixture", cwd: "C:/studio" } },
    event("00", { type: "message", role: "user" }),
    event("01", { type: "function_call", name: "exec_command", call_id: "a", arguments: '{"cmd":"true"}' }),
    event("03", { type: "function_call_output", call_id: "a", output: "Process exited with code 0" }),
    event("10", { type: "message", role: "user" }),
    event("11", { type: "function_call", name: "exec_command", call_id: "b", arguments: '{"cmd":"true"}' }),
    event("12", { type: "function_call_output", call_id: "b", output: "Process exited with code 0" }),
  ].map((entry) => JSON.stringify(entry)).join("\n"));
  assert.deepEqual(parseCodexTranscript(file).turns, [
    { ms: 180000, tools: 1 }, { ms: 120000, tools: 1 },
  ]);
});
