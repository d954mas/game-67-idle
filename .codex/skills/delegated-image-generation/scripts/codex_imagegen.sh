#!/usr/bin/env bash
# Generate a REAL image via the OFFICIAL codex CLI imagegen tool (gpt-image-2),
# headless, on the ChatGPT subscription. Sanctioned path (codex's own documented
# image feature) — no backend spoofing, no ToS gray zone, free on the plan.
#
# The crux is the PROMPT: with a weak prompt codex is lazy and FAKES the image by
# drawing it with code/SVG/PIL (programmer-art, ~tens of KB) while claiming it used
# imagegen. This wrapper bakes in the forcing language that makes it actually call
# the tool, then VERIFIES by file size (real gpt-image-2 ≈ MBs; a code-drawn fake is
# tiny). NOTE: in `codex exec` the image is returned INLINE and codex self-decodes
# it to the file, so ~/.codex/generated_images does NOT grow — size + eyeball are
# the real check, never codex's own "done, verified" text.
#
# Usage: codex_imagegen.sh --prompt "<DETAILED PROMPT>" --out tmp/out.png [--size 1024x1024] [--min-kb 200]
set -u
PROMPT="" OUT="" SIZE="1024x1024" MINKB=200
while [ $# -gt 0 ]; do
  case "$1" in
    --prompt) PROMPT="$2"; shift 2;;
    --out)    OUT="$2"; shift 2;;
    --size)   SIZE="$2"; shift 2;;
    --min-kb) MINKB="$2"; shift 2;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done
[ -n "$PROMPT" ] && [ -n "$OUT" ] || { echo "usage: codex_imagegen.sh --prompt P --out PATH.png [--size 1024x1024]" >&2; exit 2; }

# absolute, forward-slash save target (codex needs an explicit path)
case "$OUT" in
  /*|[A-Za-z]:*) ABS="$OUT";;
  *) ABS="$(pwd)/$OUT";;
esac
mkdir -p "$(dirname "$ABS")"
rm -f "$ABS"

INSTR="Use the imagegen image-generation tool (gpt-image-2) to GENERATE a real raster image: ${PROMPT}. Target size about ${SIZE}. Save the resulting PNG to '${ABS}'.

HARD RULES (follow exactly):
1. You MUST actually invoke the imagegen tool to create the pixels and decode its returned image to the file. This is mandatory.
2. Do NOT draw or synthesize the image with code, SVG, PIL/Pillow, ImageMagick, System.Drawing, matplotlib, cairo or any shapes/vectors. Code-drawn output is a FAILURE.
3. If the imagegen tool is NOT available in this context, print exactly 'IMAGEGEN UNAVAILABLE' and STOP. Do not fake it, do not substitute another image.
4. No text or watermark in the image. After saving, report the exact tool you used."

LOG="$(mktemp)"
codex exec --dangerously-bypass-approvals-and-sandbox --skip-git-repo-check -C "$(pwd)" "$INSTR" 2>&1 | tee "$LOG" | tail -6

# The FILE is ground truth. Do NOT trust the transcript: codex echoes the rule
# text ("IMAGEGEN UNAVAILABLE") and its own "done, verified" claims even when it
# faked — only believe size + your own eyeballs.
rc=1
if [ -f "$ABS" ]; then
  KB=$(( $(stat -c%s "$ABS") / 1024 ))
  if [ "$KB" -lt "$MINKB" ]; then
    echo "RESULT: SUSPECT FAKE — $ABS is ${KB}KB (< ${MINKB}KB); codex likely code-drew it. Read the PNG to confirm before using." >&2
  else
    echo "OK wrote $ABS (${KB} KB) via official codex imagegen — eyeball it to confirm."
    rc=0
  fi
elif grep -q "IMAGEGEN UNAVAILABLE" "$LOG"; then
  echo "RESULT: no file, and codex said imagegen is UNAVAILABLE in this context." >&2
else
  echo "RESULT: FAILED — no file written at $ABS" >&2
fi
rm -f "$LOG"
exit $rc
