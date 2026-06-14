import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "art-job-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function run(args, cwd) {
  return spawnSync(process.execPath, [join(root, "tools/assets/validate_art_job.mjs"), ...args], {
    cwd,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function writeAuditReport(dir, path, schema, verdict = "pass", problems = [], extra = {}) {
  mkdirSync(join(dir, dirname(path)), { recursive: true });
  const report = {
    schema,
    version: 1,
    verdict,
    problems,
  };
  if (schema === "game.generated_ui_asset_audit" || schema === "game.generated_source_derivation_audit" || schema === "game.slice9_design_policy_audit") {
    report.crop_manifest = "gamedesign/projects/test/data/ui-kit-crop.json";
  }
  if (schema === "game.generated_ui_asset_audit") {
    report.assets = [{ id: "panel" }, { id: "resource_icon" }, { id: "enemy" }];
  }
  if (schema === "game.generated_source_derivation_audit") {
    report.assets = [{ id: "panel" }, { id: "enemy" }];
  }
  if (schema === "game.slice9_design_policy_audit") {
    report.assets = [{ id: "panel" }];
  }
  if (schema === "game.ui_composition_proof") {
    report.asset_manifest = "gamedesign/projects/test/data/ui-kit-assets.json";
    report.output = "gamedesign/projects/test/art/previews/ui-kit-composition-proof.png";
    report.items = [{ base_id: "panel", status: verdict, problems }];
  }
  if (schema === "game.atlas_metadata_audit") {
    report.asset_manifest = "gamedesign/projects/test/data/ui-kit-assets.json";
    report.assets = [{ id: "panel" }, { id: "resource_icon" }, { id: "enemy" }];
  }
  if (schema === "game.source_sheet_intake_audit") report.status = verdict;
  if (schema === "game.source_sheet_intake_audit") report.source = "gamedesign/projects/test/art/ui-source.png";
  Object.assign(report, extra);
  writeFileSync(join(dir, path), `${JSON.stringify(report, null, 2)}\n`, "utf8");
}

function writeAtlasPack(dir, path, overrides = {}) {
  mkdirSync(join(dir, dirname(path)), { recursive: true });
  const pack = {
    schema: "game.ui_atlas_pack",
    version: 1,
    purpose: "review_validation_atlas_not_engine_runtime_pack",
    label_overlay: true,
    asset_manifest: "gamedesign/projects/test/data/ui-kit-assets.json",
    output_dir: "assets/runtime/ui-kit-atlas",
    atlases: [
      {
        pack_group: "ui_common",
        purpose: "review_validation_atlas_not_engine_runtime_pack",
        label_overlay: true,
        path: "assets/runtime/ui-kit-atlas/ui_common.png",
        labeled_preview_path: "assets/runtime/ui-kit-atlas/ui_common-labeled.png",
        size: [256, 128],
        physical_entry_count: 3,
        alias_count: 0,
        entries: [
          { id: "panel", kind: "slice9", atlas_rect: [3, 3, 96, 64], padded_rect: [1, 1, 100, 68], extrude: 2 },
          { id: "resource_icon", kind: "icon", atlas_rect: [105, 3, 64, 64], padded_rect: [103, 1, 68, 68], extrude: 2 },
          { id: "enemy", kind: "sprite", atlas_rect: [175, 3, 64, 64], padded_rect: [173, 1, 68, 68], extrude: 2 },
        ],
      },
    ],
    ...overrides,
  };
  writeFileSync(join(dir, path), `${JSON.stringify(pack, null, 2)}\n`, "utf8");
}

function writeAtlasPackAudit(dir, path, overrides = {}) {
  mkdirSync(join(dir, dirname(path)), { recursive: true });
  const audit = {
    schema: "game.ui_atlas_pack_audit",
    version: 1,
    atlas_pack: "gamedesign/projects/test/data/ui-kit-atlas-pack.json",
    asset_manifest: "gamedesign/projects/test/data/ui-kit-assets.json",
    verdict: "pass",
    problems: [],
    atlases: [{ pack_group: "ui_common", status: "pass", problems: [], entry_count: 3 }],
    ...overrides,
  };
  writeFileSync(join(dir, path), `${JSON.stringify(audit, null, 2)}\n`, "utf8");
}

function writeEdgeProofReport(dir, path, imageOutput, overrides = {}) {
  mkdirSync(join(dir, dirname(path)), { recursive: true });
  const report = {
    schema: "game.ui_asset_edge_proof",
    version: 1,
    crop_manifest: "gamedesign/projects/test/data/ui-kit-crop.json",
    image_output: imageOutput,
    counts: {
      total: 0,
      visible: 0,
      transparent_rgb: 0,
      reasons: {},
    },
    rows: [
      {
        asset_id: "panel",
        kind: "slice9",
        output: "assets/runtime/ui-kit/panel.png",
        side: "right",
        rect: [90, 0, 6, 64],
        counts: {
          total: 0,
          visible: 0,
          transparent_rgb: 0,
          reasons: {},
        },
      },
    ],
    ...overrides,
  };
  writeFileSync(join(dir, path), `${JSON.stringify(report, null, 2)}\n`, "utf8");
}

function writeValidDraft(dir) {
  mkdirSync(join(dir, "gamedesign/projects/test/art_requests"), { recursive: true });
  mkdirSync(join(dir, "gamedesign/projects/test/data"), { recursive: true });
  const job = "gamedesign/projects/test/art_requests/ui-kit.json";
  const crop = "gamedesign/projects/test/data/ui-kit-crop.json";
  const runtime = "gamedesign/projects/test/data/ui-kit-assets.json";
  writeFileSync(join(dir, job), `${JSON.stringify({
    schema: "game.art_job",
    version: 1,
    id: "ui-kit",
    asset_family: "runtime-ui-kit",
    visual_targets: ["gamedesign/projects/test/art/design_bible.md"],
    reusable_kinds: ["slice9", "icon", "sprite"],
    required_asset_groups: [
      {
        id: "panel_slice9",
        kind: "slice9",
        states: ["default"],
        content_policy: "runtime text inside content area",
        stretch_zone_policy: "plain repeatable center and straight edges",
        decor_overlay_policy: "ornaments are corner-only or separate overlay sprites",
        target_preview_sizes: [[160, 96], [240, 160]],
      },
      {
        id: "resource_icons",
        kind: "icon",
        size_class: "64px source",
      },
      {
        id: "enemy_sprite",
        kind: "sprite",
        anchor_policy: "pivot required",
      },
    ],
    must_not_bake: ["button labels", "debug text", "game state values"],
    generation_contract: {
      source_families: ["blank UI kit sheet", "isolated icon sheet"],
      source_family_roles: {
        "blank UI kit sheet": "slice9 bases and blank control states",
        "isolated icon sheet": "semantic icon set with gutters",
      },
      final_asset_policy: {
        procedural_art_allowed: "debug_only",
        runtime_final_must_use_generated_or_artist_source: true,
        layered_source_required_for_ui: true,
      },
      prompt_constraints: ["no readable text", "clear gutters between assets", "no unique decoration inside slice9 stretch zones"],
      metadata_to_record: [
        "provider or generator",
        "model/workflow",
        "workflow file or workflow json",
        "seed",
        "prompt",
        "negative prompt",
        "source family role",
        "accepted source image path",
        "rejected candidate notes",
      ],
    },
    expected_outputs: {
      source_art: [],
      generation_records: [],
      crop_manifest: crop,
      runtime_manifest: runtime,
      runtime_dir: "assets/runtime/ui-kit",
    },
    runtime_composition: { buttons: "slice9 plus runtime text" },
  }, null, 2)}\n`, "utf8");
  writeFileSync(join(dir, crop), `${JSON.stringify({
    schema: "game.art_crop_manifest",
    version: 1,
    art_job: job,
    output_dir: "assets/runtime/ui-kit",
    sources: [],
  }, null, 2)}\n`, "utf8");
  writeFileSync(join(dir, runtime), `${JSON.stringify({
    schema: "game.asset_manifest",
    version: 1,
    art_job: job,
    runtime_dir: "assets/runtime/ui-kit",
    assets: [],
  }, null, 2)}\n`, "utf8");
  return job;
}

function writeStrictValidJob(dir, recordOverrides = {}) {
  const job = writeValidDraft(dir);
  mkdirSync(join(dir, "gamedesign/projects/test/art"), { recursive: true });
  mkdirSync(join(dir, "gamedesign/projects/test/art/workflows"), { recursive: true });
  mkdirSync(join(dir, "gamedesign/projects/test/art/generation_records"), { recursive: true });
  mkdirSync(join(dir, "gamedesign/projects/test/reviews"), { recursive: true });
  mkdirSync(join(dir, "assets/runtime/ui-kit"), { recursive: true });
  writeFileSync(join(dir, "gamedesign/projects/test/art/ui-source.png"), "fake-png", "utf8");
  writeFileSync(join(dir, "gamedesign/projects/test/art/workflows/ui-source.json"), "{}", "utf8");
  writeFileSync(join(dir, "assets/runtime/ui-kit/panel.png"), "fake-png", "utf8");
  writeFileSync(join(dir, "assets/runtime/ui-kit/icon.png"), "fake-png", "utf8");
  writeFileSync(join(dir, "assets/runtime/ui-kit/enemy.png"), "fake-png", "utf8");
  const assetAudit = "gamedesign/projects/test/reviews/ui-kit-asset-audit.json";
  const sourceSheetIntakeAudit = "gamedesign/projects/test/reviews/ui-kit-source-intake-audit.json";
  const sourceDerivationAudit = "gamedesign/projects/test/reviews/ui-kit-source-derivation-audit.json";
  const slice9DesignAudit = "gamedesign/projects/test/reviews/ui-kit-slice9-design-audit.json";
  const compositionProof = "gamedesign/projects/test/reviews/ui-kit-composition-proof.json";
  const compositionProofPng = "gamedesign/projects/test/art/previews/ui-kit-composition-proof.png";
  const sourceFamilyCoverageAudit = "gamedesign/projects/test/reviews/ui-kit-source-family-coverage-audit.json";
  const atlasMetadataAudit = "gamedesign/projects/test/reviews/ui-kit-atlas-metadata-audit.json";
  const atlasPack = "gamedesign/projects/test/data/ui-kit-atlas-pack.json";
  const atlasPackAudit = "gamedesign/projects/test/reviews/ui-kit-atlas-pack-audit.json";
  writeAuditReport(dir, assetAudit, "game.generated_ui_asset_audit");
  writeAuditReport(dir, sourceSheetIntakeAudit, "game.source_sheet_intake_audit");
  writeAuditReport(dir, sourceDerivationAudit, "game.generated_source_derivation_audit");
  writeAuditReport(dir, slice9DesignAudit, "game.slice9_design_policy_audit");
  writeAuditReport(dir, compositionProof, "game.ui_composition_proof");
  writeAuditReport(dir, sourceFamilyCoverageAudit, "game.source_family_coverage_audit");
  writeAuditReport(dir, atlasMetadataAudit, "game.atlas_metadata_audit");
  writeAtlasPack(dir, atlasPack);
  writeAtlasPackAudit(dir, atlasPackAudit);
  mkdirSync(join(dir, "assets/runtime/ui-kit-atlas"), { recursive: true });
  writeFileSync(join(dir, "assets/runtime/ui-kit-atlas/ui_common.png"), "fake-png", "utf8");
  writeFileSync(join(dir, "assets/runtime/ui-kit-atlas/ui_common-labeled.png"), "fake-png", "utf8");
  mkdirSync(join(dir, dirname(compositionProofPng)), { recursive: true });
  writeFileSync(join(dir, compositionProofPng), "fake-png", "utf8");

  const generationRecord = "gamedesign/projects/test/art/generation_records/ui-source.json";
  writeFileSync(join(dir, generationRecord), `${JSON.stringify({
    schema: "game.art_generation_record",
    version: 1,
    id: "ui-source",
    provider: "test-generator",
    model_or_workflow: "test-workflow",
    workflow_path: "gamedesign/projects/test/art/workflows/ui-source.json",
    seed: 1234,
    prompt: "blank fantasy UI source family",
    negative_prompt: "text, watermark",
    source_family_role: "blank UI kit sheet",
    accepted_source_image: "gamedesign/projects/test/art/ui-source.png",
    final_art_source: "generated",
    ...recordOverrides,
  }, null, 2)}\n`, "utf8");

  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.source_art = ["gamedesign/projects/test/art/ui-source.png"];
  jobData.expected_outputs.generation_records = [generationRecord];
  jobData.expected_outputs.asset_audit = [assetAudit];
  jobData.expected_outputs.source_sheet_intake_audit = [sourceSheetIntakeAudit];
  jobData.expected_outputs.source_derivation_audit = [sourceDerivationAudit];
  jobData.expected_outputs.slice9_design_audit = [slice9DesignAudit];
  jobData.expected_outputs.composition_proof = [compositionProof, compositionProofPng];
  jobData.expected_outputs.source_family_coverage_audit = [sourceFamilyCoverageAudit];
  jobData.expected_outputs.atlas_metadata_audit = [atlasMetadataAudit];
  jobData.expected_outputs.atlas_pack = [atlasPack];
  jobData.expected_outputs.atlas_pack_audit = [atlasPackAudit];
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const crop = "gamedesign/projects/test/data/ui-kit-crop.json";
  writeFileSync(join(dir, crop), `${JSON.stringify({
    schema: "game.art_crop_manifest",
    version: 1,
    art_job: job,
    output_dir: "assets/runtime/ui-kit",
    sources: [{
      id: "ui-source",
      path: "gamedesign/projects/test/art/ui-source.png",
      crops: [
        {
          id: "panel",
          kind: "slice9",
          rect: [0, 0, 96, 64],
          output: "assets/runtime/ui-kit/panel.png",
          slice9: { left: 12, top: 12, right: 12, bottom: 12 },
          content: { x: 18, y: 18, w: 60, h: 32 },
          target_preview_sizes: [[160, 96], [240, 160]],
        },
        {
          id: "resource_icon",
          kind: "icon",
          rect: [96, 0, 64, 64],
          output: "assets/runtime/ui-kit/icon.png",
          semantic_role: "resource",
          size_class: "64px source",
          trim_padding: 6,
          isolate_component: "center",
        },
        {
          id: "enemy",
          kind: "sprite",
          rect: [160, 0, 64, 64],
          output: "assets/runtime/ui-kit/enemy.png",
          pivot: [32, 56],
        },
      ],
    }],
  }, null, 2)}\n`, "utf8");

  const runtime = "gamedesign/projects/test/data/ui-kit-assets.json";
  writeFileSync(join(dir, runtime), `${JSON.stringify({
    schema: "game.asset_manifest",
    version: 1,
    art_job: job,
    runtime_dir: "assets/runtime/ui-kit",
    assets: [
      {
        id: "panel",
        kind: "slice9",
        path: "assets/runtime/ui-kit/panel.png",
        slice9: { left: 12, top: 12, right: 12, bottom: 12 },
        content: { x: 18, y: 18, w: 60, h: 32 },
        target_preview_sizes: [[160, 96], [240, 160]],
      },
      {
        id: "resource_icon",
        kind: "icon",
        path: "assets/runtime/ui-kit/icon.png",
        semantic_role: "resource",
      },
      {
        id: "enemy",
        kind: "sprite",
        path: "assets/runtime/ui-kit/enemy.png",
        pivot: [32, 56],
      },
    ],
  }, null, 2)}\n`, "utf8");

  return { job, generationRecord };
}

function writePromptPacket(dir, path, overrides = {}) {
  mkdirSync(join(dir, path, ".."), { recursive: true });
  writeFileSync(join(dir, path), `${JSON.stringify({
    schema: "game.source_sheet_prompt_packet",
    version: 1,
    job_id: "ui-kit",
    asset_family: "runtime-ui-kit",
    source_family: "blank UI kit sheet",
    source_family_role: "slice9 bases and blank control states",
    prompt: "Create a cut-ready blank UI kit sheet with gutters.",
    negative_prompt: "text, watermark, fused icons",
    acceptance_checklist: ["No readable text.", "Clear gutters between assets."],
    ...overrides,
  }, null, 2)}\n`, "utf8");
}

test("validates a draft UI art job contract", (t) => {
  const dir = tempDir(t);
  const job = writeValidDraft(dir);
  const result = run(["--job", job], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /draft-valid/);
});

test("strict mode rejects missing source sheets and crops", (t) => {
  const dir = tempDir(t);
  const job = writeValidDraft(dir);
  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /strict mode requires expected_outputs.source_art/);
  assert.match(result.stdout, /strict mode requires crop manifest sources/);
});

test("reports missing reusable UI contract pieces", (t) => {
  const dir = tempDir(t);
  mkdirSync(join(dir, "gamedesign/projects/test/art_requests"), { recursive: true });
  const job = "gamedesign/projects/test/art_requests/bad.json";
  writeFileSync(join(dir, job), `${JSON.stringify({
    schema: "game.art_job",
    id: "bad",
    asset_family: "bad",
    expected_outputs: {},
  })}\n`, "utf8");
  const result = run(["--job", job], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /art job needs visual_targets/);
  assert.match(result.stdout, /reusable_kinds should include slice9/);
  assert.match(result.stdout, /must_not_bake should include button labels/);
});

test("strict mode validates slice9 content and runtime metadata", (t) => {
  const dir = tempDir(t);
  const job = writeValidDraft(dir);
  mkdirSync(join(dir, "gamedesign/projects/test/art"), { recursive: true });
  mkdirSync(join(dir, "gamedesign/projects/test/art/workflows"), { recursive: true });
  mkdirSync(join(dir, "gamedesign/projects/test/art/generation_records"), { recursive: true });
  mkdirSync(join(dir, "assets/runtime/ui-kit"), { recursive: true });
  writeFileSync(join(dir, "gamedesign/projects/test/art/ui-source.png"), "fake-png", "utf8");
  writeFileSync(join(dir, "gamedesign/projects/test/art/workflows/ui-source.json"), "{}", "utf8");
  writeFileSync(join(dir, "assets/runtime/ui-kit/panel.png"), "fake-png", "utf8");
  writeFileSync(join(dir, "assets/runtime/ui-kit/icon.png"), "fake-png", "utf8");
  writeFileSync(join(dir, "assets/runtime/ui-kit/enemy.png"), "fake-png", "utf8");

  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.source_art = ["gamedesign/projects/test/art/ui-source.png"];
  const assetAudit = "gamedesign/projects/test/reviews/ui-kit-asset-audit.json";
  const sourceSheetIntakeAudit = "gamedesign/projects/test/reviews/ui-kit-source-intake-audit.json";
  writeAuditReport(dir, assetAudit, "game.generated_ui_asset_audit");
  writeAuditReport(dir, sourceSheetIntakeAudit, "game.source_sheet_intake_audit");
  jobData.expected_outputs.asset_audit = [assetAudit];
  jobData.expected_outputs.source_sheet_intake_audit = [sourceSheetIntakeAudit];
  const generationRecord = "gamedesign/projects/test/art/generation_records/ui-source.json";
  writeFileSync(join(dir, generationRecord), `${JSON.stringify({
    schema: "game.art_generation_record",
    version: 1,
    id: "ui-source",
    provider: "test-generator",
    model_or_workflow: "test-workflow",
    workflow_path: "gamedesign/projects/test/art/workflows/ui-source.json",
    seed: 1234,
    prompt: "blank fantasy UI source family",
    negative_prompt: "text, watermark",
    source_family_role: "blank UI kit sheet",
    accepted_source_image: "gamedesign/projects/test/art/ui-source.png",
    final_art_source: "generated",
  }, null, 2)}\n`, "utf8");
  jobData.expected_outputs.generation_records = [generationRecord];
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const crop = "gamedesign/projects/test/data/ui-kit-crop.json";
  writeFileSync(join(dir, crop), `${JSON.stringify({
    schema: "game.art_crop_manifest",
    version: 1,
    art_job: job,
    output_dir: "assets/runtime/ui-kit",
    sources: [{
      id: "ui-source",
      path: "gamedesign/projects/test/art/ui-source.png",
      crops: [
        {
          id: "panel",
          kind: "slice9",
          rect: [0, 0, 96, 64],
          output: "assets/runtime/ui-kit/panel.png",
          slice9: { left: 12, top: 12, right: 12, bottom: 12 },
          content: { x: 18, y: 18, w: 60, h: 32 },
          target_preview_sizes: [[160, 96], [240, 160]],
        },
        {
          id: "resource_icon",
          kind: "icon",
          rect: [96, 0, 64, 64],
          output: "assets/runtime/ui-kit/icon.png",
          semantic_role: "resource",
          size_class: "64px source",
          trim_padding: 6,
          isolate_component: "center",
        },
        {
          id: "enemy",
          kind: "sprite",
          rect: [160, 0, 64, 64],
          output: "assets/runtime/ui-kit/enemy.png",
          pivot: [32, 56],
        },
      ],
    }],
  }, null, 2)}\n`, "utf8");

  const runtime = "gamedesign/projects/test/data/ui-kit-assets.json";
  writeFileSync(join(dir, runtime), `${JSON.stringify({
    schema: "game.asset_manifest",
    version: 1,
    art_job: job,
    runtime_dir: "assets/runtime/ui-kit",
    assets: [
      {
        id: "panel",
        kind: "slice9",
        path: "assets/runtime/ui-kit/panel.png",
        slice9: { left: 12, top: 12, right: 12, bottom: 12 },
        content: { x: 18, y: 18, w: 60, h: 32 },
        target_preview_sizes: [[160, 96], [240, 160]],
      },
      {
        id: "resource_icon",
        kind: "icon",
        path: "assets/runtime/ui-kit/icon.png",
        semantic_role: "resource",
      },
      {
        id: "enemy",
        kind: "sprite",
        path: "assets/runtime/ui-kit/enemy.png",
        pivot: [32, 56],
      },
    ],
  }, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /strict-valid/);
});

test("strict mode rejects runtime manifest missing a crop asset", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const runtime = "gamedesign/projects/test/data/ui-kit-assets.json";
  const runtimeData = JSON.parse(readFileSync(join(dir, runtime), "utf8"));
  runtimeData.assets = runtimeData.assets.filter((asset) => asset.id !== "resource_icon");
  writeFileSync(join(dir, runtime), `${JSON.stringify(runtimeData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /runtime manifest missing asset for crop resource_icon/);
});

test("strict mode rejects runtime asset path that differs from crop output", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const runtime = "gamedesign/projects/test/data/ui-kit-assets.json";
  const runtimeData = JSON.parse(readFileSync(join(dir, runtime), "utf8"));
  const asset = runtimeData.assets.find((item) => item.id === "panel");
  asset.path = "assets/runtime/ui-kit/other-panel.png";
  writeFileSync(join(dir, "assets/runtime/ui-kit/other-panel.png"), "fake-png", "utf8");
  writeFileSync(join(dir, runtime), `${JSON.stringify(runtimeData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /runtime asset panel path must match crop output assets\/runtime\/ui-kit\/panel\.png/);
});

test("strict mode rejects runtime manifest pointing at another crop manifest", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const runtime = "gamedesign/projects/test/data/ui-kit-assets.json";
  const runtimeData = JSON.parse(readFileSync(join(dir, runtime), "utf8"));
  runtimeData.crop_manifest = "gamedesign/projects/test/data/other-crop.json";
  writeFileSync(join(dir, runtime), `${JSON.stringify(runtimeData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /runtime manifest crop_manifest should match expected_outputs.crop_manifest/);
});

test("draft mode rejects malformed edge proof list", (t) => {
  const dir = tempDir(t);
  const job = writeValidDraft(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.edge_proofs = "gamedesign/projects/test/art/previews/edge-proof.png";
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.edge_proofs must be an array/);
});

test("draft mode rejects malformed edge proof report list", (t) => {
  const dir = tempDir(t);
  const job = writeValidDraft(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.edge_proof_reports = "gamedesign/projects/test/reviews/edge-proof.json";
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.edge_proof_reports must be an array/);
});

test("strict mode validates listed edge proof evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const proof = "gamedesign/projects/test/art/previews/ui-edge-proof.png";
  const proofReport = "gamedesign/projects/test/reviews/ui-edge-proof.json";
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.edge_proofs = [proof];
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  let result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /strict mode edge proof missing/);

  mkdirSync(join(dir, "gamedesign/projects/test/art/previews"), { recursive: true });
  writeFileSync(join(dir, proof), "fake-png", "utf8");
  result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /strict mode edge proofs require expected_outputs.edge_proof_reports/);

  jobData.expected_outputs.edge_proof_reports = [proofReport];
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");
  result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.edge_proof_reports missing/);

  writeEdgeProofReport(dir, proofReport, proof, { schema: "wrong.schema" });
  result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.edge_proof_reports JSON schema must be game\.ui_asset_edge_proof/);

  writeEdgeProofReport(dir, proofReport, proof);
  result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /strict-valid/);
});

