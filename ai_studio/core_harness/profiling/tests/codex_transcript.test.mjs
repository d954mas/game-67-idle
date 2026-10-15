import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import { syncBuiltinESMExports } from "node:module";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { parseCodexTranscript } from "../codex_transcript.mjs";

function fixture(t, text) {
  const directory = fs.mkdtempSync(join(tmpdir(), "codex-lines-"));
  t.after(() => fs.rmSync(directory, { recursive: true, force: true }));
  const file = join(directory, "session.jsonl");
  fs.writeFileSync(file, text);
  return file;
}

function interceptReads(file, body, { chunkSize = 1, failAfter = Infinity } = {}) {
  const original = { openSync: fs.openSync, readSync: fs.readSync,
    readFileSync: fs.readFileSync, closeSync: fs.closeSync };
  let descriptor, reads = 0, closes = 0;
  fs.openSync = (...args) => {
    const opened = original.openSync(...args);
    if (args[0] === file) descriptor = opened;
    return opened;
  };
  fs.readFileSync = (...args) => {
    if (args[0] === file) {
      throw Object.assign(new RangeError("whole-file transcript string exceeds V8 limit"), { code: "ERR_STRING_TOO_LONG" });
    }
    return original.readFileSync(...args);
  };
  fs.readSync = (fd, buffer, offset, length, position) => {
    if (fd !== descriptor) return original.readSync(fd, buffer, offset, length, position);
    assert.ok(length <= 64 * 1024, "transcript reads must use a bounded buffer");
    if (reads++ >= failAfter) throw new Error("synthetic read failure");
    return original.readSync(fd, buffer, offset, Math.min(length, chunkSize), position);
  };
  fs.closeSync = (fd) => {
    if (fd === descriptor) closes += 1;
    return original.closeSync(fd);
  };
  syncBuiltinESMExports();
  try { return body(() => ({ reads, closes })); }
  finally { Object.assign(fs, original); syncBuiltinESMExports(); }
}

const meta = (id) => JSON.stringify({ type: "session_meta", payload: { id } });

test("Codex parsing streams split UTF8 and CRLF, retaining malformed and final-line numbers", (t) => {
  const output = "Exit code: 0\r\nПривет 🐈\r\nфинал";
  const file = fixture(t, [meta("ветка 🐈"), "", " \t", "{malformed",
    JSON.stringify({ type: "response_item", timestamp: "2026-10-16T00:00:00Z", payload: {
      type: "function_call", name: "exec_command", call_id: "call", arguments: JSON.stringify({ cmd: "echo Привет 🐈" }) } }),
    JSON.stringify({ type: "response_item", timestamp: "2026-10-16T00:00:01Z", payload: {
      type: "function_call_output", call_id: "call", output } }),
  ].join("\r\n"));
  interceptReads(file, (metrics) => {
    const result = parseCodexTranscript(file);
    assert.equal(typeof result.then, "undefined", "public parser remains synchronous");
    assert.equal(result.records.length, 2);
    assert.equal(result.records[0].session_id, "ветка 🐈");
    assert.equal(result.records[0].__line, 1);
    assert.equal(result.records[1].__line, 6);
    assert.deepEqual(result.records[1].commands, ["echo Привет 🐈"]);
    assert.equal(result.records[1].output_chars, output.length);
    assert.equal(result.records[1].output_lines, 3);
    assert.equal(result.records[1].duration_ms, 1000);
    assert.equal(result.errors.length, 1);
    assert.match(result.errors[0], /^line 4: invalid JSON:/);
    assert.ok(metrics().reads > 1);
    assert.equal(metrics().closes, 1);
  });
});

test("Codex parsing retains a record spanning full chunks and final newline semantics", (t) => {
  const id = "Я🐈".repeat(16000);
  const file = fixture(t, `${meta(id)}\r\n{broken}\r\n${meta("last")}\r\n`);
  interceptReads(file, (metrics) => {
    const result = parseCodexTranscript(file);
    assert.deepEqual(result.records.map((record) => [record.__line, record.session_id]), [[1, id], [3, "last"]]);
    assert.equal(result.errors.length, 1);
    assert.match(result.errors[0], /^line 2: invalid JSON:/);
    assert.ok(metrics().reads >= 3);
    assert.equal(metrics().closes, 1);
  }, { chunkSize: 64 * 1024 });
});

test("Codex streaming closes its descriptor when a chunk read fails", (t) => {
  const file = fixture(t, `${meta("session")}\n`);
  interceptReads(file, (metrics) => {
    assert.throws(() => parseCodexTranscript(file), /synthetic read failure/);
    assert.equal(metrics().closes, 1);
  }, { failAfter: 1 });
});
