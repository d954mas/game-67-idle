#!/usr/bin/env node
// Build a generator-facing source-sheet prompt packet from an art job contract.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

function usage() {
  console.log(`usage:
  node tools/assets/job/plan_source_sheet_prompt.mjs --job <art-job.json> --source-family "<family>" --output <packet.md> [--json-output <packet.json>] [--intake-audit <audit.json>] [--key-color #00ff00] [--allow-chroma-after-preserve-risk] [--force]

Creates a deterministic prompt/negative-prompt/checklist packet for generating
cuttable game source sheets. The packet is derived from the art job contract,
not freeform chat.`);
}

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

function parseArgs(argv) {
  const values = {};
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") values.help = true;
    else if (arg === "--force") values.force = true;
    else if (arg === "--allow-chroma-after-preserve-risk") values.allow_chroma_after_preserve_risk = true;
    else if (arg.startsWith("--")) {
      const key = arg.slice(2).replaceAll("-", "_");
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values[key] = value;
      index += 1;
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return values;
}

function readJson(path) {
  try {
    return JSON.parse(readFileSync(resolve(path), "utf8"));
  } catch (error) {
    fail(`cannot read JSON ${path}: ${error.message}`);
  }
}

function hasText(value) {
  return typeof value === "string" && value.trim().length > 0;
}

function normalize(value) {
  return String(value || "").trim().toLowerCase();
}

function uniqueStrings(items) {
  const result = [];
  const seen = new Set();
  for (const item of items) {
    if (!hasText(item)) continue;
    const normalized = normalize(item);
    if (seen.has(normalized)) continue;
    seen.add(normalized);
    result.push(String(item).trim());
  }
  return result;
}

function groupMatchesFamily(group, family) {
  const kind = normalize(group.kind);
  const id = normalize(group.id);
  const need = normalize(group.need || group.role);
  const target = normalize(family);
  if (target.includes("blank ui") || target.includes("ui kit")) {
    return kind === "slice9" || id.includes("ui") || need.includes("button") || need.includes("panel");
  }
  if (target.includes("icon")) return kind === "icon" || id.includes("icon") || need.includes("icon");
  if (target.includes("decor") || target.includes("overlay")) {
    return id.includes("decor") || id.includes("overlay") || need.includes("badge") || need.includes("gem") || need.includes("ornament") || need.includes("cap") || need.includes("medallion");
  }
  if (target.includes("sprite") || target.includes("fx")) return kind === "sprite" || kind === "effect" || need.includes("sprite") || need.includes("effect");
  if (target.includes("map") || target.includes("background")) return kind === "background" || kind === "tile" || id.includes("map") || need.includes("map");
  return true;
}

function familyKind(sourceFamily) {
  const family = normalize(sourceFamily);
  if (family.includes("blank ui") || family.includes("ui kit")) return "blank_ui";
  if (family.includes("icon")) return "icon";
  if (family.includes("decor") || family.includes("overlay")) return "decor";
  if (family.includes("sprite") || family.includes("fx")) return "sprite";
  if (family.includes("map") || family.includes("background")) return "map";
  return "generic";
}

function layoutRowsForFamily(sourceFamily, relevantGroups) {
  const kind = familyKind(sourceFamily);
  const groupIds = relevantGroups.map((group) => group.id).filter(hasText);
  if (kind === "blank_ui") {
    return [
      {
        id: "large_slice9_bases",
        purpose: "large blank panels, journals, modals, inventory frames",
        item_policy: "one isolated base per slot; blank content area; no center badges or unique stretch-zone ornaments",
        asset_group_ids: groupIds,
      },
      {
        id: "button_and_chip_bases",
        purpose: "primary/secondary button bases, chips, tabs, compact controls",
        item_policy: "separate default/pressed/disabled/selected states; no baked labels or icons",
        asset_group_ids: groupIds,
      },
      {
        id: "bar_and_strip_bases",
        purpose: "progress tracks, status strips, separators, meter frames",
        item_policy: "split caps and repeatable center strips where ornaments would stretch",
        asset_group_ids: groupIds,
      },
    ];
  }
  if (kind === "icon") {
    return [
      {
        id: "core_gameplay_icons",
        purpose: "health, shield, currency, quest, travel, resource, lock/unlock icons",
        item_policy: "one centered silhouette per slot; no frames fused into icons; readable at gameplay size",
        asset_group_ids: groupIds,
      },
      {
        id: "state_and_resource_variants",
        purpose: "rarity/state/resource variants with shared visual language",
        item_policy: "consistent lighting and padding; no touching shadows between neighboring slots",
        asset_group_ids: groupIds,
      },
    ];
  }
  if (kind === "decor") {
    return [
      {
        id: "corner_and_edge_overlays",
        purpose: "corner caps, edge caps, dividers, glow strips, ornamental bars",
        item_policy: "non-stretch overlays only; obvious anchor point; transparent/chroma padding around every sprite",
        asset_group_ids: groupIds,
      },
      {
        id: "badges_and_fixed_ornaments",
        purpose: "badges, gems, locks, seals, medallions, plaques, banners",
        item_policy: "each ornament isolated as its own sprite; never baked into panel centers",
        asset_group_ids: groupIds,
      },
    ];
  }
  return [
    {
      id: "isolated_assets",
      purpose: "all requested reusable runtime assets for this source family",
      item_policy: "one isolated component per slot with clear gutters and no composed runtime screen",
      asset_group_ids: groupIds,
    },
  ];
}

function normalizeCustomLayoutRows(rows) {
  if (!Array.isArray(rows) || rows.length === 0) return [];
  return rows
    .filter((row) => row && hasText(row.id))
    .map((row) => {
      const slots = Array.isArray(row.slots) ? row.slots.filter(hasText).map((slot) => String(slot).trim()) : [];
      return {
        id: String(row.id).trim(),
        purpose: hasText(row.purpose) ? String(row.purpose).trim() : slots.length > 0 ? "requested row-major source slots" : "requested custom source row",
        item_policy: hasText(row.item_policy)
          ? String(row.item_policy).trim()
          : "one isolated component per slot with clear gutters and no composed runtime screen",
        slots,
      };
    });
}

function buildSourceSheetLayout(sourceFamily, keyColor, relevantGroups, customLayout = null) {
  const kind = familyKind(sourceFamily);
  const customRows = normalizeCustomLayoutRows(customLayout?.rows);
  const customCanvas = Array.isArray(customLayout?.canvas) && customLayout.canvas.length === 2 ? customLayout.canvas : null;
  const customMargin = Number.isFinite(customLayout?.outer_margin_px) ? customLayout.outer_margin_px : 64;
  const customGutter = Number.isFinite(customLayout?.min_gutter_px) ? customLayout.min_gutter_px : kind === "icon" || kind === "decor" ? 64 : 48;
  return {
    version: 1,
    sheet_role: "cuttable_source_sheet",
    family_kind: kind,
    recommended_canvas: {
      size_px: customCanvas || [2048, 2048],
      background: keyColor,
      background_policy: hasText(customLayout?.background?.notes)
        ? customLayout.background.notes
        : "perfectly flat chroma or true transparency only",
    },
    placement: {
      mode: "row_major_grid",
      edge_margin_px_min: customMargin,
      gutter_px_min: customGutter,
      allow_overlap: false,
      allow_composed_ui_screen: false,
      allow_baked_runtime_text: false,
    },
    rows: customRows.length > 0 ? customRows : layoutRowsForFamily(sourceFamily, relevantGroups),
    cut_policy: {
      one_component_per_slot: true,
      preserve_empty_chroma_lanes: true,
      crop_after_intake_not_by_prompt_coordinates: true,
      fixed_ornaments_are_separate_overlays: true,
    },
  };
}

function renderLayoutInstruction(layout) {
  const rowText = layout.rows
    .map((row, index) => {
      const slotText = Array.isArray(row.slots) && row.slots.length > 0 ? ` Slots in order: ${row.slots.join(", ")}.` : "";
      return `Row ${index + 1} ${row.id}: ${row.purpose}; ${row.item_policy}.${slotText}`;
    })
    .join(" ");
  return (
    `Arrange the sheet as a ${layout.placement.mode} with at least ${layout.placement.edge_margin_px_min}px outer margin ` +
    `and ${layout.placement.gutter_px_min}px gutters between all visible pixels and shadows. ${rowText} ` +
    "Leave empty chroma lanes between rows and slots. Do not compose these assets into a runtime screen."
  );
}

function keyColorAdviceFromAudit(path) {
  if (!path) return { color: "", action: "", recommendedNextStep: null, blockingReasons: [] };
  const audit = readJson(path);
  const recommendedNextStep = audit.recommended_next_step && typeof audit.recommended_next_step === "object" ? audit.recommended_next_step : null;
  const color = hasText(audit.next_prompt_key_color)
    ? audit.next_prompt_key_color
    : hasText(audit.suggested_key_color)
      ? audit.suggested_key_color
      : hasText(recommendedNextStep?.key_color)
        ? recommendedNextStep.key_color
      : "";
  const recommendedAction = hasText(recommendedNextStep?.action) ? recommendedNextStep.action : "";
  const action = hasText(audit.key_color_action)
    ? audit.key_color_action
    : recommendedAction === "split_preserve_or_dual_plate_alpha"
      ? "split_preserve_or_dual_plate_alpha"
      : recommendedAction === "regenerate_source_sheet_with_safer_key_color"
        ? "regenerate_with_next_prompt_key_color"
        : "";
  return {
    color,
    action,
    recommendedNextStep,
    blockingReasons: Array.isArray(audit.blocking_reasons) ? audit.blocking_reasons : [],
  };
}

function buildPacket(job, sourceFamily, options) {
  const contract = job.generation_contract || {};
  const families = contract.source_families || [];
  if (!families.some((family) => normalize(family) === normalize(sourceFamily))) {
    fail(`source family "${sourceFamily}" is not listed in generation_contract.source_families`);
  }

  const role = contract.source_family_roles?.[sourceFamily] || "";
  const auditKeyAdvice = keyColorAdviceFromAudit(options.intakeAudit);
  if (auditKeyAdvice.action === "split_preserve_or_dual_plate_alpha" && options.allowChromaAfterPreserveRisk !== true) {
    fail(
      "intake audit recommends split/preserve or dual-plate alpha; refusing to create another chroma prompt. " +
        "Use a dual-plate alpha workflow or pass --allow-chroma-after-preserve-risk for a diagnostic override."
    );
  }
  const keyColor = options.keyColor || auditKeyAdvice.color || "#00ff00";
  const keyColorSource = options.keyColor ? "explicit_override" : auditKeyAdvice.color ? "intake_audit" : "default";
  const diagnosticChromaOverride = auditKeyAdvice.action === "split_preserve_or_dual_plate_alpha" && options.allowChromaAfterPreserveRisk === true;
  const constraints = uniqueStrings(contract.prompt_constraints || []);
  const mustNotBake = uniqueStrings(job.must_not_bake || []);
  const rejects = uniqueStrings(job.qa_rejects || []);
  const relevantGroups = (job.required_asset_groups || []).filter((group) => groupMatchesFamily(group, sourceFamily));
  const finalPolicy = contract.final_asset_policy || {};
  const sourceSheetLayout = buildSourceSheetLayout(sourceFamily, keyColor, relevantGroups, contract.source_sheet_layout);

  const promptLines = [
    `Create a production source sheet for ${job.asset_family || job.id}.`,
    `Source family: ${sourceFamily}.`,
    role ? `Role: ${role}.` : "",
    "This is a cuttable source sheet, not a gameplay screenshot, mockup, landing page, or composed UI screen.",
    renderLayoutInstruction(sourceSheetLayout),
    `Use a perfectly flat chroma background ${keyColor} or true transparency; do not use gradients, shadows, glow, or texture in the background.`,
    "Keep every asset isolated with generous gutters and no overlap between shadows.",
    "Keep all labels, numbers, quest text, counters, and state values blank for runtime composition.",
    normalize(sourceFamily).includes("blank ui") ? "For slice9 bases, keep stretch zones structurally boring: corners, straight edges, fill, and repeatable texture only." : "",
    normalize(sourceFamily).includes("decor") || normalize(sourceFamily).includes("overlay")
      ? "Create only non-stretch decorative overlays: corner caps, badges, gems, medallions, locks, banners, dividers, and ornamental flourishes with clear transparent/chroma padding and anchorable silhouettes."
      : "",
    normalize(sourceFamily).includes("icon") ? "Create only semantic gameplay/resource icons with strong silhouettes, no frames fused into icons, and enough padding for alpha trim." : "",
    "Put unique gems, badges, medallions, locks, banners, and center ornaments in separate overlay sprites, not inside stretchable bases.",
    "Use consistent perspective, lighting, material language, and line weight across the sheet.",
  ].filter(Boolean);

  const negativeItems = uniqueStrings([
    ...mustNotBake,
    ...rejects,
    "readable text",
    "fake letters",
    "watermark",
    "fused icons inside buttons",
    "cropped silhouettes",
    "merged components",
    "busy stretch centers",
    "non-flat chroma background",
    "purple or red-blue halo on transparent edges",
  ]);

  const checklist = [
    "Source sheet is not a full gameplay screenshot.",
    `Background is flat ${keyColor} or true transparent alpha.`,
    "No readable text, fake glyphs, labels, numbers, or watermark.",
    "Components have clear gutters and do not share antialias/shadow pixels.",
    normalize(sourceFamily).includes("blank ui") ? "Slice9 centers and long edges have no unique ornaments that will stretch." : "No stretchable slice9 bases are mixed into this source family.",
    normalize(sourceFamily).includes("decor") || normalize(sourceFamily).includes("overlay") ? "Every decorative overlay is isolated as a separate sprite with padding and obvious anchor point." : "Ornaments that should not stretch are separate overlay sprites with visible isolation.",
    "Icons/sprites have full silhouettes with padding for alpha trim.",
    "Source can pass source-sheet intake before crop rectangles are trusted.",
    "Accepted output will get a generation record with provider/model/workflow/seed or no-seed reason.",
  ];

  return {
    schema: "game.source_sheet_prompt_packet",
    version: 1,
    job_id: job.id,
    asset_family: job.asset_family,
    source_family: sourceFamily,
    source_family_role: role,
    suggested_key_color: keyColor,
    key_color_source: keyColorSource,
    intake_key_color_action: auditKeyAdvice.action,
    intake_recommended_next_step: auditKeyAdvice.recommendedNextStep,
    intake_blocking_reasons: auditKeyAdvice.blockingReasons,
    diagnostic_chroma_override: diagnosticChromaOverride,
    source_sheet_layout: sourceSheetLayout,
    prompt: promptLines.join(" "),
    negative_prompt: negativeItems.join(", "),
    constraints,
    relevant_asset_groups: relevantGroups,
    final_asset_policy: finalPolicy,
    acceptance_checklist: checklist,
  };
}

function renderMarkdown(packet) {
  const lines = [
    "---",
    "type: SourceSheetPromptPacket",
    `job_id: ${packet.job_id}`,
    `source_family: ${packet.source_family}`,
    `suggested_key_color: ${packet.suggested_key_color}`,
    `key_color_source: ${packet.key_color_source}`,
    `intake_key_color_action: ${packet.intake_key_color_action || "none"}`,
    "---",
    "",
    `# Source Sheet Prompt Packet: ${packet.source_family}`,
    "",
    "## Prompt",
    "",
    packet.prompt,
    "",
    "## Negative Prompt",
    "",
    packet.negative_prompt,
    "",
    "## Acceptance Checklist",
    "",
    ...packet.acceptance_checklist.map((item) => `- ${item}`),
    "",
    "## Source Sheet Layout",
    "",
    `sheet_role: ${packet.source_sheet_layout?.sheet_role || "none"}`,
    `placement: ${packet.source_sheet_layout?.placement?.mode || "none"}`,
    `outer_margin_px_min: ${packet.source_sheet_layout?.placement?.edge_margin_px_min || "none"}`,
    `gutter_px_min: ${packet.source_sheet_layout?.placement?.gutter_px_min || "none"}`,
    "",
    ...(packet.source_sheet_layout?.rows || []).map((row, index) => {
      const slotText = Array.isArray(row.slots) && row.slots.length > 0 ? `; slots: ${row.slots.join(", ")}` : "";
      return `- row ${index + 1} \`${row.id}\`: ${row.purpose}; ${row.item_policy}${slotText}`;
    }),
    "",
    "## Intake Routing",
    "",
    `recommended_next_step: ${packet.intake_recommended_next_step?.action || "none"}`,
    `blocking_reasons: ${renderBlockingReasons(packet.intake_blocking_reasons)}`,
    "",
    "## Relevant Asset Groups",
    "",
  ];
  if (packet.relevant_asset_groups.length === 0) {
    lines.push("- none");
  } else {
    for (const group of packet.relevant_asset_groups) {
      lines.push(`- ${group.id || "(unnamed)"} (${group.kind || "unknown"}): ${group.need || group.role || ""}`);
    }
  }
  lines.push("");
  return lines.join("\n");
}

function renderBlockingReasons(blockingReasons) {
  return blockingReasons.length > 0 ? blockingReasons.map((reason) => reason.code || "unknown").join(", ") : "none";
}

function writeText(path, text, force) {
  const fullPath = resolve(path);
  if (existsSync(fullPath) && !force) fail(`refusing to overwrite existing file: ${path}; pass --force`);
  mkdirSync(dirname(fullPath), { recursive: true });
  writeFileSync(fullPath, text, "utf8");
  console.log(`wrote ${path}`);
}

const args = parseArgs(process.argv.slice(2));
if (args.help) {
  usage();
  process.exit(0);
}
for (const key of ["job", "source_family", "output"]) {
  if (!args[key]) fail(`--${key.replaceAll("_", "-")} is required`);
}

const job = readJson(args.job);
const packet = buildPacket(job, args.source_family, {
  intakeAudit: args.intake_audit,
  keyColor: args.key_color,
  allowChromaAfterPreserveRisk: args.allow_chroma_after_preserve_risk,
});
writeText(args.output, renderMarkdown(packet), args.force === true);
if (args.json_output) {
  writeText(args.json_output, `${JSON.stringify(packet, null, 2)}\n`, args.force === true);
}