test("strict mode rejects accepted edge proof report with bad marks", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const proof = "gamedesign/projects/test/art/previews/ui-edge-proof.png";
  const proofReport = "gamedesign/projects/test/reviews/ui-edge-proof.json";
  mkdirSync(join(dir, "gamedesign/projects/test/art/previews"), { recursive: true });
  writeFileSync(join(dir, proof), "fake-png", "utf8");
  writeEdgeProofReport(dir, proofReport, proof, {
    counts: {
      total: 2,
      visible: 1,
      transparent_rgb: 1,
      reasons: { green_screen_spill: 2 },
    },
    rows: [
      {
        asset_id: "panel",
        kind: "slice9",
        output: "assets/runtime/ui-kit/panel.png",
        side: "right",
        rect: [90, 0, 6, 64],
        counts: {
          total: 2,
          visible: 1,
          transparent_rgb: 1,
          reasons: { green_screen_spill: 2 },
        },
      },
    ],
  });
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.edge_proofs = [proof];
  jobData.expected_outputs.edge_proof_reports = [proofReport];
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs\.edge_proof_reports JSON total bad marks must be 0 for accepted edge proof/);
});

test("strict mode rejects edge proof report for another crop manifest or image", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const proof = "gamedesign/projects/test/art/previews/ui-edge-proof.png";
  const otherProof = "gamedesign/projects/test/art/previews/other-edge-proof.png";
  const proofReport = "gamedesign/projects/test/reviews/ui-edge-proof.json";
  mkdirSync(join(dir, "gamedesign/projects/test/art/previews"), { recursive: true });
  writeFileSync(join(dir, proof), "fake-png", "utf8");
  writeFileSync(join(dir, otherProof), "fake-png", "utf8");
  writeEdgeProofReport(dir, proofReport, otherProof, {
    crop_manifest: "gamedesign/projects/test/data/other-crop.json",
  });
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.edge_proofs = [proof];
  jobData.expected_outputs.edge_proof_reports = [proofReport];
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.edge_proof_reports JSON crop_manifest must match expected_outputs.crop_manifest/);
  assert.match(result.stdout, /expected_outputs.edge_proof_reports JSON image_output must match expected_outputs.edge_proofs/);
});

