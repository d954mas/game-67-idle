# Test Goldens

## Purpose

A game's numbers are design knobs. Balance constants, spawn counts, world
layout digests, render-target sizes and budgets move on purpose, on almost every
iteration. A test that writes such a value into its own source turns each design
change into test repair, and the repair is what makes iteration slow -- not the
test run.

This feature keeps the watched value in a small text bank beside the test.
Drift still fails the test, but re-recording is one command, and the diff shows
exactly which number moved.

Use it when accidental drift must be caught and the value itself belongs to
design. Prefer a plain invariant -- a range, an ordering, monotonicity -- when
one exists; a golden is the fallback for values no invariant describes, such as
a digest.

## Public surface

C tests:

```c
#include "features/test_goldens/test_goldens.h"

TEST_ASSERT_EQUAL_UINT64(test_golden_u64("planet_layout", "world_digest", digest), digest);
TEST_ASSERT_DOUBLE_WITHIN(0.5, test_golden_f64("planet_layout", "total_mass", mass), mass);
```

Node contract tests:

```js
import { golden } from "../../features/test-goldens/lib/test_goldens.mjs";

assert.equal(golden("web_budget", "wasm_bytes", actualBytes), actualBytes);
```

## Modes

- Compare (default): the helper returns the recorded value and the test asserts
  against it, so the failure message carries both numbers.
- Record (`GAME_UPDATE_GOLDENS=1`): the helper stores the actual value and
  returns it, so the same test passes and the bank file carries the change.

A key with no recorded value is a failure that names the command to record it.
Recording never rewrites unrelated keys.

## Bank format

One file per bank, `GAME_GOLDENS_DIR/<bank>.golden`, sorted, LF, `key = value`:

```text
object_count = 5722
total_mass = 1051.94
world_digest = 13559702318300364888
```

The format is deliberately flat and reviewable: a golden that a reviewer cannot
read in a diff is a golden nobody checks.

## Validation

`tests/test_test_goldens.c` covers recording, comparing a drifted value, and
insertion that leaves neighbouring keys untouched; it runs as the
`test_test_goldens` CTest in the template and in any game that registers it.
`lib/test_goldens.test.mjs` covers the same contract for the Node half plus the
one-line rule and the unrecorded-key message.
`features/validate_contracts.mjs` validates this router and manifest.

## Compatibility

PATCH changes preserve the bank format, the helper signatures, and the two
environment variables. MINOR changes may add helpers for further value types or
accept additional bank locations. MAJOR changes may alter the bank format or the
meaning of an unrecorded key, which invalidates recorded banks.

## Extension points

A game may point `GAME_GOLDENS_DIR` at any directory, so a suite that needs
per-preset banks can separate them. Further value types belong in this feature
rather than in a game's test helpers: the bank format is the contract both
halves share.

## Boundary

The feature owns storage and mode, not assertions. It does not know Unity, does
not compare values, and does not decide what deserves a golden -- see the game
test policy in `AGENTS.md`.
