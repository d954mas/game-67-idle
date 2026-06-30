# Verification And Prompts

Load this reference before retrying a failed generation, writing prompts for
game-use assets, or diagnosing fake/code-drawn output.

## Force Real Generation

Weak prompts can make an agentic CLI fake the image by writing code-drawn output
with SVG, PIL, ImageMagick, System.Drawing, shapes, or vectors. The wrapper bakes
in the required forcing prompt:

1. Actually invoke the image generation tool and decode its returned image.
2. Do not draw with code, SVG, PIL, ImageMagick, System.Drawing, shapes, or
   vectors; code-drawn output is a failure.
3. If imagegen is unavailable, print `IMAGEGEN UNAVAILABLE` and stop.
4. Report the exact tool used.

The user-facing prompt should still include: "real raster image (not
code-drawn)" and "Do not write or run any drawing code".

## Verify By Size Plus Eyeball

Do not trust transcript text. A delegated CLI can claim it used imagegen even
when it generated a fake.

Rules:

- Read or inspect the PNG yourself.
- Real gpt-image-2 output is usually around 1-2.5 MB.
- A code-drawn fake is often around 20-50 KB.
- The wrapper flags anything below about 200 KB as a suspected fake.
- `~/.codex/generated_images` not growing is normal for `codex exec`; the image
  can arrive inline and be decoded to the requested file.
- File size is a fake detector only. It does not prove art quality.

## Prompt Game-Use Assets As Composable Parts

For game-use art, generate composable parts, not baked composites:

- Chroma-keyed characters: use a flat key colour up to the outline and say
  `NO drop/contact shadow` to avoid cutout fringes.
- Health, XP, and progress bars: generate the EMPTY frame only; fill in-engine.
- Slots, cells, panels, and buttons: generate EMPTY surfaces with
  `NO icon/content inside`; draw icons and labels in-engine.
- For style comparison, keep one composition and vary only the art style.
- Limit a style gate to at most three generations before user choice.

Transparency note: codex image paths do not reliably produce alpha. Generate on
a flat key colour and chroma-key in post, or use the official REST path where
transparent output is required and supported.

## Dead-Ends

Do not retry these paths unless the environment has materially changed:

- Weak prompts to codex: it can code-draw a fake and claim success. Use the
  forcing wrapper; it works when forced.
- gemini CLI: no real image tool here. It returns `CANNOT_GENERATE` or writes a
  Python drawing script. Pinning image model variants does not fix function
  calling here.
- Stale `GEMINI_API_KEY` or `GOOGLE_API_KEY`: can hijack gemini auth. Neutralize
  per call if needed.
- Node TLS behind Avast HTTPS scanning: use `NODE_OPTIONS=--use-system-ca` for
  node export tools; do not disable strict SSL. Go/agy and Rust/codex generally
  do not need this workaround.
