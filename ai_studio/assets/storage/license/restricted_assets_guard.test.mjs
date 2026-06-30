import { test } from "node:test";
import assert from "node:assert/strict";
import { auditTrackedAssets, deriveAssetId } from "./restricted_assets_guard.mjs";
import { isPublishable, requiresAttribution, requiresNotice, hasAttributionInfo } from "./restricted.mjs";

const CC0 = { asset_id: "polypizza__car__cc0", license: "CC0-1.0", license_url: "https://creativecommons.org/publicdomain/zero/1.0/" };
const PAID = { asset_id: "cg__nature__paid", license: "CGTrader Royalty Free", publish: "false" };
const CCBY = {
  asset_id: "polypizza__enemy__cc-by-4-0",
  license: "CC-BY-4.0",
  license_url: "https://creativecommons.org/licenses/by/4.0/",
  attribution_required: "true",
  author: "Example Artist",
  source_page: "https://poly.pizza/m/example",
};

test("publishable CC0 source asset passes", () => {
  const cat = new Map([[CC0.asset_id, CC0]]);
  const r = auditTrackedAssets([`assets/packs/vehicles/files/${CC0.asset_id}/car.glb`], { recordsByAssetId: cat });
  assert.equal(r.ok, true, JSON.stringify(r.violations));
});

test("publishable CC0 preview passes (resolved by asset-id)", () => {
  const cat = new Map([[CC0.asset_id, CC0]]);
  const r = auditTrackedAssets([`assets/previews/${CC0.asset_id}/preview.png`], { recordsByAssetId: cat });
  assert.equal(r.ok, true, JSON.stringify(r.violations));
});

test("tracked file under assets/restricted/ is a violation", () => {
  const r = auditTrackedAssets(["assets/restricted/source/models/x/x.glb"], {});
  assert.equal(r.ok, false);
  assert.match(r.violations[0].reason, /restricted/);
});

test("paid asset committed under assets/packs/ is a violation", () => {
  const cat = new Map([[PAID.asset_id, PAID]]);
  const r = auditTrackedAssets([`assets/packs/nature/files/${PAID.asset_id}/nature.glb`], { recordsByAssetId: cat });
  assert.equal(r.ok, false);
  assert.match(r.violations[0].reason, /not publishable/);
});

test("CC-BY publishable asset missing attribution is dev warning and release violation", () => {
  const withoutCredit = { ...CCBY, author: "", source_page: "" };
  assert.equal(requiresAttribution(CCBY), true);
  assert.equal(hasAttributionInfo(CCBY), true);
  assert.equal(hasAttributionInfo(withoutCredit), false);

  assert.equal(
    auditTrackedAssets([`assets/packs/enemies/files/${CCBY.asset_id}/enemy.glb`], { recordsByAssetId: new Map([[CCBY.asset_id, CCBY]]) }).ok,
    true,
  );
  const r = auditTrackedAssets([`assets/packs/enemies/files/${CCBY.asset_id}/enemy.glb`], { recordsByAssetId: new Map([[CCBY.asset_id, withoutCredit]]) });
  assert.equal(r.ok, true);
  assert.equal(r.warnings.length, 2);
  assert.match(r.warnings[0].reason, /requires attribution before release/);

  const release = auditTrackedAssets([`assets/packs/enemies/files/${CCBY.asset_id}/enemy.glb`], { recordsByAssetId: new Map([[CCBY.asset_id, withoutCredit]]), release: true });
  assert.equal(release.ok, false);
  assert.match(release.violations[0].reason, /requires attribution before release/);
});

test("binary with no manifest and not allowlisted is a violation", () => {
  const r = auditTrackedAssets(["assets/packs/mystery/files/mystery/m.glb"], { recordsByAssetId: new Map() });
  assert.equal(r.ok, false);
  assert.match(r.violations[0].reason, /no manifest record/);
});

test("legacy allowlist prefix passes", () => {
  const r = auditTrackedAssets(["assets/meshes/foo_cc0.glb"], { allowlistPrefixes: ["assets/meshes/"] });
  assert.equal(r.ok, true, JSON.stringify(r.violations));
});

test("non-asset files are ignored", () => {
  const r = auditTrackedAssets(
    ["assets/packs/pack/README.md", "assets/shaders/a.frag", "tasks/evidence/shot.png"],
    {},
  );
  assert.equal(r.ok, true, JSON.stringify(r.violations));
});

test("deriveAssetId handles pack and previews layouts", () => {
  assert.deepEqual(deriveAssetId("assets/packs/props/files/id1/a.glb"), { pack: "props", assetId: "id1" });
  assert.deepEqual(deriveAssetId("assets/previews/id2/p.png"), { assetId: "id2" });
  assert.deepEqual(deriveAssetId("assets/meshes/flat.glb"), {});
});

test("game-folder-prefixed paths are audited (per-game asset layout)", () => {
  assert.equal(auditTrackedAssets([`mygame/assets/packs/vehicles/files/${CC0.asset_id}/car.glb`], { recordsByAssetId: new Map([[CC0.asset_id, CC0]]) }).ok, true);
  assert.equal(auditTrackedAssets([`mygame/assets/packs/nature/files/${PAID.asset_id}/n.glb`], { recordsByAssetId: new Map([[PAID.asset_id, PAID]]) }).ok, false);
  assert.equal(auditTrackedAssets(["template/assets/restricted/source/x/x.glb"], {}).ok, false, "restricted under a game folder is a violation");
});

test("deriveAssetId handles a game-folder prefix", () => {
  assert.deepEqual(deriveAssetId("template/assets/packs/props/files/id3/a.glb"), { pack: "props", assetId: "id3" });
  assert.deepEqual(deriveAssetId("g/assets/previews/id4/p.png"), { assetId: "id4" });
});

test("per-game allowlist prefix passes (template starter mesh)", () => {
  assert.equal(auditTrackedAssets(["template/assets/meshes/cube.glb"], { allowlistPrefixes: ["template/assets/meshes/"] }).ok, true);
});

test("publishability precedence: explicit publish overrides license", () => {
  assert.equal(isPublishable({ license: "CC0-1.0", publish: "false" }), false);
  assert.equal(isPublishable({ license: "Proprietary", redistribution_allowed: "true", publish: "true", license_url: "https://example.test/eula" }), false);
  assert.equal(isPublishable({ license: "Studio Custom", redistribution_allowed: "true", publish: "true", license_url: "https://example.test/license" }), true);
  assert.equal(isPublishable({ license: "" }), false);
  assert.equal(isPublishable({ license: "OFL-1.1" }), true);
  assert.equal(requiresNotice({ license: "OFL-1.1" }), true);
  assert.equal(requiresAttribution({ license: "OFL-1.1" }), false);
});
