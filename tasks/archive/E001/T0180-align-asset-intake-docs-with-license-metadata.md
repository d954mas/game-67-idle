---
id: T0180
title: Align asset intake docs with license metadata
status: done
epic: E001
priority: P2
tags: [assets, licensing, docs]
created: 2026-06-30
updated: 2026-06-30
---

## What
Asset intake already records license kind, source page, author/vendor,
attribution/notice flags, rights flags, and publishability, but the public
README/skill command examples show only the short `--license-url` path. Align
the documentation and CLI help with the real license metadata model so agents do
not drop provenance or publishability data during intake.

## Done when

- [x] `accept.mjs --help` no longer presents `--license-url` as a required
      positional field.
- [x] Asset Storage and Intake docs show the important license/provenance
      options for acceptance.
- [x] `nt-asset-workflow` source-library reference matches the same intake
      command shape.
- [x] Validate intake tests, help output, docs, taskboard, and map.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after reviewing intake command docs against
  `accept.mjs`; docs showed only the short license URL path while the tool
  supports the richer license metadata required by the current asset policy.
- 2026-07-01: Updated accept help, Asset Storage README, Asset Intake README,
  and `nt-asset-workflow` source-library reference to show source page,
  author/vendor, license kind, attribution/notice flags, and custom-license
  rights flags. Validated intake tests, help output, docs, map, and taskboard.
- 2026-07-01: Reworked the README/skill command examples to keep the command
  line readable and list important license/provenance options separately.
- 2026-07-01: Aligned asset intake help/docs/skill reference with license
  metadata options; validated intake tests/help/docs/map/taskboard.
