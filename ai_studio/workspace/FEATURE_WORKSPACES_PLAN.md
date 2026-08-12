# Feature Workspaces Plan

## Outcome

Add one repository-owned CLI that creates, inspects, validates, and removes an
isolated pair of Git worktrees for parallel game feature work:

```text
<workspace-base>/<name>/
  workspace.json
  studio/                                      detached Studio worktree
    external/neotolis-engine/                  initialized submodule checkout
    features/
    games/private/<game-id>/                   game worktree on its own branch
```

An agent starts from `<workspace-base>/<name>/studio`. Its searches, generated
files, build directories, temporary files, and uncommitted edits remain inside
that workspace. Git object databases and explicitly external asset stores may be
shared, but working files are not.

This tool manages filesystem and Git isolation only. It does not create
Taskboard projects, epics, or tasks; choose agent roles; launch agents; merge
branches; or edit the engine repository.

## Decisions

### Source state

- The source Studio and game worktrees may be dirty.
- `new` resolves and records the committed Studio and game `HEAD` values at the
  start of the operation. Only those commits seed the new workspace.
- Staged, unstaged, untracked, ignored, and generated files are never copied.
- Dirty source state produces a warning that names the affected repository and
  reports counts by category. It does not block creation.
- A requested task must exist in the game commit, and the committed Taskboard
  counter must cover its numeric id. A task that exists only in the source
  working tree is rejected. This proves that the selected commit contains a
  structurally valid assigned card; the counter alone is not a global
  cross-branch reservation mechanism.
- Version one always seeds both worktrees from the source repositories' current
  committed `HEAD` values. It does not accept arbitrary refs. This keeps every
  parallel task on one authoritative game lineage while dirty working files
  remain behind.
- The source game must be on an attached local integration branch. The CLI
  records its full ref name and creation-time tip; detached source games are
  rejected. Merge/divergence reporting uses that recorded ref, never a
  hard-coded `master` name.

### Repository topology

- Studio uses `git worktree add --detach`; feature agents do not need a Studio
  branch and must treat Studio, `features/`, and `external/neotolis-engine` as
  read-only unless separately assigned Studio work.
- The engine submodule is initialized at the exact gitlink recorded by the
  selected Studio commit. The CLI never edits or advances the engine.
- The private game uses its own repository's `git worktree add -b` at the exact
  selected game commit. Default branch name:
  `agent/<task-id-lowercase>-<workspace-name>`.
- Workspace names and generated branch components use lowercase kebab-case.
  Existing paths, worktree registrations, and branches cause a deterministic
  failure; the CLI never silently appends a timestamp.
- The game stays at `studio/games/private/<game-id>` so existing workspace
  discovery, CMake depth detection, Taskboard routing, and privacy checks keep
  their normal contracts.

### Taskboard and local stores

- `--task T####` is required for `new` in the first version.
- The task must belong to the selected game store at the selected game commit,
  live in the active-task tree, and be eligible for implementation (`backlog`,
  `todo`, or `doing`). Raw ideas, review-only cards, closed history, and
  Studio-store tasks are rejected.
- The registry refuses a second live workspace for the same qualified task.
- The feature agent may update and commit its assigned task through `review`.
  Final `done` closure and archive routing happen after integration in the main
  game worktree.
- Agents in feature workspaces must not create Taskboard items. This keeps the
  committed counter unchanged and follows the lead-only creation boundary.
- Tracked Taskboard data is naturally present in the game worktree.
- Ignored Canvas projects, local evidence, `build/`, `tmp/`, `.vite/`, and
  other ignored state are absent. `check` reports this as expected isolation,
  not data loss.

### Ports and registry

- Each workspace receives advisory leases for a native DevAPI port and a web
  server port. Suggested default pools are `17900..17999` and `5190..5290`;
  both remain configurable.
- Leases live only in `<workspace-base>/.feature-workspaces/`, outside every Git
  worktree. The registry is local machine state, not project state.
- Registry mutation is guarded by an exclusive lock directory. While holding
  the lock, creation scans live manifests, verifies candidate ports can bind on
  loopback, leases the pair among CLI-managed workspaces, and writes the
  creating manifest atomically. The sockets are then closed; this coordinates
  our CLI instances but cannot reserve ports against unrelated processes.
- A bounded lock wait fails with the lock owner metadata and recovery advice.
  Stale locks are reported by `check`; they are never silently stolen by `new`.
  `recover` may quarantine a lock only when its metadata names the local host
  and its owner PID is provably absent. Ambiguous ownership stops for manual
  intervention.
