import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import test from "node:test";

const templateRoot = dirname(dirname(fileURLToPath(import.meta.url)));

test("blank starter declares state without gameplay feature defaults", () => {
  const dependencies = JSON.parse(readFileSync(join(templateRoot, "game-dependencies.json"), "utf8"));

  const ids = dependencies.features.map(({ id }) => id);
  assert.ok(ids.includes("game-state"));
  for (const optional of ["items-core", "progression-core", "audio-core", "ui-kit"]) {
    assert.equal(ids.includes(optional), false);
  }
});
