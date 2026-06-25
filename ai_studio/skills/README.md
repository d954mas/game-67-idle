# Skills

Owns reusable agent procedures.

The current canonical skill entrypoints remain in `.codex/skills/` until the
sync/export tooling is migrated. Generated `.claude/skills/` stays a generated
compatibility surface.

Migration target:

- keep skill entrypoints thin;
- move long method text into domain references;
- make each skill point to the owning `ai_studio/<domain>/` contract.