- The assigned ports are written to `workspace.json` and printed as concrete
  launch arguments. Source files and CMake caches are not edited to store them.
- Launch-time bind failure is an expected external race. `reallocate-ports`
  acquires the registry lock, verifies the workspace processes are stopped,
  leases a new pair, and atomically updates the manifest and active record.

## CLI contract

Owning file: `ai_studio/workspace/feature_workspaces.mjs`.

```text
node ai_studio/workspace/feature_workspaces.mjs new \
  --game <game-id> \
  --task T0129 \
  --name death-reward

node ai_studio/workspace/feature_workspaces.mjs list [--base <path>] [--json]
node ai_studio/workspace/feature_workspaces.mjs check <name> [--base <path>] [--json]
node ai_studio/workspace/feature_workspaces.mjs recover <name> [--base <path>] [--json]
node ai_studio/workspace/feature_workspaces.mjs reallocate-ports <name> [--base <path>] [--json]
node ai_studio/workspace/feature_workspaces.mjs remove <name> [--base <path>] [--json]
```

Optional `new` flags:

```text
--root <studio-root>
--base <workspace-base>
--branch <new-game-branch>
--devapi-port <port>
--web-port <port>
```

The default base is the sibling `<studio-folder-name>-workspaces` directory;
for a checkout named `studio` it resolves to the sibling `studio-workspaces`.
Every
command accepts the same `--base` override. There is no hidden per-command base
discovery.

All commands support stable JSON output for agents. Human output leads with the
workspace path, task, branch, source commits, ports, and next action.

## Manifest

`workspace.json` is the recovery authority for a workspace operation:

```json
{
  "schema": "ai_studio.feature_workspace.v1",
  "state": "creating",
  "operationId": "random UUID",
  "name": "death-reward",
  "taskId": "T0129",
  "gameId": "<game-id>",
  "createdAt": "ISO-8601",
  "sourceStudioRoot": "absolute canonical path",
  "sourceStudioCommit": "40-char commit",
  "sourceGameRoot": "absolute canonical path",
  "sourceGameCommit": "40-char commit",
  "integrationRef": "refs/heads/master",
  "integrationTipAtCreate": "40-char commit",
  "studioWorktree": "absolute canonical path",
  "gameWorktree": "absolute canonical path",
  "gameBranch": "agent/t0129-death-reward",
  "ports": { "devapi": 17900, "web": 5190 },
  "ownership": {
    "studioCommonGitDir": "canonical path",
    "gameCommonGitDir": "canonical path",
    "engineGitlink": "40-char commit"
  },
  "completedSteps": []
}
```

The manifest starts in `creating`, is atomically rewritten after every
completed step, and becomes `ready` only after validation. A compact active
record lives at `<base>/.feature-workspaces/active/<name>.json`; port leases and
the operation nonce live there as well. `list` enumerates those records rather
than arbitrary base children and exposes incomplete manifests without changing
their state.

Paths stored in either record are assertions only. Every operation derives its
targets from `canonical(base)/validated-name/{studio,workspace.json}`, then
compares recorded paths and Git identities to the derived reality.

## Creation transaction

1. Discover the Studio root and selected private game through the existing
   workspace catalog. Resolve physical paths and reject path escapes.
2. Resolve both current `HEAD` values to immutable commit ids, resolve the
   source game's attached integration ref, and capture source dirty-state
   summaries for warnings only.
3. Read the task and `.counters.json` from the selected game commit with Git
   object commands, not from the source filesystem. Validate task ownership,
   active location, and counter monotonicity.
4. Validate the workspace name, base, destination, generated branch, and both
   repositories. The base must be a physical non-reparse directory (or be
   created by this operation); reject case-insensitive collisions, symlinks,
   junctions, other reparse points, existing destinations, branches, or
   worktrees.
5. Acquire the local registry lock and allocate ports. Atomically publish the
   `creating` active record first; it contains the operation id, derived paths,
   ports, source identities, and no claim that a directory exists yet. Then
   create the workspace directory and atomically write `workspace.json`,
   recording each boundary in the active record. Thus a crash before directory
   creation is visible, and a crash after directory creation but before the
   manifest is recoverable from the deterministic active record. The active
   record itself is the port lease; there is no separately persisted lease
   object whose ordering can diverge.