test("strict mode requires generated UI asset audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  delete jobData.expected_outputs.asset_audit;
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /strict mode requires expected_outputs.asset_audit/);
});

test("strict mode rejects failing generated UI asset audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-asset-audit.json";
  writeAuditReport(dir, audit, "game.generated_ui_asset_audit", "fail", ["purple edge halo remains"]);

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.asset_audit JSON verdict\/status must be pass/);
  assert.match(result.stdout, /expected_outputs.asset_audit JSON must not list problems/);
});

test("strict mode rejects generated UI asset audit for another crop manifest", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-asset-audit.json";
  writeAuditReport(dir, audit, "game.generated_ui_asset_audit", "pass", [], {
    crop_manifest: "gamedesign/projects/test/data/other-crop.json",
  });

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.asset_audit JSON crop_manifest must match expected_outputs.crop_manifest/);
});

test("strict mode rejects generated UI asset audit missing a crop id", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-asset-audit.json";
  writeAuditReport(dir, audit, "game.generated_ui_asset_audit", "pass", [], {
    assets: [{ id: "panel" }, { id: "enemy" }],
  });

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.asset_audit JSON missing audited crop id resource_icon/);
});

test("strict mode rejects failing source sheet intake audit evidence when provided", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-source-intake-audit.json";
  writeAuditReport(dir, audit, "game.source_sheet_intake_audit", "fail", ["closest component gap 0px is below required 24px"]);

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.source_sheet_intake_audit JSON verdict\/status must be pass/);
  assert.match(result.stdout, /expected_outputs.source_sheet_intake_audit JSON must not list problems/);
});

