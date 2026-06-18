#!/usr/bin/env bash
# Generate a dual-plate white/black PAIR by CHAINING edits, then gate the pair.
#
# Why a chain: dual-plate alpha is exact only when the two plates show the SAME
# subject in the SAME place. Generating white and black INDEPENDENTLY lets the
# model redraw the subject -> ghosting. Instead we generate the white plate from
# the source, then generate the black plate as an EDIT OF THE WHITE PLATE, so the
# subject is already final and only the background fill changes. Then the
# acceptance gate (tools/assets/cutout/dual_plate_pair_gate.py) checks the pair really is
# consistent before you spend an extraction on it.
#
# Path: generate_image.py edit mode (codex OAuth backend, gpt-image-2, --input-image).
# Transparency is NOT requested (the codex backend rejects it) — that is the whole
# point of dual-plate: recover alpha from the pair instead of from the model.
#
# Usage:
#   gen_dual_plate.sh --source tmp/subject_on_green.png --name angel_wings [--out-dir tmp/dual] [--size 1024x1024]
set -u
SOURCE="" OUTDIR="tmp/dual" NAME="asset" SIZE="1024x1024"
while [ $# -gt 0 ]; do
  case "$1" in
    --source)  SOURCE="$2"; shift 2;;
    --name)    NAME="$2"; shift 2;;
    --out-dir) OUTDIR="$2"; shift 2;;
    --size)    SIZE="$2"; shift 2;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done
[ -n "$SOURCE" ] || { echo "usage: gen_dual_plate.sh --source SUBJECT.png --name N [--out-dir D] [--size S]" >&2; exit 2; }
[ -f "$SOURCE" ] || { echo "source not found: $SOURCE" >&2; exit 2; }

SKILL_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SKILL_DIR/../../../.." && pwd)"
GEN="$SKILL_DIR/generate_image.py"
mkdir -p "$OUTDIR"
WHITE="$OUTDIR/${NAME}_white.png"
BLACK="$OUTDIR/${NAME}_black.png"

# Shared subject-lock clause (the part that prevents redraw).
LOCK="Keep the subject EXACTLY as in the input image: same position, same scale, same rotation, same silhouette, every internal detail, every anti-aliased edge, and all soft elements (glow, particles, smoke, transparency, cast shadow) pixel-for-pixel identical. Do NOT re-render, re-light, restyle, sharpen, denoise, upscale, crop, pad, rotate, mirror, add, remove, or recolour the subject. If you cannot keep it identical, return it unchanged rather than improving it."
# Background-fill clause template (the part that makes a clean plate).
flatbg() { echo "Fill the ENTIRE canvas edge-to-edge with solid flat $1. No gradient, no vignette, no texture, no noise, no lighting falloff, no reflection, and do NOT add any new shadow on the background."; }

WHITE_PROMPT="Edit the input image: replace its background with solid flat white #ffffff. $(flatbg 'white #ffffff') ${LOCK} Output the same subject on pure white #ffffff."
# Frame the black plate as a pure background recolour, not a redraw — this is what
# keeps a glow/soft-alpha subject consistent enough to pass the pair gate.
BLACK_PROMPT="This is a BACKGROUND-COLOR SWAP only, not a redraw. The subject is final. Output the EXACT same image pixel-for-pixel, with the ONLY change being the flat background recoloured to solid black #000000. $(flatbg 'black #000000') ${LOCK} Do NOT re-render or re-light the subject for the dark background. Output the same subject on pure black #000000."

echo ">>> [1/3] white plate: edit '$SOURCE' -> '$WHITE'"
python "$GEN" --input-image "$SOURCE" --prompt "$WHITE_PROMPT" --out "$WHITE" --size "$SIZE" --quality high || {
  echo "white plate generation failed" >&2; exit 1; }

# The generator redraws soft elements (glow/sparkles) between background colours,
# so the black plate often drifts. Retry it (always editing the SAME white plate)
# until the pair gate PASSES, keeping the most-consistent attempt.
ATTEMPTS="${DUAL_PLATE_ATTEMPTS:-4}"
GATE="$REPO_ROOT/tools/assets/cutout/dual_plate_pair_gate.py"
echo ">>> [2/3] black plate: edit the white plate -> black, retry until gate PASSES (max $ATTEMPTS)"
best_frac=101; best_file=""
for try in $(seq 1 "$ATTEMPTS"); do
  cand="$OUTDIR/${NAME}_black_try${try}.png"
  python "$GEN" --input-image "$WHITE" --prompt "$BLACK_PROMPT" --out "$cand" --size "$SIZE" --quality high || {
    echo "black plate generation failed" >&2; exit 1; }
  py -3.12 "$GATE" --light "$WHITE" --dark "$cand" --json-output "$OUTDIR/${NAME}_gate.json" >/dev/null 2>&1
  frac=$(grep -o '"inconsistent_fraction":[ ]*[0-9.]*' "$OUTDIR/${NAME}_gate.json" | grep -o '[0-9.]*$')
  verdict=$(grep -o '"verdict":[ ]*"[a-z]*"' "$OUTDIR/${NAME}_gate.json" | sed 's/.*"\([a-z]*\)"$/\1/')
  echo "    try $try/$ATTEMPTS: gate=${verdict:-?} (${frac:-?} inconsistent)"
  if [ -n "$frac" ] && awk "BEGIN{exit !($frac < $best_frac)}"; then best_frac="$frac"; best_file="$cand"; fi
  [ "$verdict" = "pass" ] && break
done
[ -n "$best_file" ] && cp "$best_file" "$BLACK"

echo ">>> [3/3] acceptance gate on the kept best pair"
py -3.12 "$GATE" --light "$WHITE" --dark "$BLACK" --json-output "$OUTDIR/${NAME}_gate.json"
GATE_RC=$?

echo "================= RESULT ================="
echo "white: $WHITE"
echo "black: $BLACK"
if [ "$GATE_RC" -eq 0 ]; then
  echo "PAIR OK -> extract: py -3.12 tools/assets/cutout/dual_plate_alpha.py --light '$WHITE' --dark '$BLACK' --output '$OUTDIR/${NAME}_rgba.png' --alpha-combine proj"
else
  echo "PAIR REJECTED (see gate above) -> re-run gen_dual_plate.sh to regenerate; do NOT matte this pair."
fi
exit $GATE_RC
