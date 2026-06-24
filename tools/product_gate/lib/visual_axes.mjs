// The six visual-quality axes every product-gate visual check scores, plus the
// shared 1-5 score contract. Pure data + a tiny predicate, kept SEPARATE from
// state-matrix IO and llm-json parsing (no god-file) so a tool that only needs
// the axis list does not drag in heavier surface.
export const VISUAL_AXES = [
  "composition",
  "readability",
  "ui_controls",
  "action_direction",
  "art_quality",
  "audience_fit",
];

// A valid per-axis score is an integer 1-5.
export function isAxisScore(value) {
  return Number.isInteger(value) && value >= 1 && value <= 5;
}
