# Canvas validation

Run focused tests while changing a domain, then the complete Canvas and Chat
suites:

```powershell
node --test ai_studio/assets/canvas/tests/*.test.mjs
node --test ai_studio/assets/canvas/chat/tests/*.test.mjs
node ai_studio/architecture_map/validate_map.mjs
node ai_studio/core_harness/validation/doc_reference_check.mjs
node ai_studio/taskboard/cli.mjs validate --json
```

For hosting smoke, start Studio Shell and open `/canvas`; verify the console is
clean, project open/edit/undo works, and Chat can start and cancel a turn.
