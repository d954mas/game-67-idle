#!/usr/bin/env node
// Validate the reusable AI pipeline base.
//
//   node tools/pipeline_validate.mjs [--quick] [--full] [--dry-run]

import { existsSync, rmSync, readdirSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));
const stamp = new Date().toISOString().replace(/[:.]/g, "-");
const exportDir = join(root, "tmp", `pipeline-validate-${stamp}`);
const args = process.argv.slice(2);

function usage() {
  console.error(`usage:
  node tools/pipeline_validate.mjs [--quick] [--full] [--dry-run] [--reexport-tests] [--keep-exports <n>] [--no-prune]

Modes:
  --quick    core workflow validation only (default; use this after narrow edits)
  --full     quick checks plus deep asset/runtime validation + a minimal export
             self-check (reserve for portable-base/export/runtime/release gates)
  --dry-run  print the selected commands without running them

Export depth (with --full):
  --reexport-tests  also re-run the full test battery inside the export. Default
                    skips it (the suites already ran in-repo this invocation;
                    the export + skill eval + taskboard validate already prove
                    the copy is runnable). Use after export-tooling changes.

Housekeeping:
  --keep-exports <n>  keep only the newest n tmp/pipeline-validate-* dirs (default 3)
  --no-prune          do not prune old tmp/pipeline-validate-* dirs`);
  process.exit(2);
}

// Pull out the optional --keep-exports <n> value before the unknown-arg check.
let keepExports = 3;
{
  const idx = args.indexOf("--keep-exports");
  if (idx !== -1) {
    const value = Number.parseInt(args[idx + 1], 10);
    if (!Number.isInteger(value) || value < 0) usage();
    keepExports = value;
    args.splice(idx, 2);
  }
}
const prune = !args.includes("--no-prune");

const allowedArgs = new Set(["--quick", "--full", "--dry-run", "--reexport-tests", "--no-prune", "--help", "-h"]);
for (const arg of args) {
  if (!allowedArgs.has(arg)) usage();
}
if (args.includes("--help") || args.includes("-h")) usage();
if (args.includes("--quick") && args.includes("--full")) usage();

const fullMode = args.includes("--full");
const mode = fullMode ? "full" : "quick";
const dryRun = args.includes("--dry-run");

function run(label, args, opts = {}) {
  const { cwd = root, exe = process.execPath } = opts;
  console.log(`\n== ${label}`);
  console.log(`$ ${[exe, ...args].map((arg) => (/\s/.test(arg) ? JSON.stringify(arg) : arg)).join(" ")}`);
  if (dryRun) return;
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
  if (dryRun) {
    console.log("python runner: <dry-run>");
    return { exe: "python", args: [] };
  }
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

// Full mode copies the repo into tmp/pipeline-validate-<stamp>/. Left unchecked
// these accumulate (observed: 126 dirs / 362MB). Prune to the newest N.
function pruneOldExports(keep) {
  const tmpDir = join(root, "tmp");
  if (!existsSync(tmpDir)) return;
  let dirs = [];
  try {
    dirs = readdirSync(tmpDir)
      .filter((name) => name.startsWith("pipeline-validate-"))
      .map((name) => join(tmpDir, name))
      .filter((path) => {
        try {
          return statSync(path).isDirectory();
        } catch {
          return false;
        }
      })
      .sort(); // names are ISO timestamps, so lexical sort == chronological
  } catch {
    return;
  }
  const stale = dirs.slice(0, Math.max(0, dirs.length - keep));
  for (const dir of stale) {
    rmSync(dir, { recursive: true, force: true });
  }
  if (stale.length > 0) {
    console.log(`pruned ${stale.length} old tmp/pipeline-validate-* dir(s); kept newest ${keep}`);
  }
}

console.log(`mode: ${mode}${dryRun ? " (dry-run)" : ""}`);

if (prune && !dryRun) {
  pruneOldExports(keepExports);
}

if (fullMode && existsSync(exportDir)) {
  rmSync(exportDir, { recursive: true, force: true });
}

// Quick core workflow checks. These are safe as the default validation path
// after narrow pipeline/tooling edits.
run("taskboard summary", ["tools/taskboard/cli.mjs", "summary"]);
run("ai facade syntax", ["--check", "tools/ai.mjs"]);
run("ai facade tests", ["--test", "tools/ai.test.mjs"]);
run("skill eval", ["tools/skills_eval.mjs"]);
run("skills sync check", ["tools/skills_sync.mjs", "--check"]);
run("pipeline validation tests", ["--test", "tools/pipeline_validate.test.mjs"]);
run("taskboard validate", ["tools/taskboard/cli.mjs", "validate"]);
run("taskboard tests", ["--test", "tools/taskboard/test.mjs"]);
run("ai profile tests", ["--test", "tools/ai_profile/test.mjs"]);
if (existsSync(join(root, "tools", "game_context", "test.mjs"))) {
  run("game context tests", ["--test", "tools/game_context/test.mjs"]);
}
if (existsSync(join(root, "tools", "product_gate", "test.mjs"))) {
  run("product gate tests", ["--test", "tools/product_gate/test.mjs"]);
}

if (!fullMode) {
  console.log(`\nok: reusable pipeline quick validation passed`);
  console.log(`hint: run node tools/pipeline_validate.mjs --full for portable export/runtime/deep asset gates`);
  process.exit(0);
}

// Full validation is intentionally explicit: it repeats relevant checks in a
// fresh export and includes heavy asset/runtime validation.
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
    "tools/assets/audit_project_asset_boundaries.test.mjs",
    "tools/assets/audit_source_family_coverage.test.mjs",
  ]);
}
let python = null;
if (existsSync(join(root, "tools", "assets", "normalize_source_sheet_chroma_test.py"))) {
  python = findPythonRunner();
  run("source sheet preprocessing tests", [...python.args, "-m", "unittest", "tools.assets.atomic_io_test", "tools.assets.chroma_key_alpha_test", "tools.assets.dual_plate_alpha_test", "tools.assets.normalize_source_sheet_chroma_test", "tools.assets.audit_source_sheet_intake_test"], { exe: python.exe });
}
if (existsSync(join(root, "tools", "assets", "audit_generated_ui_assets_test.py"))) {
  python ||= findPythonRunner();
  run("generated UI asset audit tests", [...python.args, "-m", "unittest", "tools.assets.audit_generated_ui_assets_test", "tools.assets.render_ui_asset_edge_proof_test", "tools.assets.render_ui_composition_proof_test", "tools.assets.build_ui_atlas_pack_test", "tools.assets.audit_ui_atlas_pack_test", "tools.assets.plan_runtime_crops_from_intake_test", "tools.assets.build_runtime_assets_from_crop_plan_test"], { exe: python.exe });
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
  run("state codegen variant tests", [...python.args, "-m", "unittest", "tools.state_codegen.generate_state_test"], { exe: python.exe });
}
if (existsSync(join(root, "CMakePresets.json"))) {
  run("cmake configure", ["--preset", "native-debug"], { exe: "cmake" });
}