test("strict mode rejects source sheet intake audit for another source", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-source-intake-audit.json";
  writeAuditReport(dir, audit, "game.source_sheet_intake_audit", "pass", [], {
    source: "gamedesign/projects/test/art/other-source.png",
  });

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.source_sheet_intake_audit JSON source must match expected source art or crop source/);
});

test("final-art mode requires source sheet intake audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  delete jobData.expected_outputs.source_sheet_intake_audit;
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /final-art mode requires expected_outputs.source_sheet_intake_audit/);
});

test("final-art mode requires generated source derivation audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  delete jobData.expected_outputs.source_derivation_audit;
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /final-art mode requires expected_outputs.source_derivation_audit/);
});

test("final-art mode requires slice9 design policy audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  delete jobData.expected_outputs.slice9_design_audit;
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /final-art mode requires expected_outputs.slice9_design_audit/);
});

test("final-art mode requires composition proof evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  delete jobData.expected_outputs.composition_proof;
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /final-art mode requires expected_outputs.composition_proof/);
});

test("final-art mode requires atlas metadata audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  delete jobData.expected_outputs.atlas_metadata_audit;
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /final-art mode requires expected_outputs.atlas_metadata_audit/);
});

test("final-art mode requires atlas pack evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  delete jobData.expected_outputs.atlas_pack;
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /final-art mode requires expected_outputs.atlas_pack/);
});