6. Add the detached Studio worktree at the resolved Studio commit.
7. Before initialization, prove the exact engine gitlink object exists in the
   source engine repository. Initialize only `external/neotolis-engine` using
   the local source engine as a command-scoped URL override, checkout mode, and
   `--no-fetch`; never resolve the network URL from `.gitmodules`. Snapshot and
   assert that the source repositories' persistent Git configuration is
   unchanged apart from expected worktree/module administrative records. If
   the exact object is unavailable locally, fail before this step.
8. Create `studio/games/private/`, then add the game worktree and new branch at
   the resolved game commit.
9. Run private-game preflight against the new Studio root, Taskboard validation
   for the selected game store, and structural checks for engine/features.
10. Mark the manifest `ready`, release the registry lock, and print the agent
    working directory plus port-specific launch examples.

On ordinary failure, unwind completed steps in reverse order: remove the game
worktree, deinitialize the engine submodule, remove the Studio worktree, release
ports, and remove the reserved workspace directory. The Studio worktree uses
Git's single `--force` form only after exact ownership and clean-state checks:
Git otherwise refuses a linked worktree that initialized a submodule, even
after deinitialization. The game worktree is never force-removed, and Git's
double-force lock override is forbidden. If safe rollback cannot complete,
preserve the manifest as
`recovery-required` with exact residual paths and commands.

If the process dies after a persistence or Git boundary, `recover <name>`
inventories reality instead of trusting `completedSteps` and resumes according
to the active record's transaction mode. A `creating` record rolls creation
back; a `removing` record resumes forward removal from the first incomplete
boundary, including states after `workspace.json` deletion and after
`portsReleased:true`. A `recovery-required` record retains its original
transaction mode. Recovery only claims an artifact when all immutable evidence
matches: derived canonical target, expected common-Git-dir identity, worktree
registration, detached Studio commit or game branch name/ref, and engine
gitlink. Exact owned artifacts are handled idempotently. Missing artifacts are
accepted only when the active record shows their boundary was reached or
reality proves the exact prior operation completed. Any mismatch preserves the
active record, marks it `recovery-required`, and prints manual evidence;
recovery never uses force or `git worktree prune`.

Recovery explicitly covers all pre-Git persistence states: active-record only,
active record plus empty workspace directory, and manifest published before or
after any later step marker. An owned empty directory may be removed
non-recursively; a nonempty pre-manifest directory or identity mismatch stops
for manual inspection. Failure injection tests run immediately before and
after active-record publication, directory creation, manifest publication, and
every subsequent manifest/active-record rewrite.

## Check and list behavior

`list` is a bounded manifest scan. For each workspace it reports:

- manifest state, task, game, commits, branch, and ports;
- whether both directories and both Git worktree registrations exist;
- dirty counts for Studio and game;
- game branch divergence from its recorded source commit and from the recorded
  integration ref's current tip;
- whether the task still exists in the workspace and has remained the assigned
  task;
- whether the loopback ports appear occupied.

`check <name>` additionally runs the private-game preflight, filesystem
Taskboard validation, submodule/gitlink verification, path-containment checks,
lock-owner diagnostics, and branch ownership checks. It does not modify or
repair anything. It resolves an active record first and a registry tombstone
second, so checking a removed name produces an explicit removed result.

## Removal transaction

1. Resolve the workspace from its active registry name and derive targets only
   as children of the canonical configured base. Treat manifest paths as
   comparisons, never deletion targets. Reject case aliases and any reparse
   point on the base, workspace, Studio, private-game, or relevant ancestor
   paths.
2. Require a `ready` live manifest for a fresh removal and acquire the registry
   lock. If the active record is already `removing` or removal-mode
   `recovery-required`, route to the same state-aware recovery engine; the
   active record remains sufficient authority even after `workspace.json` has
   been removed.
3. Refuse removal when either worktree has staged, unstaged, or non-ignored
   untracked changes. The nested game path is ignored by Studio and inspected
   separately; the engine submodule must also be clean at its recorded gitlink.
   Ignored build output may be removed because it is workspace-local and
   reproducible. Refuse while either assigned port is listening so a live game
   or server cannot lose its working directory.
4. Record whether the game branch contains commits beyond its source commit and
   whether those commits are merged into the recorded integration ref's current
   tip.
   Removal may proceed with a clean unmerged branch because the branch remains
   in the game repository, but output must prominently state that it is
   unmerged. Version one never deletes branches.
