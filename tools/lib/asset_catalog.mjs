// Canonical on-disk schema for a shared-library catalog record + its license file.
// The frontmatter field set/order, the tag formatting, and the license layout live
// HERE so the three writers (accept_incoming_asset, asset_review/promote,
// import_poly_pizza) cannot drift apart on the catalog schema. Callers pass the
// field VALUES + optional extra frontmatter lines + their own markdown body.
//
// This is a WRITER lib only. The leak-guard READS the catalog via
// find_assets.parseFrontmatter and must NOT depend on this module (keep its dep set
// minimal). One job: serialize the record/license; no I/O, no orchestration.

// Render a YAML inline list exactly as the catalog uses it: [a, b, c].
export function tagList(tags) {
  const arr = Array.isArray(tags)
    ? tags
    : String(tags || "").split(",");
  return `[${arr.map((t) => String(t).trim()).filter(Boolean).join(", ")}]`;
}

// The canonical catalog frontmatter block (`---` ... `---`), no trailing newline.
// `fields` supplies every value; `extra` is caller-specific extra frontmatter lines
// (e.g. texture tileable/wrap_mode, or a vendor pack ref) appended before the close.
export function catalogFrontmatter(fields, extra = "") {
  const f = fields;
  return `---
type: Game Asset
title: ${f.title}
description: ${f.description}
resource: ${f.resource}
tags: ${tagList(f.tags)}
timestamp: ${f.timestamp}
asset_id: ${f.assetId}
kind: ${f.kind}
status: ${f.status}
origin: ${f.origin}
license: ${f.license}
license_url: ${f.licenseUrl}
attribution_required: ${f.attributionRequired}
commercial_use: ${f.commercialUse}
modification_allowed: ${f.modificationAllowed}
redistribution_allowed: ${f.redistributionAllowed}
publish: ${f.publish}
shipping_decision: ${f.shippingDecision}
${extra ? `${extra}\n` : ""}---`;
}

// The canonical licenses/<asset-id>/license.md body.
export function licenseMarkdown(f) {
  return `# License: ${f.license}

- Asset id: ${f.assetId}
- Origin: ${f.origin}
- License URL: ${f.licenseUrl}
- Attribution required: ${f.attributionRequired}
- Commercial use: ${f.commercialUse}
- Modification allowed: ${f.modificationAllowed}
- Redistribution allowed: ${f.redistributionAllowed}
- Publishable (commit to open repo): ${f.publish}
- Shipping decision: ${f.shippingDecision}
- Direct download: ${f.directDownload}
- Source page: ${f.sourcePage}
- Author/vendor: ${f.authorVendor}
`;
}
