import test from "node:test";
import assert from "node:assert/strict";
import { join, resolve } from "node:path";
import { toPosix, relCwdPosix } from "./paths.mjs";

test("toPosix converts backslashes to forward slashes", () => {
  assert.equal(toPosix("a\\b\\c"), "a/b/c");
  assert.equal(toPosix("already/posix"), "already/posix");
  assert.equal(toPosix(""), "");
  assert.equal(toPosix(null), "");
  assert.equal(toPosix(undefined), "");
});

test("relCwdPosix returns a posix path relative to cwd", () => {
  const abs = join(process.cwd(), "sub", "deep", "f.txt");
  assert.equal(relCwdPosix(abs), "sub/deep/f.txt");
});

test("relCwdPosix returns the original string for paths outside cwd", () => {
  const parent = resolve(process.cwd(), "..");
  assert.equal(relCwdPosix(parent), parent);
});
