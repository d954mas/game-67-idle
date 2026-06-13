import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "game-context-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function writeStarter(root) {
  mkdirSync(join(root, "tools", "game_context"), { recursive: true });
  mkdirSync(join(root, "tools", "taskboard"), { recursive: true });
  mkdirSync(join(root, "gamedesign", "meme-evolution", "data"), { recursive: true });
  mkdirSync(join(root, "src"), { recursive: true });
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(join(root, "AGENTS.md"), `# AGENTS.md

## Project

- Active game concept: \`67 World\`, child-friendly meme merge/evolution game.

## Direction

- The current product target is a release-quality native PC/mobile game.
- The current runtime surface in \`src/main.c\` is the active native game.
- Reference study is a hard implementation gate. If a user names a reference
  game/style, do not implement gameplay from memory.
- For polished, child-testable, generated-art, UI, or release-quality visual
  work, use the visual art direction skill.

## Validation

- Product target is mobile + PC. Native desktop/PC is the preferred
  development and automation harness once implementation starts.
- Hard gate: for playable game work, do not create, serve, validate, or pivot to
  a web prototype/page/app because it seems faster or prettier. Use the native
  PC build first.
`, "utf8");
  writeFileSync(join(root, "tasks", "STATUS.md"), `# Project Status

## Current Gate

Native PC review gate.

## Next Priorities

1. Run child-test acceptance.

## Blocking Work

- Manual child-test is required.

## Required Validation

\`\`\`powershell
cmake --build --preset native-debug
\`\`\`
`, "utf8");
  writeFileSync(join(root, "tools", "taskboard", "cli.mjs"), `#!/usr/bin/env node
import { readFileSync } from "node:fs";
console.log(readFileSync("tasks/STATUS.md", "utf8"));
`, "utf8");
  writeFileSync(join(root, "CMakePresets.json"), "{}\n", "utf8");
  writeFileSync(join(root, "src", "main.c"), "int main(void){return 0;}\n", "utf8");
  writeFileSync(join(root, "gamedesign", "meme-evolution", "gdd.md"), "# GDD\n", "utf8");
  writeFileSync(join(root, "gamedesign", "meme-evolution", "data", "balance.json"), "{}\n", "utf8");
}

test("game iteration context preserves wrapped hard gates", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  writeStarter(fixture);
  const json = join(fixture, "tmp", "context.json");
  const result = spawnSync(process.execPath, [
    join(repoRoot, "tools", "game_context", "iteration_context.mjs"),
    "--json-output",
    json,
  ], {
    cwd: fixture,
    encoding: "utf8",
    shell: false,
  });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /Game Iteration Context Pack/);
  assert.match(result.stdout, /Native desktop\/PC is the preferred development/);
  assert.match(result.stdout, /do not create, serve, validate, or pivot to a web prototype/);
  assert.equal(existsSync(json), true);
  const context = JSON.parse(readFileSync(json, "utf8"));
  assert.equal(context.concept.includes("67 World"), true);
  assert.ok(context.hard_gates.some((gate) => gate.includes("web prototype/page/app")));
  assert.ok(context.runtime_sources.includes("src/main.c"));
  assert.ok(context.design_sources.includes("gamedesign/meme-evolution/gdd.md"));
  assert.ok(context.before_coding_checklist.some((item) => item.includes("runtime harness")));
});
