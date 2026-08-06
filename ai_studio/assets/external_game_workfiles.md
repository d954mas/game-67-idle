# External Game Workfiles

Heavy authoring and production workfiles that are not themselves shipping game
assets live outside Git in the synchronized game workspace:

```text
<YandexDisk>/gamedev/games/<game-id>/
```

This rule applies to public and private games.

## Classification

Keep in the game repository:

- runtime/build assets actually consumed by the game;
- code, generators, conversion and validation scripts;
- compact configuration and decision records;
- license, provenance, integrity hashes and external-workfile manifests.

Keep in the external game workspace:

- Blender, Maya, Substance, Photoshop and similar authoring masters;
- raw video/audio captures and editable media projects;
- large source references and review renders;
- simulation, bake, render and application caches;
- intermediate exports that are not consumed by the game.

Keep disposable, reproducible output under repository `tmp/`; it is neither a
durable external workfile nor a committed asset.

## Required Layout

Use the repository game id exactly:

```text
<YandexDisk>/gamedev/games/<game-id>/
  blender/
  source-art/
  media/
  renders/
  references/
  caches/
```

Media-specific projects may add narrower folders below these roots. Do not
create new top-level per-game folders beside `gamedev/games/`.

## Manifest Contract

Every accepted external workfile must have a committed game-local manifest
that records:

- logical path relative to `<YandexDisk>/gamedev/games/<game-id>/`;
- byte size and SHA-256;
- producing script/config or manual-authoring provenance;
- accepted gate/version and the file's role;
- which game asset, if any, is later derived from it.

Use logical paths in committed shared metadata. Machine-specific absolute paths
belong only in private/local records. A game build must never require the
external workspace directly; promote a reviewed output into the game asset root
before runtime or packaging integration.

## Git Boundary

File size alone does not decide whether something belongs in Git. A small
`.blend` remains an authoring master; a large `.glb` may remain in Git when it
is the licensed, approved asset consumed by the game. Paid,
non-redistributable, or unknown-license binaries remain outside Git regardless
of role.