test("final-art mode requires atlas pack audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  delete jobData.expected_outputs.atlas_pack_audit;
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /final-art mode requires expected_outputs.atlas_pack_audit/);
});

test("final-art mode requires source family coverage audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  delete jobData.expected_outputs.source_family_coverage_audit;
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /final-art mode requires expected_outputs.source_family_coverage_audit/);
});

test("final-art mode rejects failing atlas metadata audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-atlas-metadata-audit.json";
  writeAuditReport(dir, audit, "game.atlas_metadata_audit", "fail", ["panel missing pack_group"]);

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.atlas_metadata_audit JSON verdict\/status must be pass/);
  assert.match(result.stdout, /expected_outputs.atlas_metadata_audit JSON must not list problems/);
});

test("final-art mode rejects atlas metadata audit for another runtime manifest", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-atlas-metadata-audit.json";
  writeAuditReport(dir, audit, "game.atlas_metadata_audit", "pass", [], {
    asset_manifest: "gamedesign/projects/test/data/other-assets.json",
  });

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.atlas_metadata_audit JSON asset_manifest must match expected_outputs.runtime_manifest/);
});

test("final-art mode rejects atlas pack for another runtime manifest", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const pack = "gamedesign/projects/test/data/ui-kit-atlas-pack.json";
  writeAtlasPack(dir, pack, {
    asset_manifest: "gamedesign/projects/test/data/other-assets.json",
  });

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.atlas_pack JSON asset_manifest must match expected_outputs.runtime_manifest/);
});

