// Skill PRESENCE check: asserts each .codex/skills/*/SKILL.md (+ its references)
// still contains its required anchor strings. This is a lint for presence, NOT a
// behavioural/quality eval — the human rubric (skill-eval-playbook.md) is that.
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));
const skillRoot = join(root, ".codex", "skills");

const SKILL_CHECKS = [
  {
    name: "task-manager",
    frontmatter: [
      "capturing",
      "refining",
      "decomposing",
      "planning",
      "prioritizing",
      "reporting",
      "add a task",
      "what should we do next",
      "backlog",
      "deferred",
    ],
    body: [
      "ai_studio/taskboard/README.md",
      "ai_studio/taskboard/cli.mjs",
      "cli.mjs summary",
      "cli.mjs context",
      "tasks/STATUS.md",
      "Read full",
      "Done when",
      "## Log",
      "evidence line",
      "idea",
      "backlog",
      "status: idea",
      "reporting rules",
    ],
  },
  {
    name: "ai-pipeline-maintenance",
    frontmatter: [
      "reusable AI development pipeline",
      "reducing hot Markdown/context",
      "splitting agent docs or skills",
      "duplicated rules",
      "validators",
      "context budgets",
      "skills sync/eval",
      "portable export",
      "post-review pipeline improvements",
    ],
    body: [
      "ai_studio/README.md",
      "ai_studio/core_harness/workflow/README.md",
      "ai_studio/core_harness/orchestration/README.md",
      "docs/ai-pipeline/quality-validation.md",
      "docs/ai-pipeline/profiling-reuse.md",
      "pipeline-maintenance-playbook.md",
      "skill-placement.md",
      "git status --short --untracked-files=all",
      "ai_studio/taskboard/cli.mjs summary",
      "tools/context_budget.mjs",
      "tools/skills_eval.mjs",
      "tools/skills_sync.mjs --check",
      "ai_studio/core_harness/validation/doc_reference_check.mjs",
      "tools/product_gate/repeated_failure_guard.mjs",
      "ai_studio/taskboard/cli.mjs validate",
      "tools/pipeline_validate.mjs",
      "tools/pipeline_validate.mjs --review",
      "tools/pipeline_validate.mjs --full",
      "tools/pipeline_validate.mjs",
      "source-of-truth",
      "Mechanical Guard Pattern",
      "Validation Matrix",
      "Report Shape",
      "New Skill Triage",
      "trigger or workflow phrase",
      "What user phrase should activate it",
      "What work does the agent repeat",
      "What eval anchor proves the skill still exists",
      "task log, or project GDD",
      "Create New Skill",
      "Update Existing Skill",
      "Current Skill Ownership",
      "Pipeline cleanup, context budgets, file/skill placement",
      "Native runtime driving, screenshots, video, DevAPI command contracts",
      "Do not create a new skill for the universal AI pipeline",
      "Keep Hot Docs Thin",
    ],
  },
  {
    name: "game-runtime-automation",
    frontmatter: [
      "DevAPI",
      "command.describe",
      "ui.tree",
      "ui.click",
      "frame.wait",
      "smoke tests",
      "screenshots",
      "recordings",
      "native PC validation",
      "visual QA",
      "nonblank",
    ],
    body: [
      "endpoints",
      "command.describe",
      "native desktop/PC",
      "WASM/web",
      "screenshots",
      "recordings",
      "build/logs",
      "Do not enable automation in release builds",
      "Required command metadata",
      "method",
      "params_shape",
      "result_shape",
      "frame_behavior",
      "side_effects",
      "Visual QA",
      "nonblank output",
      "readable UI text",
      "controls respond",
    ],
  },
  {
    name: "primary-gdd-pipeline",
    frontmatter: [
      "game concept",
      "first GDD",
      "visual GDD site",
      "reference pack",
      "fake shots",
      "art bible",
      "runtime asset checklist",
      "implementation handoff",
      "core loop",
      "currencies",
      "UI",
      "game-ready art direction",
    ],
    body: [
      "Definition of Done",
      "reference",
      "fake shot",
      "runtime asset",
      "implementation plan",
      "tmp/",
      "Stop for user review",
      "player verb",
      "first-slice test",
      "living source of truth",
      "visual/runtime evidence",
      "mechanics-depth audit",
      "reference deconstruction",
      "screen grammar",
      "mismatch audit",
      "Reference Intake",
      "Definition of Ready",
      "Reference Digest",
      "Source Ladder",
      "Reference Evidence Board",
      "Parallel reference work",
      "not ready for implementation",
      "max 3",
      "3-7 refs",
      "first playable slice",
      "data/core_loop.json",
      "data/ui_flow.json",
      "data/asset_manifest.json",
      "data/combat.json",
      "playbook-map.md",
      "quality-review-playbook.md",
      "skill-eval-playbook.md",
      "design-stewardship.md",
      "Report Shape",
    ],
  },
  {
    name: "game-feature-iteration",
    frontmatter: [
      "playable game feature",
      "gameplay mechanics",
      "vertical slice",
      "build",
      "release",
      "CMake presets",
      "packaging",
    ],
    body: [
      "AGENTS.md",
      "tools/game_context/iteration_context.mjs",
      "smallest playable slice",
      "iteration-cycle-playbook",
      "CMakePresets.json",
      "Build, Launch, And Release Tasks",
      "asset-pack generation",
      "smallest affected build",
      "native desktop",
      "reference deconstruction",
      "screen grammar",
      "first-60-seconds",
      "mismatch audit",
      "Definition of Ready",
      "Reference Intake",
      "Reference Digest",
      "Source Ladder",
      "Reference Evidence Board",
      "Parallel reference work",
      "not ready for implementation",
      "product_gate/review.mjs",
      "close_slice.mjs",
      "5-line session contract",
      "mismatch list",
      "Product gate fail blocks",
      "slice_hygiene.mjs",
      "30 changed files",
      "promise push",
      "stale fail audits",
    ],
  },
  {
    name: "game-state-management",
    frontmatter: [
      "schema-first",
      "migrations",
      "DevAPI state commands",
      "fixtures",
      "JSON save/load",
    ],
    body: [
      "state/*.schema.json",
      "generate_state.py",
      "native-debug",
      "game_state_actions",
      "Do not hand-edit",
      "map<string,T>",
      "reserved",
      "Review Checklist",
      "state-contract.md",
    ],
  },
  {
    name: "game-asset-pipeline",
    frontmatter: [
      "textures",
      "atlases",
      "pack builders",
      "runtime asset loading failures",
    ],
    body: [
      "source of truth",
      "shared downloaded/free source",
      "OKF Markdown records",
      "tags/resource",
      "ai_pipeline_assets",
      "bootstrap_shared_asset_library.mjs",
      "download_source_asset.mjs",
      "accept_incoming_asset.mjs",
      "_quarantine",
      "license",
      "integrity",
      "preview_2x2",
      "seam_audit",
      "generated",
      "reproducible",
      "Fail loudly",
      "project-relative",
      "Do not assume the pack/material path is too slow",
      "smallest pack build",
      "measured failure",
      "slice9 margins",
      "new_art_job.mjs",
      "new_generation_record.mjs",
      "non-empty workflow JSON",
      "no-seed reason",
      "chroma_key_alpha.py",
      "--final-art",
      "art job",
    ],
  },
  {
    name: "game-texture-generation",
    frontmatter: [
      "standalone game material textures",
      "world surfaces",
      "ground",
      "terrain",
      "props",
      "assets",
      "armor parts",
      "armor",
      "metal",
      "plastic",
      "stylized studs",
      "marketplace/downloaded assets",
      "texture provenance",
      "Do not use for atlases",
    ],
    body: [
      "Usage Classes",
      "Tileable material",
      "Asset material",
      "Painted metal material",
      "One-off decal",
      "Material map set",
      "Procedural proof",
      "Stylized Studs Texture",
      "Make the base seamless only if it will repeat",
      "Overlay a studs grid",
      "Break or erase studs",
      "Downloaded Assets",
      "License and allowed use",
      "Tiling",
      "Scale",
      "Mips/zoom",
      "Runtime path",
      "Provenance",
      "audit_tileable_texture.py",
      "2x2 preview",
      "Model Material Textures",
      "UV assumption",
      "texture-brief-template.md",
      "Must be seamless",
      "Can ship in repository/build",
    ],
  },
  {
    name: "game-visual-art-direction",
    frontmatter: [
      "game visual direction",
      "art assets",
      "UI kits",
      "fake shots",
      "sprites",
      "generated visuals",
      "child-friendly visual polish",
      "release-quality presentation",
      "placeholder",
    ],
    body: [
      "accepted visual target",
      "reference deconstruction",
      "screen grammar",
      "mismatch audit",
      "Reference Intake",
      "art request packet",
      "art job",
      "candidate policy",
      "reusable kind",
      "must-not-bake",
      "slice9 insets",
      "runtime harness",
      "Produce visual assets before polishing placeholder render code",
      "durable project folders",
      "Inspect generated outputs",
      "runtime asset checklist",
      "primary runtime",
      "Art-First Gate",
      "Reusable UI Gate",
      "slice9-ready",
      "Do not bake labels",
      "Generate icons separately",
      "Generate border, tile, highlight, empty-slot",
      "shape-renderer rectangles",
      "debug buttons",
      "imagegen",
      "chroma-key",
      "raw generated source art",
      "Visual Review Checklist",
      "screenshot evidence path",
      "candidate batch",
      "new_art_job.mjs",
      "empty `{}` placeholders",
      "no-seed reason",
      "normalize_source_sheet_chroma.py",
      "audit_source_sheet_intake.py",
      "--final-art",
      "Definition of Ready",
      "Reference Intake",
      "Reference Digest",
      "Source Ladder",
      "Reference Evidence Board",
      "Parallel reference work",
      "product_gate/review.mjs",
      "close_slice.mjs",
      "5-line visual session contract",
      "screenshot-vs-target mismatch",
      "named mismatch",
    ],
  },
  {
    name: "generated-game-ui-assets",
    frontmatter: [
      "generating",
      "reusable game UI asset kits",
      "AI art",
      "UI source sheets",
      "slice9 panels",
      "art bibles",
      "crop manifests",
      "runtime manifests",
      "responsive UI layout audits",
      "cropped/fringed generated UI assets",
    ],
    body: [
      "game-visual-art-direction",
      "game-asset-pipeline",
      "game-runtime-automation",
      "art bible",
      "new_art_job.mjs",
      "new_generation_record.mjs",
      "non-empty",
      "no-seed-reason",
      "chroma_key_alpha.py",
      "normalize_source_sheet_chroma.py",
      "blank UI kit sheet",
      "isolated icon sheet",
      "full mockups only as visual targets",
      "baked text",
      "tight gutters",
      "slice9",
      "content",
      "target_preview_sizes",
      "trim padding",
      "component isolation",
      "border-connected key color",
      "must not redraw panels",
      "procedural shapes",
      "contact sheet",
      "validate_art_job.mjs",
      "new_generation_record.mjs",
      "--final-art",
      "audit_source_sheet_intake.py",
      "product_gate/review.mjs",
      "responsive_layout_audit.mjs",
      "Desktop and portrait are separate compositions",
      "one full-width primary action",
      "Report Shape",
      "5-line session contract",
      "runtime integration",
    ],
  },
  {
    name: "chat-session-reflection",
    frontmatter: [
      "reflecting",
      "long AI-assisted work session",
      "bottlenecks",
      "mistakes",
      "wasted time",
      "weak tool use",
      "context loss",
      "planning gaps",
      "quality risks",
      "pipeline improvement",
      "profiler/telemetry gaps",
      "analytics review",
    ],
    body: [
      "AGENTS.md",
      "ai_studio/README.md",
      "tasks/STATUS.md",
      "ai_studio/taskboard/cli.mjs summary",
      "ai_studio/taskboard/cli.mjs context",
      "git status --short",
      "high-cost context",
      "durable evidence",
      "evidence quality",
      "wall-clock coverage",
      "symptom",
      "cause",
      "faster path",
      "tool use",
      "passive profiling telemetry",
      "AI_PIPELINE_HISTORY.md",
      "tmp/session_profiles",
      "tools/ai_profile/status.mjs",
      "node tools/ai_profile/status.mjs",
      "node tools/ai_profile/status.mjs --verbose",
      "node tools/ai_profile/import_codex_session.mjs",
      "unresolved failures",
      "slowest recorded work",
      "long manual/research/review gaps",
      "stale bundles",
      "context management",
      "planning",
      "product quality",
      "agent behavior",
      "Top 10 improvements",
      "highest-leverage process changes",
      "tasks/",
    ],
  },
  {
    name: "delegated-image-generation",
    frontmatter: [
      "GENERATE real raster art",
      "fake shots",
      "icon/source sheets",
      "sprites",
      "UI art",
      "native image model",
      "Antigravity",
      "CLI",
    ],
    body: [
      "agy",
      "real raster image",
      "--dangerously-skip-permissions",
      "codex exec",
      "codex_imagegen.sh",
      "pick the PNG off disk",
      "START=$(date +%s)",
      "tmp/",
      "Do not write or run any drawing code",
      "Generate composable parts",
      "NO drop/contact shadow",
      "EMPTY frame only",
      "NO icon/content inside",
      "Dead-ends",
      "gemini CLI",
      "it works when forced",
      "GEMINI_API_KEY",
      "NODE_OPTIONS=--use-system-ca",
      "generated-game-ui-assets",
      "visual gate",
    ],
  },
  {
    name: "design-source-knowledge",
    frontmatter: [
      "game-design research sources",
      "reusable knowledge",
      "source notes",
      "articles/videos/reference packets",
      "gamedesign/knowledge",
      "research sources",
      "knowledge indexes/logs",
      "source/knowledge hygiene",
    ],
    body: [
      "AGENTS.md",
      "gamedesign/README.md",
      "gamedesign/knowledge/README.md",
      "gamedesign/sources/README.md",
      "gamedesign/knowledge/index.md",
      "reference_deconstruction.md",
      "Source Intake",
      "source quality",
      "observed",
      "secondary",
      "inferred",
      "unknown",
      "Promotion Workflow",
      "project wiki",
      "Reference-Driven Work",
      "Source Ladder",
      "current-build mismatch",
      "not ready for implementation",
      "Quality Review",
      "index/log updates",
      "Report",
    ],
  },
  {
    name: "app-tunnel",
    frontmatter: [
      "public URL",
      "cloudflared",
      "phone",
    ],
    body: [
      "serve_tunnel.mjs",
      "TUNNEL_URL",
      "ephemeral",
      "Tear down",
    ],
  },
  {
    name: "game-3d-models",
    frontmatter: [
      "glb/gltf/obj/fbx",
      "model packs",
      "debug shape-renderer",
    ],
    body: [
      "obj_to_glb.py",
      "Texture dedup",
      "map_Kd",
      "nt_mesh_renderer",
      "sponza",
    ],
  },
  {
    name: "game-asset-prep",
    frontmatter: [
      "engine-ready",
      "shared library",
      "reuse lifecycle",
    ],
    body: [
      "obj_to_glb.py",
      "find_assets",
      "promote.mjs",
      "Prepare by type",
      "split-by-material",
    ],
  },
];

