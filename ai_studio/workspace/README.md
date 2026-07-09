# Workspace

Workspace owns the local/private mount registry for game workspaces that are
available to this checkout but must not become public Studio state.

## Game Registries

- `games/games.json` is the public, tracked registry for games that may appear
  in committed Studio files, generated IDE entries, asset source lists, and
  aggregate agent context.
- `ai_studio/workspace/games.local.json` is the local, ignored registry for
  private or machine-local game mounts. It may contain private game ids, aliases,
  paths, and remotes.

Private entries are excluded by default. Tools must call the workspace resolver
with an explicit active workspace or `--include-private` behavior before private
mounts can appear in an aggregate list.

## Local Registry Shape

```json
{
  "schema": "ai_studio.workspace.games.local.v1",
  "games": [
    {
      "schemaVersion": 1,
      "storeId": "game:secret-game",
      "kind": "game",
      "gameId": "secret-game",
      "root": "games/secret-game",
      "visibility": "private",
      "gitRoot": "games/secret-game",
      "commitPolicy": "nested-private",
      "enabledStores": ["assets", "taskboard", "canvas", "evidence"],
      "publicAlias": "optional-safe-display-name",
      "aliases": [],
      "remoteHints": []
    }
  ]
}
```

Required rules:

- `storeId` is `game:<gameId>`.
- `root` and `gitRoot` are `games/<gameId>`.
- `visibility` is `private` or `local`.
- `commitPolicy` is `nested-private` or `local-only`.
- Duplicate game ids are rejected unless a local entry explicitly overrides a
  public fixture record.

## Private Root Git Boundary

The public `.gitignore` ignores only `ai_studio/workspace/games.local.json`.
Do not add a broad `games/*` rule because public games may be tracked.

For a private nested repository at `games/<id>`, ignore that concrete root in
local parent-git config:

```text
# .git/info/exclude
games/<id>/
```

The private game may still have its own `games/<id>/.git` and private remote.
The parent Studio repo must not track private files or a public gitlink for that
root.

## Commands

List public games only:

```powershell
node ai_studio/workspace/games.mjs list --json
```

List private mounts explicitly, after preflight:

```powershell
node ai_studio/workspace/games.mjs list --include-private --json
```

Run the privacy preflight before private mounts feed any generator or aggregate
surface:

```powershell
node ai_studio/workspace/games.mjs preflight --json
```

Guard a parent Git command before an agent or helper runs it:

```powershell
node ai_studio/workspace/games.mjs git-guard --command "git add ." --json
```

Codex and Claude shell `PreToolUse` hooks run `hook-guard` from the generated
agent hook configs. The guard blocks broad or private-root parent `git add` and
`git clean` commands when local private mounts exist, and blocks `git commit`
when preflight finds tracked, staged, gitlink, ignore, nested-git, or tracked
text leak violations.

Preflight fails if a private root or local registry is tracked, staged, recorded
as a parent gitlink, not ignored by the parent repo, or leaked through tracked
text files such as task metadata, canvas refs, evidence paths, generated IDE
files, staged blobs, or public registry data.

## Validation

```powershell
node --test ai_studio/workspace/tests/private_games_registry.test.mjs
node ai_studio/workspace/games.mjs preflight --json
node ai_studio/core_harness/agent_surfaces/hooks_sync.mjs --check
```
