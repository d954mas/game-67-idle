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

The guard checks git-tracked files under registered game/template asset roots. It
does not scan private libraries for compliance; it prevents restricted files
from entering this public repo.

This folder owns the policy and command surface.

## Files

- `registry.mjs`: canonical license decision and validation registry.
- `restricted.mjs`: shared helper surface for publishability, restricted-root
  routing, attribution/notice checks, and binary asset classification.
- `restricted_assets_guard.mjs`: git-tracked public-repo leak guard.
- `restricted_assets_exceptions.json`: empty-by-default, repo-relative exception
  prefixes for already reviewed public binaries that cannot yet be manifested.

## Commands

```powershell
node ai_studio/assets/storage/license/restricted_assets_guard.mjs
node ai_studio/assets/storage/license/restricted_assets_guard.mjs --release
node --test ai_studio/assets/storage/license/restricted_assets_guard.test.mjs
```

The normal guard fails closed only for actual public-repo risks: restricted
tracked files, missing manifest records, or non-publishable binaries.
`--release` also blocks missing CC-BY attribution and notice-bearing license
metadata so credits or third-party notices cannot be forgotten before shipping.
