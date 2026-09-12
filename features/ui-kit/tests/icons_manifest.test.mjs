import { createHash } from "node:crypto";
import { readdirSync, readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { join } from "node:path";
import test from "node:test";
import assert from "node:assert/strict";

// ui_icons.h names the set once; the files, the pack manifest and the
// tracked-binary inventory each have to carry exactly that set, or a glyph
// binds to nothing and the composite that draws it shows a hole.

const FEATURE = fileURLToPath(new URL("../", import.meta.url));
const STUDIO = join(FEATURE, "..", "..");
const ICONS = join(FEATURE, "assets", "icons");

const header = readFileSync(join(FEATURE, "include", "features", "ui_kit", "ui_icons.h"), "utf8");
const names = [...header.matchAll(/X\([A-Z_]+, "([a-z_]+)"\)/g)].map((m) => m[1]);
const files = readdirSync(ICONS).filter((f) => f.endsWith(".png")).map((f) => f.slice(0, -4)).sort();
const rows = readFileSync(join(FEATURE, "assets", "packs", "ui-kit-icons", "assets.jsonl"), "utf8")
  .split("\n").filter((l) => l.trim()).map((l) => JSON.parse(l));

test("ui_icons.h names the files that exist, and nothing else", () => {
  assert.ok(names.length > 0, "no X(...) entries parsed from ui_icons.h");
  assert.deepEqual([...names].sort(), files);
  assert.equal(new Set(names).size, names.length, "duplicate glyph name");
});

test("every glyph has a manifest row whose hash and size match the file", () => {
  const byFile = new Map(rows.map((r) => [r.source_resource, r]));
  for (const name of names) {
    const row = byFile.get(`icons/${name}.png`);
    assert.ok(row, `no manifest row for ${name}`);
    const data = readFileSync(join(ICONS, `${name}.png`));
    assert.equal(row.sha256, createHash("sha256").update(data).digest("hex"), `${name}: sha256 drifted`);
    assert.equal(row.bytes, data.length, `${name}: byte count drifted`);
    assert.equal(row.license, "CC0-1.0");
  }
  assert.equal(rows.length, names.length, "manifest rows for glyphs that no longer exist");
});

test("the tracked-binary inventory lists every glyph", () => {
  const inventory = JSON.parse(readFileSync(join(STUDIO, "ai_studio", "assets", "manifests", "tracked_binary_inventory.json"), "utf8"));
  const tracked = new Set(inventory.entries.map((e) => e.path));
  for (const name of names) {
    assert.ok(tracked.has(`features/ui-kit/assets/icons/${name}.png`), `${name} missing from the inventory`);
  }
});
