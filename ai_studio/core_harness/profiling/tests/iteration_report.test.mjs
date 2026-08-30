import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

import {
  buildIterationReport,
  claudeProjectDirs,
  parseClaudeTranscript,
  renderIterationReport,
} from "../iteration_report.mjs";

function tempDir() {
  return mkdtempSync(join(tmpdir(), "ai-iteration-report-"));
}

function claudeLines(day) {
  const at = (minute, second = 0) => `2026-08-${day}T10:${String(minute).padStart(2, "0")}:${String(second).padStart(2, "0")}.000Z`;
  return [
    { type: "user", timestamp: at(0), message: { role: "user", content: "fix the shop screen" } },
    {
      type: "assistant",
      timestamp: at(1),
      message: { role: "assistant", content: [{ type: "tool_use", id: "t1", name: "Bash", input: { command: "rg --files tmp" } }] },
    },
    {
      type: "user",
      timestamp: at(3),
      toolUseResult: { stdout: "x".repeat(5000), stderr: "" },
      message: { role: "user", content: [{ type: "tool_result", tool_use_id: "t1", content: "ok" }] },
    },
    {
      type: "assistant",
      timestamp: at(4),
      message: { role: "assistant", content: [{ type: "tool_use", id: "t2", name: "Bash", input: { command: "ctest --test-dir build/native-debug" } }] },
    },
    {
      type: "user",
      timestamp: at(5),
      toolUseResult: { stdout: "100% tests passed", stderr: "" },
      message: { role: "user", content: [{ type: "tool_result", tool_use_id: "t2", is_error: true, content: "boom" }] },
    },
  ].map((line) => JSON.stringify(line)).join("\n");
}

function codexLines(day) {
  const at = (minute) => `2026-08-${day}T11:${String(minute).padStart(2, "0")}:00.000Z`;
  return [
    { timestamp: at(0), type: "session_meta", payload: { id: "sess-1", cwd: "C:\\work\\demo-repo" } },
    { timestamp: at(1), type: "response_item", payload: { type: "message", role: "user", content: [{ type: "input_text", text: "port the helper" }] } },
    {
      timestamp: at(2),
      type: "response_item",
      payload: { type: "function_call", call_id: "c1", name: "shell", arguments: JSON.stringify({ command: "cat games/private/demo/tests/huge.c" }) },
    },
    {
      timestamp: at(4),
      type: "response_item",
      payload: {
        type: "function_call_output",
        call_id: "c1",
        output: "Script completed\nWarning: truncated output (original token count: 12345)\n" + "y".repeat(2000),
      },
    },
  ].map((line) => JSON.stringify(line)).join("\n");
}

test("claude transcripts yield tool records and turn shape", () => {
  const dir = tempDir();
  try {
    const file = join(dir, "session.jsonl");
    writeFileSync(file, claudeLines("20"), "utf8");
    const parsed = parseClaudeTranscript(file);
    assert.equal(parsed.records.length, 2);
    assert.equal(parsed.records[0].category, "research");
    assert.equal(parsed.records[0].duration_ms, 120000);
    assert.equal(parsed.records[1].result, "fail");
    assert.equal(parsed.turns.length, 1);
    assert.equal(parsed.turns[0].tools, 2);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("project directories are matched by the flattened cwd slug", () => {
  const dir = tempDir();
  try {
    const repo = join(dir, "repo", "demo-repo");
    const projects = join(dir, "projects");
    const slug = repo.replace(/[\\/:]/g, "-").replace(/^-+/, "");
    mkdirSync(join(projects, slug), { recursive: true });
    mkdirSync(join(projects, "C--other-repo"), { recursive: true });
    const found = claudeProjectDirs(projects, repo);
    assert.equal(found.length, 1);
    assert.match(found[0], /demo-repo$/);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("the report totals both harnesses and keeps truncated tokens visible", () => {
  const dir = tempDir();
  try {
    const repo = join(dir, "demo-repo");
    mkdirSync(repo, { recursive: true });
    const projects = join(dir, "projects");
    const slug = repo.replace(/[\\/:]/g, "-").replace(/^-+/, "");
    mkdirSync(join(projects, slug), { recursive: true });
    writeFileSync(join(projects, slug, "session.jsonl"), claudeLines("20"), "utf8");

    const codex = join(dir, "codex", "2026", "08", "20");
    mkdirSync(codex, { recursive: true });
    writeFileSync(join(codex, "rollout-demo.jsonl"), codexLines("20"), "utf8");

    const report = buildIterationReport({
      repoRoot: repo,
      claudeProjects: projects,
      codexSessions: codex,
      sinceMs: Date.parse("2026-08-19T00:00:00.000Z"),
      now: Date.parse("2026-08-21T00:00:00.000Z"),
    });

    assert.equal(report.schema, "ai_studio.profiling.iteration_report.v1");
    assert.equal(report.harness.claude.sessions, 1);
    assert.equal(report.harness.claude.calls, 2);
    assert.equal(report.harness.codex.sessions, 1);
    assert.equal(report.harness.codex.truncated_tokens, 12345);
    assert.ok(report.harness.claude.fail_rate > 0);
    assert.ok(report.categories.some((category) => category.name === "research"));
    assert.ok(report.noisiest_commands[0].output_mb > 0);

    const text = renderIterationReport(report);
    assert.match(text, /Iteration report/);
    assert.match(text, /Truncated output/);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("a window with no transcripts renders without throwing", () => {
  const dir = tempDir();
  try {
    const report = buildIterationReport({
      repoRoot: join(dir, "demo-repo"),
      claudeProjects: join(dir, "missing-projects"),
      codexSessions: join(dir, "missing-codex"),
      sinceMs: Date.parse("2026-08-19T00:00:00.000Z"),
      now: Date.parse("2026-08-21T00:00:00.000Z"),
    });
    assert.equal(report.harness.claude.calls, 0);
    assert.equal(report.categories.length, 0);
    assert.match(renderIterationReport(report), /harness  sessions/);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});