function splitFrontmatter(text) {
  const match = text.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n([\s\S]*)$/);
  if (!match) {
    return { frontmatter: "", body: text };
  }
  return { frontmatter: match[1], body: match[2] };
}

function readReferenceBodies(skillDir) {
  const refsDir = join(skillDir, "references");
  if (!existsSync(refsDir)) return "";
  const bodies = [];
  const walk = (dir) => {
    for (const name of readdirSync(dir)) {
      const path = join(dir, name);
      const stat = statSync(path);
      if (stat.isDirectory()) {
        walk(path);
      } else if (name.endsWith(".md")) {
        bodies.push(readFileSync(path, "utf8"));
      }
    }
  };
  walk(refsDir);
  return bodies.join("\n");
}

function includesText(haystack, needle) {
  return haystack.toLocaleLowerCase().includes(needle.toLocaleLowerCase());
}

let failures = 0;

// Every skill in .codex/skills must have a check entry; a skill silently
// outside the presence check is how regressions slip through.
const checkedNames = new Set(SKILL_CHECKS.map((c) => c.name));
for (const name of readdirSync(skillRoot)) {
  const dir = join(skillRoot, name);
  if (!statSync(dir).isDirectory() || !existsSync(join(dir, "SKILL.md"))) continue;
  if (!checkedNames.has(name)) {
    console.error(`FAIL ${name}: skill exists but has no check entry in tools/skills_eval.mjs`);
    failures += 1;
  }
}

