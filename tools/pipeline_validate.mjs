#!/usr/bin/env node
// Validate the reusable AI pipeline base in this repo and in a fresh export.
//
//   node tools/pipeline_validate.mjs

import { existsSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));
const stamp = new Date().toISOString().replace(/[:.]/g, "-");
const exportDir = join(root, "tmp", `pipeline-validate-${stamp}`);

function run(label, args, opts = {}) {
  const { cwd = root, exe = process.execPath } = opts;
  console.log(`\n== ${label}`);
  console.log(`$ ${[exe, ...args].map((arg) => (/\s/.test(arg) ? JSON.stringify(arg) : arg)).join(" ")}`);
  const result = spawnSync(exe, args, {
    cwd,
    stdio: "inherit",
    shell: false,
  });
  if (result.error) {
    console.error(`error: ${label} failed to start: ${result.error.message}`);
    process.exit(1);
  }
  if (result.status !== 0) {
    console.error(`error: ${label} failed with exit code ${result.status}`);
    process.exit(result.status || 1);
  }
}

function findPythonRunner() {
  const candidates = [
    { exe: "py", args: ["-3.12"] },
    { exe: "python", args: [] },
    { exe: "python3", args: [] },
  ];
  for (const candidate of candidates) {
    const result = spawnSync(candidate.exe, [...candidate.args, "--version"], {
      cwd: root,
      encoding: "utf8",
      shell: false,
      stdio: "pipe",
    });
    if (result.status === 0) {
      const version = `${result.stdout || result.stderr}`.trim();
      console.log(`python runner: ${candidate.exe} ${candidate.args.join(" ")} ${version}`.replace(/\s+/g, " "));
      return candidate;
    }
  }
  console.error("error: no working Python runner found; tried py -3.12, python, and python3");
  process.exit(1);
}

if (existsSync(exportDir)) {
  rmSync(exportDir, { recursive: true, force: true });
}

run("taskboard summary", ["tools/taskboard/cli.mjs", "summary"]);
run("ai facade syntax", ["--check", "tools/ai.mjs"]);
run("ai facade tests", ["--test", "tools/ai.test.mjs"]);
run("skill eval", ["tools/skills_eval.mjs"]);
run("taskboard validate", ["tools/taskboard/cli.mjs", "validate"]);
run("taskboard tests", ["--test", "tools/taskboard/test.mjs"]);
run("ai profile tests", ["--test", "tools/ai_profile/test.mjs"]);
if (existsSync(join(root, "tools", "game_context", "test.mjs"))) {
  run("game context tests", ["--test", "tools/game_context/test.mjs"]);
}
if (existsSync(join(root, "tools", "product_gate", "test.mjs"))) {
  run("product gate tests", ["--test", "tools/product_gate/test.mjs"]);
}
if (existsSync(join(root, "tools", "assets", "new_generation_record.test.mjs"))) {
  run("generated art job node tests", [
    "--test",
    "tools/assets/plan_source_sheet_prompt.test.mjs",
    "tools/assets/plan_missing_source_family_prompts.test.mjs",
    "tools/assets/new_generation_record.test.mjs",
    "tools/assets/validate_art_job.test.mjs",
    "tools/assets/audit_slice9_design_policy.test.mjs",
    "tools/assets/audit_atlas_metadata.test.mjs",
    "tools/assets/audit_runtime_ui_asset_usage.test.mjs",
    "tools/assets/audit_source_family_coverage.test.mjs",
  ]);
}
let python = null;
if (existsSync(join(root, "tools", "assets", "normalize_source_sheet_chroma_test.py"))) {
  python = findPythonRunner();
  run("source sheet preprocessing tests", [...python.args, "-m", "unittest", "tools.assets.chroma_key_alpha_test", "tools.assets.dual_plate_alpha_test", "tools.assets.normalize_source_sheet_chroma_test", "tools.assets.audit_source_sheet_intake_test"], { exe: python.exe });
}
if (existsSync(join(root, "tools", "assets", "audit_generated_ui_assets_test.py"))) {
  python ||= findPythonRunner();
  run("generated UI asset audit tests", [...python.args, "-m", "unittest", "tools.assets.audit_generated_ui_assets_test", "tools.assets.render_ui_asset_edge_proof_test", "tools.assets.render_ui_composition_proof_test", "tools.assets.build_ui_atlas_pack_test", "tools.assets.audit_ui_atlas_pack_test"], { exe: python.exe });
}
if (existsSync(join(root, "tools", "assets", "audit_generated_source_derivation_test.py"))) {
  python ||= findPythonRunner();
  run("generated source derivation audit tests", [...python.args, "-m", "unittest", "tools.assets.audit_generated_source_derivation_test"], { exe: python.exe });
}

