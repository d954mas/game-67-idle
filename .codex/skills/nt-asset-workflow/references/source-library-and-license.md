# Source Library And License

Load this when searching, adding, promoting, pulling, or recording assets.

## Current Architecture

- Global/private library: `C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets`.
- AI Studio storage code: `ai_studio/assets/storage/`.
- AI Studio viewer/pull/promote UI/tooling: `ai_studio/assets/viewer/`.
- Generated fast data: `tmp/ai_studio/assets/index`, `tmp/ai_studio/assets/snapshots`,
  and `tmp/ai_studio/assets/previews`.

The global library is for reuse and discovery. A game must copy selected assets
into its own asset root and keep provenance. Do not load runtime files directly
from the global library.

## Search First

Use storage search for agent work:

```powershell
node ai_studio/assets/storage/search.mjs --query "<need>" --kind <model|texture|material|audio|font|ui> --json
node ai_studio/assets/storage/search.mjs --source-path templates/template/assets --source-type local --query "<need>" --json
```

Search reads the generated index. Add `--refresh` only after files or manifests
changed and the search must include those changes.

Use the site for visual browsing:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open
```

Open `http://127.0.0.1:8765/asset_viewer/`.

## Pull Into A Game

```powershell
node ai_studio/assets/viewer/pull.mjs --ids <asset-id> --to <game>/assets --apply
```

Dry-run first if the destination is unclear. The game-local copy should keep a
link to the source asset id. Paid/non-publishable assets are routed by
`pull.mjs`; read `restricted-assets.md` before changing that behavior.

## Promote To The Library

Promote game-local or review-selected assets only after license/provenance are
clear:

```powershell
node ai_studio/assets/viewer/promote.mjs --manifest <review-manifest.json> --ids <ids> --source <source> --license <license> --origin <mine|ai|sourced> --apply
```

Required metadata:

- `origin`: `mine`, `ai`, or `sourced`;
- license name and URL/file;
- `license_kind`: `cc`, `spdx`, `custom`, `private`, or `unknown`;
- `publish`, `redistribution_allowed`, `commercial_use`, and
  `modification_allowed`;
- source page/path and author/vendor when known;
- `credit_text` when attribution is required;
- title, description, tags, kind, and pack if it belongs to a pack.

## Intake New Assets

Use manifest intake for new reviewed asset sources:

```powershell
node ai_studio/assets/storage/intake/stage.mjs --source-root <asset-source> --input <file-or-folder> --source <source> --slug <candidate>
node ai_studio/assets/storage/intake/accept.mjs --source-root <asset-source> --source <source> --slug <candidate> --file <relative-file> --pack <pack-id> --asset-id <id> --kind <kind> --license <license>
node ai_studio/assets/storage/intake/reject.mjs --source-root <asset-source> --source <source> --slug <candidate>
```

`stage` records provenance and hashes only. `accept` writes Pack Manifest
metadata under `packs/` or `restricted/packs/` according to the license decision,
then moves the candidate folder from `_incoming/` to `_accepted/` as audit trail.
When accepting, record known license/provenance options: `--license-url`,
`--source-page-url`, `--author-vendor`, and `--license-kind`.
For custom/private/unknown licenses, pass explicit rights flags when known:
`--commercial-use`, `--modification-allowed`, `--redistribution-allowed`, and
`--publish`.

## License Gate

Accept an asset only when the license/provenance is enough to decide:

- commercial use;
- modification;
- redistribution;
- attribution requirement;
- publishability in a public git repo.

Unknown or non-redistributable binaries are restricted. Do not commit them.

Known open licenses are decided by `ai_studio/assets/storage/license/registry.mjs`.
Custom licenses default to `publish=false`. Use `--publish true` only when the
license evidence explicitly allows redistribution, commercial use, and
modification in this project.

For CC-BY or any asset with `attribution_required: true`, `credit_text` or
author/credit plus source page are required before release, but this is allowed
as development debt while evaluating assets. For notice-bearing licenses such as
OFL, keep the license URL/file and author/vendor before release so third-party
notices can be generated.
