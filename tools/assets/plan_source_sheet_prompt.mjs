#!/usr/bin/env node
// Build a generator-facing source-sheet prompt packet from an art job contract.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

function usage() {
  console.log(`usage:
  node tools/assets/plan_source_sheet_prompt.mjs --job <art-job.json> --source-family "<family>" --output <packet.md> [--json-output <packet.json>] [--intake-audit <audit.json>] [--key-color #00ff00] [--force]

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

function keyColorAdviceFromAudit(path) {
  if (!path) return { color: "", action: "" };
  const audit = readJson(path);
  const color = hasText(audit.next_prompt_key_color)
    ? audit.next_prompt_key_color
    : hasText(audit.suggested_key_color)
      ? audit.suggested_key_color
      : "";
  return {
    color,
    action: hasText(audit.key_color_action) ? audit.key_color_action : "",
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
  const keyColor = options.keyColor || auditKeyAdvice.color || "#00ff00";
  const keyColorSource = options.keyColor ? "explicit_override" : auditKeyAdvice.color ? "intake_audit" : "default";
  const constraints = uniqueStrings(contract.prompt_constraints || []);
  const mustNotBake = uniqueStrings(job.must_not_bake || []);
  const rejects = uniqueStrings(job.qa_rejects || []);
  const relevantGroups = (job.required_asset_groups || []).filter((group) => groupMatchesFamily(group, sourceFamily));
  const finalPolicy = contract.final_asset_policy || {};

  const promptLines = [
    `Create a production source sheet for ${job.asset_family || job.id}.`,
    `Source family: ${sourceFamily}.`,
    role ? `Role: ${role}.` : "",
    "This is a cuttable source sheet, not a gameplay screenshot, mockup, landing page, or composed UI screen.",
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
});
writeText(args.output, renderMarkdown(packet), args.force === true);
if (args.json_output) {
  writeText(args.json_output, `${JSON.stringify(packet, null, 2)}\n`, args.force === true);
}
