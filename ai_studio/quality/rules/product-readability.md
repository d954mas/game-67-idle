# Product Readability Rule

Apply when the changed work affects a player-facing screen, interaction, UI,
onboarding, feedback, or task flow.

## Questions

- Where am I?
- What can I do now?
- What changed after the action?
- Why does it matter?
- What should I do next?

## Product Gate

For contested or strict product checks, record a product gate instead of
self-grading from memory:

```powershell
node tools/product_gate/review.mjs
```

For verification-only review, use the narrow named check and return
CONFIRM/REFUTE rather than broad commentary.
