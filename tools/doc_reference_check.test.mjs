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

test("doc reference check ignores historical task archive links", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    mkdirSync(join(dir, "tasks", "archive", "E999"), { recursive: true });
    writeFileSync(
      join(dir, "tasks", "archive", "E999", "T9999-old-prototype.md"),
      "Archived task references removed prototype artifact `gamedesign/projects/old-game/gdd.md`.\n",
      "utf8",
    );
    const result = run(["--root", dir]);
    assert.equal(result.status, 0, result.stderr);
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

test("doc reference check rejects retired deep reflection command", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(
      join(dir, "AI_PIPELINE.md"),
      "Old command:\n\n```powershell\nnode tools/ai.mjs reflect --deep\n```\n",
      "utf8",
    );
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /retired command `node tools\/ai\.mjs reflect --deep`/);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check rejects direct pipeline validator command in docs", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(
      join(dir, "AI_PIPELINE.md"),
      "Old command:\n\n```powershell\nnode tools/pipeline_validate.mjs --review\n```\n",
      "utf8",
    );
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /internal command `node tools\/pipeline_validate\.mjs`/);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check rejects retired context pressure wording", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(join(dir, "AI_PIPELINE.md"), "Use --review for strict context pressure.\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /retired phrase `context pressure`/);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check passes an existing non-markdown tool reference", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    mkdirSync(join(dir, "tools"), { recursive: true });
    writeFileSync(join(dir, "tools", "sample_tool.mjs"), "// sample\n", "utf8");
    writeFileSync(join(dir, "tasks", "README.md"), "Run `tools/sample_tool.mjs`.\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 0, result.stderr);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check fails a missing non-markdown tool reference", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(join(dir, "tasks", "README.md"), "Run `tools/missing_tool.mjs`.\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /tools\/missing_tool\.mjs/);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check tolerates an omitted regenerated subsystem", () => {
  // The portable export base omits tools/devapi + tools/state_codegen; a skill
  // referencing them must not fail when the whole subsystem dir is absent.
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(
      join(dir, ".codex", "skills", "sample", "SKILL.md"),
      "See `references/detail.md`, `tools/devapi/devapi_client.py`, and `tools/state_codegen/generate_state.py`.\n",
      "utf8",
    );
    const result = run(["--root", dir]);
    assert.equal(result.status, 0, result.stderr);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check still fails a missing file inside a present subsystem", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    mkdirSync(join(dir, "tools", "devapi"), { recursive: true });
    writeFileSync(
      join(dir, ".codex", "skills", "sample", "SKILL.md"),
      "See `references/detail.md` and `tools/devapi/missing_client.py`.\n",
      "utf8",
    );
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /tools\/devapi\/missing_client\.py/);
  } finally {
    cleanup(dir);
  }
});
