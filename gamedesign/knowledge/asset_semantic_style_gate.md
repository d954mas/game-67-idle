---
type: Knowledge
title: Asset Semantic And Style Gate
description: Reusable gate for rejecting generated assets that technically cut cleanly but mean the wrong thing or mix visual styles.
tags: [knowledge, art, ui, assets, semantic, style, gate]
timestamp: 2026-06-17T00:00:00Z
---

# Asset Semantic And Style Gate

Use this before slicing or integrating generated UI icons, decor, sprites, and
other runtime assets. Pixel audits can prove crop hygiene, but they cannot prove
that an armor icon reads as armor, a resource icon reads as the right resource,
or a set shares one style.

## Pipeline Placement

Run the semantic/style review after selecting a source sheet candidate and
before crop planning:

1. accepted visual target / art bible;
2. source family prompt and generated candidates;
3. **semantic/style review gate**;
4. source-sheet intake / crop planning;
5. runtime asset build;
6. pixel, edge, composition, runtime screenshot, and product gates.

Do not spend runtime integration time on a source sheet that fails this gate.
Reject or regenerate the source family first.

## Review Contract

Create a JSON review with schema `game.asset_semantic_style_review`:

```json
{
  "schema": "game.asset_semantic_style_review",
  "source_family": "isolated icon sheet",
  "accepted_visual_target": "gamedesign/projects/<game>/visual/art_bible.md",
  "pipeline_stage": "pre_slice",
  "style_contract": {
    "style_group": "chunky-icy-ui-icons",
    "required_traits": ["chunky silhouette", "icy material", "same camera angle"],
    "forbidden_mixes": ["coin badge reused as block icon", "castle silhouette used as armor"]
  },
  "assets": [
    {
      "id": "frost_block_icon",
      "intended_role": "resource icon for frost blocks",
      "observed_subject": "icy block shard",
      "evidence": "contact-sheet row 1 col 2",
      "style_group": "chunky-icy-ui-icons",
      "semantic_match": "pass",
      "style_match": "pass",
      "composability": "pass",
      "verdict": "accept",
      "problems": []
    }
  ]
}
```

Run:

```powershell
node tools/assets/audit_asset_semantic_style.mjs `
  --review gamedesign/projects/<game>/reviews/<asset-family>-semantic-style.json `
  --json-output gamedesign/projects/<game>/reviews/<asset-family>-semantic-style-audit.json `
  --report gamedesign/projects/<game>/reviews/<asset-family>-semantic-style-audit.md
```

Accepted assets must have `semantic_match`, `style_match`, and `composability`
all set to `pass`, and they must use the same `style_group` as the family
contract. Rejected candidates are useful evidence, but they must name the
observed problem.

## Voxelheim Rejection Examples

These are reusable examples, not Voxelheim-only rules:

- HUD Blocks reused coin/badge-like art and read as a dirty green second
  currency instead of a block resource. Reject as `semantic_match: fail`.
- A fantasy upgrade icon that reads as a castle/tower instead of armor is a
  semantic failure even if the crop is clean. Reject before runtime integration.
- Mixed icon treatments in one set, such as chunky rendered badges beside flat
  vector symbols, fail `style_match` because the family no longer reads as one
  intentional UI kit.
- Composite props that fuse two unrelated objects into one silhouette fail
  `composability` because they will not work as small reusable runtime icons.

## Done Signal

This gate passes only when the selected asset family has:

- one accepted visual target;
- one style contract;
- one observed-subject review per accepted asset;
- explicit rejected examples for confusing candidates;
- a passing `game.asset_semantic_style_audit` JSON report.