test("final-art mode rejects non-review atlas pack evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const pack = "gamedesign/projects/test/data/ui-kit-atlas-pack.json";
  writeAtlasPack(dir, pack, {
    purpose: undefined,
    label_overlay: false,
    atlases: [
      {
        pack_group: "ui_common",
        path: "assets/runtime/ui-kit-atlas/ui_common.png",
        size: [256, 128],
        entries: [
          { id: "panel", kind: "slice9", atlas_rect: [3, 3, 96, 64], padded_rect: [1, 1, 100, 68], extrude: 2 },
          { id: "resource_icon", kind: "icon", atlas_rect: [105, 3, 64, 64], padded_rect: [103, 1, 68, 68], extrude: 2 },
          { id: "enemy", kind: "sprite", atlas_rect: [175, 3, 64, 64], padded_rect: [173, 1, 68, 68], extrude: 2 },
        ],
      },
    ],
  });

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.atlas_pack JSON purpose must be review_validation_atlas_not_engine_runtime_pack/);
  assert.match(result.stdout, /expected_outputs.atlas_pack JSON label_overlay must be true for final-art review evidence/);
  assert.match(result.stdout, /expected_outputs.atlas_pack atlas ui_common needs labeled_preview_path for final-art review/);
});

test("final-art mode rejects missing labeled atlas preview image", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  rmSync(join(dir, "assets/runtime/ui-kit-atlas/ui_common-labeled.png"), { force: true });

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.atlas_pack atlas labeled preview missing: assets\/runtime\/ui-kit-atlas\/ui_common-labeled\.png/);
});

test("final-art mode rejects atlas pack missing a crop id", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const pack = "gamedesign/projects/test/data/ui-kit-atlas-pack.json";
  writeAtlasPack(dir, pack, {
    atlases: [
      {
        pack_group: "ui_common",
        path: "assets/runtime/ui-kit-atlas/ui_common.png",
        size: [128, 128],
        entries: [{ id: "panel", kind: "slice9", atlas_rect: [3, 3, 96, 64], padded_rect: [1, 1, 100, 68], extrude: 2 }],
      },
    ],
  });

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.atlas_pack JSON missing packed asset id resource_icon/);
});

test("final-art mode rejects failing atlas pack audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-atlas-pack-audit.json";
  writeAtlasPackAudit(dir, audit, {
    verdict: "fail",
    problems: ["panel top extrusion pixel mismatch"],
  });

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.atlas_pack_audit JSON verdict\/status must be pass/);
  assert.match(result.stdout, /expected_outputs.atlas_pack_audit JSON must not list problems/);
});

