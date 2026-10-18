# Feature Testing

This module owns the dedicated test consumer for reusable Studio features.
It is outside `templates/` and is not offered by game creation or asset-source
template discovery. The blank starter tests only its own foundation.

## Public surface

```powershell
node ai_studio/feature_testing/run.mjs
```

The entrypoint runs the Studio `features` domain: feature contracts, Node and
Python checks, then the native CMake/CTest consumer. The Studio facade owns
test selection and output capture; this module does not maintain another tier
list. CTest labels remain the authority for `core`, `slow`, and `taste`.

For a focused native iteration, configure `consumer/` and select tests by CTest
name or label. Build outputs stay under `consumer/build/` and are ignored.

## Consumer fixture

`consumer/` preserves the integration code, schemas, content, registered assets
and tests needed to exercise existing features together. Its internal template
metadata exists only for toolchain fixtures; it is not a reusable game template.
The sample executable is excluded from the default build. There is no gameplay
or UI requirement imposed on a game created from the blank starter.

Settings and resource-panel source folders are optional copy-then-own reference
examples. Their INSTALL files declare schemas, theme, localization and scene
dependencies. Shared invariant implementations live in `features/`, not in this
fixture. The consumer compiles portal metrics from its shared owner and runs the
header-only limb solver's own invariant test.

## Validation and boundaries

`features/validate_contracts.mjs` checks this consumer's dependency seed and
reference manifests as well as actual template seeds. Native tests are wired
through the consumer's CMake files; adding a test does not require a second list
of test tiers. The Studio inventory check verifies tracked tests have an owner
and CMake registration.

Assets retain their existing provenance, license, origin and hashes. Private
game examples stay in private repositories. This fixture owns integration
evidence, not game design, engine internals, or release approval.
