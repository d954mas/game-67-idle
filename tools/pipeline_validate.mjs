#!/usr/bin/env node
// Validate the reusable AI pipeline base.
//
//   node tools/pipeline_validate.mjs [--quick] [--full] [--dry-run]

import { existsSync, readFileSync, rmSync, readdirSync, statSync } from "node:fs";
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
  --no-prune          do not prune old tmp/pipeline-validate-* dirs

Environment:
  AI_PIPELINE_PYTHON  Python command for full Python gates; may include args,
                      for example "C:\\venv\\Scripts\\python.exe" or "uv run python"`);
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

const GENERATED_ART_JOB_NODE_TESTS = [
  "tools/assets/job/plan_source_sheet_prompt.test.mjs",
  "tools/assets/job/plan_missing_source_family_prompts.test.mjs",
  "tools/assets/job/new_generation_record.test.mjs",
  "tools/assets/job/validate_art_job.test.mjs",
  "tools/assets/job/audit_slice9_design_policy.test.mjs",
  "tools/assets/job/audit_atlas_metadata.test.mjs",
  "tools/assets/job/audit_runtime_ui_asset_usage.test.mjs",
  "tools/assets/job/audit_project_asset_boundaries.test.mjs",
  "tools/assets/job/audit_source_family_coverage.test.mjs",
];

const SOURCE_SHEET_PREPROCESSING_TESTS = [
  "tools.assets.atomic_io_test",
  "tools.assets.chroma_key_alpha_test",
  "tools.assets.cutout.dual_plate_alpha_test",
  "tools.assets.cutout.dual_plate_pair_gate_test",
  "tools.assets.cutout.key_matte_test",
  "tools.assets.cutout.route_cutout_test",
  "tools.assets.intake.normalize_source_sheet_chroma_test",
  "tools.assets.intake.audit_source_sheet_intake_test",
];

const GENERATED_UI_ASSET_AUDIT_TESTS = [
  "tools.assets.audit.render_ui_composition_proof_test",
  "tools.assets.pack.build_ui_atlas_pack_test",
  "tools.assets.pack.audit_ui_atlas_pack_test",
  "tools.assets.crop.plan_runtime_crops_from_intake_test",
  "tools.assets.assemble.build_runtime_assets_from_crop_plan_test",
];

const GENERATED_SOURCE_DERIVATION_TESTS = [
  "tools.assets.audit.audit_generated_source_derivation_test",
];

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

function runNodeTests(label, tests, opts = {}) {
  run(label, ["--test", ...tests], opts);
}

function runPythonUnittests(label, python, tests, opts = {}) {
  run(label, [...python.args, "-m", "unittest", ...tests], { ...opts, exe: python.exe });
}

function splitCommandLine(command) {
  const parts = [];
  let current = "";
  let quote = "";
  let escaping = false;
  for (const char of command.trim()) {
    if (escaping) {
      current += char;
      escaping = false;
      continue;
    }
    if (char === "\\") {
      escaping = true;
      continue;
    }
    if (quote) {
      if (char === quote) quote = "";
      else current += char;
      continue;
    }
    if (char === '"' || char === "'") {
      quote = char;
      continue;
    }
    if (/\s/.test(char)) {
      if (current) {
        parts.push(current);
        current = "";
      }
      continue;
    }
    current += char;
  }
  if (escaping) current += "\\";
  if (current) parts.push(current);
  return parts;
}

function configuredPythonCandidate() {
  const configured = process.env.AI_PIPELINE_PYTHON;
  if (!configured) return null;
  const parts = splitCommandLine(configured);
  if (parts.length === 0) return null;
  return { exe: parts[0], args: parts.slice(1), source: "AI_PIPELINE_PYTHON" };
}

function findPythonRunner(requiredModules = []) {
  const configured = configuredPythonCandidate();
  if (dryRun) {
    if (configured) {
      console.log(`python runner: <dry-run> ${configured.exe} ${configured.args.join(" ")}`.replace(/\s+/g, " "));
      return configured;
    }
    console.log("python runner: <dry-run>");
    return { exe: "python", args: [] };
  }
  const candidates = [];
  if (configured) candidates.push(configured);
  if (process.platform === "win32") {
    candidates.push({ exe: "C:\\Python312\\python.exe", args: [] });
  }
  candidates.push(
    { exe: "py", args: ["-3.12"] },
    { exe: "python", args: [] },
    { exe: "python3", args: [] },
  );
  const probe = [
    "import sys",
    ...requiredModules.map((name) => `import ${name}`),
    "print(sys.version.split()[0])",
  ].join("; ");
  const skipped = [];
  for (const candidate of candidates) {
    if (candidate.exe.includes("\\") && !existsSync(candidate.exe)) {
      skipped.push(`${candidate.exe} (missing)`);
      continue;
    }
    const result = spawnSync(candidate.exe, [...candidate.args, "-c", probe], {
      cwd: root,
      encoding: "utf8",
      shell: false,
      stdio: "pipe",
    });
    if (result.status === 0) {
      const version = `${result.stdout || result.stderr}`.trim();
      const modules = requiredModules.length ? ` with ${requiredModules.join(",")}` : "";
      console.log(`python runner: ${candidate.exe} ${candidate.args.join(" ")} ${version}${modules}`.replace(/\s+/g, " "));
      return candidate;
    }
    const output = `${result.stdout || ""}${result.stderr || ""}`.trim().split(/\r?\n/).pop() || `exit ${result.status}`;
    skipped.push(`${candidate.exe} ${candidate.args.join(" ")} (${output})`.trim());
  }
  const modules = requiredModules.length ? ` with required modules: ${requiredModules.join(", ")}` : "";
  console.error(`error: no working Python runner found${modules}`);
  for (const item of skipped) console.error(`- tried ${item}`);
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
run("skills sync tests", ["--test", "tools/skills_sync.test.mjs"]);
run("context budget report", ["tools/context_budget.mjs"]);
run("doc reference check", ["tools/doc_reference_check.mjs"]);
run("pipeline validation tests", ["--test", "tools/pipeline_validate.test.mjs"]);
run("context budget tests", ["--test", "tools/context_budget.test.mjs"]);
run("doc reference tests", ["--test", "tools/doc_reference_check.test.mjs"]);
run("bootstrap export tests", ["--test", "tools/bootstrap/export_base.test.mjs"]);
run("repeated product gate failure guard", ["tools/product_gate/repeated_failure_guard.mjs"]);
run("taskboard validate", ["tools/taskboard/cli.mjs", "validate"]);

// Guard: catch a STATUS<->runtime contradiction. A "clean seed" repo must not
// hide a live game in src/clean_seed_main.c (the exact blocker that let a 1676-
// line Mine Cards game build while STATUS said "no active game"). Fast; runs always.
console.log("\n== status/runtime contradiction guard");
{
  const statusPath = join(root, "tasks", "STATUS.md");
  const seedPath = join(root, "src", "clean_seed_main.c");
  const SEED_MAX_LINES = 600; // the clean seed is ~362 lines; a real game is 1000+
  if (existsSync(statusPath) && existsSync(seedPath)) {
    const status = readFileSync(statusPath, "utf8");
    const seedLines = readFileSync(seedPath, "utf8").split(/\r?\n/).length;
    if (/no active game concept/i.test(status) && seedLines > SEED_MAX_LINES) {
      console.error(
        `error: STATUS.md says "no active game concept" but src/clean_seed_main.c is ${seedLines} lines — that is a game, not a clean seed. Reset the runtime (node tools/bootstrap/reset_to_seed.mjs) or correct STATUS.md.`
      );
      process.exit(1);
    }
    console.log("ok: STATUS and runtime are consistent");
  } else {
    console.log("ok: no STATUS/seed to compare");
  }
}
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
if (existsSync(join(root, "tools", "assets", "job", "new_generation_record.test.mjs"))) {
  runNodeTests("generated art job node tests", GENERATED_ART_JOB_NODE_TESTS);
}
let python = null;
if (existsSync(join(root, "tools", "assets", "intake", "normalize_source_sheet_chroma_test.py"))) {
  python = findPythonRunner(["PIL", "numpy", "scipy", "pymatting"]);
  runPythonUnittests("source sheet preprocessing tests", python, SOURCE_SHEET_PREPROCESSING_TESTS);
}
if (existsSync(join(root, "tools", "assets", "audit", "render_ui_composition_proof_test.py"))) {
  python ||= findPythonRunner();
  runPythonUnittests("generated UI asset audit tests", python, GENERATED_UI_ASSET_AUDIT_TESTS);
}
if (existsSync(join(root, "tools", "assets", "audit", "audit_generated_source_derivation_test.py"))) {
  python ||= findPythonRunner();
  runPythonUnittests("generated source derivation audit tests", python, GENERATED_SOURCE_DERIVATION_TESTS);
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
run("exported doc reference check", ["tools/doc_reference_check.mjs"], { cwd: exportDir });
run("exported taskboard validate", ["tools/taskboard/cli.mjs", "validate"], { cwd: exportDir });

if (!args.includes("--reexport-tests")) {
  console.log(`\nskipped the in-export test battery (suites already ran in-repo); pass --reexport-tests to re-run them in the export`);
  console.log(`\nok: reusable pipeline validation passed`);
  console.log(`export: ${exportDir}`);
  process.exit(0);
}

run("exported ai facade tests", ["--test", "tools/ai.test.mjs"], { cwd: exportDir });
run("exported doc reference tests", ["--test", "tools/doc_reference_check.test.mjs"], { cwd: exportDir });
run("exported bootstrap export tests", ["--test", "tools/bootstrap/export_base.test.mjs"], { cwd: exportDir });
run("exported taskboard tests", ["--test", "tools/taskboard/test.mjs"], { cwd: exportDir });
run("exported ai profile tests", ["--test", "tools/ai_profile/test.mjs"], { cwd: exportDir });
if (existsSync(join(exportDir, "tools", "game_context", "test.mjs"))) {
  run("exported game context tests", ["--test", "tools/game_context/test.mjs"], { cwd: exportDir });
}
if (existsSync(join(exportDir, "tools", "product_gate", "test.mjs"))) {
  run("exported product gate tests", ["--test", "tools/product_gate/test.mjs"], { cwd: exportDir });
}
if (existsSync(join(exportDir, "tools", "assets", "job", "new_generation_record.test.mjs"))) {
  runNodeTests("exported generated art job node tests", GENERATED_ART_JOB_NODE_TESTS, { cwd: exportDir });
}
if (existsSync(join(exportDir, "tools", "assets", "intake", "normalize_source_sheet_chroma_test.py"))) {
  python ||= findPythonRunner();
  runPythonUnittests("exported source sheet preprocessing tests", python, SOURCE_SHEET_PREPROCESSING_TESTS, {
    cwd: exportDir,
  });
}
if (existsSync(join(exportDir, "tools", "assets", "audit", "render_ui_composition_proof_test.py"))) {
  python ||= findPythonRunner();
  runPythonUnittests("exported generated UI asset audit tests", python, GENERATED_UI_ASSET_AUDIT_TESTS, {
    cwd: exportDir,
  });
}
if (existsSync(join(exportDir, "tools", "assets", "audit", "audit_generated_source_derivation_test.py"))) {
  python ||= findPythonRunner();
  runPythonUnittests("exported generated source derivation audit tests", python, GENERATED_SOURCE_DERIVATION_TESTS, {
    cwd: exportDir,
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
