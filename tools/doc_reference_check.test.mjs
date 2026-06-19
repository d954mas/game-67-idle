import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));

function tempDir() {
  return mkdtempSync(join(tmpdir(), "doc-reference-check-test-"));
}

function cleanup(dir) {
  rmSync(dir, { recursive: true, force: true });
}

function run(args) {
  return spawnSync(process.execPath, ["tools/doc_reference_check.mjs", ...args], {
    cwd: root,
    encoding: "utf8",
    stdio: "pipe",
  });
}

function writeMinimalRoot(dir) {
  mkdirSync(join(dir, "tasks", "guides"), { recursive: true });
  mkdirSync(join(dir, ".codex", "skills", "sample", "references"), { recursive: true });
  writeFileSync(join(dir, "AGENTS.md"), "# Agents\n\nSee `tasks/README.md`.\n", "utf8");
  writeFileSync(join(dir, "AI_PIPELINE.md"), "# Pipeline\n", "utf8");
  writeFileSync(join(dir, "tasks", "README.md"), "See `tasks/guides/protocol.md`.\n", "utf8");
  writeFileSync(join(dir, "tasks", "guides", "protocol.md"), "# Protocol\n", "utf8");
  writeFileSync(
    join(dir, ".codex", "skills", "sample", "SKILL.md"),
    "See `references/detail.md` and `tasks/README.md`.\n",
    "utf8",
  );
  writeFileSync(join(dir, ".codex", "skills", "sample", "references", "detail.md"), "# Detail\n", "utf8");
}

test("doc reference check passes existing local markdown references", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    const result = run(["--root", dir]);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /markdown file\(s\) checked/);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check fails missing local markdown references", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(join(dir, "tasks", "README.md"), "See `tasks/guides/missing.md`.\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /tasks\/README\.md -> tasks\/guides\/missing\.md/);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check ignores bare backticked template names", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(join(dir, "tasks", "README.md"), "Template names: `gdd.md`, `STATUS.md`.\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 0, result.stderr);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check still fails missing markdown links", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(join(dir, "tasks", "README.md"), "See [missing](tasks/guides/missing.md).\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /tasks\/README\.md -> tasks\/guides\/missing\.md/);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check rejects retired ai validate file command", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(
      join(dir, "AI_PIPELINE.md"),
      "Old command:\n\n```powershell\nnode tools/ai.mjs validate --file AI_PIPELINE.md\n```\n",
      "utf8",
    );
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /retired command `node tools\/ai\.mjs validate --file`/);
  } finally {
    cleanup(dir);
  }
});
