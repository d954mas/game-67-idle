# UI Asset Rules (reference manual)

Deep reference-manual detail for the `generated-game-ui-assets` skill. Load the
section that matches the current task; the always-loaded SKILL body keeps only
the workflow, gate tiers, failure response, and report shape.

## Slice9 Rules

- Do not uniform-scale generated panels/buttons in runtime when the UI needs
  resizing. Use slice9 geometry or split corners/edges/center.
- Do not replace failed generated UI with procedural/programmer art and call it
  done. Code-generated art is allowed only as debug scaffolding or a recorded
  exception; final generated UI must come from generated or artist-authored
  source art, with code limited to cutting, validating, packing, and composing.
- If a builder reads a generated sheet and then creates a new panel/button with
  drawing primitives, that output is procedural scaffold, not generated art.
- For generated-source crop manifests, run
  `audit_generated_source_derivation.py` so source-derived PNGs are compared against
  the accepted source crop after chroma cleanup. A pass here proves the builder
  cut the source art; a fail usually means trim/resize policy is missing or the
  builder redrew the asset.
- Keep labels, counters, prices, timers, quest names, and state values in code.
- Keep content safe areas clear of ornate corners and gems.
- Validate minimum sizes: target width must exceed left+right margins and
  target height must exceed top+bottom margins.
- If long-edge ornamentation stretches visibly, regenerate cleaner long edges
  or split decorative caps from stretchable centers.
- Do not bury a usage limitation in prose. If a generated button is only safe
  as a large primary action, set `usage_policy.size_class` to `large_only` and
  list compact button roles in `disallowed_uses`.
- After runtime integration, validate actual placements against `usage_policy`
  with `audit_runtime_ui_asset_usage.mjs`. A desktop screenshot can still be
  wrong if the code squeezes a large-only generated button into a 260x64 rect
  while the manifest says its minimum safe size is 280x104.
- Do not treat one uniform edge-padding threshold as enough. Low controls such
  as buttons may need side-specific padding gates so horizontal ornaments are
  protected without destroying vertical proportions. Large panels and icon
  frames still need all-side padding proof.
- Minimum preview sizes must be product-realistic. If `left + right` or
  `top + bottom` margins leave no center at a listed target size, the asset is
  not valid for that size even if the PNG audit passes.
- Every slice9 base must have a content safe area and target previews that
  include the declared minimum runtime size plus at least one stress size
  around 125% of a minimum dimension. A source-size contact sheet is not enough:
  the design-policy audit should fail missing min/stress preview coverage.
- Keep slice9 base art structurally boring: corners, straight edges, fill, and
  repeatable texture only. Unique center gems, side medallions, banners,
  badges, labels, locks, and cap ornaments must be exported as separate overlay
  sprites with anchors, not baked into the stretchable base texture. Record that
  contract in `stretch_policy`; the audit should fail if the manifest relies on
  chat notes instead of machine-readable policy.
- Treat beautiful fixed decoration as composition data. A panel top plaque,
  side gem, screw, lock, rarity crest, divider, glow strip, or button cap needs
  its own crop id, `anchor`, `z_order`, `allowed_base_ids`, and
  `offset_bounds` min/max rules.
  If it cannot be named as a separate overlay asset id that exists in the
  manifest, it is probably unsafe inside a resizable base.
  `overlay_family` alone is planning prose, not proof: final slice9 policy
  evidence needs concrete `overlay_asset_ids`.
- Runtime composition proof must treat content safe areas as reserved for text,
  prices, counters, and state values. Decorative overlays that cross those
  bounds need explicit `allow_content_overlap`; otherwise the proof should fail
  before integration.
- Progress bars are systems, not one strip: track base, fill strip/tile, left
  cap, right cap, marker/handle, disabled/locked overlay, optional glow, and
  runtime label. Each part needs a semantic id and atlas metadata.

## Atlas And Reuse Rules

- Pack by runtime lifetime and screen family: `ui_common`, `ui_panel_family`,
  `ui_icons_core`, `ui_map`, `ui_fx`, or a project-specific equivalent. Avoid
  one giant atlas when many screens use only a small subset.
- Every atlas/runtime entry needs metadata for `id`, `kind`, `pack_group`,
  source crop, atlas rect, trim/original size, pivot/anchor, slice9 margins,
  content safe area, state role, and source family.
- Run `audit_atlas_metadata.mjs` before treating a generated UI kit as final
  art. The runtime manifest should make trim, bleed, extrusion, padding,
  rotation, scale variant, alias policy, and sprite/decor-overlay placement
  metadata machine-readable.
- Build a labeled review atlas with `build_ui_atlas_pack.py --label-review`
  before final-art claims when the lead needs to inspect outputs. This atlas is
  a proof/contact artifact: it records `atlas_rect`, `padded_rect`, extrusion,
  slice9 margins, content safe areas, source paths, physical entry count, and
  alias count. The labeled preview must put exact asset names in
  `review_label.rect` free space outside the asset `padded_rect`; do not place
  labels over the art or over other labels. Preserve the exact id and alias
  list in `review_label.text`; wrap only the rendered preview text through
  `review_label.lines`; store `review_label.placement` as `right` or `bottom`;
  and keep labels readable enough for the lead to choose assets directly from
  the atlas. The Markdown report must also include a human-readable asset id
  index with the labeled preview path and label rectangles, because the lead
  should be able to say which ids to integrate from one review artifact.
  It must not be presented as the game's final runtime atlas. Use
  `--profile --profile-output tmp/asset-profiles/<name>.json` while optimizing
  atlas economy so telemetry preserves occupancy and padded-asset ratios
  without committing timing-only evidence churn.
