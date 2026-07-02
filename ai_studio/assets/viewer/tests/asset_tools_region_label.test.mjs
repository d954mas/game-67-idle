import { strict as assert } from "node:assert";
import test from "node:test";
import { fitRegionOverlayLabel, regionOverlayLabel } from "../asset_tools_region_label.mjs";

test("regionOverlayLabel uses a region name when one is present", () => {
  assert.equal(regionOverlayLabel({ id: "region_004", name: "Gold Chest" }, 3), "Gold Chest");
});

test("regionOverlayLabel falls back to the visible region number", () => {
  assert.equal(regionOverlayLabel({ id: "region_004", name: "   " }, 3), "4");
});

test("fitRegionOverlayLabel trims long labels to the available badge width", () => {
  const measureText = (value) => ({ width: String(value).length * 8 });

  assert.equal(fitRegionOverlayLabel("Very Long Sword Icon Name", 80, measureText), "Very Lo...");
  assert.equal(fitRegionOverlayLabel("Small", 80, measureText), "Small");
});
