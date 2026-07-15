import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { delimiter, dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";

import { studioPythonPath } from "./python.mjs";

const root = resolve(import.meta.dirname, "../..");
const runner = "node ai_studio/dev_environment/python_run.mjs";
const runnerPath = join(root, "ai_studio", "dev_environment", "python_run.mjs");
const activeContracts = [
  "features/game-state/README.md",
  "features/game-state/INSTALL.md",
  "features/items-core/README.md",
  "features/items-core/INSTALL.md",
  "features/items-core/scripts/generate_items_catalog.py",
  "features/items-core/scripts/items_ops.py",
  "features/items-core/scripts/items_ops_test.py",
  "features/items-core/scripts/items_cli.py",
  "features/items-core/scripts/items_cli_test.py",
  "features/progression-core/README.md",
  "features/progression-core/scripts/generate_progression_tracks.py",
  "templates/template/README.md",
  "templates/template/devapi/README.md",
  "ai_studio/assets/items_viewer/README.md",
  "ai_studio/assets/canvas/README.md",
  "ai_studio/assets/tools/image/README.md",
  "ai_studio/assets/tools/image/_bridge/README.md",
  "ai_studio/assets/tools/image/alpha_dualplate/dual_plate_pair_gate.py",
  ".codex/skills/nt-asset-image-generation/scripts/slice_pack.py",
  ".codex/skills/nt-asset-image-generation/references/throughput-and-handoff.md",
];

function pythonFixture(t, pythonPath = ".venv") {
  const fixtureRoot = mkdtempSync(join(tmpdir(), "studio-python-"));
  t.after(() => rmSync(fixtureRoot, { recursive: true, force: true }));
  mkdirSync(join(fixtureRoot, "ai_studio"), { recursive: true });
  writeFileSync(join(fixtureRoot, "ai_studio", "studio.config.json"), JSON.stringify({
    schema: "ai_studio.studio_config.v1", pythonPath,
  }));
  return fixtureRoot;
}

test("studioPythonPath resolves the configured root venv on Windows and POSIX", (t) => {
  const fixtureRoot = pythonFixture(t);
  const windowsPython = join(fixtureRoot, ".venv", "Scripts", "python.exe");
  const posixPython = join(fixtureRoot, ".venv", "bin", "python");
  mkdirSync(dirname(windowsPython), { recursive: true });
  mkdirSync(dirname(posixPython), { recursive: true });
  writeFileSync(join(fixtureRoot, ".venv", "pyvenv.cfg"), "home = fixture\n");
  writeFileSync(windowsPython, "fixture");
  writeFileSync(posixPython, "fixture");

  assert.equal(studioPythonPath(fixtureRoot, "win32"), windowsPython);
  assert.equal(studioPythonPath(fixtureRoot, "linux"), posixPython);
});

test("studioPythonPath rejects missing, external, and non-root venv interpreters", (t) => {
  const missing = pythonFixture(t);
  assert.throws(() => studioPythonPath(missing, "win32"), /not found/);

  const external = pythonFixture(t, "C:/Python312/python.exe");
  assert.throws(() => studioPythonPath(external, "win32"), /root \.venv/);

  const other = pythonFixture(t, "tools/.venv");
  assert.throws(() => studioPythonPath(other, "win32"), /root \.venv/);
});

test("active ordinary Python contracts do not advertise ambient launchers", () => {
  for (const file of activeContracts) {
    const text = readFileSync(join(root, file), "utf8");
    assert.doesNotMatch(text, /\bpy\s+-3\.12\b|(?:^|(?<!`)`)python(?:3)?\s+(?=[\w./<])/gm, file);
  }
});

test("feature command metadata uses the canonical Studio Python runner", () => {
  for (const file of ["features/game-state/feature.json", "features/items-core/feature.json", "features/progression-core/feature.json"]) {
    const metadata = JSON.parse(readFileSync(join(root, file), "utf8"));
    for (const [name, command] of Object.entries(metadata.commands || {})) {
      assert.ok(command.startsWith(`${runner} `), `${file} commands.${name}`);
    }
  }
});

test("asset pack slicing delegates interpreter policy to the canonical runner", () => {
  const script = readFileSync(join(root, ".codex/skills/nt-asset-image-generation/scripts/slice_pack.py"), "utf8");
  const contract = readFileSync(join(root, ".codex/skills/nt-asset-image-generation/references/throughput-and-handoff.md"), "utf8");
  assert.match(script, /Path\(sys\.executable\)\.resolve\(\)/);
  assert.doesNotMatch(script, /studio\.config|_load_studio_config|resolve_python/);
  assert.match(contract, /node ai_studio\/dev_environment\/python_run\.mjs .*slice_pack\.py/);
});

test("ordinary Python requirements state their direct-only reproducibility boundary", () => {
  const requirements = readFileSync(join(root, "ai_studio/python/requirements.direct.txt"), "utf8");
  const readme = readFileSync(join(root, "ai_studio/dev_environment/README.md"), "utf8");
  assert.match(requirements, /top-level requirements/i);
  assert.match(requirements, /not a reproducible full lock/i);
  assert.match(readme, /pip resolves transitive dependencies/i);
  assert.doesNotMatch(readme, /requirements\.lock\.txt/);
});

test("canonical Python runner prepends repo imports and preserves ambient PYTHONPATH", () => {
  const ambient = join(root, "tmp", "ambient-python-modules");
  const output = execFileSync(process.execPath, [
    runnerPath,
    "-c",
    "import os; print(os.environ['PYTHONPATH'])",
  ], {
    cwd: root,
    encoding: "utf8",
    env: { ...process.env, PYTHONPATH: ambient },
  }).trim();
  assert.equal(output, `${root}${delimiter}${ambient}`);
});
