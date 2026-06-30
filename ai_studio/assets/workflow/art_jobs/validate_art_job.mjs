#!/usr/bin/env node
import { existsSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { fail } from "../../../../tools/lib/cli.mjs";
import { readJson } from "../../../../tools/lib/json.mjs";

function usage() {
  console.error(`usage:
  node ai_studio/assets/workflow/art_jobs/validate_art_job.mjs --job <art-job.json> [--strict] [--final-art]

Validates generated game UI/art job contracts before prepared integration.
Default mode is draft-friendly. Strict mode requires accepted source art,
crop entries, and prepared assets. Final-art mode also rejects procedural
debug scaffolds and incomplete generation provenance. A scoped partial prepared
slice can validate, but it is reported as partial-prepared-slice-valid instead
of a complete final-art claim.`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = {};
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") values.help = true;
    else if (arg === "--strict") values.strict = true;
    else if (arg === "--final-art") values.finalArt = true;
    else if (arg.startsWith("--")) {
      const key = arg.slice(2);
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

function readJsonOrProblem(path, problems, label) {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch (error) {
    problems.push(`${label}: cannot read JSON ${path}: ${error.message}`);
    return null;
  }
}

function projectPath(path, baseDir = process.cwd()) {
  if (!path) return "";
  return resolve(baseDir, path);
}

function hasText(value) {
  return typeof value === "string" && value.trim().length > 0;
}

function normalizeText(value) {
  return String(value || "").trim().toLowerCase().replace(/\s+/g, " ");
}

function textMatchesOrContains(left, right) {
  const a = normalizeText(left);
  const b = normalizeText(right);
  return a.length > 0 && b.length > 0 && (a === b || a.includes(b) || b.includes(a));
}

function arrayIncludes(list, expected) {
  return Array.isArray(list) && list.some((item) => String(item).toLowerCase() === expected.toLowerCase());
}

function hasRequiredGroup(job, kind) {
  return Array.isArray(job.required_asset_groups) && job.required_asset_groups.some((group) => group.kind === kind || group.id?.includes(kind));
}

function objectHasText(object, key) {
  return object && typeof object === "object" && hasText(object[key]);
}

function isPositiveNumber(value) {
  return typeof value === "number" && Number.isFinite(value) && value > 0;
}

function validateRect(rect, label, problems) {
  if (!Array.isArray(rect) || rect.length !== 4) {
    problems.push(`${label} needs rect [x,y,w,h]`);
    return null;
  }
  const [x, y, width, height] = rect;
  if (![x, y, width, height].every((value) => typeof value === "number" && Number.isFinite(value))) {
    problems.push(`${label} rect values must be finite numbers`);
    return null;
  }
  if (width <= 0 || height <= 0) problems.push(`${label} rect width/height must be positive`);
  return { x, y, width, height };
}

function validateMargins(margins, rect, label, problems) {
  if (!margins || typeof margins !== "object") {
    problems.push(`${label} needs slice9 margins`);
    return null;
  }
  for (const key of ["left", "top", "right", "bottom"]) {
    if (!isPositiveNumber(margins[key])) problems.push(`${label} slice9.${key} must be a positive number`);
  }
  if (rect && isPositiveNumber(margins.left) && isPositiveNumber(margins.right) && margins.left + margins.right >= rect.width) {
    problems.push(`${label} slice9 left+right must be smaller than crop width`);
  }
  if (rect && isPositiveNumber(margins.top) && isPositiveNumber(margins.bottom) && margins.top + margins.bottom >= rect.height) {
    problems.push(`${label} slice9 top+bottom must be smaller than crop height`);
  }
  return margins;
}

function validateContent(content, rect, label, problems) {
  if (!content || typeof content !== "object") {
    problems.push(`${label} needs content safe area`);
    return;
  }
  for (const key of ["x", "y", "w", "h"]) {
    if (!isPositiveNumber(content[key])) problems.push(`${label} content.${key} must be a positive number`);
  }
  if (rect && isPositiveNumber(content.x) && isPositiveNumber(content.y) && isPositiveNumber(content.w) && isPositiveNumber(content.h)) {
    if (content.x + content.w > rect.width || content.y + content.h > rect.height) {
      problems.push(`${label} content safe area must fit inside crop rect`);
    }
  }
}

function validatePreviewSizes(sizes, margins, label, problems) {
  if (!Array.isArray(sizes) || sizes.length === 0) {
    problems.push(`${label} needs target_preview_sizes`);
    return;
  }
  for (const size of sizes) {
    if (!Array.isArray(size) || size.length !== 2 || !isPositiveNumber(size[0]) || !isPositiveNumber(size[1])) {
      problems.push(`${label} target_preview_sizes entries must be [width,height] positive numbers`);
      continue;
    }
    if (margins && size[0] < margins.left + margins.right) problems.push(`${label} preview width ${size[0]} is smaller than left+right margins`);
    if (margins && size[1] < margins.top + margins.bottom) problems.push(`${label} preview height ${size[1]} is smaller than top+bottom margins`);
  }
}

function validateGroup(group, problems) {
  if (!hasText(group.id)) problems.push("required_asset_group needs id");
  if (!hasText(group.kind)) problems.push(`required_asset_group ${group.id || "(unknown)"} needs kind`);
  if (group.kind === "slice9") {
    if (!Array.isArray(group.target_preview_sizes) || group.target_preview_sizes.length === 0) {
      problems.push(`slice9 group ${group.id || "(unknown)"} needs target_preview_sizes`);
    }
    if (!hasText(group.content_policy)) problems.push(`slice9 group ${group.id || "(unknown)"} needs content_policy`);
    if (!hasText(group.stretch_zone_policy)) problems.push(`slice9 group ${group.id || "(unknown)"} needs stretch_zone_policy`);
    if (!hasText(group.decor_overlay_policy)) problems.push(`slice9 group ${group.id || "(unknown)"} needs decor_overlay_policy`);
    if (!Array.isArray(group.states) || group.states.length === 0) problems.push(`slice9 group ${group.id || "(unknown)"} needs states`);
  }
  if (group.kind === "icon" && !hasText(group.size_class)) problems.push(`icon group ${group.id || "(unknown)"} needs size_class`);
  if (group.kind === "sprite" && !hasText(group.anchor_policy)) problems.push(`sprite group ${group.id || "(unknown)"} needs anchor_policy`);
}

function validateGenerationContract(job, problems) {
  const contract = job.generation_contract;
  if (!contract || typeof contract !== "object") {
    problems.push("art job needs generation_contract");
    return;
  }
  for (const phrase of ["no readable text", "clear gutters between assets", "no unique decoration inside slice9 stretch zones"]) {
    const constraints = contract.prompt_constraints || [];
    if (!arrayIncludes(constraints, phrase)) problems.push(`generation_contract.prompt_constraints should include ${phrase}`);
  }
  const sourceFamilies = contract.source_families || [];
  for (const family of ["blank UI kit sheet", "isolated icon sheet"]) {
    if (!arrayIncludes(sourceFamilies, family)) problems.push(`generation_contract.source_families should include ${family}`);
  }
  const sourceFamilyRoles = contract.source_family_roles;
  if (!sourceFamilyRoles || typeof sourceFamilyRoles !== "object") {
    problems.push("generation_contract needs source_family_roles");
  } else {
    for (const family of sourceFamilies) {
      if (hasText(family) && !hasText(sourceFamilyRoles[family])) problems.push(`generation_contract.source_family_roles needs role for ${family}`);
    }
  }
  const finalPolicy = contract.final_asset_policy;
  if (!finalPolicy || typeof finalPolicy !== "object") {
    problems.push("generation_contract needs final_asset_policy");
  } else {
    if (finalPolicy.final_must_use_generated_or_artist_source !== true) {
      problems.push("generation_contract.final_asset_policy.final_must_use_generated_or_artist_source must be true");
    }
    if (finalPolicy.procedural_art_allowed !== "debug_only") {
      problems.push("generation_contract.final_asset_policy.procedural_art_allowed must be debug_only");
    }
    if (finalPolicy.layered_source_required_for_ui !== true) {
      problems.push("generation_contract.final_asset_policy.layered_source_required_for_ui must be true");
    }
  }
  const metadata = contract.metadata_to_record || [];
  for (const field of ["provider or generator", "model/workflow", "workflow file or workflow json", "seed", "prompt", "negative prompt", "source family role", "accepted source image path", "rejected candidate notes"]) {
    if (!arrayIncludes(metadata, field)) problems.push(`generation_contract.metadata_to_record should include ${field}`);
  }
}

function collectCropOutputs(crop) {
  const outputs = new Map();
  for (const source of crop?.sources || []) {
    for (const item of source.crops || []) {
      if (!hasText(item.id) || !hasText(item.output)) continue;
      outputs.set(item.id, {
        id: item.id,
        kind: item.kind,
        output: normalizePathForCompare(item.output),
      });
    }
  }
  return outputs;
}

function normalizedText(value) {
  return String(value || "").trim().toLowerCase();
}

function normalizedStringArray(value) {
  if (!Array.isArray(value)) return [];
  return value.filter(hasText).map((item) => String(item).trim());
}

function expectedRequiredSourceFamilies(job) {
  return normalizedStringArray(
    job.expected_outputs?.required_source_families ??
      job.generation_contract?.final_source_families_required ??
      job.generation_contract?.required_source_families
  );
}

function itemTextBlob(item) {
  return normalizedText(
    [
      item?.id,
      item?.kind,
      item?.role,
      item?.semantic_role,
      item?.need,
      item?.source_family,
      item?.source_family_role,
      item?.overlay_family,
      item?.source_crop,
    ]
      .filter((value) => value !== undefined && value !== null)
      .join(" ")
  );
}

function isPreparedIcon(item) {
  return normalizedText(item?.kind) === "icon";
}

function isPreparedDecorOverlay(item) {
  const kind = normalizedText(item?.kind);
  if (kind === "decor_overlay") return true;
  if (kind !== "sprite" && kind !== "effect" && kind !== "border") return false;
  const text = itemTextBlob(item);
  return /\b(decor|overlay|ornament|gem|badge|medallion|cap|ribbon|divider|flourish|lock)\b/.test(text);
}

const REQUIRED_FAMILY_PREPARED_RULES = [
  {
    family: "isolated icon sheet",
    preparedLabel: "prepared icon",
    matches: isPreparedIcon,
  },
  {
    family: "ui decor overlay sheet",
    preparedLabel: "prepared decor overlay",
    matches: isPreparedDecorOverlay,
  },
];

function validateDeliveryScope(job, requiredFamilies, problems) {
  const scope = job.expected_outputs?.delivery_scope;
  if (scope === undefined) return new Set();
  const deferred = new Set();
  if (!scope || typeof scope !== "object" || Array.isArray(scope)) {
    problems.push("expected_outputs.delivery_scope must be an object when provided");
    return deferred;
  }

  const mode = normalizedText(scope.mode);
  const includedFamilies = normalizedStringArray(scope.included_source_families);
  const deferredFamilies = normalizedStringArray(scope.deferred_source_families);
  const requiredSet = new Set(requiredFamilies);
  for (const family of deferredFamilies) deferred.add(family);

  if (!["partial_prepared_slice", "full_prepared_ui"].includes(mode)) {
    problems.push("expected_outputs.delivery_scope.mode must be partial_prepared_slice or full_prepared_ui");
  }
  if (mode === "partial_prepared_slice") {
    if (includedFamilies.length === 0) problems.push("expected_outputs.delivery_scope.partial_prepared_slice needs included_source_families");
    if (deferredFamilies.length === 0) problems.push("expected_outputs.delivery_scope.partial_prepared_slice needs deferred_source_families");
    if (!hasText(scope.reason)) problems.push("expected_outputs.delivery_scope.partial_prepared_slice needs reason");
  }
  if (mode === "full_prepared_ui" && deferredFamilies.length > 0) {
    problems.push("expected_outputs.delivery_scope.full_prepared_ui cannot defer source families");
  }
  for (const family of [...includedFamilies, ...deferredFamilies]) {
    if (!requiredSet.has(family)) {
      problems.push(`expected_outputs.delivery_scope references non-required source family: ${family}`);
    }
  }
  for (const family of requiredFamilies) {
    if (includedFamilies.includes(family) && deferredFamilies.includes(family)) {
      problems.push(`expected_outputs.delivery_scope cannot both include and defer source family: ${family}`);
    }
  }
  return deferred;
}

function validateRequiredSourceFamilyPreparedCoverage(job, crop, prepared, problems) {
  const requiredFamilies = expectedRequiredSourceFamilies(job);
  if (requiredFamilies.length === 0) return;
  const deferredFamilies = validateDeliveryScope(job, requiredFamilies, problems);
  const cropItems = [];
  for (const source of crop?.sources || []) {
    for (const item of source.crops || []) cropItems.push(item);
  }
  const preparedAssets = prepared?.assets || [];
  for (const rule of REQUIRED_FAMILY_PREPARED_RULES) {
    if (!requiredFamilies.includes(rule.family) || deferredFamilies.has(rule.family)) continue;
    const hasCrop = cropItems.some(rule.matches);
    const hasPrepared = preparedAssets.some(rule.matches);
    if (!hasCrop || !hasPrepared) {
      problems.push(
        `final-art mode required source family ${rule.family} needs ${rule.preparedLabel} crop/prepared assets or an explicit expected_outputs.delivery_scope.deferred_source_families entry`
      );
    }
  }
}

function loadGenerationRecord(entry, label, root, problems, strict) {
  if (typeof entry === "string") {
    const path = projectPath(entry, root);
    if (!existsSync(path)) {
      problems.push(`${label} file missing: ${entry}`);
      return null;
    }
    const record = readJsonOrProblem(path, problems, label);
    if (record) record.__record_path = entry.replaceAll("\\", "/");
    return record;
  }
  if (entry && typeof entry === "object") return entry;
  problems.push(`${label} must be a path string or object`);
  return null;
}

function validatePromptPacket(packet, packetPath, record, job, label, problems) {
  const packetLabel = `${label} prompt_packet`;
  if (!packet || typeof packet !== "object") {
    problems.push(`${packetLabel} must be an object`);
    return;
  }
  if (packet.schema !== "game.source_sheet_prompt_packet") {
    problems.push(`${packetLabel} schema must be game.source_sheet_prompt_packet`);
  }
  for (const key of ["source_family", "source_family_role", "prompt", "negative_prompt"]) {
    if (!objectHasText(packet, key)) problems.push(`${packetLabel} needs ${key}`);
  }
  if (!Array.isArray(packet.acceptance_checklist) || packet.acceptance_checklist.length === 0) {
    problems.push(`${packetLabel} needs non-empty acceptance_checklist`);
  } else if (!packet.acceptance_checklist.every(hasText)) {
    problems.push(`${packetLabel} acceptance_checklist entries must be non-empty strings`);
  }
  if (objectHasText(packet, "intake_key_color_action")) {
    const allowedActions = ["keep_current_key_color", "regenerate_with_next_prompt_key_color", "split_preserve_or_dual_plate_alpha"];
    if (!allowedActions.includes(packet.intake_key_color_action)) {
      problems.push(`${packetLabel} intake_key_color_action is unknown: ${packet.intake_key_color_action}`);
    }
    if (packet.intake_key_color_action === "split_preserve_or_dual_plate_alpha" && packet.diagnostic_chroma_override !== true) {
      problems.push(`${packetLabel} with split_preserve_or_dual_plate_alpha needs diagnostic_chroma_override true or a dual-plate/split workflow`);
    }
    if (packet.intake_key_color_action === "regenerate_with_next_prompt_key_color" && !objectHasText(packet, "suggested_key_color")) {
      problems.push(`${packetLabel} needs suggested_key_color when regenerating from intake key color action`);
    }
    if (!objectHasText(packet, "key_color_source")) {
      problems.push(`${packetLabel} needs key_color_source when intake_key_color_action is present`);
    }
  }
  validatePromptPacketIntakeRouting(packet, packetLabel, problems);
  validatePromptPacketSourceSheetLayout(packet, packetLabel, problems);
  if (hasText(packet.job_id) && hasText(job?.id) && packet.job_id !== job.id) {
    problems.push(`${packetLabel} job_id should match art job ${job.id}`);
  }
  if (hasText(packet.asset_family) && hasText(job?.asset_family) && packet.asset_family !== job.asset_family) {
    problems.push(`${packetLabel} asset_family should match art job ${job.asset_family}`);
  }
  const sourceFamilies = job?.generation_contract?.source_families || [];
  if (hasText(packet.source_family) && Array.isArray(sourceFamilies) && sourceFamilies.length > 0 && !arrayIncludes(sourceFamilies, packet.source_family)) {
    problems.push(`${packetLabel} source_family is not listed in generation_contract.source_families: ${packet.source_family}`);
  }
  if (objectHasText(record, "source_family")) {
    if (!textMatchesOrContains(record.source_family, packet.source_family)) {
      problems.push(`${packetLabel} source_family does not match record source_family`);
    }
  } else if (objectHasText(record, "source_family_role")) {
    const candidates = [packet.source_family, packet.source_family_role].filter(hasText);
    if (candidates.length > 0 && !candidates.some((candidate) => textMatchesOrContains(record.source_family_role, candidate))) {
      problems.push(`${packetLabel} source_family/source_family_role does not match record source_family_role`);
    }
  }
  if (!packetPath.endsWith(".json")) {
    problems.push(`${packetLabel} should reference the JSON prompt packet, not ${packetPath}`);
  }
}

function validatePromptPacketIntakeRouting(packet, packetLabel, problems) {
  const recommendedStep = packet.intake_recommended_next_step;
  if (recommendedStep !== undefined && recommendedStep !== null) {
    if (typeof recommendedStep !== "object" || Array.isArray(recommendedStep)) {
      problems.push(`${packetLabel} intake_recommended_next_step must be an object`);
    } else if (!hasText(recommendedStep.action)) {
      problems.push(`${packetLabel} intake_recommended_next_step needs action`);
    } else {
      const expectedActions = {
        regenerate_source_sheet_with_safer_key_color: "regenerate_with_next_prompt_key_color",
        split_preserve_or_dual_plate_alpha: "split_preserve_or_dual_plate_alpha",
        slice_ready: "keep_current_key_color",
      };
      const expectedAction = expectedActions[recommendedStep.action];
      if (
        expectedAction &&
        hasText(packet.intake_key_color_action) &&
        packet.intake_key_color_action !== expectedAction
      ) {
        problems.push(`${packetLabel} intake_key_color_action must match intake_recommended_next_step.action`);
      }
    }
  }
  const blockingReasons = packet.intake_blocking_reasons;
  if (blockingReasons !== undefined) {
    if (!Array.isArray(blockingReasons)) {
      problems.push(`${packetLabel} intake_blocking_reasons must be an array`);
    } else {
      for (const [index, reason] of blockingReasons.entries()) {
        if (!reason || typeof reason !== "object" || Array.isArray(reason)) {
          problems.push(`${packetLabel} intake_blocking_reasons[${index}] must be an object`);
        } else if (!hasText(reason.code)) {
          problems.push(`${packetLabel} intake_blocking_reasons[${index}] needs code`);
        }
      }
    }
  }
}

function validatePromptPacketSourceSheetLayout(packet, packetLabel, problems) {
  const layout = packet.source_sheet_layout;
  if (layout === undefined || layout === null) return;
  if (typeof layout !== "object" || Array.isArray(layout)) {
    problems.push(`${packetLabel} source_sheet_layout must be an object`);
    return;
  }
  if (!hasText(layout.sheet_role)) {
    problems.push(`${packetLabel} source_sheet_layout needs sheet_role`);
  }
  const placement = layout.placement;
  if (!placement || typeof placement !== "object" || Array.isArray(placement)) {
    problems.push(`${packetLabel} source_sheet_layout needs placement object`);
  } else {
    if (!hasText(placement.mode)) {
      problems.push(`${packetLabel} source_sheet_layout.placement needs mode`);
    }
    for (const key of ["edge_margin_px_min", "gutter_px_min"]) {
      if (!Number.isFinite(placement[key]) || placement[key] <= 0) {
        problems.push(`${packetLabel} source_sheet_layout.placement.${key} must be a positive number`);
      }
    }
    for (const key of ["allow_overlap", "allow_composed_ui_screen", "allow_baked_runtime_text"]) {
      if (placement[key] !== false) {
        problems.push(`${packetLabel} source_sheet_layout.placement.${key} must be false`);
      }
    }
  }
  if (!Array.isArray(layout.rows) || layout.rows.length === 0) {
    problems.push(`${packetLabel} source_sheet_layout needs non-empty rows`);
  } else {
    for (const [index, row] of layout.rows.entries()) {
      if (!row || typeof row !== "object" || Array.isArray(row)) {
        problems.push(`${packetLabel} source_sheet_layout.rows[${index}] must be an object`);
        continue;
      }
      for (const key of ["id", "purpose", "item_policy"]) {
        if (!hasText(row[key])) {
          problems.push(`${packetLabel} source_sheet_layout.rows[${index}] needs ${key}`);
        }
      }
    }
  }
  const cutPolicy = layout.cut_policy;
  if (!cutPolicy || typeof cutPolicy !== "object" || Array.isArray(cutPolicy)) {
    problems.push(`${packetLabel} source_sheet_layout needs cut_policy object`);
  } else {
    for (const key of ["one_component_per_slot", "preserve_empty_chroma_lanes", "crop_after_intake_not_by_prompt_coordinates"]) {
      if (cutPolicy[key] !== true) {
        problems.push(`${packetLabel} source_sheet_layout.cut_policy.${key} must be true`);
      }
    }
  }
}

function validateGenerationRecord(record, label, root, problems, strict, job = null) {
  if (!record || typeof record !== "object") {
    problems.push(`${label} must be an object`);
    return;
  }
  if (record.schema && record.schema !== "game.art_generation_record") {
    problems.push(`${label} schema must be game.art_generation_record`);
  }
  for (const key of ["id", "provider", "model_or_workflow", "source_family_role", "accepted_source_image"]) {
    if (!objectHasText(record, key)) problems.push(`${label} needs ${key}`);
  }
  if (!objectHasText(record, "prompt")) problems.push(`${label} needs prompt`);
  if (!("seed" in record) && !objectHasText(record, "no_seed_reason")) problems.push(`${label} needs seed or no_seed_reason`);
  if (!objectHasText(record, "negative_prompt")) problems.push(`${label} needs negative_prompt`);
  if (!objectHasText(record, "workflow_path") && !record.workflow_json) {
    problems.push(`${label} needs workflow_path or workflow_json`);
  }
  if (record.final_art_source === "procedural" && !hasText(record.procedural_exception)) {
    problems.push(`${label} final_art_source procedural needs procedural_exception`);
  }
  if (record.final_art_source && !["generated", "artist", "procedural"].includes(record.final_art_source)) {
    problems.push(`${label} final_art_source must be generated, artist, or procedural`);
  }
  if (strict) {
    if (objectHasText(record, "accepted_source_image") && !existsSync(projectPath(record.accepted_source_image, root))) {
      problems.push(`${label} accepted_source_image missing: ${record.accepted_source_image}`);
    }
    if (objectHasText(record, "workflow_path") && !existsSync(projectPath(record.workflow_path, root))) {
      problems.push(`${label} workflow_path missing: ${record.workflow_path}`);
    }
    if (objectHasText(record, "prompt_packet")) {
      const promptPacketPath = projectPath(record.prompt_packet, root);
      if (!existsSync(promptPacketPath)) {
        problems.push(`${label} prompt_packet missing: ${record.prompt_packet}`);
      } else {
        const packet = readJsonOrProblem(promptPacketPath, problems, `${label} prompt_packet`);
        validatePromptPacket(packet, record.prompt_packet, record, job, label, problems);
      }
    }
  }
}

function normalizePathForCompare(path) {
  return String(path || "").replaceAll("\\", "/").replace(/^\.\//, "");
}

function isUnknownSeed(seed) {
  return seed === null || seed === undefined || String(seed).trim().toLowerCase().includes("unknown");
}

function hasPartialWorkflow(record) {
  const workflow = record?.workflow_json;
  if (!workflow || typeof workflow !== "object") return false;
  const quality = String(workflow.record_quality || workflow.quality || "").toLowerCase();
  return quality.includes("partial") || quality.includes("unknown");
}

function hasEmptyWorkflowJson(record) {
  const workflow = record?.workflow_json;
  return workflow && typeof workflow === "object" && Object.keys(workflow).length === 0;
}

function validateFinalArtReadiness(job, crop, prepared, records, root, problems) {
  const usableRecords = records.filter((record) => record && typeof record === "object");
  if (usableRecords.length === 0) {
    problems.push("final-art mode requires at least one loaded generation record");
    return;
  }

  for (const record of usableRecords) {
    const label = `generation record ${record.id || record.__record_path || "(unknown)"}`;
    if (!hasText(record.final_art_source)) {
      problems.push(`${label} needs explicit final_art_source for final-art mode`);
    }
    if (record.final_art_source === "procedural") {
      problems.push(`${label} is procedural debug art; final-art mode requires generated or artist source`);
    }
    if (record.final_art_source === "generated") {
      if (isUnknownSeed(record.seed) && !hasText(record.no_seed_reason)) problems.push(`${label} generated source needs a captured non-unknown seed or no_seed_reason for final-art mode`);
      if (hasEmptyWorkflowJson(record) && !hasText(record.workflow_path)) problems.push(`${label} generated source needs non-empty workflow_json or workflow_path for final-art mode`);
      if (hasPartialWorkflow(record)) problems.push(`${label} has partial/unknown workflow provenance; final-art mode needs reproducible provenance`);
    }
  }

  const sourcePaths = new Set();
  for (const source of job.expected_outputs?.source_art || []) {
    sourcePaths.add(normalizePathForCompare(source));
  }
  for (const source of crop?.sources || []) {
    if (hasText(source.path)) sourcePaths.add(normalizePathForCompare(source.path));
  }

  const finalSourceRecords = new Map();
  for (const record of usableRecords) {
    if (["generated", "artist"].includes(record.final_art_source || "generated") && hasText(record.accepted_source_image)) {
      finalSourceRecords.set(normalizePathForCompare(record.accepted_source_image), record);
    }
  }

  for (const sourcePath of sourcePaths) {
    if (!finalSourceRecords.has(sourcePath)) {
      problems.push(`final-art mode source ${sourcePath} needs a generated or artist generation record`);
    }
  }
  validateRequiredSourceFamilyPreparedCoverage(job, crop, prepared, problems);
}

function validateJob(job, jobPath, options = {}) {
  const strict = options.strict === true || options.finalArt === true;
  const finalArt = options.finalArt === true;
  const problems = [];
  if (job.schema !== "game.art_job") problems.push("art job schema must be game.art_job");
  if (!hasText(job.id)) problems.push("art job needs id");
  if (!hasText(job.asset_family)) problems.push("art job needs asset_family");
  if (!Array.isArray(job.visual_targets) || job.visual_targets.length === 0) problems.push("art job needs visual_targets");
  if (!Array.isArray(job.reusable_kinds) || job.reusable_kinds.length === 0) problems.push("art job needs reusable_kinds");
  for (const kind of ["slice9", "icon", "sprite"]) {
    if (!arrayIncludes(job.reusable_kinds, kind)) problems.push(`reusable_kinds should include ${kind}`);
  }
  for (const phrase of ["button labels", "debug text", "game state values"]) {
    if (!arrayIncludes(job.must_not_bake, phrase)) problems.push(`must_not_bake should include ${phrase}`);
  }
  if (!job.game_composition || typeof job.game_composition !== "object") problems.push("art job needs game_composition");
  if (!job.expected_outputs || typeof job.expected_outputs !== "object") problems.push("art job needs expected_outputs");
  if (!hasText(job.expected_outputs?.crop_plan)) problems.push("expected_outputs.crop_plan is required");
  if (!hasText(job.expected_outputs?.prepared_assets)) problems.push("expected_outputs.prepared_assets is required");
  if (!hasText(job.expected_outputs?.prepared_dir)) problems.push("expected_outputs.prepared_dir is required");
  for (const kind of ["slice9", "icon", "sprite"]) {
    if (!hasRequiredGroup(job, kind)) problems.push(`required_asset_groups should include a ${kind} group`);
  }
  for (const group of job.required_asset_groups || []) validateGroup(group, problems);
  validateGenerationContract(job, problems);
  const generationRecords = job.expected_outputs?.generation_records || [];
  if (!Array.isArray(generationRecords)) {
    problems.push("expected_outputs.generation_records must be an array");
  }

  const root = process.cwd();
  const cropPath = projectPath(job.expected_outputs?.crop_plan, root);
  const preparedPath = projectPath(job.expected_outputs?.prepared_assets, root);
  if (hasText(job.expected_outputs?.crop_plan) && !existsSync(cropPath)) problems.push(`crop plan does not exist: ${job.expected_outputs.crop_plan}`);
  if (hasText(job.expected_outputs?.prepared_assets) && !existsSync(preparedPath)) problems.push(`prepared assets manifest does not exist: ${job.expected_outputs.prepared_assets}`);

  let crop = null;
  let prepared = null;
  if (existsSync(cropPath)) crop = readJson(cropPath, fail);
  if (existsSync(preparedPath)) prepared = readJson(preparedPath, fail);
  const relJob = jobPath.replaceAll("\\", "/");
  if (crop) {
    if (crop.schema !== "game.asset_crop_plan") problems.push("crop plan schema must be game.asset_crop_plan");
    if (crop.art_job && crop.art_job !== relJob) problems.push(`crop plan art_job should match ${relJob}`);
    if (!hasText(crop.prepared_dir)) problems.push("crop plan needs prepared_dir");
    if (!Array.isArray(crop.sources)) problems.push("crop plan needs sources array");
  }
  if (prepared) {
    if (prepared.schema !== "game.prepared_asset_manifest") problems.push("prepared assets manifest schema must be game.prepared_asset_manifest");
    if (prepared.art_job && prepared.art_job !== relJob) problems.push(`prepared assets manifest art_job should match ${relJob}`);
    if (!hasText(prepared.prepared_dir)) problems.push("prepared assets manifest needs prepared_dir");
    if (!Array.isArray(prepared.assets)) problems.push("prepared assets manifest needs assets array");
  }
  const expectedCropOutputs = collectCropOutputs(crop);

  if (strict) {
    const loadedGenerationRecords = [];
    if (!Array.isArray(job.expected_outputs?.source_art) || job.expected_outputs.source_art.length === 0) problems.push("strict mode requires expected_outputs.source_art");
    if (!Array.isArray(generationRecords) || generationRecords.length === 0) problems.push("strict mode requires expected_outputs.generation_records");
    for (const [index, record] of (Array.isArray(generationRecords) ? generationRecords : []).entries()) {
      const loadedRecord = loadGenerationRecord(record, `generation record ${typeof record === "string" ? record : record?.id || index}`, root, problems, true);
      validateGenerationRecord(loadedRecord, `generation record ${loadedRecord?.id || index}`, root, problems, true, job);
      if (loadedRecord) loadedGenerationRecords.push(loadedRecord);
    }
    for (const source of job.expected_outputs?.source_art || []) {
      if (!existsSync(projectPath(source, root))) problems.push(`strict mode source art missing: ${source}`);
    }
    if (!crop || !Array.isArray(crop.sources) || crop.sources.length === 0) problems.push("strict mode requires crop plan sources");
    for (const source of crop?.sources || []) {
      if (!hasText(source.id)) problems.push("crop source needs id");
      if (!hasText(source.path)) problems.push(`crop source ${source.id || "(unknown)"} needs path`);
      if (source.path && !existsSync(projectPath(source.path, root))) problems.push(`crop source file missing: ${source.path}`);
      if (!Array.isArray(source.crops) || source.crops.length === 0) problems.push(`crop source ${source.id || source.path || "(unknown)"} needs crops`);
      for (const cropItem of source.crops || []) {
        if (!hasText(cropItem.id)) problems.push("crop entry needs id");
        if (!hasText(cropItem.kind)) problems.push(`crop ${cropItem.id || "(unknown)"} needs kind`);
        if (!hasText(cropItem.output)) problems.push(`crop ${cropItem.id || "(unknown)"} needs output`);
        const label = `crop ${cropItem.id || "(unknown)"}`;
        const rect = validateRect(cropItem.rect, label, problems);
        if (cropItem.kind === "slice9") {
          const margins = validateMargins(cropItem.slice9, rect, label, problems);
          validateContent(cropItem.content || cropItem.content_rect, rect, label, problems);
          validatePreviewSizes(cropItem.target_preview_sizes || cropItem.preview_sizes, margins, label, problems);
        }
        if (cropItem.kind === "icon") {
          if (!hasText(cropItem.semantic_role)) problems.push(`${label} icon needs semantic_role`);
          if (!hasText(cropItem.size_class)) problems.push(`${label} icon needs size_class`);
          if (!isPositiveNumber(cropItem.trim_padding) && !hasText(cropItem.no_trim_reason)) {
            problems.push(`${label} icon needs trim_padding or no_trim_reason`);
          }
          if (!hasText(cropItem.isolate_component) && !hasText(cropItem.no_component_isolation_reason)) {
            problems.push(`${label} icon needs isolate_component or no_component_isolation_reason`);
          }
        }
        if (cropItem.kind === "sprite" && !cropItem.pivot && !cropItem.anchor) problems.push(`${label} sprite needs pivot or anchor`);
      }
    }
    if (!prepared || !Array.isArray(prepared.assets) || prepared.assets.length === 0) problems.push("strict mode requires prepared assets manifest assets");
    if (prepared && hasText(prepared.crop_plan) && normalizePathForCompare(prepared.crop_plan) !== normalizePathForCompare(job.expected_outputs?.crop_plan)) {
      problems.push("prepared assets manifest crop_plan should match expected_outputs.crop_plan");
    }
    const preparedAssetsById = new Map();
    for (const asset of prepared?.assets || []) {
      if (!hasText(asset.id)) problems.push("prepared asset needs id");
      if (!hasText(asset.path)) problems.push(`prepared asset ${asset.id || "(unknown)"} needs path`);
      if (!hasText(asset.kind)) problems.push(`prepared asset ${asset.id || "(unknown)"} needs kind`);
      if (asset.path && !existsSync(projectPath(asset.path, root))) problems.push(`prepared asset file missing: ${asset.path}`);
      if (hasText(asset.id)) preparedAssetsById.set(asset.id, asset);
      if (asset.kind === "slice9") {
        const margins = validateMargins(asset.slice9, null, `prepared slice9 asset ${asset.id || "(unknown)"}`, problems);
        validateContent(asset.content || asset.content_rect, null, `prepared slice9 asset ${asset.id || "(unknown)"}`, problems);
        validatePreviewSizes(asset.target_preview_sizes || asset.preview_sizes, margins, `prepared slice9 asset ${asset.id || "(unknown)"}`, problems);
      }
      if (asset.kind === "icon" && !hasText(asset.semantic_role)) problems.push(`prepared icon asset ${asset.id || "(unknown)"} needs semantic_role`);
      if (asset.kind === "sprite" && !asset.pivot && !asset.anchor) problems.push(`prepared sprite asset ${asset.id || "(unknown)"} needs pivot or anchor`);
    }
    for (const [id, cropOutput] of expectedCropOutputs.entries()) {
      const asset = preparedAssetsById.get(id);
      if (!asset) {
        problems.push(`prepared assets manifest missing asset for crop ${id}`);
        continue;
      }
      if (hasText(asset.path) && normalizePathForCompare(asset.path) !== cropOutput.output) {
        problems.push(`prepared asset ${id} path must match crop output ${cropOutput.output}`);
      }
      if (hasText(asset.kind) && hasText(cropOutput.kind) && asset.kind !== cropOutput.kind) {
        problems.push(`prepared asset ${id} kind must match crop kind ${cropOutput.kind}`);
      }
    }
    if (finalArt) {
      validateFinalArtReadiness(job, crop, prepared, loadedGenerationRecords, root, problems);
    }
  }
  return problems;
}

function validationStatus(job, values) {
  if (values.finalArt) {
    const mode = normalizedText(job.expected_outputs?.delivery_scope?.mode);
    if (mode === "partial_prepared_slice") return "partial-prepared-slice-valid";
    return "final-art-valid";
  }
  return values.strict ? "strict-valid" : "draft-valid";
}

function printDeliveryScope(job) {
  const scope = job.expected_outputs?.delivery_scope;
  if (!scope || typeof scope !== "object" || Array.isArray(scope)) return;
  const mode = normalizedText(scope.mode);
  if (mode !== "partial_prepared_slice") return;
  const included = normalizedStringArray(scope.included_source_families).join(",") || "-";
  const deferred = normalizedStringArray(scope.deferred_source_families).join(",") || "-";
  console.log(`scope: partial_prepared_slice included=${included} deferred=${deferred}`);
}

const values = parseArgs(process.argv.slice(2));
if (values.help) usage();
if (!values.job) usage();

const jobPath = values.job.replaceAll("\\", "/");
const fullJobPath = resolve(values.job);
if (!existsSync(fullJobPath)) fail(`art job does not exist: ${values.job}`);
const job = readJson(fullJobPath, fail);
const problems = validateJob(job, jobPath, { strict: values.strict === true, finalArt: values.finalArt === true });
if (problems.length > 0) {
  for (const problem of problems) console.log(`problem: ${problem}`);
  process.exit(1);
}
console.log(`ok: art job ${job.id || values.job} is ${validationStatus(job, values)}`);
if (values.finalArt) printDeliveryScope(job);
