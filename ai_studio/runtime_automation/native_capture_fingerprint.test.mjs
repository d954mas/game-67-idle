import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const script = fileURLToPath(new URL("./native_capture_fingerprint.mjs", import.meta.url));

function fixture(t, source) {
  const studio = mkdtempSync(join(tmpdir(), "capture-fingerprint-"));
  t.after(() => rmSync(studio, { recursive: true, force: true }));
  const game = join(studio, "games", "fixture");
  const module = join(game, "tools", "lib", "runtime_build.mjs");
  mkdirSync(dirname(module), { recursive: true });
  writeFileSync(module, source);
  // A game that still carries copied tooling; its packager is the marker.
  writeFileSync(join(game, "tools", "package_web.mjs"), "");
  return { studio, game };
}

function run({ studio, game }) {
  return spawnSync(process.execPath, [script, game, studio], { encoding: "utf8", windowsHide: true });
}

test("native capture uses source evidence without inventing a web compile profile", (t) => {
  const item = fixture(t, `
    export function createRuntimeBuildRecord() { throw new Error('web profile required'); }
    export function createRuntimeSourceRecord(options) {
      if ('compileProfile' in options) throw new Error('unexpected web profile');
      return { schema: 'ai_studio.runtime_source.v1', fingerprint: '${"a".repeat(64)}' };
    }
  `);
  const result = run(item);
  assert.equal(result.status, 0, result.stderr);
  assert.equal(JSON.parse(result.stdout).schema, "ai_studio.runtime_source.v1");
});

test("native capture retains legacy game build-record support", (t) => {
  const result = run(fixture(t, `
    export function createRuntimeBuildRecord() {
      return { schema: 'ai_studio.runtime_build.v1', fingerprint: '${"b".repeat(64)}' };
    }
  `));
  assert.equal(result.status, 0, result.stderr);
  assert.equal(JSON.parse(result.stdout).schema, "ai_studio.runtime_build.v1");
});

test("native capture rejects a game module that escapes through a directory link", (t) => {
  const item = fixture(t, "throw new Error('outside module executed');");
  const outside = mkdtempSync(join(tmpdir(), "capture-fingerprint-outside-"));
  t.after(() => rmSync(outside, { recursive: true, force: true }));
  const tools = join(item.game, "tools");
  rmSync(tools, { recursive: true });
  mkdirSync(join(outside, "lib"));
  writeFileSync(join(outside, "lib", "runtime_build.mjs"), "throw new Error('outside module executed');");
  writeFileSync(join(outside, "package_web.mjs"), "");
  symlinkSync(outside, tools, process.platform === "win32" ? "junction" : "dir");
  const result = run(item);
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /escapes|inside/i);
  assert.doesNotMatch(result.stderr, /outside module executed/);
});

test("a game without copied tooling is fingerprinted by the Studio module", (t) => {
  const studio = mkdtempSync(join(tmpdir(), "capture-fingerprint-studio-"));
  t.after(() => rmSync(studio, { recursive: true, force: true }));
  const game = join(studio, "games", "fixture");
  mkdirSync(join(game, "tools"), { recursive: true });
  const module = join(studio, "ai_studio", "game_tools", "lib", "runtime_build.mjs");
  mkdirSync(dirname(module), { recursive: true });
  writeFileSync(module, `export function createRuntimeSourceRecord() { return { schema: 'studio-module' }; }`);
  const result = run({ studio, game });
  assert.equal(result.status, 0, result.stderr);
  assert.equal(JSON.parse(result.stdout).schema, "studio-module");
});
