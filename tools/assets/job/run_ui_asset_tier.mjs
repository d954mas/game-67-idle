#!/usr/bin/env node
// Tiered UI-asset gate orchestrator.
//
// A normal UI iteration should not walk the whole final-art battery. This
// script prints (or, in --execute mode, would run) the exact command sequence
// for one tier so the lead/agent sees a single ordered plan instead of
// hand-assembling ~14 CLI gates from the skill text.
//
// Tiers (see .codex/skills/generated-game-ui-assets/SKILL.md "Gate Tiers"):
//   draft     every iteration, cheap   -> intake/normalize + contact sheet     (<=2 cmds)
//   integrate when wiring into a screen -> strict validate + composition proof  (~2 cmds)
//   final     only when shipping a kit  -> the full battery incl. --final-art
//
// --plan / --dry-run prints the command sequence for the tier and exits 0
// without touching any image tool, so the orchestrator is testable without
// real PNG fixtures. --execute is intentionally not implemented yet (it would
// need real image fixtures to be meaningful); use --plan and run the printed
// commands, or run the per-tier commands documented in the skill.

function parseArgs(argv) {
  const out = { tier: undefined, plan: false, execute: false, help: false, vars: {} };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === "--tier") out.tier = argv[++i];
    else if (arg === "--plan" || arg === "--dry-run") out.plan = true;
    else if (arg === "--execute") out.execute = true;
    else if (arg === "--help" || arg === "-h") out.help = true;
    else if (arg === "--job") out.vars.job = argv[++i];
    else if (arg === "--source-sheet") out.vars.sourceSheet = argv[++i];
    else if (arg === "--crop-manifest") out.vars.cropManifest = argv[++i];
    else if (arg === "--runtime-manifest") out.vars.runtimeManifest = argv[++i];
    else if (arg === "--name") out.vars.name = argv[++i];
    else fail(`unknown argument: ${arg}`);
  }
  return out;
}

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

function usage() {
  console.log(`usage: node tools/assets/job/run_ui_asset_tier.mjs --tier draft|integrate|final --plan \\
  [--job <art-job.json>] [--source-sheet <sheet.png>] \\
  [--crop-manifest <crop.json>] [--runtime-manifest <runtime.json>] [--name <label>]

Tiers:
  draft      every iteration (cheap, <=2 commands): see the asset in context.
  integrate  when wiring into the runtime screen (~2 commands): strict validate,
             composition proof.
  final      only when shipping a reusable kit: the full final-art battery.

Modes:
  --plan / --dry-run   print the exact ordered command sequence for the tier and
                       exit (no image tools run; safe and testable).
  --execute            not implemented; run the planned commands directly.`);
}

const TIERS = new Set(["draft", "integrate", "final"]);

// Placeholder tokens use <angle-bracket> form so an un-supplied path is obvious
// in the printed plan and never silently runs against a wrong file.
function v(vars, key, fallback) {
  return vars[key] && String(vars[key]).length > 0 ? vars[key] : fallback;
}

// Returns the ordered command list for a tier. Each entry is one CLI gate.
function planFor(tier, vars) {
  const job = v(vars, "job", "<art-job.json>");
  const sheet = v(vars, "sourceSheet", "<source-sheet.png>");
  const crop = v(vars, "cropManifest", "<crop-manifest.json>");
  const runtime = v(vars, "runtimeManifest", "<runtime-manifest.json>");

  if (tier === "draft") {
    return [
      `py -3.12 tools/assets/intake/audit_source_sheet_intake.py --source ${sheet} --json-output tmp/intake.json --report tmp/intake.md`,
      `py -3.12 tools/assets/assemble/build_runtime_assets_from_crop_plan.py --crop-plan <crop-plan.json> --crop-manifest ${crop} --asset-manifest ${runtime} --art-job ${job} --contact-sheet tmp/contact.png`,
    ];
  }

  if (tier === "integrate") {
    return [
      `node tools/assets/job/validate_art_job.mjs --job ${job} --strict`,
      `py -3.12 tools/assets/audit/render_ui_composition_proof.py --asset-manifest ${runtime} --output tmp/proof.png --json-output tmp/proof.json --report tmp/proof.md`,
    ];
  }

  // final: the full reusable-kit battery.
  return [
    `py -3.12 tools/assets/intake/audit_source_sheet_intake.py --source ${sheet} --json-output tmp/intake.json --report tmp/intake.md`,
    `node tools/assets/job/validate_art_job.mjs --job ${job} --strict`,
    `node tools/assets/job/audit_slice9_design_policy.mjs --crop-manifest ${crop} --runtime-manifest ${runtime} --json-output tmp/slice9.json --report tmp/slice9.md`,
    `py -3.12 tools/assets/audit/render_ui_composition_proof.py --asset-manifest ${runtime} --output tmp/proof.png --json-output tmp/proof.json --report tmp/proof.md`,
    `node tools/assets/job/audit_atlas_metadata.mjs --asset-manifest ${runtime} --json-output tmp/atlas-meta.json --report tmp/atlas-meta.md`,
    `py -3.12 tools/assets/pack/build_ui_atlas_pack.py --asset-manifest ${runtime} --output-dir tmp/review-atlas --json-output tmp/atlas-pack.json --report tmp/atlas-pack.md --label-review`,
    `py -3.12 tools/assets/pack/audit_ui_atlas_pack.py --atlas-pack tmp/atlas-pack.json --asset-manifest ${runtime} --json-output tmp/atlas-audit.json --report tmp/atlas-audit.md`,
    `node tools/assets/job/audit_source_family_coverage.mjs --job ${job} --json-output tmp/family-coverage.json --report tmp/family-coverage.md`,
    `py -3.12 tools/assets/audit/audit_generated_source_derivation.py --crop-manifest ${crop} --json-output tmp/derivation.json --report tmp/derivation.md`,
    `node tools/assets/job/audit_runtime_ui_asset_usage.mjs --asset-manifest ${runtime} --usage <runtime-usage.json> --json-output tmp/usage.json --report tmp/usage.md`,
    `node tools/assets/job/validate_art_job.mjs --job ${job} --final-art`,
  ];
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) {
    usage();
    return;
  }
  if (!args.tier) fail("--tier draft|integrate|final is required");
  if (!TIERS.has(args.tier)) fail(`unknown tier: ${args.tier} (expected draft|integrate|final)`);
  if (args.execute) {
    fail("--execute is not implemented; run with --plan and execute the printed commands, or run the per-tier commands documented in the skill");
  }
  if (!args.plan) {
    fail("only --plan / --dry-run is supported; pass --plan to print the command sequence");
  }

  const commands = planFor(args.tier, args.vars);
  console.log(`tier: ${args.tier}`);
  console.log(`commands: ${commands.length}`);
  commands.forEach((cmd, i) => {
    console.log(`${i + 1}. ${cmd}`);
  });
  if (args.tier !== "final") {
    console.log("note: proof PNGs render only on failure at this tier; the full final-art battery is opt-in (--tier final).");
  }
}

main();
