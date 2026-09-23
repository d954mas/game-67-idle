# Kimodo text-to-motion

Studio adapter for a separately installed local
[kimodo.cpp](https://github.com/localai-org/kimodo.cpp), a GGML port of NVIDIA
Kimodo. It turns an English prompt into a humanoid SOMA-30 skeleton clip
(30 fps, 2-300 frames), previews it, and retargets it onto a character rig as a
GLB clip the engine builder imports. Outputs land under `tmp/`; nothing is
promoted into a game or library automatically.

## When to use it

Source first: a library clip that fits (for example the Mixamo-skeleton packs
in the asset catalog) beats a generated one. Kimodo fills gaps — a specific
action or gesture the library lacks. It learned from realistic motion capture,
so it does not produce cartoon squash-and-stretch, exact gameplay timing, or
seamless loops, and it only knows the human skeleton.

## Licenses

- Motion models: SOMA-RP / SOMA-SEED v1.1 under the NVIDIA Open Model License,
  which allows commercial use. The adapter refuses every other checkpoint:
  SMPL-X is research-only and G1 is a robot rig.
- Text encoder: Llama-3-8B-Instruct + LLM2Vec under the Meta Llama 3 Community
  License. Games ship generated clips, never the encoder.
- Code: Apache-2.0.

Every run records these in `provenance.json` with `origin: "ai"`. A retargeted
clip is additionally bound by the character asset's own license.

## Local setup

The install lives outside the repository at
`%LOCALAPPDATA%\AIStudio\tools\kimodo` (override with `KIMODO_HOME`):

```text
kimodo/
  src/       git clone --recursive https://github.com/localai-org/kimodo.cpp src
  weights/   models/kimodo-soma-rp-v1.1-f32.gguf, Llama-3-Kimodo-Q8_0.gguf, tokenizer.gguf
```

1. Build the CPU generator with clang and Ninja:
   `cmake -S src -B src/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DKIMODO_ENABLE_VULKAN=OFF -DKIMODO_BUILD_TESTS=OFF`,
   then `cmake --build src/build/release`.
2. For GPU generation, install the Vulkan SDK and build the same way into
   `src/build/vulkan` with `-DKIMODO_ENABLE_VULKAN=ON` (set `VULKAN_SDK` and
   put its `Bin` on `PATH` first). The adapter picks that build when it
   exists; `--backend cpu` still forces the CPU path. kimodo.cpp always uses
   Vulkan device 0, which on a laptop is usually the integrated GPU: find the
   discrete GPU's index in the `ggml_vulkan: N = ...` lines of
   `tmp/ai_studio/assets/kimodo/logs/generator.log` and set
   `kimodoVulkanDevice` in `ai_studio/studio.config.local.json` (or
   `KIMODO_VULKAN_DEVICE`). If an antivirus holds new executables for
   analysis, the build hangs in `vulkan-shaders-gen.exe` at 0% CPU; exclude
   the install folder.
3. Download weights with the Hugging Face CLI (`pip install huggingface_hub`):
   `hf download LocalAI-io/Kimodo-SOMA-RP-v1.1-GGML models/kimodo-soma-rp-v1.1-f32.gguf --local-dir weights`
   and `hf download LocalAI-io/Llama-3-Kimodo-GGML Llama-3-Kimodo-Q8_0.gguf tokenizer.gguf LICENSE-META-LLAMA-3.txt NOTICE --local-dir weights`.
   Upstream `scripts/download_gguf_weights.sh` fails on Windows because it hands
   `hf` backslash paths.
4. Point `blenderExecutable` in `ai_studio/studio.config.local.json` (or
   `KIMODO_BLENDER`) at Blender 4.3 or newer.
5. `node ai_studio/assets/tools/model/kimodo/cli.mjs doctor` must report `ok: true`.

## Generate

```powershell
node ai_studio/assets/tools/model/kimodo/cli.mjs generate --prompt "A person waves with the right hand." --frames 150
```

Repeat `--prompt` to generate several clips in one generator session: the 8B
text encoder then loads once. Each clip gets a run folder under
`tmp/ai_studio/assets/kimodo/runs/` with `motion.bvh`, raw `.f32` streams, a
front/side `preview.png` of eight frames, and `provenance.json`. Identical
requests (prompt, frames, steps, seed, model, backend, kimodo commit) reuse the
existing run. Change `--seed` for a different take of the same prompt.

On the RTX 4080 Laptop test machine, two 90-frame clips at 100 steps take
47 s on the discrete GPU through Vulkan, most of it loading the text encoder,
against 304 s on the integrated GPU and 624 s on the CPU.

## Retarget

```powershell
node ai_studio/assets/tools/model/kimodo/cli.mjs retarget --motion <run-dir> --character <character.glb> --map rgpoly
```

Retarget copies each mapped bone's world rotation relative to its T-pose rest,
scales hip travel by leg length, then pins planted feet: a frame is planted when
the ankle is near its lowest height and nearly still, and a two-bone IK pass
holds the ankle on the plant's mean position. `metrics.json` reports foot slide
before and after (the verified RG Poly walk went from 0.52 m to under 0.1 mm)
and `max_rotation_step`, the largest world rotation of a mapped bone between
neighbouring frames: a value near 180 degrees is a pop worth a look in the
sheet, while fast swings in a jump or a punch reach 50-90 degrees legitimately.

The output folder under `tmp/ai_studio/assets/kimodo/retarget/` holds
`clip.glb` (character mesh, rig, and one animation named by `--clip-name`),
`sheet.png`, `metrics.json`, and `provenance.json`.

Tune a character with `--offset Bone=x,y,z` (degrees, local XYZ, repeatable),
for example to keep a wide body's arms off its torso. Offsets that should apply
to every clip of a rig belong in the map's `offsets` block.

## Rig maps

`maps/<id>.json` declares, for one rig family:

- `bones`: `[target, source]` pairs, parents before children; the source side
  uses SOMA-30 names (`Hips`, `Chest`, `LeftArm`, `LeftShin`, ...);
- `legs`: per side, the `upper`, `lower`, `foot`, `toe` bones the foot lock drives;
- `arms`: the `left` and `right` upper-arm bones that fix the rig's facing;
- `hinges`: bones that may only bend (knees). Kimodo sometimes spins a shin
  about its own axis by 100+ degrees in one frame; retarget drops that twist and
  carries the correction to the foot and toe;
- `offsets`: default per-bone corrections.

The SOMA `Hips` must be mapped; it carries the root motion.

Both rigs must rest in a T-pose. Bundled: `rgpoly` (RG Poly Cartoon City
characters). A new rig family needs a new map and a visual check of its sheet.

## Acceptance boundary

- Review `preview.png` before retargeting and `sheet.png` after it; a prompt can
  come back as a different action.
- Proportions are not retargeted: on a big-headed character a boxing guard puts
  the fists into the face. Fix that with arm offsets and check the sheet.
- The knee twist correction also turns the foot about the shin axis; on a
  crouch with a tilted shin check that planted feet stay flat.
- Proof that a clip plays in a game is a run and a screenshot there; the builder
  import is the first gate (`nt_builder_add_scene_clip` accepts the exported GLB).
- Route keepers through asset intake with `origin=ai` and keep the run's
  `provenance.json` beside the promoted file.
