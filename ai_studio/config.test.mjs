import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";

import { STUDIO_CONFIG_SCHEMA, loadStudioConfig } from "./config.mjs";

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "studio-config-"));
  mkdirSync(join(root, "ai_studio"), { recursive: true });
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return root;
}

function writeConfig(root, name, data) {
  writeFileSync(join(root, "ai_studio", name), `${JSON.stringify(data)}\n`, "utf8");
}

test("loadStudioConfig merges the ignored local override over committed defaults", (t) => {
  const root = fixture(t);
  writeConfig(root, "studio.config.json", { schema: STUDIO_CONFIG_SCHEMA, shared: "main", overridden: "main" });
  writeConfig(root, "studio.config.local.json", { overridden: "local", machineOnly: true });

  assert.deepEqual(loadStudioConfig(root), {
    schema: STUDIO_CONFIG_SCHEMA,
    shared: "main",
    overridden: "local",
    machineOnly: true,
  });
});

test("loadStudioConfig accepts a local-only config and reports missing or malformed JSON", (t) => {
  const localOnly = fixture(t);
  writeConfig(localOnly, "studio.config.local.json", { canvasProjectsRoot: "C:/local" });
  assert.equal(loadStudioConfig(localOnly).canvasProjectsRoot, "C:/local");

  const missing = fixture(t);
  assert.throws(() => loadStudioConfig(missing), /missing studio config/);

  const malformed = fixture(t);
  writeFileSync(join(malformed, "ai_studio", "studio.config.json"), "{bad json\n", "utf8");
  assert.throws(() => loadStudioConfig(malformed), /invalid studio config JSON/);
});

test("loadStudioConfig rejects a non-canonical schema from either config file", (t) => {
  const wrongMain = fixture(t);
  writeConfig(wrongMain, "studio.config.json", { schema: "ai_studio.studio_config.v2" });
  assert.throws(() => loadStudioConfig(wrongMain), /unsupported studio config schema.*studio\.config\.json/);

  const wrongLocal = fixture(t);
  writeConfig(wrongLocal, "studio.config.json", { schema: STUDIO_CONFIG_SCHEMA, shared: "main" });
  writeConfig(wrongLocal, "studio.config.local.json", { schema: "local.override.v1", shared: "local" });
  assert.throws(() => loadStudioConfig(wrongLocal), /unsupported studio config schema.*studio\.config\.local\.json/);
});

test("committed Studio config remains portable", () => {
  const configText = readFileSync(new URL("./studio.config.json", import.meta.url), "utf8");
  assert.doesNotMatch(configText, /[A-Za-z]:[\\/]|\/Users\/|\/home\//);
});
