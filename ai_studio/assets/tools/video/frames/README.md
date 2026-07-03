# frames/ — stage 2: video -> PNG frames

Decodes the generated video into per-frame PNGs.

- **Entry:** `frames.mjs` — `runFrames({runDir | video, outDir})` and a CLI:
  `node frames.mjs --run-dir <dir>` (reads `<dir>/generate/*.mp4`) or
  `node frames.mjs --video <mp4> --out <dir>`.
- **Extractor:** `extract_frames.py` (PyAV `av`, `VideoFrame.to_image()`).
- **v1 coupling (documented):** this stage runs the **ComfyUI portable EMBEDDED
  Python** (`videoGenRoot/ComfyUI_windows_portable/python_embeded/python.exe`),
  which ships PyAV + PIL, via subprocess. The repo `.venv` deliberately has no
  heavy video deps and must not gain them for this. A missing source video or
  missing embedded interpreter is a LOUD error.
- **Output:** `<runDir>/frames/frame_%03d.png` + `frames.json` (source video,
  frame count, avg fps, `av` version, embedded interpreter path).
