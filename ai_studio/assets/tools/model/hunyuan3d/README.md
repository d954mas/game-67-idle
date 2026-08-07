# Hunyuan3D 2.0 local generation

This is the Studio adapter for a separately installed local Hunyuan3D 2.0.
It supports single-image shape generation through the loopback API, plus
offline multiview shape generation and low-VRAM texturing through a local
Python runner. It writes GLB candidates under `tmp/` and records generation
provenance. It never commits model weights or silently promotes generated
output into a game or library.

## Local setup

The tested Windows install lives outside the repository by default:

```text
%LOCALAPPDATA%\AIStudio\tools\Hunyuan3DLocal\Hunyuan3D-2
```

Override that location with `HUNYUAN3D_HOME`. The adapter refuses non-loopback
URLs; override the default `http://127.0.0.1:8081` only with
`HUNYUAN3D_URL=http://localhost:<port>` or another loopback address.

Check the install, start the local server, and wait for model loading:

```powershell
node ai_studio/assets/tools/model/hunyuan3d/cli.mjs doctor
node ai_studio/assets/tools/model/hunyuan3d/cli.mjs server-start
node ai_studio/assets/tools/model/hunyuan3d/cli.mjs doctor
```

The first server start downloads the single-image checkpoint from Hugging Face.
Server logs are written under `tmp/ai_studio/assets/hunyuan3d/`. The server is
needed only for `image`; `multiview` and `texture` run in separate local Python
processes and require their checkpoints to be present in the same external HF
cache.

The verified Windows texture setup uses Python 3.10, Torch 2.5.1+cu124,
torchvision 0.20.1, torchaudio 2.5.1, `mmgp==3.2.7`, and the Python 3.10
`custom_rasterizer` wheel published by the Windows fork. Texture generation
also uses CPU model offload and VAE slicing. These versions are intentional;
the rasterizer wheel is not ABI-compatible with Torch 2.6.

For the manual Gradio UI, stop the Studio API first and run the installed
`start.bat`; the page is then available at `http://127.0.0.1:7860`. On a 12 GB
GPU, do not keep the GUI and API model processes loaded at the same time.

## Generate a candidate

Generation is plan-first and requires an explicit execution signal even though
it spends no API credits:

```powershell
node ai_studio/assets/tools/model/hunyuan3d/cli.mjs plan image --image tmp/reference.png
node ai_studio/assets/tools/model/hunyuan3d/cli.mjs run image --image tmp/reference.png --execute
```

The default mini-turbo profile uses 5 steps and octree resolution 128. A
successful run produces `model.glb` and `provenance.json` in a deterministic
fingerprint directory. Identical requests reuse that result.

For multiview input, place `front.png` plus at least one named additional view
(`back.png`, `left.png`, or `right.png`) in one folder:

```powershell
node ai_studio/assets/tools/model/hunyuan3d/cli.mjs plan multiview --views tmp/model-views
node ai_studio/assets/tools/model/hunyuan3d/cli.mjs run multiview --views tmp/model-views --execute
```

The verified quality profile uses 30 steps, octree resolution 256, and 20,000
decode chunks. Override those with `--steps`, `--octree-resolution`, and
`--num-chunks` when needed.

Texture an existing GLB with a reference image:

```powershell
node ai_studio/assets/tools/model/hunyuan3d/cli.mjs plan texture --mesh tmp/model.glb --image tmp/reference.png
node ai_studio/assets/tools/model/hunyuan3d/cli.mjs run texture --mesh tmp/model.glb --image tmp/reference.png --execute
```

On the verified RTX 4080 Laptop 12 GB system, the Studio multiview proof took
about 101 seconds and the low-VRAM texture proof took about 276 seconds after
weights were cached. Timing and quality depend on the source views and GPU.

Current acceptance boundary:

- `image` and `multiview` output geometry (`textured=false`); `texture` embeds
  one material, texture, and image and records `textured=true`;
- review silhouette, topology, scale, orientation, and GLB preview before use;
- UV seams can split vertices, so a textured export may no longer report
  watertight through a naive indexed-vertex check even when face coverage is
  unchanged;
- route keepers through asset intake with `origin=ai`;
- review the Tencent Hunyuan 3D 2.0 license and the input-image rights before
  promotion or release.