test("final-art mode rejects atlas pack audit for another pack", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-atlas-pack-audit.json";
  writeAtlasPackAudit(dir, audit, {
    atlas_pack: "gamedesign/projects/test/data/other-atlas-pack.json",
  });

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.atlas_pack_audit JSON atlas_pack must match expected_outputs.atlas_pack/);
});

test("final-art mode rejects failing source family coverage audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-source-family-coverage-audit.json";
  writeAuditReport(dir, audit, "game.source_family_coverage_audit", "fail", ["missing accepted source family: isolated icon sheet"]);

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.source_family_coverage_audit JSON verdict\/status must be pass/);
  assert.match(result.stdout, /expected_outputs.source_family_coverage_audit JSON must not list problems/);
});

test("strict mode rejects failing composition proof evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const proof = "gamedesign/projects/test/reviews/ui-kit-composition-proof.json";
  writeAuditReport(dir, proof, "game.ui_composition_proof", "fail", ["label does not fit content rect"]);

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.composition_proof JSON verdict\/status must be pass/);
  assert.match(result.stdout, /expected_outputs.composition_proof JSON item panel must not list problems/);
});

test("strict mode rejects composition proof for another runtime manifest", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const proof = "gamedesign/projects/test/reviews/ui-kit-composition-proof.json";
  writeAuditReport(dir, proof, "game.ui_composition_proof", "pass", [], {
    asset_manifest: "gamedesign/projects/test/data/other-assets.json",
  });

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.composition_proof JSON asset_manifest must match expected_outputs.runtime_manifest/);
});

test("strict mode rejects failing slice9 design policy audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-slice9-design-audit.json";
  writeAuditReport(dir, audit, "game.slice9_design_policy_audit", "fail", ["slice9 crop panel needs stretch_policy"]);

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.slice9_design_audit JSON verdict\/status must be pass/);
  assert.match(result.stdout, /expected_outputs.slice9_design_audit JSON must not list problems/);
});

test("final-art mode rejects failing generated source derivation audit evidence", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-source-derivation-audit.json";
  writeAuditReport(dir, audit, "game.generated_source_derivation_audit", "fail", ["output dimensions do not match source crop"]);

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.source_derivation_audit JSON verdict\/status must be pass/);
  assert.match(result.stdout, /expected_outputs.source_derivation_audit JSON must not list problems/);
});

test("final-art mode rejects source derivation audit for another crop manifest", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-source-derivation-audit.json";
  writeAuditReport(dir, audit, "game.generated_source_derivation_audit", "pass", [], {
    crop_manifest: "gamedesign/projects/test/data/other-crop.json",
  });

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.source_derivation_audit JSON crop_manifest must match expected_outputs.crop_manifest/);
});

test("final-art mode rejects source derivation audit missing an eligible crop id", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const audit = "gamedesign/projects/test/reviews/ui-kit-source-derivation-audit.json";
  writeAuditReport(dir, audit, "game.generated_source_derivation_audit", "pass", [], {
    assets: [{ id: "panel" }],
  });

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /expected_outputs.source_derivation_audit JSON missing audited crop id enemy/);
});

test("strict mode validates generation record prompt packet path when present", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir, {
    prompt_packet: "gamedesign/projects/test/art/prompts/missing-prompt.json",
  });

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /prompt_packet missing/);
});

test("strict mode validates generation record prompt packet schema and required fields", (t) => {
  const dir = tempDir(t);
  const promptPacket = "gamedesign/projects/test/art/prompts/ui-source-prompt.json";
  const { job } = writeStrictValidJob(dir, {
    prompt_packet: promptPacket,
  });
  writePromptPacket(dir, promptPacket, {
    schema: "wrong.schema",
    prompt: "",
    acceptance_checklist: [],
  });

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /prompt_packet schema must be game\.source_sheet_prompt_packet/);
  assert.match(result.stdout, /prompt_packet needs prompt/);
  assert.match(result.stdout, /prompt_packet needs non-empty acceptance_checklist/);
});

test("strict mode rejects prompt packet from the wrong source family", (t) => {
  const dir = tempDir(t);
  const promptPacket = "gamedesign/projects/test/art/prompts/ui-source-prompt.json";
  const { job } = writeStrictValidJob(dir, {
    prompt_packet: promptPacket,
  });
  writePromptPacket(dir, promptPacket, {
    source_family: "isolated icon sheet",
    source_family_role: "semantic icon set with gutters",
  });

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /source_family\/source_family_role does not match record source_family_role/);
});

test("strict mode accepts matching generation record prompt packet", (t) => {
  const dir = tempDir(t);
  const promptPacket = "gamedesign/projects/test/art/prompts/ui-source-prompt.json";
  const { job } = writeStrictValidJob(dir, {
    prompt_packet: promptPacket,
  });
  writePromptPacket(dir, promptPacket);

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /strict-valid/);
});

test("strict mode accepts canonical source_family with specific source_family_role", (t) => {
  const dir = tempDir(t);
  const promptPacket = "gamedesign/projects/test/art/prompts/ui-source-prompt.json";
  const { job } = writeStrictValidJob(dir, {
    source_family: "blank UI kit sheet",
    source_family_role: "accepted large modal and primary button bases",
    prompt_packet: promptPacket,
  });
  writePromptPacket(dir, promptPacket, {
    source_family: "blank UI kit sheet",
    source_family_role: "stretchable bases and blank control states",
  });

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /strict-valid/);
});

test("final-art mode validates generated source with complete provenance", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /final-art-valid/);
});

test("final-art mode rejects procedural debug records even with exception", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir);
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.generation_records.push({
    id: "procedural-panel",
    provider: "local-script",
    model_or_workflow: "procedural-debug",
    workflow_json: { type: "procedural_debug_scaffold" },
    seed: 0,
    prompt: "temporary panel geometry",
    negative_prompt: "final art claim",
    source_family_role: "temporary blank UI kit sheet geometry proof",
    accepted_source_image: "assets/runtime/ui-kit/panel.png",
    final_art_source: "procedural",
    procedural_exception: "debug scaffold only",
  });
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");
  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /is procedural debug art/);
});

