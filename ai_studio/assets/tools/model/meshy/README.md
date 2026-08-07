# Meshy 3D generation

This is the Studio's budget-gated Meshy REST client. It produces local GLB
candidates and provenance; it does not add them to a game or shared library.
Search the asset catalog first, generate only on an explicit lead request, then
review and route accepted output through asset intake.

## Safety contract

- `plan` and `balance` never create a paid task.
- `run` creates a task only with all three caller signals: `--execute`, an exact
  `--confirm-credits`, and an exactly matching `--max-credits`. Meshy has no
  server-side per-request spend-cap field, so recheck the dated pricing source
  in the plan whenever provider pricing may have changed.
- Balance is checked immediately before submission. The committed default keeps
  a 20-credit reserve. Lowering it requires both `--reserve-credits` and an
  exactly matching `--confirm-reserve-credits`, after explicit lead approval.
- The request fingerprint deduplicates identical work under
  `tmp/ai_studio/assets/meshy/`. A recorded task is resumed, never resubmitted.
- If POST outcome is ambiguous, the job becomes `SUBMISSION_UNKNOWN` and retry
  is blocked until account tasks are reconciled. This avoids double charging.
- Text generation is two-stage. Preview never auto-refines; refinement is a
  separate paid request after the geometry has been reviewed.
- Only GLB is requested. Completed output is downloaded immediately because
  Meshy retains non-Enterprise generated assets for at most three days.
- API keys are read only from `MESHY_API_KEY`; they are never written to config,
  logs, job records, or provenance.

The credit table was verified against the official pricing page on 2026-08-05.
Review that page before accepting a plan if Meshy pricing has changed.

## Setup

Create a key at <https://www.meshy.ai/settings/api> and expose it only to the
process that will run the client:

```powershell
$env:MESHY_API_KEY = "msy_..."
node ai_studio/assets/tools/model/meshy/cli.mjs balance
```

## Cost-aware workflow

Text starts with an untextured `draft` preview (Meshy 5, 5 credits):

```powershell
node ai_studio/assets/tools/model/meshy/cli.mjs plan text-preview --prompt "wooden market stall, readable silhouette"
node ai_studio/assets/tools/model/meshy/cli.mjs run text-preview --prompt "wooden market stall, readable silhouette" --execute --confirm-credits 5 --max-credits 5
```

After reviewing geometry, texture that exact preview (10 credits):

```powershell
node ai_studio/assets/tools/model/meshy/cli.mjs plan text-refine --preview-task-id <id> --profile draft
node ai_studio/assets/tools/model/meshy/cli.mjs run text-refine --preview-task-id <id> --profile draft --execute --confirm-credits 10 --max-credits 10
```

The refine profile must match the preview generation: `draft` uses Meshy 5;
`game` and `ultra` use the latest Meshy model. This prevents a paid refine from
being rejected for an incompatible preview model.

When a strong reference image exists, image-to-3D usually gives better control.
`draft` is 5 credits without texture; `game` is 15 credits with clean smart
topology, 2K PBR textures, and an 8K-face default. `quality`/`ultra` use the
more expensive latest standard model and should be requested explicitly.

```powershell
node ai_studio/assets/tools/model/meshy/cli.mjs plan image --image tmp/reference.png --profile game
```

Every successful run writes `model.glb`, `job.json`, and `provenance.json` under
its fingerprint directory. Promotion still needs license/terms review and the
normal `origin=ai`, integrity, metadata, preview, and runtime checks.
