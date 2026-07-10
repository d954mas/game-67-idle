---
id: T0372
title: Repair Architecture Map coverage descriptions and validators
status: backlog
project: P001
epic: E015
priority: P1
tags: [architecture-map, validation]
created: 2026-07-10
updated: 2026-07-10
---

## What

Repair the current Architecture Map validators and shorten human descriptions
without coupling that content rewrite to the storage split.

## Done when

- [ ] The current 13/15 baseline is green; hard-coded root-child counts are
      replaced by meaningful invariants and CLI imports have no side effects.
- [ ] Coverage truth uses `git ls-files`; generated/untracked hygiene is a
      separate optional report.
- [ ] Required agent/runtime ownership surfaces are represented.
- [ ] Descriptions are architectural, at most 240 characters, and exclude
      commands, flags, routes, test cases, and UI micro-behavior.
- [ ] A focused review proves the shortened text still lets a human locate each
      component and understand its responsibility.

## Open questions

## Log

- 2026-07-10: Owns validator/content quality only; `T0354` owns recursive
  storage and `T0371` owns graph deletion.