for (const check of SKILL_CHECKS) {
  const skillDir = join(skillRoot, check.name);
  const file = join(skillDir, "SKILL.md");
  if (!existsSync(file)) {
    console.error(`FAIL ${check.name}: missing ${file}`);
    failures += 1;
    continue;
  }

  const { frontmatter, body } = splitFrontmatter(readFileSync(file, "utf8"));
  const bodyWithReferences = `${body}\n${readReferenceBodies(skillDir)}`;
  const missingFrontmatter = check.frontmatter.filter((needle) => !includesText(frontmatter, needle));
  const missingBody = check.body.filter((needle) => !includesText(bodyWithReferences, needle));

  if (missingFrontmatter.length === 0 && missingBody.length === 0) {
    console.log(`PASS ${check.name}`);
    continue;
  }

  failures += 1;
  console.error(`FAIL ${check.name}`);
  for (const needle of missingFrontmatter) {
    console.error(`  frontmatter missing: ${needle}`);
  }
  for (const needle of missingBody) {
    console.error(`  body missing: ${needle}`);
  }
}

if (failures > 0) {
  console.error(`\nskill presence check failed: ${failures} skill(s) need attention`);
  process.exit(1);
}

console.log(`skill presence check passed: ${SKILL_CHECKS.length} skill(s) checked (anchor presence only, not behaviour)`);
