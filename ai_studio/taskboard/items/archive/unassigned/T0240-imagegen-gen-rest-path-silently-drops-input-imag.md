---
id: T0240
title: "imagegen: gen_rest path silently drops --input-image refs (wrong endpoint)"
status: done
project: P001
epic: ""
priority: P2
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Fast-worker launched: gen_rest must stop silently dropping --input-image (route to the edits endpoint via curl multipart, or refuse loudly - worker reports which).
- 2026-07-03: Fixed via edits endpoint (preferred route): curl_post_multipart sibling of curl_post, image[] part per --input-image; no-ref path byte-identical. Offline monkeypatch sanity confirmed both branches; py_compile + --help clean. Committed.
