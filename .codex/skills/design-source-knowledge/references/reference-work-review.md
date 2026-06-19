# Reference Work And Review

Load this reference when a named reference guides gameplay, UI, economy,
balance, final art, or implementation decisions, or when auditing
source/knowledge hygiene.

## Reference-Driven Work

When a named reference drives gameplay, UI, economy, balance, or final art:

- declare study mode: quick check, central deconstruction, or deep
  deconstruction;
- use the Source Ladder in `reference_deconstruction.md`;
- create or update the durable deconstruction in the project wiki;
- include source matrix, observation ledger, borrow/avoid/copy-risk,
  current-build mismatch, and next native screenshot/scenario proof;
- do not claim "grounded in refs" unless you can cite at least three labeled
  facts and one current-build mismatch from the durable doc.

If the evidence is incomplete, state: "not ready for implementation", name the
missing evidence, and continue source gathering or ask for material.

## Quality Review

Before finishing, check:

- frontmatter exists on new durable Markdown files;
- `knowledge/` pages are reusable and not disguised project docs;
- `sources/` notes stay close to source material and avoid polished
  conclusions unless clearly labeled as takeaways;
- each important conclusion has a source link/path or an explicit `inferred`
  label;
- index/log updates match the scope of the change;
- no work status, prompt dumps, temp artifacts, or generated-audit noise leaked
  into `knowledge/` or `sources/`;
- reference digests are backed by durable deconstruction docs when the
  reference controls implementation.

## Report

In the final response, state:

- files changed;
- where each source/conclusion was routed and why;
- evidence gaps or claims that remain only inferred;
- index/log updates;
- validation performed.
