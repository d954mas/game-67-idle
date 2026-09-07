# store_kit

The portal-facing half of a release: what a console draft asks for, and a
checker that answers before a month-long moderation queue does.

A moderation rejection costs the whole queue again, so nothing here is answered
from memory. Each portal has a spec file recording what its own form states, and
`check.mjs` reads a game's store folder against it.

```
node ai_studio/store_kit/check.mjs --portal yandex --dir games/<id>/release/store/yandex
```

## Contents

```text
ai_studio/store_kit/
  check.mjs              the checker: sizes, ratios, counts, byte caps, text limits
  yandex_spec.json
  playgama_spec.json
  crazygames_spec.json
```

## The form wins

Every spec carries a `source_of_truth` line saying where its numbers came from.
When the console form disagrees with a spec, the form is right and the spec file
is corrected in the same commit that discovers it. A spec may also declare what
the portal does not publish at all, under `undocumented`; those numbers are read
off the form when a draft is first created, never invented.

## Who captures the frames

The capture bot is game-owned and lives in `games/<id>/devapi/`. The beats worth
photographing are a property of the game, not of the studio, so there is no
shared bot to inherit. What every such bot must do is the same everywhere:

- Film real gameplay. An icon or cover made from a screenshot is a refusal, and
  a screenshot made from promotional art is the same refusal in reverse.
- Switch language the way a player does, through the picker, because the string
  table is chosen when it loads and a setting flipped mid-run does not change it.
- Prove the language off the running game before filing a frame under it. A card
  in one language showing another language's interface is the default failure.
- Shoot each shape separately. Desktop is landscape, mobile is portrait, and a
  portal shows one set at a time behind a select.

## What the checker cannot decide

Sizes, counts and character limits are mechanical. The rest is judgement, and
`check.mjs` prints it as a reminder rather than a verdict: no screenshot used as
an icon or cover, no borders or rounded corners, no system or portal interface
in a frame, gameplay filling most of every screenshot, and one name across the
game, the draft and the materials.

## Related

- `features/platform-sdk/references/portals/` — the requirement packets each
  spec answers to, cited to the portals' own documentation.
- `.codex/skills/nt-yandex-publish/` — the order of work for a Yandex draft.
