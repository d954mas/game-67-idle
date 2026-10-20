import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import { hashRuntimeBuildTree } from "../lib/runtime_hash_tree.cjs";

function write(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, value);
}

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "runtime-hash-tree-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return root;
}

function expected(rows) {
  const digest = createHash("sha256");
  for (const [path, value] of rows) {
    const bytes = Buffer.from(value, "utf8");
    digest.update(path).update("\0").update(String(bytes.length)).update("\0")
      .update(createHash("sha256").update(bytes).digest("hex")).update("\n");
  }
  return { files: rows.length, sha256: digest.digest("hex") };
}

test("runtime tree hash serializes selected files in deterministic path order", (t) => {
  const root = fixture(t);
  write(join(root, "src", "z.c"), "z\n");
  write(join(root, "src", "a.c"), "a\n");
  write(join(root, "README.md"), "documentation\n");
  write(join(root, "tests", "tree.test.mjs"), "test\n");
  write(join(root, "design", "items", "balance.lua"), "return {}\n");
  write(join(root, "design", "data", "selected.json"), "{}\n");

  assert.deepEqual(hashRuntimeBuildTree({
    root,
    ignoredRoots: new Set(["design", "tests"]),
    ignoredAnywhere: new Set(),
    extraRoots: ["design/items"],
    extraFiles: ["design/data/selected.json"],
  }), expected([
    ["src/a.c", "a\n"],
    ["src/z.c", "z\n"],
    ["design/items/balance.lua", "return {}\n"],
    ["design/data/selected.json", "{}\n"],
  ]));
});

test("runtime tree hash delegates game-local exclusions without widening selected inputs", (t) => {
  const root = fixture(t);
  write(join(root, "src", "keep.c"), "keep\n");
  write(join(root, "deploy", "secrets", "token.txt"), "private\n");

  assert.deepEqual(hashRuntimeBuildTree({
    root,
    ignoredRoots: new Set(),
    ignoredAnywhere: new Set(),
    shouldIgnoreEntry: ({ prefix, name }) => prefix === "deploy/secrets" && name === "token.txt",
  }), expected([["src/keep.c", "keep\n"]]));
});

test("runtime tree hash streams a file larger than one chunk and detects a byte change", (t) => {
  const root = fixture(t);
  const path = join(root, "assets", "payload.bin");
  const first = Buffer.alloc(1024 * 1024 + 257, 0x5a);
  first[1024 * 1024] = 0x31;
  write(path, first);

  const before = hashRuntimeBuildTree({ root, ignoredRoots: new Set(), ignoredAnywhere: new Set() });
  assert.deepEqual(before, expected([["assets/payload.bin", first]]));

  const second = Buffer.from(first);
  second[second.length - 1] ^= 0xff;
  write(path, second);
  const after = hashRuntimeBuildTree({ root, ignoredRoots: new Set(), ignoredAnywhere: new Set() });
  assert.equal(after.files, 1);
  assert.notEqual(after.sha256, before.sha256);
});

test("runtime tree hash rejects selected paths that use a symbolic link or escape the root", (t) => {
  const root = fixture(t);
  const outside = fixture(t);
  write(join(outside, "selected.json"), "outside\n");
  try {
    symlinkSync(outside, join(root, "design"), process.platform === "win32" ? "junction" : "dir");
  } catch (error) {
    if (["EPERM", "EACCES", "ENOTSUP"].includes(error.code)) return t.skip(`symlink unavailable: ${error.code}`);
    throw error;
  }

  assert.throws(() => hashRuntimeBuildTree({
    root,
    ignoredRoots: new Set(["design"]),
    ignoredAnywhere: new Set(),
    extraFiles: ["design/selected.json"],
  }), /symbolic link|escapes/i);
});

test("runtime tree hash rejects malformed selected relative paths", (t) => {
  const root = fixture(t);
  write(join(root, "src", "main.c"), "main\n");
  for (const extraFiles of [["../outside"], ["src//main.c"], ["src\\main.c"], ["C:main.c"], ["src/main.c:stream"], ["src/\0main.c"]]) {
    assert.throws(() => hashRuntimeBuildTree({
      root,
      ignoredRoots: new Set(),
      ignoredAnywhere: new Set(),
      extraFiles,
    }), /relative path is invalid/i);
  }
});