5. Before the first destructive cleanup, atomically change the active record
   from `ready` to `removing`, set `transactionMode:remove`, and journal the
   validated clean-state/merge assessment. Every following cleanup, worktree,
   submodule, filesystem, and port boundary is journaled atomically before the
   next destructive step, so a crash can never masquerade as a ready workspace.
6. Enumerate reproducible ignored output separately in the game, Studio, and
   engine with Git's ignored-file view. Remove only literal, physically
   contained, non-reparse entries returned by Git; never follow links or
   recursively delete a repository root. For Studio cleanup, an ignored
   candidate is forbidden when it equals, contains, or is contained by either
   nested Git root (game or engine). Thus `games/private/`, the game subtree,
   and the engine subtree are protected, while unrelated siblings such as
   Studio-local `build/` remain eligible. Clean game and engine ignored output
   only through their own Git views. Then remove the game worktree first.
7. Deinitialize the engine submodule from the Studio worktree. This explicit
   step prevents the broken-submodule failure observed in old worktree cleanup.
8. Remove the Studio worktree with Git's single `--force` form after the clean
   and ownership gates above. This is required for a linked worktree that
   initialized a submodule; double-force and force-removal of the game worktree
   remain forbidden.
9. Keep the active record as recovery authority through the entire filesystem
   teardown. Remove `workspace.json`, and use
   non-recursive empty-directory removal for `<base>/<name>`; unexpected files
   stop removal while the active record remains. Then mark `portsReleased:true`
   in that same active record so allocation ignores its former advisory ports.
   Finally rewrite the active record as the tombstone payload and atomically
   rename it within the registry to
   `<base>/.feature-workspaces/tombstones/<name>.json`. That rename is the final
   commit of removal; there is never a state with stale leases and neither live
   recovery authority nor tombstone. Tombstones contain name, task, branch,
   commits, removal time, and any unmerged-branch warning.

Any failed removal stops at the failed step and preserves a recovery-required
active record; the live manifest is retained when it still exists but is not
required after its recorded deletion boundary. Deletion targets are derived
from base and name, not supplied by either record. The CLI never follows a
reparse point or recursively deletes a repository/workspace root.

## Implementation increments

### Increment 1: pure contract and Git inspection

Files:

- `ai_studio/workspace/feature_workspaces.mjs`
- `ai_studio/workspace/tests/feature_workspaces.test.mjs`

Implement argument parsing, name/branch normalization, root/base confinement,
commit resolution, dirty summaries, committed-task lookup, committed counter
validation, integration-ref capture, registry/tombstone schemas, manifest
parsing/serialization, and human/JSON result shapes. The committed-state task
validator reads blobs from the selected Git tree and verifies the unique active
path, filename/frontmatter id, implementation-eligible status, project/epic
ownership, and counter. The later filesystem validator remains the existing
Taskboard store validator plus assigned-task checks.

Verification:

```text
node --test ai_studio/workspace/tests/feature_workspaces.test.mjs
```

Acceptance:

- Dirty source repositories are accepted and reported.
- Only committed objects influence task/ref validation.
- A task present only as an untracked or modified file is rejected.
- A wrong filename/frontmatter id, malformed or missing counter, ineligible
  status, broken project/epic ownership, duplicate live assignment, and
  detached source game are rejected.
- Malformed names, paths, refs, task ids, manifests, and counters fail before
  filesystem mutation.

### Increment 2: transactional creation

Implement registry locking, advisory port leases, detached Studio worktree creation,
engine submodule initialization, nested game worktree creation, validation, and
reverse-order rollback. Inject command execution and failure points so every
transaction boundary is testable without global Git configuration.

Verification adds fixture repositories containing a local engine submodule and
a nested private game repository. Test success plus injected failure after each
completed step.

Acceptance:

- A successful fixture has the required topology, exact commits, distinct
  ports, valid private discovery, and its assigned committed task.
- Dirty tracked and untracked source files do not appear in the new workspace.
- Every injected failure either restores the pre-call state or leaves one
  explicit recovery-required active record (and a manifest when that boundary
  was reached) with no silent registrations.
- Two concurrent creates receive distinct paths, branches, and port pairs.
- Missing local engine objects fail before submodule mutation and no test may
  require network access.
- Crash injection around every active-record, directory, manifest, and Git
  boundary is recoverable through the immutable ownership matrix.

### Increment 3: inspection and safe removal

