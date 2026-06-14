---
type: Source Notes
title: Poki Platform Notes For Rune Marches
description: Project-specific source notes for first Poki-audience test planning.
tags: [poki, playtest, web, mobile, sources]
timestamp: 2026-06-13T00:00:00Z
---

# Poki Platform Notes

Checked: 2026-06-13.

These notes support the first Rune Marches playtest packet. They are not a
claim that the game is submission-ready.

## Official Sources

- Poki Developers: `https://developers.poki.com/`
- Poki SDK Docs: `https://sdk.poki.com/`
- Poki SDK Requirements: `https://sdk.poki.com/requirements/`
- Poki SDK Game Events: `https://sdk.poki.com/game-events.html`
- Poki Developer Program FAQ: `https://developers.poki.com/developer-program`

## Constraints To Treat As Gates Later

- The game must work on mobile and tablet, not just desktop.
- Canvas should use a 16:9 aspect ratio and fit the full browser viewport.
- External requests need allowlisting in the Poki Inspector.
- The game should avoid unapproved external branding, links, stores, platform
  SDKs, tracking, and third-party ads.
- Local storage behavior needs testing under adblock and incognito/private
  browser modes.
- The official SDK event surface includes loading, gameplay start/stop, happy
  time, commercial break, and rewarded break events.

## Test-Program Signals

- Poki describes a Player Fit test before Web Fit, with about 500 plays.
- Their public FAQ describes Web Fit as a larger traffic step, with metrics
  such as CTR, average play time, and click-to-play.
- Their FAQ names average playtime above three minutes and at least 25% of
  players playing longer than three minutes as examples of strong signals.

## Rune Marches Assumptions

- Web build and Poki SDK integration are paused until that lane is explicitly
  reactivated.
- Native PC evidence can validate FTUE flow, visual readability, and event
  definitions, but cannot prove Poki platform fit.
- The first audience test should be informal/private before any claim of Poki
  review or Web Fit readiness.
