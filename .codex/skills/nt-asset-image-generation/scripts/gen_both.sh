#!/usr/bin/env bash
# Compare mode: generate the SAME prompt via BOTH engines in PARALLEL, so you can
# pick the best. codex/gpt-image-2 = higher fidelity + cleaner text; agy/Antigravity
# = often more "game-y" / blocky-plastic. "Best" is a VISUAL judgment вЂ” Read both
# PNGs and judge against the brief / fake-shot direction. NEVER a pixel metric.
#
# Usage:
#   gen_both.sh --prompt "<DETAILED PROMPT>" --name shipicon [--out-dir tmp/gen] \
#               [--size 1024x1024] [--aspect "square 1:1"]
set -u
PROMPT="" OUTDIR="tmp/gen" NAME="asset" SIZE="1024x1024" ASPECT="square 1:1"
while [ $# -gt 0 ]; do
  case "$1" in
    --prompt)  PROMPT="$2"; shift 2;;
    --name)    NAME="$2"; shift 2;;
    --out-dir) OUTDIR="$2"; shift 2;;
    --size)    SIZE="$2"; shift 2;;
    --aspect)  ASPECT="$2"; shift 2;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done
[ -n "$PROMPT" ] || { echo "usage: gen_both.sh --prompt P --name N [--out-dir D] [--size S] [--aspect A]" >&2; exit 2; }

SKILL_DIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUTDIR"
WINROOT="$(pwd -W 2>/dev/null || pwd)"        # C:/... form for the agy (Windows) binary
CODEX_OUT="$OUTDIR/${NAME}_codex.png"
AGY_OUT="$OUTDIR/${NAME}_agy.png"
AGY="${AGY_COMMAND:-agy.exe}"

echo ">>> generating '${NAME}' via BOTH engines in parallel ..."

# --- codex (official imagegen) in background ---
( bash "$SKILL_DIR/codex_imagegen.sh" --prompt "$PROMPT" --out "$CODEX_OUT" --size "$SIZE" \
    > "$OUTDIR/.${NAME}_codex.log" 2>&1 ) &
CODEX_PID=$!

# --- agy (Antigravity) in background ---
ABS_AGY="$WINROOT/$AGY_OUT"
( rm -f "$AGY_OUT"; "$AGY" --dangerously-skip-permissions -p \
    "Use your built-in image generation to create one real raster image (not code-drawn), ${ASPECT}: ${PROMPT}. Save the PNG to ${ABS_AGY} . Do not write or run any drawing code." \
    < /dev/null > "$OUTDIR/.${NAME}_agy.log" 2>&1 ) &
AGY_PID=$!

wait $CODEX_PID; wait $AGY_PID

echo "================= RESULTS ================="
for label in "codex:$CODEX_OUT" "agy:$AGY_OUT"; do
  eng="${label%%:*}"; f="${label#*:}"
  if [ -f "$f" ]; then
    echo "[$eng] OK  $f ($(( $(stat -c%s "$f")/1024 )) KB)"
  else
    echo "[$eng] MISSING $f  (see $OUTDIR/.${NAME}_${eng}.log)"
  fi
done
echo "Next: Read BOTH PNGs and pick the best against the brief (qualitative, never pixel)."