Implement `list`, `check`, `recover`, `reallocate-ports`, and
manifest-driven reverse-order removal. Cover
clean merged, clean unmerged, dirty, missing-path, stale-registration,
submodule, stale/ambiguous locks, external port races, ignored build output,
tombstone lookup, and partial-removal cases.

Acceptance:

- Dirty worktrees are never removed.
- Dirty engine and Studio untracked-state cases are independently detected
  before any destructive command.
- When Studio reports `games/private/` as ignored, cleanup preserves the nested
  game and its ignored files until the game repository's own cleanup and
  successful `git worktree remove` step.
- A simultaneous Studio-local ignored build directory is cleaned, proving the
  nested-root predicate does not exclude unrelated Studio siblings.
- Removal recovery resumes from active-record-only `removing` states after
  manifest deletion, port release, and immediately before tombstone rename.
- Clean unmerged game work retains its branch and is reported.
- The engine is deinitialized before Studio removal.
- Arbitrary paths, junction escapes, corrupted manifests, and branch ownership
  mismatches cannot become deletion targets.
- Re-running `check` after successful removal finds no live worktree or port
  lease and does find the tombstone.

### Increment 4: documentation and repository integration

Update:

- `ai_studio/workspace/README.md` with lifecycle commands and recovery rules;
- root `AGENTS.md` with a short pointer requiring this CLI for parallel feature
  work and forbidding manual game copies;
- `ai_studio/README.md` command routing if needed;
- Studio quality ownership only if the new test file is not already selected by
  the `ai_studio/workspace` domain.

Do not duplicate the operational contract in `AGENTS.md`.

Verification:

```text
node ai_studio/studio.mjs verify --domain workspace
git diff --check
```

Perform one deterministic Windows real-worktree smoke after the fixture suite:

1. Create a disposable Studio checkout at the current committed Studio head,
   wire its engine submodule from the local engine object store, and run
   `games/new_game.mjs` there to create the private game
   `feature-workspace-smoke` from `templates/template`. In that disposable
   nested game only, seed and commit one fixture epic plus eligible `T0001` and
   `T0002` cards with a matching counter. This is test-fixture construction,
   not mutation of a real Taskboard.
2. Run this CLI against the disposable source to create `smoke-one` for `T0001`
   and `smoke-two` for `T0002`. Both must record the same source heads.
3. Configure each game independently with
   `cmake -S . -B build/devapi-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
   -DGAME_DEVAPI_ENABLED=ON` and build both trees.
4. Launch each `build/devapi-debug/bin/game.exe` with its recorded
   `--devapi <port>` and `--fresh-state`. The disposable source is created from
   `templates/template`, whose committed `main.c` owns both flags.
5. Probe both DevAPI endpoints through the fixture game's committed DevAPI
   client, prove each returns its own process state, and
   stop both processes before removal.
6. In each game's ignored `tmp/feature-workspace-web/`, write an `index.html`
   sentinel containing that workspace name. Run
   `node tools/serve_web.mjs --dir tmp/feature-workspace-web
   --port <recorded-web-port>` in both workspaces and require each URL to return
   its own expected sentinel. Stop both servers, remove the disposable
   workspaces through the CLI, and delete the outer test fixture through its
   test-owned cleanup.

If the host lacks Ninja, a compiler, or a graphical session, fixture tests stay
mandatory and the real-worktree smoke is recorded as an explicit manual quality
checkpoint rather than silently passed.

## End-to-end acceptance

- From dirty Studio and game sources, `new` produces a clean workspace at the
  two recorded `HEAD` commits and warns about every omitted dirty-state class.
- The committed assigned Taskboard card and counter are present; uncommitted
  Taskboard state is absent and cannot be selected.
- Two feature workspaces can build and run concurrently with independent build
  trees and distinct ports.
- Searching from either Studio root does not traverse the other workspace.
- `check` detects manual tampering, stale registrations, branch mismatch,
  submodule drift, port conflicts, and incomplete transactions without writes.
- `remove` preserves dirty or uncommitted work, retains game branches, removes
  clean nested worktrees in the correct order, and never deletes outside its
  manifest-owned workspace.
- A killed create can be recovered without stale-lock deadlock or deletion of
  artifacts whose immutable ownership no longer matches.

## Deferred extensions

- Launching or supervising agent processes.
- Automatic merge/rebase/cherry-pick into the main game branch.
- Copying dirty working-tree state.
- Sharing ignored Canvas/evidence state.
- Branch deletion or remote publication.
- Supporting public games or Studio-editing workspaces.