// Runtime seed checks. Skipped automatically in workflow-only exports, which
// have no state schema or CMake presets.
if (existsSync(join(root, "state", "game_state.schema.json"))) {
  python ||= findPythonRunner();
  run("state codegen", [...python.args, "tools/state_codegen/generate_state.py"], { exe: python.exe });
}
if (existsSync(join(root, "CMakePresets.json"))) {
  run("cmake configure", ["--preset", "native-debug"], { exe: "cmake" });
}

run("portable export", ["tools/bootstrap/export_base.mjs", "--target", exportDir]);
run("exported skill eval", ["tools/skills_eval.mjs"], { cwd: exportDir });
run("exported ai facade tests", ["--test", "tools/ai.test.mjs"], { cwd: exportDir });
run("exported taskboard validate", ["tools/taskboard/cli.mjs", "validate"], { cwd: exportDir });
run("exported taskboard tests", ["--test", "tools/taskboard/test.mjs"], { cwd: exportDir });
run("exported ai profile tests", ["--test", "tools/ai_profile/test.mjs"], { cwd: exportDir });
if (existsSync(join(exportDir, "tools", "game_context", "test.mjs"))) {
  run("exported game context tests", ["--test", "tools/game_context/test.mjs"], { cwd: exportDir });
}
if (existsSync(join(exportDir, "tools", "product_gate", "test.mjs"))) {
  run("exported product gate tests", ["--test", "tools/product_gate/test.mjs"], { cwd: exportDir });
}
if (existsSync(join(exportDir, "tools", "assets", "new_generation_record.test.mjs"))) {
  run("exported generated art job node tests", [
    "--test",
    "tools/assets/plan_source_sheet_prompt.test.mjs",
    "tools/assets/plan_missing_source_family_prompts.test.mjs",
    "tools/assets/new_generation_record.test.mjs",
    "tools/assets/validate_art_job.test.mjs",
    "tools/assets/audit_slice9_design_policy.test.mjs",
    "tools/assets/audit_atlas_metadata.test.mjs",
    "tools/assets/audit_runtime_ui_asset_usage.test.mjs",
    "tools/assets/audit_source_family_coverage.test.mjs",
  ], { cwd: exportDir });
}
if (existsSync(join(exportDir, "tools", "assets", "normalize_source_sheet_chroma_test.py"))) {
  python ||= findPythonRunner();
  run("exported source sheet preprocessing tests", [...python.args, "-m", "unittest", "tools.assets.chroma_key_alpha_test", "tools.assets.dual_plate_alpha_test", "tools.assets.normalize_source_sheet_chroma_test", "tools.assets.audit_source_sheet_intake_test"], {
    cwd: exportDir,
    exe: python.exe,
  });
}
if (existsSync(join(exportDir, "tools", "assets", "audit_generated_ui_assets_test.py"))) {
  python ||= findPythonRunner();
  run("exported generated UI asset audit tests", [...python.args, "-m", "unittest", "tools.assets.audit_generated_ui_assets_test", "tools.assets.render_ui_asset_edge_proof_test", "tools.assets.render_ui_composition_proof_test", "tools.assets.build_ui_atlas_pack_test", "tools.assets.audit_ui_atlas_pack_test"], {
    cwd: exportDir,
    exe: python.exe,
  });
}
if (existsSync(join(exportDir, "tools", "assets", "audit_generated_source_derivation_test.py"))) {
  python ||= findPythonRunner();
  run("exported generated source derivation audit tests", [...python.args, "-m", "unittest", "tools.assets.audit_generated_source_derivation_test"], {
    cwd: exportDir,
    exe: python.exe,
  });
}

console.log(`\nok: reusable pipeline validation passed`);
console.log(`export: ${exportDir}`);
