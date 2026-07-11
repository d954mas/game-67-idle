import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import test from "node:test";

const root = resolve(import.meta.dirname, "../..");
const runner = "node ai_studio/dev_environment/python_run.mjs";
const activeContracts = [
  "features/game-state/README.md",
  "features/game-state/INSTALL.md",
  "features/items-core/README.md",
  "features/items-core/INSTALL.md",
  "features/items-core/scripts/generate_items_catalog.py",
  "features/items-core/scripts/items_ops.py",
  "features/items-core/scripts/items_ops_test.py",
  "features/progression-core/README.md",
  "features/progression-core/scripts/generate_progression_tracks.py",
  "templates/template/README.md",
  "templates/template/devapi/README.md",
  "games/web-dressup/README.md",
  "games/web-dressup/devapi/README.md",
  "ai_studio/assets/items_viewer/README.md",
  "ai_studio/assets/canvas/README.md",
  "ai_studio/assets/tools/image/README.md",
  "ai_studio/assets/tools/image/_bridge/README.md",
  "ai_studio/assets/tools/image/alpha_dualplate/dual_plate_pair_gate.py",
];

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