// Minimal export self-check (always): proves the copy/allowlist produced a
// runnable project. The full test battery below already ran in-repo this same
// invocation, so it is re-run in the export only with --reexport-tests.
run("portable export", ["tools/bootstrap/export_base.mjs", "--target", exportDir]);
run("exported skill eval", ["tools/skills_eval.mjs"], { cwd: exportDir });
run("exported taskboard validate", ["tools/taskboard/cli.mjs", "validate"], { cwd: exportDir });

if (!args.includes("--reexport-tests")) {
  console.log(`\nskipped the in-export test battery (suites already ran in-repo); pass --reexport-tests to re-run them in the export`);
  console.log(`\nok: reusable pipeline validation passed`);
  console.log(`export: ${exportDir}`);
  process.exit(0);
}

run("exported ai facade tests", ["--test", "tools/ai.test.mjs"], { cwd: exportDir });
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
    "tools/assets/audit_project_asset_boundaries.test.mjs",
    "tools/assets/audit_source_family_coverage.test.mjs",
  ], { cwd: exportDir });
}
if (existsSync(join(exportDir, "tools", "assets", "normalize_source_sheet_chroma_test.py"))) {
  python ||= findPythonRunner();
  run("exported source sheet preprocessing tests", [...python.args, "-m", "unittest", "tools.assets.atomic_io_test", "tools.assets.chroma_key_alpha_test", "tools.assets.dual_plate_alpha_test", "tools.assets.normalize_source_sheet_chroma_test", "tools.assets.audit_source_sheet_intake_test"], {
    cwd: exportDir,
    exe: python.exe,
  });
}
if (existsSync(join(exportDir, "tools", "assets", "audit_generated_ui_assets_test.py"))) {
  python ||= findPythonRunner();
  run("exported generated UI asset audit tests", [...python.args, "-m", "unittest", "tools.assets.audit_generated_ui_assets_test", "tools.assets.render_ui_asset_edge_proof_test", "tools.assets.render_ui_composition_proof_test", "tools.assets.build_ui_atlas_pack_test", "tools.assets.audit_ui_atlas_pack_test", "tools.assets.plan_runtime_crops_from_intake_test", "tools.assets.build_runtime_assets_from_crop_plan_test"], {
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
if (existsSync(join(exportDir, "state", "game_state.schema.json"))) {
  python ||= findPythonRunner();
  run("exported state codegen", [...python.args, "tools/state_codegen/generate_state.py"], {
    cwd: exportDir,
    exe: python.exe,
  });
  run("exported state codegen variant tests", [...python.args, "-m", "unittest", "tools.state_codegen.generate_state_test"], {
    cwd: exportDir,
    exe: python.exe,
  });
}

console.log(`\nok: reusable pipeline validation passed`);
console.log(`export: ${exportDir}`);
