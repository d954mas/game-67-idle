# generate/ — stage 1: image + motion -> video

Submits the profile workflow to the local ComfyUI WAN 2.2 I2V stack and collects
the generated mp4 into the run folder.

- **Entry:** `generate.mjs` — `runGenerate({image, text, profile, seed, outDir, name, host})`
  and a CLI: `node generate.mjs --image <png> --text "<motion>" --profile draft|final [--seed N] [--out <runDir>] [--name <slug>] [--host 127.0.0.1:8188]`.
- **What it does:** resolves the `videoGenRoot` profile workflow
  (`draft_workflow_api.json` / `final_workflow_api.json`); checks ComfyUI is UP
  (LOUD with the exact start command if down — v1 does NOT autostart); copies the
  input PNG into `ComfyUI/input/`; injects the hardened positive prompt (prefix +
  motion), the seed (both KSampler experts), and the image into the graph;
  submits via `/prompt`; polls `/history` to completion (LOUD on node/exec
  errors); copies the output mp4 into `<runDir>/generate/`; writes `params.json`
  provenance (prompt, prefix, seed, profile, workflow, models, comfy prompt_id,
  timings).
- **Node deps:** Node 18+ built-in `fetch`/`crypto` only.
- **Output:** `<runDir>/generate/<name>.mp4` + `<runDir>/generate/params.json`.
