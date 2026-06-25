# Tools

Owns stable AI-studio command surfaces and their contracts.

The current executable location is still root `tools/` because scripts, docs,
and validators call those paths directly. During migration, root commands should
remain as compatibility shims while reviewed implementation code moves under
the owning `ai_studio/` domain.

## Rule

An agent-facing tool needs:

- a clear command name;
- compact output;
- helpful failures;
- tests or a validation route;
- an owner domain.

