# Studio skill authoring

Checked against official documentation on 2026-10-18. This guide owns the
Studio's authoring conventions; provider behavior remains defined by the
linked sources.

## Sources

- [OpenAI: build skills](https://learn.chatgpt.com/docs/build-skills): local
  discovery, progressive loading, optional UI metadata, and invocation policy.
- [Anthropic: Claude Code skills](https://code.claude.com/docs/en/skills):
  project discovery, frontmatter, invocation controls, and supporting files.
- [Anthropic: authoring practices](https://platform.claude.com/docs/en/agents-and-tools/agent-skills/best-practices):
  concise instructions, conditional references, and evaluation before expansion.
- [Agent Skills specification](https://agentskills.io/specification): portable
  metadata and directory contract.

## Ownership and discovery

Edit the existing canonical `.codex/skills/<name>/SKILL.md` and its supporting
resources. Run `skills_sync.mjs` in this module to generate discovery copies
under both `.agents/skills/` for current Codex and `.claude/skills/` for Claude
Code. The legacy canonical path remains to preserve Studio callers and older
hosts. Do not maintain independent copies of instructions or references.

Both hosts first discover metadata, then read the selected entrypoint, and read
conditional references only when needed. A generated copy must resolve from the
repository root even when a session starts in a game directory. Hand-written
skills are preserved by the generator; check their names for discovery collisions
before adding a canonical skill with the same name. Hosts that scan both legacy
and current locations can show duplicates; use one discovery location in that
host rather than independently editing either generated copy.

## Write the smallest useful contract

- Use a lowercase hyphenated name matching its folder and a concise description
  that starts with the actual task and tells the agent when to use it.
- Keep purpose, essential constraints, input/output expectations, and reference
  routing in the entrypoint. Move long conditional procedures into references.
- Prefer links to the owning module over copied commands, schemas, or policies.
  Keep instructions portable between Codex and Claude; host-specific mechanics
  belong in a clearly selected reference or adapter.
- Add scripts only for deterministic or repeatedly executed work. Run changed
  scripts against real inputs or isolated fixtures.
- Preserve authorization boundaries. A skill does not grant permission to
  publish, send messages, expose a service, or create Taskboard items.
- Keep project facts in their owners and game facts in the game's private
  design knowledge. Do not put private game identities in Studio skill files.

## Provider-specific settings

For Codex, optional `agents/openai.yaml` owns appearance, tool dependencies, and
`policy.allow_implicit_invocation`. The default is automatic discovery.
For Claude Code, `disable-model-invocation` controls automatic invocation;
`user-invocable` controls the user's command menu. These are separate controls.
Claude's `allowed-tools` grants tool pre-approval; it is not a tool restriction.
Do not copy these provider-specific settings into the other host's metadata.

The Studio generator carries `name`, `description` and the entrypoint body into
generated copies, copies
Codex's existing `agents/openai.yaml` into the current Codex discovery folder,
and maps `allow_implicit_invocation: false` to Claude's
`disable-model-invocation: true`. Markdown links into the skill folder are
rebased to `.codex/skills/<name>/`; links that climb to the repository root stay
as written because both surfaces sit at the same depth. A manual file with a canonical name is preserved and reported as a
collision. Other host-specific fields require an explicit generator extension
with tests before they can govern a generated copy.

## Verify a change

1. Read the description as the only context: check representative matching and
   neighboring non-matching requests. Avoid descriptions that claim every task.
2. Follow the entrypoint on a realistic task; verify references resolve and the
   result respects the requested scope. Use independent forward testing when
   the workflow's complexity warrants it.
3. Run the surface tests and generator, then `sync.mjs --check` and the harness
   doc reference check. Structural validation does not prove trigger quality or
   successful behavior in either live host.

```powershell
node --test ai_studio/core_harness/agent_surfaces/tests/skills_sync.test.mjs
node ai_studio/core_harness/agent_surfaces/skills_sync.mjs
node ai_studio/core_harness/agent_surfaces/sync.mjs --check
node ai_studio/core_harness/validation/doc_reference_check.mjs
```
