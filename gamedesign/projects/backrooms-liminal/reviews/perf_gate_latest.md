# Backrooms Liminal Perf Gate

- Verdict: **PASS**
- Build: native-debug, 1280x720
- Samples: 80 chunks, 5 frames/chunk
- Median frame wait: 20.00 ms (budget 22.00, PASS)
- P95 frame wait: 20.06 ms (budget 25.00, PASS)
- Max frame wait: 20.21 ms (budget 35.00, PASS)
- Draw calls: 3 (budget 8, PASS)
- Frame vertices: 1392 (budget 3200, PASS)
- Portal overlay vertices: 1386 (budget 1450, PASS)
- Mouse-look: yaw 0.0000 -> 0.3960 (PASS)

Notes:
- This gate measures native gameplay frame pacing through DevAPI frame waits.
- It intentionally does not include framebuffer/video capture readback cost.
