import { test } from "node:test";
import assert from "node:assert/strict";
import { auditTrackedAssets, deriveAssetId } from "./restricted_assets_guard.mjs";
import { isPublishable } from "../restricted.mjs";

const CC0 = { asset_id: "polypizza__car__cc0", license: "CC0-1.0", license_url: "https://creativecommons.org/publicdomain/zero/1.0/" };
const PAID = { asset_id: "cg__nature__paid", license: "CGTrader Royalty Free", publish: "false" };

test("publishable CC0 source asset passes", () => {
  const cat = new Map([[CC0.asset_id, CC0]]);
  const r = auditTrackedAssets([`assets/source/models/${CC0.asset_id}/car.glb`], { catalogByAssetId: cat });
  assert.equal(r.ok, true, JSON.stringify(r.violations));
});

test("publishable CC0 preview passes (resolved by asset-id)", () => {
  const cat = new Map([[CC0.asset_id, CC0]]);
  const r = auditTrackedAssets([`assets/previews/${CC0.asset_id}/preview.png`], { catalogByAssetId: cat });
  assert.equal(r.ok, true, JSON.stringify(r.violations));
});

test("tracked file under assets/restricted/ is a violation", () => {
  const r = auditTrackedAssets(["assets/restricted/source/models/x/x.glb"], {});
  assert.equal(r.ok, false);
  assert.match(r.violations[0].reason, /restricted/);
});

test("paid asset committed under assets/source/ is a violation", () => {
  const cat = new Map([[PAID.asset_id, PAID]]);
  const r = auditTrackedAssets([`assets/source/models/${PAID.asset_id}/nature.glb`], { catalogByAssetId: cat });
  assert.equal(r.ok, false);
  assert.match(r.violations[0].reason, /not publishable/);
});

test("binary with no catalog and not allowlisted is a violation", () => {
  const r = auditTrackedAssets(["assets/source/models/mystery/m.glb"], { catalogByAssetId: new Map() });
  assert.equal(r.ok, false);
  assert.match(r.violations[0].reason, /no catalog record/);
});

test("legacy allowlist prefix passes", () => {
  const r = auditTrackedAssets(["assets/meshes/foo_cc0.glb"], { allowlistPrefixes: ["assets/meshes/"] });
  assert.equal(r.ok, true, JSON.stringify(r.violations));
});

test("non-asset files are ignored", () => {
  const r = auditTrackedAssets(
    ["assets/catalog/models/x.md", "assets/shaders/a.frag", "tasks/evidence/shot.png"],
    {},
  );
  assert.equal(r.ok, true, JSON.stringify(r.violations));
});

test("deriveAssetId handles source and previews layouts", () => {
  assert.deepEqual(deriveAssetId("assets/source/models/id1/a.glb"), { kindDir: "models", assetId: "id1" });
  assert.deepEqual(deriveAssetId("assets/previews/id2/p.png"), { assetId: "id2" });
  assert.deepEqual(deriveAssetId("assets/meshes/flat.glb"), {});
});

test("publishability precedence: explicit publish overrides license", () => {
  assert.equal(isPublishable({ license: "CC0-1.0", publish: "false" }), false);
  assert.equal(isPublishable({ license: "Proprietary", redistribution_allowed: "true" }), true);
  assert.equal(isPublishable({ license: "" }), false);
  assert.equal(isPublishable({ license: "OFL-1.1" }), true);
});