- Audit review atlases with `audit_ui_atlas_pack.py`; a proof image is not
  trusted until coverage, bounds, overlap, alias reuse, and extrusion pixel
  checks pass, including exact review-label text and non-overlapping review
  labels. Labeled review atlases must prove wrapped `review_label.lines` fit in
  their label rects and keep labels out of the clean atlas image; labels belong
  only in `labeled_preview_path`. Clean atlas pixels with alpha 0 must also
  have RGB 0, and visible clean-atlas pixels must be inside declared packed
  `padded_rect`s; hidden green/purple key colors under transparency or visible
  orphan pixels in free atlas space are packing failures even when the image
  looks visually acceptable. The labeled preview must be pixel-identical to the
  clean atlas outside declared label rects; it is a review overlay, not a second
  editable atlas, and it should not add debug outlines over assets. The pack
  JSON and each labeled atlas entry must declare the same
  `labeled_preview_policy` so this is machine-readable, not chat context.
- Use trim only with padding, alpha bleed, edge extrusion, and shape padding.
  Tight alpha crops without bleed/extrude are a known cause of 1-2 pixel halos
  and neighboring-pixel leaks.
- Prefer overlays over duplicated full controls. Common variants should be
  base button + state overlay + selected/locked/affordable overlay + icon +
  runtime label unless the material or silhouette truly changes.
- Alias duplicate regions where the same pixels serve different semantic ids.
  Store the semantic ids in metadata instead of duplicating the bitmap. In a
  review atlas, alias entries should point to the same rect as the physical
  source and the physical source label must list linked aliases.
- Record scale variants deliberately (`1x`, `2x`, mobile/desktop). Keep layout
  coordinates and atlas variants stable enough to avoid fractional artifacts.

## Icon And Sprite Rules

- Generated icon sheets need generous gutters. If expanded crop rects catch
  neighboring shadows, reject the source or isolate the intended component.
- Manual crop rectangles are only a starting point. Runtime icon output must
  pass alpha padding, key-fringe, purple edge-halo, transparent-edge RGB bleed
  audit, and fully transparent RGB-zero audit.
- For disputed 1-2 pixel edges, generate an edge proof image with zoomed
  top/right/bottom/left alpha-boundary strips on a checkerboard. The proof
  should mark the same bad edge classes as the pixel audit, including purple
  halo, source-key spill, saturated green-screen spill, and hidden bad RGB in
  transparent edge pixels. Normal contact sheets are too weak for this class of
  defect. Use `--asset-id` and `--side` to create small proof images for the
  exact reported edge. Write `--json-output` and `--report` when comparing
  fixes so the review records per-side counts by reason, not only a screenshot.
  Add `--profile --profile-output tmp/asset-profiles/<name>.json` for slow
  proof runs so the slowest asset side is printed and timing plus the analysis
  engine (`numpy` fast path or portable `python` fallback) is preserved in a
  sidecar instead of dirtying durable JSON/Markdown evidence.
  Add `--only-problems` when a full proof sheet is too tall to review; this
  keeps every side in JSON while omitting clean sides from the PNG/Markdown.
  Store accepted proof image paths in `expected_outputs.edge_proofs` and JSON
  report paths in `expected_outputs.edge_proof_reports` only when
  `counts.total` is zero; reports with bad marks document candidates to reject
  or keep debugging rather than accepted outputs.
- Preserve intentional purple/magic colors with explicit manifest policy; do
  not globally delete interior colors because they resemble the key background.
  `preserve_purple_edges` only suppresses intentional purple/magenta edge
  checks; source-key and green-screen edge leaks must still fail.
- Preserve intentional saturated green edge colors with explicit
  `preserve_green_edges` manifest policy. Otherwise visible green-screen spill
  and hidden green RGB in transparent edge pixels are extraction failures even
  when the crop manifest did not declare `green_screen.key`.
- Reject source sheets where the chroma/key background is too close to the
  intended art palette. Exact key-color pixels inside component bounds are a
  source failure unless deliberately authored and separately masked; broad
  key/halo hue conflicts should be rare and documented.
- Treat visible 1-2 pixel dark purple, dark maroon/magenta, or red-blue edge lines as extraction
  failures, not acceptable polish noise. The audit should catch both bright
  magenta fringe and very dark low-saturation halos on the outer alpha contour,
  including near-black purple pixels such as `#26022d` when they touch transparency.
- Record pivots/anchors before code uses sprites or map markers.

## Responsive UI Rules

- Desktop and portrait are separate compositions using the same reusable
  assets. Do not squeeze desktop HUDs into phone layout.
- Portrait should show fewer simultaneous status values, one full-width primary action,
  secondary actions below, and short journal/objective text.
- A screenshot that looks acceptable is not enough if clickable geometry is
  wrong. Use `ui.tree` layout audit for action bounds when available.
- Product pass requires both player-read evidence and no obvious overlap,
  clipped text, or unusable touch targets.
