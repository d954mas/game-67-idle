# Post AA Example

## Purpose

Preserve a copy-and-adapt reference for three world-rendering modes without
shipping dormant antialiasing code in every game:

- native output (`OFF`), one world fragment per present pixel;
- FXAA at native resolution;
- DPR-aware SSAA with a bounded world density.

The Planet Eater browser comparison selected native output: at DPR 1.5 it kept
close detail sharper than FXAA and avoided the fill cost of SSAA. The example
remains useful for a future game whose art direction values smoother silhouettes
more than texture sharpness.

## Public surface

This is a reference, not an in-place runtime module. `example/post_aa_policy.h`
contains the renderer-independent scale policy. `example/post_aa.glsl` contains
FXAA and four-tap box-downsample functions designed to be folded into an
existing world composite pass.

A consumer copies only the mode it needs into its game-owned post pipeline. UI
must render after the composite at native framebuffer resolution.

## Validation

Compile and run `tests/test_post_aa_policy.c`, then validate the integrated
shader in the consumer's real WebGL2 browser build. Compare modes in the same
browser process and window after shader warmup; native synthetic rendering is
not performance evidence for WebGL.

The original Planet Eater comparison used CSS 1036x905 at DPR 1.5. FXAA reduced
frame-time spikes but visibly softened close detail; native output was selected.

## Compatibility

PATCH changes may clarify docs or fix equivalent shader math. MINOR changes may
add an independent algorithm or test without changing existing modes. MAJOR
changes may alter mode values, sampling footprints, thresholds, or scale policy.

## Extension points

Tune FXAA contrast thresholds for a game's palette, replace the box footprint,
or supply a different density ceiling through copied game-owned constants. Do
not add runtime registries or universal switches: post composition, effects,
target allocation, and visual taste remain owned by each game.
