import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

import { golden, readBank } from "./test_goldens.mjs";

function scratch() {
  return mkdtempSync(join(tmpdir(), "test-goldens-"));
}

test("recording stores the actual value and keeps the bank sorted", () => {
  const dir = scratch();
  try {
    const env = { GAME_GOLDENS_DIR: dir, GAME_UPDATE_GOLDENS: "1" };
    golden("web_budget", "wasm_bytes", 1234, env);
    golden("web_budget", "assets_bytes", 99, env);
    assert.equal(readFileSync(join(dir, "web_budget.golden"), "utf8"), "assets_bytes = 99\nwasm_bytes = 1234\n");
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("comparing returns the recorded value so the caller's assertion fails", () => {
  const dir = scratch();
  try {
    golden("web_budget", "wasm_bytes", 1234, { GAME_GOLDENS_DIR: dir, GAME_UPDATE_GOLDENS: "1" });
    const compare = { GAME_GOLDENS_DIR: dir };
    assert.equal(golden("web_budget", "wasm_bytes", 4321, compare), 1234);
    assert.equal(readBank("web_budget", compare).get("wasm_bytes"), "1234");
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("an unrecorded key names the command that records it", () => {
  const dir = scratch();
  try {
    assert.throws(() => golden("web_budget", "missing", 1, { GAME_GOLDENS_DIR: dir }), /--update-goldens/);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("text goldens stay on one line", () => {
  const dir = scratch();
  try {
    const env = { GAME_GOLDENS_DIR: dir, GAME_UPDATE_GOLDENS: "1" };
    assert.equal(golden("copy", "title", "Planet Eater", env), "Planet Eater");
    assert.throws(() => golden("copy", "body", "two\nlines", env), /one line/);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});
