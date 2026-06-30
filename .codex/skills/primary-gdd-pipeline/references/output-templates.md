# Output Templates

Load this file only when writing repeated GDD pipeline artifacts. Keep outputs
short and replace placeholders with concrete project details.

## Session State (`tmp/session_state.md`)

```markdown
# Session State

## Active DoD
- Must exist:
- Out of scope:
- Accepted proof:

## Current Gate
- Gate:
- Status:
- Next action:

## Accepted Direction
- Visual:
- Gameplay:
- Tone:
- No-go:

## Open Questions
- [ ] Question:

## Changed Files
- File:

## Generated Assets
- Final:
- Temporary:
- Rejected:

## Validation
- Command:
- Result:
- Missing manual check:
```

## Design Decision Entry

```markdown
## YYYY-MM-DD - Decision title

- Status: accepted | rejected | superseded | needs review
- Decision:
- Why:
- Applies to:
- Source/user quote:
- Revisit when:
```

## Reference Pack Row

```markdown
## Ref: Name

- Source quality: user-provided | primary/studio | marketplace/store | secondary article | unverified
- Link:
- Player fantasy:
- Core loop:
- UI density:
- Economy/progression:
- Visual tone:
- Borrow:
- Avoid:
- Copy-risk:
```

## Risk Gate

```markdown
## Risk: Name

- Type: fun | UX | production | technical | asset/legal | balance
- Why it matters:
- Smallest test:
- Evidence needed:
- Owner action:
- Stop/continue rule:
```

## Final Status Report

```markdown
- DoD status:
- Files changed:
- Current gate:
- Next gate:
- Visual proof tier:
- Decisions captured:
- Assumptions needing review:
- Validation:
- Next prompt/checkpoint:
```
