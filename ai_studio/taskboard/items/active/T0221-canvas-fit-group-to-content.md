---
id: T0221
title: "Canvas: fitGroup op - resize group frame to fit its content"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Lead request (2026-07-02, right after T0219 landed): "я видел что могу задать
[размер группы] руками. а что если я хочу сделать больше, под размер всех
детей? мб какая-то кнопка fit group или что-то такое". Group frame size is
manual-only today; add a Figma-like "Resize to fit": `fitGroup({projectId,
groupId, padding?})` sets the group bbox to the union of ALL descendant boxes
(elements + nested subgroup frames; reuse the elementsBBox math sliceRegions
uses), default padding small (sliceRegions uses 24; pick one default, expose
the arg). Children never move - only the frame changes. One journal entry,
undoable. Works with clip on (frame change re-evaluates the clip - that is the
point of the button). Tool parity: inspector "Fit to content" button next to
Position & Size, group context-menu item, CLI `group-fit <id> --group <gid>
[--padding n]`, HTTP route.

## Done when

- [ ] fitGroup op: bbox = union of descendant closure boxes + padding; empty group = loud error (nothing to fit); one journal entry; undo restores old frame
- [ ] inspector button + context-menu item + CLI + HTTP route (parity)
- [ ] works at 2 nesting levels (fits around a nested subgroup's frame) and with clip=true
- [ ] tests + gates green

## Open questions

## Log
- 2026-07-02: Created from lead request during T0219 review window. Build right after T0203 lands (same site files in flight).
