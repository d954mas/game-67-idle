import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { runInNewContext } from "node:vm";

import { createStoreZip } from "../../core_harness/tool_lib/zip_store.mjs";
import { checkPlan, createPlanRunner, frameStats, keyEvent, parseFlags, readJsonInput, storageScripts, unpackStoreZip, viewportFor, VIEWPORTS } from "../lib/web_page_plan.mjs";
import { webShot } from "../web_shot.mjs";

function runner(viewport = VIEWPORTS.desktop, frames = []) {
  const calls = [];
  const shots = [];
  const { run, tap } = createPlanRunner({
    viewport,
    sleep: async () => {},
    shot: async (name) => { shots.push(name); },
    call: async (method, params) => {
      calls.push([method, params]);
      if (method !== "Runtime.evaluate") return {};
      return { result: { value: params.expression.includes("slice") ? frames : 0 } };
    },
  });
  return { run, tap, calls, shots };
}

test("flags are value pairs and an unknown one is refused", () => {
  assert.deepEqual(parseFlags(["--zip", "a.zip", "--out", "o"], ["zip", "out"]), { zip: "a.zip", out: "o" });
  assert.throws(() => parseFlags(["--save-kye", "x"], ["save-key"]), /unknown argument --save-kye/);
  assert.throws(() => parseFlags(["--zip"], ["zip"]), /--zip needs a value/);
});

test("a plan with an unknown step or key is refused before Chrome starts", () => {
  assert.ok(checkPlan([["wait", 1], ["hold", "w", 1], ["keys", ["a", " "], 1], ["key", "Escape", 1]]));
  assert.throws(() => checkPlan([["fly", 1]]), /unknown plan step/);
  assert.throws(() => checkPlan([["hold", "F1", 1]]), /unknown key/);
  assert.throws(() => checkPlan({ wait: 1 }), /JSON array/);
  assert.throws(() => viewportFor("watch"), /unknown mode watch/);
});

test("plan JSON comes as text or as a file", () => {
  const dir = mkdtempSync(join(tmpdir(), "web-plan-"));
  try {
    const file = join(dir, "plan.json");
    writeFileSync(file, '[["shot","a"]]');
    assert.deepEqual(readJsonInput(file), [["shot", "a"]]);
    assert.deepEqual(readJsonInput('[["shot","b"]]'), [["shot", "b"]]);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("letters map to their key codes", () => {
  assert.deepEqual(keyEvent("w"), { key: "w", code: "KeyW", windowsVirtualKeyCode: 87, nativeVirtualKeyCode: 87 });
  assert.equal(keyEvent(" ").code, "Space");
});

test("a click taps on a touch viewport and presses the mouse on a desktop one", async () => {
  const phone = runner(VIEWPORTS.phone);
  await phone.run([["click", 5, 6]]);
  assert.deepEqual(phone.calls.map(([, params]) => params.type), ["touchStart", "touchEnd"]);
  const desktop = runner(VIEWPORTS.desktop);
  await desktop.run([["click", 5, 6]]);
  assert.deepEqual(desktop.calls.map(([, params]) => params.type), ["mouseMoved", "mousePressed", "mouseReleased"]);
});

test("every plan step reaches the page in order", async () => {
  const { run, calls, shots } = runner(VIEWPORTS.desktop, [0, 10, 20, 50]);
  const perf = await run([["shot", "a"], ["burst", "b", 2, 0], ["walkburst", "w", "c", 2, 0], ["keys", ["a", "d"], 0], ["hold", "s", 0],
    ["move", 1, 2], ["blur"], ["perf", 0, "walk"]]);
  assert.deepEqual(shots, ["a", "b0", "b1", "c0", "c1"]);
  const keys = calls.filter(([method]) => method === "Input.dispatchKeyEvent").map(([, params]) => `${params.type}:${params.key}`);
  assert.deepEqual(keys, ["rawKeyDown:w", "keyUp:w", "rawKeyDown:a", "rawKeyDown:d", "keyUp:a", "keyUp:d", "rawKeyDown:s", "keyUp:s"]);
  assert.deepEqual(perf.walk, { n: 3, mean: 16.67, p95: 30 });
});

test("frame stats survive a page that drew nothing", () => {
  assert.deepEqual(frameStats([]), { n: 0, mean: null, p95: null });
});

test("the save goes in once per tab under its key, storage entries on every load", () => {
  assert.throws(() => storageScripts({ save: "{}" }), /--save needs --save-key/);
  const [saveScript, storageScript] = storageScripts({ save: "S1", saveKey: "game/slot", storage: { "game/other": "O" } });
  const store = () => { const data = new Map(); return { data, getItem: (k) => data.get(k) ?? null, setItem: (k, v) => data.set(k, v) }; };
  const localStorage = store();
  const sessionStorage = store();
  const scope = { localStorage, sessionStorage, console };
  runInNewContext(saveScript, scope);
  runInNewContext(storageScript, scope);
  assert.equal(localStorage.data.get("game/slot"), "S1");
  assert.equal(localStorage.data.get("game/other"), "O");
  localStorage.data.set("game/slot", "written by the game");
  localStorage.data.delete("game/other");
  runInNewContext(saveScript, scope);
  runInNewContext(storageScript, scope);
  assert.equal(localStorage.data.get("game/slot"), "written by the game");
  assert.equal(localStorage.data.get("game/other"), "O");
});

test("a store zip unpacks with its folders", () => {
  const dir = mkdtempSync(join(tmpdir(), "web-plan-zip-"));
  const zip = join(dir, "a.zip");
  writeFileSync(zip, createStoreZip([{ path: "index.html", bytes: Buffer.from("x") }, { path: "packs/boot.ntpack", bytes: Buffer.from("y") }]));
  const site = unpackStoreZip(zip);
  try {
    assert.equal(readFileSync(join(site, "packs", "boot.ntpack"), "utf8"), "y");
    assert.ok(existsSync(join(site, "index.html")));
  } finally {
    rmSync(site, { recursive: true, force: true });
    rmSync(dir, { recursive: true, force: true });
  }
});

test("the CLI refuses a bad call before unpacking or starting Chrome", async () => {
  await assert.rejects(webShot(["--out", "o"]), /--zip <package\.zip> and --out <dir> are required/);
  await assert.rejects(webShot(["--zip", "missing.zip", "--out", "o", "--plan", '[["fly"]]']), /unknown plan step/);
  await assert.rejects(webShot(["--zip", "missing.zip", "--out", "o", "--mode", "watch"]), /unknown mode/);
});
