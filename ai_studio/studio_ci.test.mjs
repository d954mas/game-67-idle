import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

const workflow = readFileSync(new URL("../.github/workflows/studio-verify.yml", import.meta.url), "utf8");

test("Studio CI is a Windows Ubuntu matrix with pinned current setup contracts", () => {
  assert.match(workflow, /matrix:\s*[\s\S]*os:\s*\[ubuntu-latest, windows-latest\]/);
  assert.match(workflow, /uses: actions\/checkout@v7/);
  assert.match(workflow, /submodules: recursive/);
  assert.match(workflow, /uses: actions\/setup-node@v6/);
  assert.match(workflow, /node-version: ['"]24['"]/);
  assert.match(workflow, /package-manager-cache: false/);
  assert.match(workflow, /uses: actions\/setup-python@v6/);
  assert.match(workflow, /python-version: ['"]3\.12['"]/);
  assert.match(workflow, /python_setup\.mjs --base-python "\$pythonLocation\/bin\/python"/);
  assert.match(workflow, /python_setup\.mjs --base-python "\$env:pythonLocation\\python\.exe"/);
  assert.match(workflow, /name: Install native Linux dependencies/);
  assert.match(workflow, /apt-get install -y --no-install-recommends/);
  for (const packageName of [
    "libwayland-bin",
    "libwayland-dev",
    "wayland-protocols",
    "libxkbcommon-dev",
    "libxrandr-dev",
    "libxinerama-dev",
    "libxcursor-dev",
    "libxi-dev",
    "libgl1-mesa-dev",
  ]) {
    assert.match(workflow, new RegExp(`\\b${packageName}\\b`));
  }
  assert.match(workflow, /EMSDK_VERSION: ['"]4\.0\.10['"]/);
  assert.match(workflow, /github\.com\/emscripten-core\/emsdk\.git/);
  assert.match(workflow, /emsdk install "\$EMSDK_VERSION"/);
  assert.match(workflow, /emsdk activate "\$EMSDK_VERSION"/);
  assert.match(workflow, /echo "EMSDK=\$EMSDK_PATH" >> "\$GITHUB_ENV"/);
  assert.doesNotMatch(workflow, /source\s+[^\n]*emsdk_env\.sh/);
});

test("blocking full verification is separate from advisory timing", () => {
  assert.match(workflow, /name: Blocking Studio verification[\s\S]*node ai_studio\/studio\.mjs verify --full/);
  assert.doesNotMatch(workflow, /Build real reference-template wasm release|Assert wasm release artifact/);
  assert.match(workflow, /name: Advisory timing[\s\S]*continue-on-error: true/);
  assert.match(workflow, /toolchain_benchmark\.mjs --game template --samples 1/);
  assert.doesNotMatch(workflow, /--game (?!template)[^\s]+/);
});
