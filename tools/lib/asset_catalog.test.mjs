import { test } from "node:test";
import assert from "node:assert/strict";
import { catalogFrontmatter, licenseMarkdown, tagList } from "./asset_catalog.mjs";

const FIELDS = {
  title: "Wooden Chair",
  description: "A chair",
  resource: "files/models/chair01/",
  tags: "furniture,chair",
  timestamp: "2026-06-24T00:00:00.000Z",
  assetId: "chair01",
  kind: "model",
  status: "accepted",
  origin: "sourced",
  license: "CC0-1.0",
  licenseUrl: "https://creativecommons.org/publicdomain/zero/1.0/",
  attributionRequired: "false",
  commercialUse: "true",
  modificationAllowed: "true",
  redistributionAllowed: "true",
  publish: "true",
  shippingDecision: "allowed",
};

test("tagList formats a csv or array as [a, b], trimming + dropping blanks", () => {
  assert.equal(tagList("furniture, chair ,"), "[furniture, chair]");
  assert.equal(tagList(["a", " b "]), "[a, b]");
  assert.equal(tagList(""), "[]");
});

test("catalogFrontmatter emits the canonical field block byte-for-byte", () => {
  assert.equal(
    catalogFrontmatter(FIELDS),
    `---
type: Game Asset
title: Wooden Chair
description: A chair
resource: files/models/chair01/
tags: [furniture, chair]
timestamp: 2026-06-24T00:00:00.000Z
asset_id: chair01
kind: model
status: accepted
origin: sourced
license: CC0-1.0
license_url: https://creativecommons.org/publicdomain/zero/1.0/
attribution_required: false
commercial_use: true
modification_allowed: true
redistribution_allowed: true
publish: true
shipping_decision: allowed
---`,
  );
});

test("catalogFrontmatter appends extra frontmatter lines before the close", () => {
  const fm = catalogFrontmatter(FIELDS, "tileable: true\nwrap_mode: repeat");
  assert.match(fm, /shipping_decision: allowed\ntileable: true\nwrap_mode: repeat\n---$/);
});

test("licenseMarkdown emits the canonical license body byte-for-byte", () => {
  assert.equal(
    licenseMarkdown({
      ...FIELDS,
      directDownload: "https://kenney.nl/x.glb",
      sourcePage: "https://kenney.nl/p",
      authorVendor: "Kenney",
    }),
    `# License: CC0-1.0

- Asset id: chair01
- Origin: sourced
- License URL: https://creativecommons.org/publicdomain/zero/1.0/
- Attribution required: false
- Commercial use: true
- Modification allowed: true
- Redistribution allowed: true
- Publishable (commit to open repo): true
- Shipping decision: allowed
- Direct download: https://kenney.nl/x.glb
- Source page: https://kenney.nl/p
- Author/vendor: Kenney
`,
  );
});
