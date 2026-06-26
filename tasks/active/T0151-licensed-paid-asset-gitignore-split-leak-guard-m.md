---
id: T0151
title: Licensed/paid asset gitignore split + leak guard + manual paid intake
status: review
epic: E001
priority: P1
tags: [pipeline, assets, licensing, git, guard]
created: 2026-06-24
updated: 2026-06-24
---

## What

Let the project USE paid/licensed assets (e.g. CGTrader subscription packs) while
keeping the open git repo legally clean. Problem: today all binary assets are
committed (55 files: `.glb/.gltf/.png` under `assets/`). Paid assets must never
enter git — current repo and future game repos are public.

Decision (lead, 2026-06-24):
- Partition by **publishability** (right to redistribute), not by source or type.
- One gitignored root `assets/restricted/` mirrors `assets/source|previews/...`.
- Catalog `.md` + license `.md` are committed for EVERY asset (metadata only,
  no binary) — they are the reconstruction recipe (re-pull from private library).
- For **any** committed asset a license must be recorded, and a guard must block
  a missing-license or non-publishable binary from being tracked (covers both the
  pulled-from-library path and the dropped-by-hand path).
- Paid intake is **manual**: lead downloads the pack, drops it in the private
  library; no direct download URL is stored (account-gated). Provenance keeps
  only the product page + sha256.
- A skill parses a paid pack (unzip -> inventory formats -> pick source ->
  convert to glb -> dedup textures -> preview -> catalog publish:false).

Library binaries (incl. paid) live in the private YandexDisk library, off git.
Builder uses explicit paths in `build_packs.c`, so a restricted file just needs to
exist locally at build time — no builder scan change required.

## Done when

- [x] `.gitignore` ignores `assets/restricted/`
- [x] Publishability predicate defined (publish -> redistribution_allowed -> license-string; unknown = NOT publishable) — `tools/assets/restricted.mjs`
- [x] `pull.mjs` routes non-publishable binaries+previews to `assets/restricted/`, writes `publish:` into the committed catalog record
- [x] Leak guard `tools/assets/audit/restricted_assets_guard.mjs` (+ tests) runs as the asset module validation path; passes on current tree, fails on a planted paid leak
- [x] Manual paid intake (no download URL) + `accept_incoming_asset.mjs` supports `--publish false` and paid license
- [x] Paid-pack lifecycle documented (NatureGradientPack.zip worked example) in game-asset-pipeline `references/restricted-paid-assets.md` + game-asset-prep pointer
- [x] AGENTS.md invariant + skill docs updated; my changes pass focused guard/tests/taskboard/skills/doc-ref checks

## Open questions

- Canonical license string for CGTrader subscription assets (e.g. "CGTrader Royalty Free")?
- Backfill catalog records for pre-catalog legacy assets (assets/meshes/*, blockside_city_base, kenney/quaternius) vs keep on legacy allowlist?

## Log

- orchestration: used
  objective: let the project use paid/licensed assets without leaking binaries into the open repo — gitignore split + publishability predicate + leak guard + manual paid intake + parse skill + docs
  allowed files: .gitignore, ai_studio/assets/asset_viewer/pull.mjs, tools/assets/restricted.mjs, tools/assets/audit/**, tools/assets/intake/**, AGENTS.md, docs/ai-pipeline/**, .codex/skills/**, tasks/active/T0151-licensed-paid-asset-gitignore-split-leak-guard-m.md
  tool-use guard: verify exact repo paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First for line windows; keep evidence commands read-only
  expected output: committed metadata-only catalogs + gitignored assets/restricted/ for paid binaries; guard green on tree and red on a planted leak; intake + skill for CGTrader packs; AGENTS invariant + doc
  evidence command: node tools/assets/audit/restricted_assets_guard.mjs && node --test tools/assets/audit/restricted_assets_guard.test.mjs && node ai_studio/taskboard/cli.mjs validate
  stop condition: guard passes current tree + blocks a planted paid leak; manual intake records sha256+product-page (no download URL) with publish:false; parse skill documented; focused validation green
  independent reviewer: lead (d954mas) via /code-review on the branch diff
- 2026-06-24 Created. Researched current pipeline: binaries currently committed;
  catalog/license already recorded for polypizza (CC0). Builder uses explicit
  paths (no scan). Example paid pack: NatureGradientPack.zip (fbx/mb/blend/obj+mtl
  + Texture/, manual download, no shareable URL). Plan = Phase 1 safety mechanics
  (gitignore + guard + pull routing + publish field), Phase 2 manual paid intake,
  Phase 3 parse skill + docs.
- 2026-06-24 Implemented all 3 phases. Phase 1: `.gitignore` + `tools/assets/restricted.mjs`
  predicate + `pull.mjs` routing + guard (`tools/assets/audit/restricted_assets_guard.mjs`,
  tests. Proven: guard green on tree (60 paths),
  red on a planted paid binary AND a force-added `assets/restricted/` file; planted
  state reverted clean. Phase 2: `download_source_asset.mjs --manual/--source-page-url/
  --publish` + `accept_incoming_asset.mjs --publish`; end-to-end on the real
  NatureGradientPack.zip into a tmp library -> intake stored product page only (no
  download link), catalog `publish:false`, pull routed the binary to `restricted/` and
  committed only the `.md`. Phase 3: reference `restricted-paid-assets.md` + SKILL/AGENTS
  pointers; skills re-synced. Green: guard+tests, taskboard validate, agent surface sync check,
  doc_reference_check, and focused tests.
  Broad pipeline validation runner was later removed in favor of module-owned
  validators. Work uncommitted on master pending lead review.
