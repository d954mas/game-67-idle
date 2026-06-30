import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { auditPolyPizzaCatalog } from "../poly_pizza_audit.mjs";

function writeRecord(path, frontmatter, body = "") {
  writeFileSync(path, `---\n${frontmatter.trim()}\n---\n${body}`, "utf8");
}

test("auditPolyPizzaCatalog reports bundle count mismatches and multi-pack assets", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "poly-pizza-audit-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const library = join(root, "library");
  mkdirSync(join(library, "catalog", "models", "animated-enemies"), { recursive: true });
  mkdirSync(join(library, "catalog", "models", "animals"), { recursive: true });

  writeRecord(join(library, "catalog", "models", "animated-enemies", "_pack.md"), `
type: Asset Pack
title: Animated Enemies
pack: animated-enemies
source: poly.pizza
kind: model
license: CC0-1.0
origin: sourced
count: 2
`, "\n- Source page: https://poly.pizza/bundle/Animated-Enemies-a53OJwHrhh\n");
  writeRecord(join(library, "catalog", "models", "animated-enemies", "rat.md"), `
type: Game Asset
asset_id: polypizza__rat__cc0
title: Rat
kind: model
origin: sourced
license: CC0-1.0
pack: animated-enemies
`, "\n- Source page: https://poly.pizza/m/rat\n");
  writeRecord(join(library, "catalog", "models", "animated-enemies", "wasp.md"), `
type: Game Asset
asset_id: polypizza__wasp__cc0
title: Wasp
kind: model
origin: sourced
license: CC0-1.0
pack: animated-enemies
`, "\n- Source page: https://poly.pizza/m/wasp\n");
  writeRecord(join(library, "catalog", "models", "animals", "snake.md"), `
type: Game Asset
asset_id: polypizza__snake__cc0
title: Snake
kind: model
origin: sourced
license: CC0-1.0
pack: animals
packs: [animals, animated-enemies]
`, "\n- Source page: https://poly.pizza/m/snake\n");

  const report = await auditPolyPizzaCatalog(library);

  assert.equal(report.summary.poly_pizza_assets, 3);
  assert.equal(report.summary.poly_pizza_pack_docs, 1);
  assert.equal(report.summary.declared_count_mismatches, 1);
  assert.equal(report.pack_count_mismatches[0].pack, "animated-enemies");
  assert.equal(report.pack_count_mismatches[0].declared_count, 2);
  assert.equal(report.pack_count_mismatches[0].membership_count, 3);
  assert.equal(report.summary.multi_pack_assets, 1);
  assert.equal(report.multi_pack_assets[0].asset_id, "polypizza__snake__cc0");
});