test("final-art mode rejects partial generated provenance", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir, {
    workflow_path: undefined,
    workflow_json: { record_quality: "partial because original seed was not captured" },
    seed: "unknown",
  });
  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /needs a captured non-unknown seed or no_seed_reason/);
  assert.match(result.stdout, /partial\/unknown workflow provenance/);
});

test("final-art mode allows explicit no-seed reason for providers without seed", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir, {
    seed: undefined,
    no_seed_reason: "provider does not expose stable seed data",
  });
  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /final-art-valid/);
});

test("final-art mode requires explicit source kind", (t) => {
  const dir = tempDir(t);
  const { job, generationRecord } = writeStrictValidJob(dir);
  const record = JSON.parse(readFileSync(join(dir, generationRecord), "utf8"));
  delete record.final_art_source;
  writeFileSync(join(dir, generationRecord), `${JSON.stringify(record, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /needs explicit final_art_source/);
});

test("final-art mode rejects empty inline workflow for generated source", (t) => {
  const dir = tempDir(t);
  const { job } = writeStrictValidJob(dir, {
    workflow_path: undefined,
    workflow_json: {},
  });
  const result = run(["--job", job, "--final-art"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /needs non-empty workflow_json or workflow_path/);
});

test("strict mode rejects icon crops without trim and component policy", (t) => {
  const dir = tempDir(t);
  const job = writeValidDraft(dir);
  mkdirSync(join(dir, "gamedesign/projects/test/art"), { recursive: true });
  mkdirSync(join(dir, "assets/runtime/ui-kit"), { recursive: true });
  writeFileSync(join(dir, "gamedesign/projects/test/art/ui-source.png"), "fake-png", "utf8");
  writeFileSync(join(dir, "assets/runtime/ui-kit/icon.png"), "fake-png", "utf8");

  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.source_art = ["gamedesign/projects/test/art/ui-source.png"];
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");

  const crop = "gamedesign/projects/test/data/ui-kit-crop.json";
  writeFileSync(join(dir, crop), `${JSON.stringify({
    schema: "game.art_crop_manifest",
    version: 1,
    art_job: job,
    output_dir: "assets/runtime/ui-kit",
    sources: [{
      id: "ui-source",
      path: "gamedesign/projects/test/art/ui-source.png",
      crops: [{
        id: "resource_icon",
        kind: "icon",
        rect: [96, 0, 64, 64],
        output: "assets/runtime/ui-kit/icon.png",
        semantic_role: "resource",
        size_class: "64px source",
      }],
    }],
  }, null, 2)}\n`, "utf8");

  const runtime = "gamedesign/projects/test/data/ui-kit-assets.json";
  writeFileSync(join(dir, runtime), `${JSON.stringify({
    schema: "game.asset_manifest",
    version: 1,
    art_job: job,
    runtime_dir: "assets/runtime/ui-kit",
    assets: [{ id: "resource_icon", kind: "icon", path: "assets/runtime/ui-kit/icon.png", semantic_role: "resource" }],
  }, null, 2)}\n`, "utf8");

  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /icon needs trim_padding or no_trim_reason/);
  assert.match(result.stdout, /icon needs isolate_component or no_component_isolation_reason/);
});

test("strict mode rejects missing generation provenance", (t) => {
  const dir = tempDir(t);
  const job = writeValidDraft(dir);
  mkdirSync(join(dir, "gamedesign/projects/test/art"), { recursive: true });
  writeFileSync(join(dir, "gamedesign/projects/test/art/ui-source.png"), "fake-png", "utf8");
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.source_art = ["gamedesign/projects/test/art/ui-source.png"];
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");
  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /strict mode requires expected_outputs.generation_records/);
});

test("strict mode rejects missing generation record files", (t) => {
  const dir = tempDir(t);
  const job = writeValidDraft(dir);
  mkdirSync(join(dir, "gamedesign/projects/test/art"), { recursive: true });
  writeFileSync(join(dir, "gamedesign/projects/test/art/ui-source.png"), "fake-png", "utf8");
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.source_art = ["gamedesign/projects/test/art/ui-source.png"];
  jobData.expected_outputs.generation_records = ["gamedesign/projects/test/art/generation_records/missing.json"];
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");
  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /generation record .* file missing/);
});

test("strict mode rejects procedural final art without exception", (t) => {
  const dir = tempDir(t);
  const job = writeValidDraft(dir);
  mkdirSync(join(dir, "gamedesign/projects/test/art/workflows"), { recursive: true });
  writeFileSync(join(dir, "gamedesign/projects/test/art/ui-source.png"), "fake-png", "utf8");
  writeFileSync(join(dir, "gamedesign/projects/test/art/workflows/ui-source.json"), "{}", "utf8");
  const jobData = JSON.parse(readFileSync(join(dir, job), "utf8"));
  jobData.expected_outputs.source_art = ["gamedesign/projects/test/art/ui-source.png"];
  jobData.expected_outputs.generation_records = [{
    id: "ui-source",
    provider: "local-script",
    model_or_workflow: "procedural-debug",
    workflow_path: "gamedesign/projects/test/art/workflows/ui-source.json",
    seed: 0,
    prompt: "draw a panel with code",
    negative_prompt: "none",
    source_family_role: "blank UI kit sheet",
    accepted_source_image: "gamedesign/projects/test/art/ui-source.png",
    final_art_source: "procedural",
  }];
  writeFileSync(join(dir, job), `${JSON.stringify(jobData, null, 2)}\n`, "utf8");
  const result = run(["--job", job, "--strict"], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /final_art_source procedural needs procedural_exception/);
});
