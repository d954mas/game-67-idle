# License System

The license system protects the public git repository from private, paid,
unknown-license, or non-redistributable asset binaries, and keeps enough
metadata to generate credits/notices later.

## Decision Rule

Every asset source uses the same publishability split:

- publishable assets: binaries may live under `assets/source/`,
  `assets/previews/`, or other committed asset folders when metadata proves
  redistribution is allowed;
- restricted assets: binaries must live under gitignored `assets/restricted/`;
  only metadata, license notes, and reconstruction/source notes are committed.

Unknown and custom licenses are restricted by default. A custom license becomes
publishable only with an explicit `publish: true`, `redistribution_allowed:
true`, `commercial_use: true`, `modification_allowed: true`, and license
evidence (`license_url` or `license_file`). Paid/private/non-redistributable
licenses stay restricted even if a caller accidentally asks for publish.
`license_url` is only for absolute HTTP(S) URLs. Repository-local evidence uses
`license_file`; the integrity core confines that path to the repository and
requires an existing tracked file or an explicit same-change `evidence_files`
declaration in the tracked binary inventory.

Publishable licenses can still require attribution or notices. Missing
attribution/notice metadata is release debt, not a development blocker:

- CC-BY assets require `credit_text` or author/credit plus `source_page` before
  release;
- OFL/code-style licenses require a license notice before release, not an
  in-game CC-BY credit;
- CC0 assets do not require credit, but still keep provenance.

Every manifested asset should preserve:

- `license`, `license_url` or `license_file`, and `license_kind`;
- `publish`, `redistribution_allowed`, `commercial_use`, and
  `modification_allowed`;
- `attribution_required`, `notice_required`, and `credit_text` when needed;
- `origin`, `source_page`, `author_vendor`, integrity, and source id.

## Scope

The guard checks every git-tracked binary blob in the repository, including
game/template product assets, AI Studio fonts, generated outputs, and test
fixtures. It also classifies gitlink boundaries without descending into them.
It does not scan private libraries for compliance; it prevents restricted or
unaccounted binaries from entering this public repo.

This folder owns the policy and command surface.

## Files

- `registry.mjs`: canonical license decision and validation registry.
- `restricted.mjs`: shared helper surface for publishability, restricted-root
  routing, attribution/notice checks, and binary asset classification.
- `restricted_assets_guard.mjs`: the single compatible CLI adapter over the
  Pack Manifest integrity core.

## Commands

```powershell
node ai_studio/assets/licenses/restricted_assets_guard.mjs
node ai_studio/assets/licenses/restricted_assets_guard.mjs --scope templates/template/assets
node ai_studio/assets/licenses/restricted_assets_guard.mjs --json
node --test ai_studio/assets/licenses/restricted_assets_guard.test.mjs
```

The guard is release-strict by default. It fails closed on git setup errors
(exit 2), missing or unexpected inventory entries, malformed or conflicting
metadata, unsafe/case-colliding paths, unverified hashes, unknown provenance or
origin, and non-publishable licenses. Text output is capped; use `--scope` for a
small owner-specific report or `--json` for stable machine output. Scope is a
validated repository-relative inventory/source path; escaping or unmatched
scopes are setup errors with exit 2. Unknown or malformed CLI arguments also
fail with exit 2 instead of being ignored.
