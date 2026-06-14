#!/usr/bin/env node
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

const REVIEW_ATLAS_PURPOSE = "review_validation_atlas_not_engine_runtime_pack";

function usage() {
  console.error(`usage:
  node tools/assets/validate_art_job.mjs --job <art-job.json> [--strict] [--final-art]

Validates generated game UI/art job contracts before runtime integration.
Default mode is draft-friendly. Strict mode requires accepted source art,
crop entries, and runtime assets. Final-art mode also rejects procedural
debug scaffolds and incomplete generation provenance.`);
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

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

function readJson(path) {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch (error) {
    fail(`cannot read JSON ${path}: ${error.message}`);
  }
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
    if (finalPolicy.runtime_final_must_use_generated_or_artist_source !== true) {
      problems.push("generation_contract.final_asset_policy.runtime_final_must_use_generated_or_artist_source must be true");
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

function asArrayField(value, fieldName, problems) {
  if (value === undefined) return null;
  if (!Array.isArray(value)) {
    problems.push(`${fieldName} must be an array when provided`);
    return null;
  }
  return value;
}

function validateAuditEvidence(paths, fieldName, expectedSchema, root, problems, strict, expected = {}) {
  const items = asArrayField(paths, fieldName, problems);
  if (!items) return;
  for (const item of items) {
    if (!hasText(item)) {
      problems.push(`${fieldName} path must be a non-empty string`);
      continue;
    }
    const fullPath = projectPath(item, root);
    if (!existsSync(fullPath)) {
      if (strict) problems.push(`${fieldName} missing: ${item}`);
      continue;
    }
    if (!item.toLowerCase().endsWith(".json")) continue;
    const report = readJsonOrProblem(fullPath, problems, fieldName);
    if (!report) continue;
    if (report.schema !== expectedSchema) {
      problems.push(`${fieldName} JSON schema must be ${expectedSchema}: ${item}`);
    }
    const status = report.verdict ?? report.status;
    if (status !== "pass") {
      problems.push(`${fieldName} JSON verdict/status must be pass: ${item}`);
    }
    if (Array.isArray(report.problems) && report.problems.length > 0) {
      problems.push(`${fieldName} JSON must not list problems: ${item}`);
    }
    if (expected.cropManifest) {
      if (!objectHasText(report, "crop_manifest")) {
        problems.push(`${fieldName} JSON needs crop_manifest: ${item}`);
      } else if (normalizePathForCompare(report.crop_manifest) !== normalizePathForCompare(expected.cropManifest)) {
        problems.push(`${fieldName} JSON crop_manifest must match expected_outputs.crop_manifest: ${item}`);
      }
    }
    if (expected.assetManifest) {
      if (!objectHasText(report, "asset_manifest")) {
        problems.push(`${fieldName} JSON needs asset_manifest: ${item}`);
      } else if (normalizePathForCompare(report.asset_manifest) !== normalizePathForCompare(expected.assetManifest)) {
        problems.push(`${fieldName} JSON asset_manifest must match expected_outputs.runtime_manifest: ${item}`);
      }
    }
    if (expected.sourceArtPaths) {
      if (!objectHasText(report, "source")) {
        problems.push(`${fieldName} JSON needs source: ${item}`);
      } else if (!expected.sourceArtPaths.has(normalizePathForCompare(report.source))) {
        problems.push(`${fieldName} JSON source must match expected source art or crop source: ${item}`);
      }
    }
    if (expected.assetIds && expected.assetIds.size > 0) {
      if (!Array.isArray(report.assets)) {
        problems.push(`${fieldName} JSON needs assets coverage list: ${item}`);
      } else {
        const reportedIds = new Set(report.assets.map((asset) => String(asset?.id || "")).filter((id) => id.length > 0));
        for (const id of expected.assetIds) {
          if (!reportedIds.has(id)) {
            problems.push(`${fieldName} JSON missing audited crop id ${id}: ${item}`);
          }
        }
      }
    }
  }
}

function validateCompositionProofEvidence(paths, fieldName, root, problems, strict, expected = {}) {
  const items = asArrayField(paths, fieldName, problems);
  if (!items) return;
  let jsonReports = 0;
  for (const item of items) {
    if (!hasText(item)) {
      problems.push(`${fieldName} path must be a non-empty string`);
      continue;
    }
    const fullPath = projectPath(item, root);
    if (!existsSync(fullPath)) {
      if (strict) problems.push(`${fieldName} missing: ${item}`);
      continue;
    }
    if (!item.toLowerCase().endsWith(".json")) continue;
    jsonReports += 1;
    const report = readJsonOrProblem(fullPath, problems, fieldName);
    if (!report) continue;
    if (report.schema !== "game.ui_composition_proof") {
      problems.push(`${fieldName} JSON schema must be game.ui_composition_proof: ${item}`);
    }
    const status = report.verdict ?? report.status;
    if (status !== "pass") {
      problems.push(`${fieldName} JSON verdict/status must be pass: ${item}`);
    }
    if (expected.runtimeManifest) {
      if (!objectHasText(report, "asset_manifest")) {
        problems.push(`${fieldName} JSON needs asset_manifest: ${item}`);
      } else if (normalizePathForCompare(report.asset_manifest) !== normalizePathForCompare(expected.runtimeManifest)) {
        problems.push(`${fieldName} JSON asset_manifest must match expected_outputs.runtime_manifest: ${item}`);
      }
    }
    if (objectHasText(report, "output") && strict && !existsSync(projectPath(report.output, root))) {
      problems.push(`${fieldName} JSON output image missing: ${report.output}`);
    }
    if (!Array.isArray(report.items)) {
      problems.push(`${fieldName} JSON needs items coverage list: ${item}`);
      continue;
    }
    for (const entry of report.items) {
      if ((entry?.verdict ?? entry?.status) !== "pass") {
        problems.push(`${fieldName} JSON item ${entry?.base_id || "(unknown)"} must pass: ${item}`);
      }
      if (Array.isArray(entry?.problems) && entry.problems.length > 0) {
        problems.push(`${fieldName} JSON item ${entry?.base_id || "(unknown)"} must not list problems: ${item}`);
      }
    }
    if (expected.slice9Ids && expected.slice9Ids.size > 0) {
      const reportedIds = new Set(report.items.map((entry) => String(entry?.base_id || "")).filter((id) => id.length > 0));
      for (const id of expected.slice9Ids) {
        if (!reportedIds.has(id)) {
          problems.push(`${fieldName} JSON missing slice9 base id ${id}: ${item}`);
        }
      }
    }
  }
  if (strict && items.length > 0 && jsonReports === 0) {
    problems.push(`${fieldName} needs a JSON composition proof report`);
  }
}

function validateEdgeProofReportEvidence(paths, fieldName, root, problems, strict, expected = {}) {
  const items = asArrayField(paths, fieldName, problems);
  if (!items) return;
  let jsonReports = 0;
  for (const item of items) {
    if (!hasText(item)) {
      problems.push(`${fieldName} path must be a non-empty string`);
      continue;
    }
    const fullPath = projectPath(item, root);
    if (!existsSync(fullPath)) {
      if (strict) problems.push(`${fieldName} missing: ${item}`);
      continue;
    }
    if (!item.toLowerCase().endsWith(".json")) continue;
    jsonReports += 1;
    const report = readJsonOrProblem(fullPath, problems, fieldName);
    if (!report) continue;
    if (report.schema !== "game.ui_asset_edge_proof") {
      problems.push(`${fieldName} JSON schema must be game.ui_asset_edge_proof: ${item}`);
    }
    if (expected.cropManifest) {
      if (!objectHasText(report, "crop_manifest")) {
        problems.push(`${fieldName} JSON needs crop_manifest: ${item}`);
      } else if (normalizePathForCompare(report.crop_manifest) !== normalizePathForCompare(expected.cropManifest)) {
        problems.push(`${fieldName} JSON crop_manifest must match expected_outputs.crop_manifest: ${item}`);
      }
    }
    if (!objectHasText(report, "image_output")) {
      problems.push(`${fieldName} JSON needs image_output: ${item}`);
    } else {
      if (strict && !existsSync(projectPath(report.image_output, root))) {
        problems.push(`${fieldName} JSON image_output missing: ${report.image_output}`);
      }
      if (expected.edgeProofImages?.size > 0 && !expected.edgeProofImages.has(normalizePathForCompare(report.image_output))) {
        problems.push(`${fieldName} JSON image_output must match expected_outputs.edge_proofs: ${item}`);
      }
    }
    if (!report.counts || typeof report.counts !== "object") {
      problems.push(`${fieldName} JSON needs counts: ${item}`);
    } else {
      for (const key of ["total", "visible", "transparent_rgb"]) {
        if (typeof report.counts[key] !== "number" || report.counts[key] < 0) {
          problems.push(`${fieldName} JSON counts.${key} must be a non-negative number: ${item}`);
        }
      }
      if (!report.counts.reasons || typeof report.counts.reasons !== "object" || Array.isArray(report.counts.reasons)) {
        problems.push(`${fieldName} JSON counts.reasons must be an object: ${item}`);
      }
    }
    if (!Array.isArray(report.rows)) {
      problems.push(`${fieldName} JSON needs rows: ${item}`);
    }
  }
  if (strict && items.length > 0 && jsonReports === 0) {
    problems.push(`${fieldName} needs a JSON edge proof report`);
  }
}

function validateAtlasPackEvidence(paths, fieldName, root, problems, strict, expected = {}) {
  const items = asArrayField(paths, fieldName, problems);
  if (!items) return;
  let jsonReports = 0;
  for (const item of items) {
    if (!hasText(item)) {
      problems.push(`${fieldName} path must be a non-empty string`);
      continue;
    }
    const fullPath = projectPath(item, root);
    if (!existsSync(fullPath)) {
      if (strict) problems.push(`${fieldName} missing: ${item}`);
      continue;
    }
    if (!item.toLowerCase().endsWith(".json")) continue;
    jsonReports += 1;
    const pack = readJsonOrProblem(fullPath, problems, fieldName);
    if (!pack) continue;
    if (pack.schema !== "game.ui_atlas_pack") {
      problems.push(`${fieldName} JSON schema must be game.ui_atlas_pack: ${item}`);
    }
    if (expected.requireReviewAtlas) {
      if (pack.purpose !== REVIEW_ATLAS_PURPOSE) {
        problems.push(`${fieldName} JSON purpose must be ${REVIEW_ATLAS_PURPOSE}: ${item}`);
      }
      if (pack.label_overlay !== true) {
        problems.push(`${fieldName} JSON label_overlay must be true for final-art review evidence: ${item}`);
      }
    }
    if (expected.runtimeManifest) {
      if (!objectHasText(pack, "asset_manifest")) {
        problems.push(`${fieldName} JSON needs asset_manifest: ${item}`);
      } else if (normalizePathForCompare(pack.asset_manifest) !== normalizePathForCompare(expected.runtimeManifest)) {
        problems.push(`${fieldName} JSON asset_manifest must match expected_outputs.runtime_manifest: ${item}`);
      }
    }
    if (!Array.isArray(pack.atlases) || pack.atlases.length === 0) {
      problems.push(`${fieldName} JSON needs non-empty atlases: ${item}`);
      continue;
    }
    const reportedIds = new Set();
    for (const atlas of pack.atlases) {
      if (!hasText(atlas?.pack_group)) problems.push(`${fieldName} atlas needs pack_group: ${item}`);
      if (expected.requireReviewAtlas) {
        const packGroup = atlas?.pack_group || "(unknown)";
        if (atlas?.purpose !== REVIEW_ATLAS_PURPOSE) {
          problems.push(`${fieldName} atlas ${packGroup} purpose must be ${REVIEW_ATLAS_PURPOSE}: ${item}`);
        }
        if (atlas?.label_overlay !== true) {
          problems.push(`${fieldName} atlas ${packGroup} label_overlay must be true for final-art review evidence: ${item}`);
        }
        if (!hasText(atlas?.labeled_preview_path)) {
          problems.push(`${fieldName} atlas ${packGroup} needs labeled_preview_path for final-art review: ${item}`);
        } else if (strict && !existsSync(projectPath(atlas.labeled_preview_path, root))) {
          problems.push(`${fieldName} atlas labeled preview missing: ${atlas.labeled_preview_path}`);
        }
      }
      if (!hasText(atlas?.path)) {
        problems.push(`${fieldName} atlas needs path: ${item}`);
      } else if (strict && !existsSync(projectPath(atlas.path, root))) {
        problems.push(`${fieldName} atlas image missing: ${atlas.path}`);
      }
      if (!Array.isArray(atlas?.size) || atlas.size.length !== 2 || atlas.size.some((value) => typeof value !== "number" || value <= 0)) {
        problems.push(`${fieldName} atlas needs positive size [w,h]: ${item}`);
      }
      if (!Array.isArray(atlas?.entries) || atlas.entries.length === 0) {
        problems.push(`${fieldName} atlas needs entries: ${item}`);
        continue;
      }
      for (const entry of atlas.entries) {
        if (!hasText(entry?.id)) problems.push(`${fieldName} atlas entry needs id: ${item}`);
        else reportedIds.add(entry.id);
        for (const rectName of ["atlas_rect", "padded_rect"]) {
          const rect = entry?.[rectName];
          if (!Array.isArray(rect) || rect.length !== 4 || rect.some((value, index) => typeof value !== "number" || value < 0 || (index >= 2 && value <= 0))) {
            problems.push(`${fieldName} atlas entry ${entry?.id || "(unknown)"} needs valid ${rectName}: ${item}`);
          }
        }
        if (typeof entry?.extrude !== "number" || entry.extrude < 1) problems.push(`${fieldName} atlas entry ${entry?.id || "(unknown)"} needs extrude >= 1: ${item}`);
      }
    }
    if (expected.assetIds && expected.assetIds.size > 0) {
      for (const id of expected.assetIds) {
        if (!reportedIds.has(id)) {
          problems.push(`${fieldName} JSON missing packed asset id ${id}: ${item}`);
        }
      }
    }
  }
  if (strict && items.length > 0 && jsonReports === 0) {
    problems.push(`${fieldName} needs a JSON atlas pack manifest`);
  }
}

function validateAtlasPackAuditEvidence(paths, fieldName, root, problems, strict, expected = {}) {
  const items = asArrayField(paths, fieldName, problems);
  if (!items) return;
  for (const item of items) {
    if (!hasText(item)) {
      problems.push(`${fieldName} path must be a non-empty string`);
      continue;
    }
    const fullPath = projectPath(item, root);
    if (!existsSync(fullPath)) {
      if (strict) problems.push(`${fieldName} missing: ${item}`);
      continue;
    }
    if (!item.toLowerCase().endsWith(".json")) continue;
    const report = readJsonOrProblem(fullPath, problems, fieldName);
    if (!report) continue;
    if (report.schema !== "game.ui_atlas_pack_audit") {
      problems.push(`${fieldName} JSON schema must be game.ui_atlas_pack_audit: ${item}`);
    }
    const status = report.verdict ?? report.status;
    if (status !== "pass") {
      problems.push(`${fieldName} JSON verdict/status must be pass: ${item}`);
    }
    if (Array.isArray(report.problems) && report.problems.length > 0) {
      problems.push(`${fieldName} JSON must not list problems: ${item}`);
    }
    if (expected.runtimeManifest) {
      if (!objectHasText(report, "asset_manifest")) {
        problems.push(`${fieldName} JSON needs asset_manifest: ${item}`);
      } else if (normalizePathForCompare(report.asset_manifest) !== normalizePathForCompare(expected.runtimeManifest)) {
        problems.push(`${fieldName} JSON asset_manifest must match expected_outputs.runtime_manifest: ${item}`);
      }
    }
    if (expected.atlasPacks && expected.atlasPacks.size > 0) {
      if (!objectHasText(report, "atlas_pack")) {
        problems.push(`${fieldName} JSON needs atlas_pack: ${item}`);
      } else if (!expected.atlasPacks.has(normalizePathForCompare(report.atlas_pack))) {
        problems.push(`${fieldName} JSON atlas_pack must match expected_outputs.atlas_pack: ${item}`);
      }
    }
  }
}

function collectCropIds(crop, allowedKinds = null) {
  const ids = new Set();
  for (const source of crop?.sources || []) {
    for (const item of source.crops || []) {
      if (!hasText(item.id)) continue;
      if (allowedKinds && !allowedKinds.has(item.kind)) continue;
      ids.add(item.id);
    }
  }
  return ids;
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

function validateFinalArtReadiness(job, crop, records, root, problems) {
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
  if (!job.runtime_composition || typeof job.runtime_composition !== "object") problems.push("art job needs runtime_composition");
  if (!job.expected_outputs || typeof job.expected_outputs !== "object") problems.push("art job needs expected_outputs");
  if (!hasText(job.expected_outputs?.crop_manifest)) problems.push("expected_outputs.crop_manifest is required");
  if (!hasText(job.expected_outputs?.runtime_manifest)) problems.push("expected_outputs.runtime_manifest is required");
  if (!hasText(job.expected_outputs?.runtime_dir)) problems.push("expected_outputs.runtime_dir is required");
  if (job.expected_outputs?.edge_proofs !== undefined && !Array.isArray(job.expected_outputs.edge_proofs)) {
    problems.push("expected_outputs.edge_proofs must be an array when provided");
  }
  const edgeProofReportEvidence = asArrayField(
    job.expected_outputs?.edge_proof_reports ?? job.expected_outputs?.edge_proof_report,
    "expected_outputs.edge_proof_reports",
    problems
  );
  const assetAuditEvidence = asArrayField(job.expected_outputs?.asset_audit, "expected_outputs.asset_audit", problems);
  const sourceDerivationAuditEvidence = asArrayField(
    job.expected_outputs?.source_derivation_audit ?? job.expected_outputs?.source_derivation_audits,
    "expected_outputs.source_derivation_audit",
    problems
  );
  const sourceSheetIntakeAuditEvidence = asArrayField(
    job.expected_outputs?.source_sheet_intake_audit ?? job.expected_outputs?.source_sheet_intake_audits,
    "expected_outputs.source_sheet_intake_audit",
    problems
  );
  const slice9DesignAuditEvidence = asArrayField(
    job.expected_outputs?.slice9_design_audit ?? job.expected_outputs?.slice9_design_audits,
    "expected_outputs.slice9_design_audit",
    problems
  );
  const sourceFamilyCoverageEvidence = asArrayField(
    job.expected_outputs?.source_family_coverage_audit ?? job.expected_outputs?.source_family_coverage_audits,
    "expected_outputs.source_family_coverage_audit",
    problems
  );
  const compositionProofEvidence = asArrayField(
    job.expected_outputs?.composition_proof ?? job.expected_outputs?.composition_proofs,
    "expected_outputs.composition_proof",
    problems
  );
  const atlasMetadataAuditEvidence = asArrayField(
    job.expected_outputs?.atlas_metadata_audit ?? job.expected_outputs?.atlas_metadata_audits,
    "expected_outputs.atlas_metadata_audit",
    problems
  );
  const atlasPackEvidence = asArrayField(
    job.expected_outputs?.atlas_pack ?? job.expected_outputs?.atlas_packs,
    "expected_outputs.atlas_pack",
    problems
  );
  const atlasPackAuditEvidence = asArrayField(
    job.expected_outputs?.atlas_pack_audit ?? job.expected_outputs?.atlas_pack_audits,
    "expected_outputs.atlas_pack_audit",
    problems
  );
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
  const cropPath = projectPath(job.expected_outputs?.crop_manifest, root);
  const runtimePath = projectPath(job.expected_outputs?.runtime_manifest, root);
  if (hasText(job.expected_outputs?.crop_manifest) && !existsSync(cropPath)) problems.push(`crop manifest does not exist: ${job.expected_outputs.crop_manifest}`);
  if (hasText(job.expected_outputs?.runtime_manifest) && !existsSync(runtimePath)) problems.push(`runtime manifest does not exist: ${job.expected_outputs.runtime_manifest}`);

  let crop = null;
  let runtime = null;
  if (existsSync(cropPath)) crop = readJson(cropPath);
  if (existsSync(runtimePath)) runtime = readJson(runtimePath);
  const relJob = jobPath.replaceAll("\\", "/");
  if (crop) {
    if (crop.schema !== "game.art_crop_manifest") problems.push("crop manifest schema must be game.art_crop_manifest");
    if (crop.art_job && crop.art_job !== relJob) problems.push(`crop manifest art_job should match ${relJob}`);
    if (!hasText(crop.output_dir)) problems.push("crop manifest needs output_dir");
    if (!Array.isArray(crop.sources)) problems.push("crop manifest needs sources array");
  }
  if (runtime) {
    if (runtime.schema !== "game.asset_manifest") problems.push("runtime manifest schema must be game.asset_manifest");
    if (runtime.art_job && runtime.art_job !== relJob) problems.push(`runtime manifest art_job should match ${relJob}`);
    if (!hasText(runtime.runtime_dir)) problems.push("runtime manifest needs runtime_dir");
    if (!Array.isArray(runtime.assets)) problems.push("runtime manifest needs assets array");
  }
  const expectedSourcePaths = new Set();
  for (const source of job.expected_outputs?.source_art || []) {
    if (hasText(source)) expectedSourcePaths.add(normalizePathForCompare(source));
  }
  for (const source of crop?.sources || []) {
    if (hasText(source.path)) expectedSourcePaths.add(normalizePathForCompare(source.path));
  }
  const expectedCropIds = collectCropIds(crop);
  const slice9CropIds = collectCropIds(crop, new Set(["slice9"]));
  const sourceDerivationCropIds = collectCropIds(crop, new Set(["slice9", "border", "tile", "sprite"]));
  const expectedCropOutputs = collectCropOutputs(crop);

  if (strict) {
    const loadedGenerationRecords = [];
    if (!Array.isArray(job.expected_outputs?.source_art) || job.expected_outputs.source_art.length === 0) problems.push("strict mode requires expected_outputs.source_art");
    if (!Array.isArray(generationRecords) || generationRecords.length === 0) problems.push("strict mode requires expected_outputs.generation_records");
    if (!Array.isArray(assetAuditEvidence) || assetAuditEvidence.length === 0) problems.push("strict mode requires expected_outputs.asset_audit");
    validateAuditEvidence(assetAuditEvidence, "expected_outputs.asset_audit", "game.generated_ui_asset_audit", root, problems, true, {
      cropManifest: job.expected_outputs?.crop_manifest,
      assetIds: expectedCropIds,
    });
    if (Array.isArray(sourceSheetIntakeAuditEvidence)) {
      validateAuditEvidence(
        sourceSheetIntakeAuditEvidence,
        "expected_outputs.source_sheet_intake_audit",
        "game.source_sheet_intake_audit",
        root,
        problems,
        true,
        { sourceArtPaths: expectedSourcePaths }
      );
    }
    if (Array.isArray(sourceDerivationAuditEvidence)) {
      validateAuditEvidence(
        sourceDerivationAuditEvidence,
        "expected_outputs.source_derivation_audit",
        "game.generated_source_derivation_audit",
        root,
        problems,
        true,
        {
          cropManifest: job.expected_outputs?.crop_manifest,
          assetIds: sourceDerivationCropIds,
        }
      );
    }
    if (Array.isArray(slice9DesignAuditEvidence)) {
      validateAuditEvidence(
        slice9DesignAuditEvidence,
        "expected_outputs.slice9_design_audit",
        "game.slice9_design_policy_audit",
        root,
        problems,
        true,
        {
          cropManifest: job.expected_outputs?.crop_manifest,
          assetIds: slice9CropIds,
        }
      );
    }
    if (Array.isArray(atlasMetadataAuditEvidence)) {
      validateAuditEvidence(
        atlasMetadataAuditEvidence,
        "expected_outputs.atlas_metadata_audit",
        "game.atlas_metadata_audit",
        root,
        problems,
        true,
        {
          assetManifest: job.expected_outputs?.runtime_manifest,
          assetIds: expectedCropIds,
        }
      );
    }
    if (Array.isArray(atlasPackEvidence)) {
      validateAtlasPackEvidence(
        atlasPackEvidence,
        "expected_outputs.atlas_pack",
        root,
        problems,
        true,
        {
          runtimeManifest: job.expected_outputs?.runtime_manifest,
          assetIds: expectedCropIds,
          requireReviewAtlas: finalArt,
        }
      );
    }
    const expectedAtlasPacks = new Set(
      (atlasPackEvidence || [])
        .filter((item) => hasText(item) && item.toLowerCase().endsWith(".json"))
        .map((item) => normalizePathForCompare(item))
    );
    if (Array.isArray(atlasPackAuditEvidence)) {
      validateAtlasPackAuditEvidence(
        atlasPackAuditEvidence,
        "expected_outputs.atlas_pack_audit",
        root,
        problems,
        true,
        {
          runtimeManifest: job.expected_outputs?.runtime_manifest,
          atlasPacks: expectedAtlasPacks,
        }
      );
    }
    if (Array.isArray(compositionProofEvidence)) {
      validateCompositionProofEvidence(
        compositionProofEvidence,
        "expected_outputs.composition_proof",
        root,
        problems,
        true,
        {
          runtimeManifest: job.expected_outputs?.runtime_manifest,
          slice9Ids: slice9CropIds,
        }
      );
    }
    for (const [index, record] of (Array.isArray(generationRecords) ? generationRecords : []).entries()) {
      const loadedRecord = loadGenerationRecord(record, `generation record ${typeof record === "string" ? record : record?.id || index}`, root, problems, true);
      validateGenerationRecord(loadedRecord, `generation record ${loadedRecord?.id || index}`, root, problems, true, job);
      if (loadedRecord) loadedGenerationRecords.push(loadedRecord);
    }
    for (const source of job.expected_outputs?.source_art || []) {
      if (!existsSync(projectPath(source, root))) problems.push(`strict mode source art missing: ${source}`);
    }
    for (const proof of job.expected_outputs?.edge_proofs || []) {
      if (!hasText(proof)) {
        problems.push("strict mode edge proof path must be a non-empty string");
      } else if (!existsSync(projectPath(proof, root))) {
        problems.push(`strict mode edge proof missing: ${proof}`);
      }
    }
    const expectedEdgeProofImages = new Set(
      (job.expected_outputs?.edge_proofs || [])
        .filter((item) => hasText(item))
        .map((item) => normalizePathForCompare(item))
    );
    if ((job.expected_outputs?.edge_proofs || []).length > 0 && (!Array.isArray(edgeProofReportEvidence) || edgeProofReportEvidence.length === 0)) {
      problems.push("strict mode edge proofs require expected_outputs.edge_proof_reports");
    }
    if (Array.isArray(edgeProofReportEvidence)) {
      validateEdgeProofReportEvidence(
        edgeProofReportEvidence,
        "expected_outputs.edge_proof_reports",
        root,
        problems,
        true,
        {
          cropManifest: job.expected_outputs?.crop_manifest,
          edgeProofImages: expectedEdgeProofImages,
        }
      );
    }
    if (!crop || !Array.isArray(crop.sources) || crop.sources.length === 0) problems.push("strict mode requires crop manifest sources");
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
    if (!runtime || !Array.isArray(runtime.assets) || runtime.assets.length === 0) problems.push("strict mode requires runtime manifest assets");
    if (runtime && hasText(runtime.crop_manifest) && normalizePathForCompare(runtime.crop_manifest) !== normalizePathForCompare(job.expected_outputs?.crop_manifest)) {
      problems.push("runtime manifest crop_manifest should match expected_outputs.crop_manifest");
    }
    const runtimeAssetsById = new Map();
    for (const asset of runtime?.assets || []) {
      if (!hasText(asset.id)) problems.push("runtime asset needs id");
      if (!hasText(asset.path)) problems.push(`runtime asset ${asset.id || "(unknown)"} needs path`);
      if (!hasText(asset.kind)) problems.push(`runtime asset ${asset.id || "(unknown)"} needs kind`);
      if (asset.path && !existsSync(projectPath(asset.path, root))) problems.push(`runtime asset file missing: ${asset.path}`);
      if (hasText(asset.id)) runtimeAssetsById.set(asset.id, asset);
      if (asset.kind === "slice9") {
        const margins = validateMargins(asset.slice9, null, `runtime slice9 asset ${asset.id || "(unknown)"}`, problems);
        validateContent(asset.content || asset.content_rect, null, `runtime slice9 asset ${asset.id || "(unknown)"}`, problems);
        validatePreviewSizes(asset.target_preview_sizes || asset.preview_sizes, margins, `runtime slice9 asset ${asset.id || "(unknown)"}`, problems);
      }
      if (asset.kind === "icon" && !hasText(asset.semantic_role)) problems.push(`runtime icon asset ${asset.id || "(unknown)"} needs semantic_role`);
      if (asset.kind === "sprite" && !asset.pivot && !asset.anchor) problems.push(`runtime sprite asset ${asset.id || "(unknown)"} needs pivot or anchor`);
    }
    for (const [id, cropOutput] of expectedCropOutputs.entries()) {
      const asset = runtimeAssetsById.get(id);
      if (!asset) {
        problems.push(`runtime manifest missing asset for crop ${id}`);
        continue;
      }
      if (hasText(asset.path) && normalizePathForCompare(asset.path) !== cropOutput.output) {
        problems.push(`runtime asset ${id} path must match crop output ${cropOutput.output}`);
      }
      if (hasText(asset.kind) && hasText(cropOutput.kind) && asset.kind !== cropOutput.kind) {
        problems.push(`runtime asset ${id} kind must match crop kind ${cropOutput.kind}`);
      }
    }
    if (finalArt) {
      if (!Array.isArray(sourceSheetIntakeAuditEvidence) || sourceSheetIntakeAuditEvidence.length === 0) {
        problems.push("final-art mode requires expected_outputs.source_sheet_intake_audit");
      }
      if (!Array.isArray(sourceDerivationAuditEvidence) || sourceDerivationAuditEvidence.length === 0) {
        problems.push("final-art mode requires expected_outputs.source_derivation_audit");
      }
      if (!Array.isArray(slice9DesignAuditEvidence) || slice9DesignAuditEvidence.length === 0) {
        problems.push("final-art mode requires expected_outputs.slice9_design_audit");
      }
      if (!Array.isArray(compositionProofEvidence) || compositionProofEvidence.length === 0) {
        problems.push("final-art mode requires expected_outputs.composition_proof");
      }
      if (!Array.isArray(atlasMetadataAuditEvidence) || atlasMetadataAuditEvidence.length === 0) {
        problems.push("final-art mode requires expected_outputs.atlas_metadata_audit");
      }
      if (!Array.isArray(atlasPackEvidence) || atlasPackEvidence.length === 0) {
        problems.push("final-art mode requires expected_outputs.atlas_pack");
      }
      if (!Array.isArray(atlasPackAuditEvidence) || atlasPackAuditEvidence.length === 0) {
        problems.push("final-art mode requires expected_outputs.atlas_pack_audit");
      }
      if (!Array.isArray(sourceFamilyCoverageEvidence) || sourceFamilyCoverageEvidence.length === 0) {
        problems.push("final-art mode requires expected_outputs.source_family_coverage_audit");
      } else {
        validateAuditEvidence(
          sourceFamilyCoverageEvidence,
          "expected_outputs.source_family_coverage_audit",
          "game.source_family_coverage_audit",
          root,
          problems,
          true
        );
      }
      validateFinalArtReadiness(job, crop, loadedGenerationRecords, root, problems);
    }
  }
  return problems;
}

const values = parseArgs(process.argv.slice(2));
if (values.help) usage();
if (!values.job) usage();

const jobPath = values.job.replaceAll("\\", "/");
const fullJobPath = resolve(values.job);
if (!existsSync(fullJobPath)) fail(`art job does not exist: ${values.job}`);
const job = readJson(fullJobPath);
const problems = validateJob(job, jobPath, { strict: values.strict === true, finalArt: values.finalArt === true });
if (problems.length > 0) {
  for (const problem of problems) console.log(`problem: ${problem}`);
  process.exit(1);
}
console.log(`ok: art job ${job.id || values.job} is ${values.finalArt ? "final-art-valid" : values.strict ? "strict-valid" : "draft-valid"}`);
