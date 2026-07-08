# platform-sdk Install

This feature is currently contract-first. Do not install runtime code from this
folder yet; the implementation slice must add source files and tests first.

Use this manual to keep the future install shape explicit.

## Planned Install

When implementation files exist, a consuming template or game should wire this
as an L1 feature:

1. Add the feature's include/source or JS module path according to the
   implementation language chosen by the target template.
2. Configure target platform separately from platform SDK adapter:

   ```text
   target: local | itch | poki | yandex | playgama
   platformSdk: mock | poki | yandex | playgama
   ```

3. Use the canonical mapping unless a test fixture overrides it:

   ```text
   local    -> mock
   itch     -> mock
   poki     -> poki
   yandex   -> yandex
   playgama -> playgama
   ```

4. Make the target selection a build-time define/config value so only the
   selected SDK adapter is imported, compiled, or linked. A release build must
   not ship unused portal SDK URLs.
5. Route game code through the wrapper only. Game code must not call platform
   globals directly.
6. Read runtime identity and policy from the wrapper:

   ```text
   target platform
   platform SDK
   externalLinksAllowed
   ```

   Prefer capability checks for UI decisions. Example: show Telegram links only
   when `externalLinksAllowed` is true.
7. Emit local scorecard events from the wrapper regardless of target.

## Verify

Contract-only validation:

```powershell
node ai_studio/taskboard/cli.mjs validate --json
node ai_studio/architecture_map/validate_map.mjs
```

Future runtime validation must add:

- mock SDK unit tests with deterministic ad outcomes;
- fake injected SDK tests for Poki, Yandex, and Playgama adapters;
- browser smoke for local and itch targets with no network dependency;
- release artifact inspection proving only the selected SDK adapter URL is
  present;
- one real-SDK smoke per portal before submission.

## Uninstall

For the contract-only state, remove references to this feature from planning
docs and taskboard items. After runtime implementation exists, remove wrapper
wiring, feature sources, target config, scorecard event hooks, and tests from
the consuming template/game.
