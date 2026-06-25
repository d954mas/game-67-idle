#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";

const repoRoot = process.cwd();
const refactorOutPath = "docs/ai-pipeline/architecture-map.html";
const fullOutPath = "docs/ai-pipeline/architecture-map-full.html";
const studioTreePath = "ai_studio/tree.json";
const outDir = path.posix.dirname(refactorOutPath);
const graphWidth = 1480;
const graphHeight = 900;

const modules = [
  { id: "hot", title: "Hot Routing Docs", kind: "map", color: "#2563eb", x: 190, y: 110, description: "Short always-nearby docs: repo policy, workflow map, and current routing." },
  { id: "skills", title: "Skill Layer", kind: "procedure", color: "#0f766e", x: 515, y: 95, description: "Reusable procedures loaded by trigger; each skill owns one repeated work type." },
  { id: "tasks", title: "Task & Orchestration", kind: "state", color: "#6d28d9", x: 850, y: 120, description: "Durable work state, packet presets, task lifecycle, and handoff contracts." },
  { id: "facade", title: "Agent CLI Facades", kind: "public api", color: "#b45309", x: 1180, y: 155, description: "Stable command surfaces agents should call before touching internals." },
  { id: "validation", title: "Validation & Gates", kind: "guard", color: "#b91c1c", x: 1290, y: 470, description: "Mechanical gates, validation routing, product/readability checks, and failure stops." },
  { id: "design", title: "Design Knowledge", kind: "knowledge", color: "#15803d", x: 170, y: 430, description: "Reusable game design knowledge, source notes, GDD/project wiki, and reference grounding." },
  { id: "assets", title: "Asset Pipeline", kind: "factory", color: "#0891b2", x: 480, y: 390, description: "Source-first asset search, provenance, licenses, catalog records, and project-local copies." },
  { id: "art", title: "Generated Art/UI Factory", kind: "factory", color: "#c026d3", x: 735, y: 455, description: "Prompt packets, source sheets, cutouts, crop plans, UI atlases, and generated art records." },
  { id: "runtime", title: "Template & Game Runtime", kind: "runtime", color: "#1d4ed8", x: 980, y: 560, description: "Game starter template, per-game runtime structure, DevAPI automation, and state codegen." },
  { id: "engine", title: "Engine Boundary", kind: "external", color: "#334155", x: 1280, y: 760, description: "External Neotolis engine boundary; use public APIs before custom runtime code." },
  { id: "profile", title: "Profiler & Feedback", kind: "telemetry", color: "#ca8a04", x: 520, y: 760, description: "Passive profiler records repeated friction and promotes durable lessons into tools/docs/tasks." },
  { id: "export", title: "Export & Harness Sync", kind: "distribution", color: "#16a34a", x: 900, y: 790, description: "Portable pipeline export plus generated Codex/Claude skills and hook surfaces." }
];

const moduleEdges = [
  ["hot", "skills", "routes", "Hot docs route work to one matching skill instead of embedding procedure."],
  ["skills", "tasks", "loads state", "Skills decide when taskboard context and evidence are needed."],
  ["tasks", "facade", "bounds commands", "Task scope and packets constrain what tools should do."],
  ["facade", "validation", "proves output", "Public commands return compact output and route to gates."],
  ["design", "runtime", "specifies slice", "GDD/core loop/art direction define the runtime slice."],
  ["design", "assets", "requests assets", "Design creates asset needs with source-first constraints."],
  ["assets", "art", "fallback generate", "Generation is a fallback after reusable/source assets fail."],
  ["assets", "runtime", "copies local", "Runtime uses project-local copies, not shared library paths."],
  ["art", "runtime", "feeds runtime art", "Atlases/crops/manifests become game-local runtime assets."],
  ["runtime", "engine", "uses API", "Game code uses engine public APIs before custom code."],
  ["runtime", "validation", "evidence", "Playable slices produce screenshots, smokes, and gate artifacts."],
  ["validation", "profile", "friction", "Repeated gate/tool failures become improvement signals."],
  ["profile", "hot", "promotes", "Durable lessons go back into maps, skills, validators, or tasks."],
  ["facade", "export", "portable surface", "Only stable public surfaces belong in export allowlists."],
  ["export", "skills", "syncs", "Codex skills are canonical; Claude skills are generated pointers."]
];

const explicitDocPaths = [
  "README.md",
  "AGENTS.md",
  "CLAUDE.md",
  "ai_studio/README.md",
  "ai_studio/core_harness/README.md",
  "tools/README.md",
  "tools/bootstrap/TEMPLATE.md",
  "docs/ai-pipeline/architecture-map.md",
  "docs/ai-pipeline/agent-workflow.md",
  "docs/ai-pipeline/quality-validation.md",
  "docs/ai-pipeline/profiling-reuse.md",
  "docs/ai-pipeline/subagent-protocol.md",
  "docs/ai-pipeline/orchestration-playbook.md",
  "ai_studio/taskboard/README.md",
  "tasks/STATUS.md",
  "ai_studio/taskboard/task-store-reference.md",
  "gamedesign/README.md",
  "gamedesign/projects/README.md",
  "gamedesign/sources/README.md",
  "gamedesign/knowledge/README.md",
  "gamedesign/knowledge/index.md",
  "template/README.md",
  "template/CONVENTIONS.md",
  "external/neotolis-engine/README.md",
  "external/neotolis-engine/AGENTS.md",
  "external/neotolis-engine/CLAUDE.md",
  "external/neotolis-engine/docs/neotolis_engine_spec_1.md"
];

const exactToolNotes = new Map([
  ["tools/pipeline_validate.mjs", "Validation orchestrator for reusable pipeline checks, review/full modes, export checks, docs, skills, and gates."],
  ["tools/context_budget.mjs", "Checks hot docs and skill entrypoints against context-budget limits."],
  ["tools/doc_reference_check.mjs", "Finds stale local references, retired commands, and broken Markdown/document links."],
  ["tools/skills_eval.mjs", "Audits skill entrypoints, trigger shape, references, and duplicated procedure risk."],
  ["tools/sync.mjs", "Runs cross-harness sync for generated skills and hooks."],
  ["tools/skills_sync.mjs", "Generates Claude-facing skill pointers from canonical Codex skills."],
  ["tools/hooks_sync.mjs", "Generates Codex/Claude hook config from one shared source."],
  ["ai_studio/taskboard/cli.mjs", "Taskboard facade for task context, CRUD, validation, epics, and subagent packet templates."],
  ["ai_studio/taskboard/server.mjs", "Local taskboard web UI server."],
  ["tools/bootstrap/new_game.mjs", "Copies template/ into a new game folder."],
  ["tools/bootstrap/export_base.mjs", "Exports the portable AI pipeline base into another project."],
  ["tools/game_context/new_prototype.mjs", "Creates wiki/GDD/task/status scaffolding for a new prototype concept."],
  ["tools/game_context/iteration_context.mjs", "Builds compact current-game iteration context from wiki, tasks, and gate state."],
  ["tools/game_context/workflow_guard.mjs", "Blocks active-game expansion under unresolved lead rejection or missing reference readiness."],
  ["tools/product_gate/review.mjs", "Records product/readability gate reviews for screenshots and current game state."],
  ["tools/product_gate/visual_rejection_lock.mjs", "Turns lead visual rejection into a strict failure stop and next-path record."],
  ["tools/product_gate/visual_material_floor.mjs", "Fails flat-tint/fallback GLB rendering when sourced material proof is required."],
  ["tools/product_gate/repeated_failure_guard.mjs", "Stops repeated product/strict failures from turning into endless polishing."],
  ["tools/product_gate/responsive_layout_audit.mjs", "Audits UI tree bounds and responsive layout issues."],
  ["tools/assets/source/find_assets.mjs", "Source-first search over the shared asset library and free-source routes."],
  ["tools/assets/restricted.mjs", "Central publishable/restricted asset policy helper."],
  ["tools/assets/intake/bootstrap_shared_asset_library.mjs", "Initializes shared asset library folders and record templates."],
  ["tools/assets/intake/download_source_asset.mjs", "Downloads or records manual source assets with provenance and integrity."],
  ["tools/assets/intake/accept_incoming_asset.mjs", "Accepts staged incoming assets into the shared library with catalog/license metadata."],
  ["tools/assets/job/new_art_job.mjs", "Creates generated-art job scaffolding, crop manifest, asset manifest, and targets."],
  ["tools/assets/job/plan_source_sheet_prompt.mjs", "Builds source-sheet prompt packets from art jobs and intake constraints."],
  ["tools/assets/job/validate_art_job.mjs", "Strict validator for art job contracts, prompt packets, crop manifests, and runtime manifests."],
  ["tools/asset_review/build_review.mjs", "Builds a review gallery from shared/project asset records."],
  ["tools/asset_review/serve_gallery.mjs", "Serves generated asset review galleries locally."],
  ["tools/asset_review/pull.mjs", "Pulls selected asset records into a project-local runtime location."],
  ["tools/devapi/devapi_cli.py", "CLI client for runtime DevAPI commands."],
  ["tools/devapi/iterate.py", "Runs runtime automation and iteration loops through DevAPI."],
  ["tools/devapi/capture_window.py", "Captures the game window for evidence."],
  ["tools/devapi/pixel_health.py", "Checks screenshot pixel health and nonblank output."],
  ["tools/devapi/ui_readability.py", "Audits screenshot readability for UI evidence."],
  ["tools/state_codegen/generate_state.py", "Generates C GameState APIs from state schema and migrations."],
  ["tools/architecture_map/build_architecture_map.mjs", "Builds this source-based Obsidian-style architecture graph from Markdown and tool files."]
]);

const domainNotes = [
  ["ai_studio/taskboard/", "tasks", "Taskboard support code; source contract: ai_studio/taskboard/README.md and task-store-reference.md."],
  ["tools/ai_profile/", "profile", "Passive profiling support; source contract: docs/ai-pipeline/profiling-reuse.md."],
  ["tools/bootstrap/", "export", "Bootstrap/export support; source contract: tools/bootstrap/TEMPLATE.md and tools/README.md."],
  ["tools/game_context/", "design", "Prototype/game context support; source contract: ai_studio/README.md and project wiki routing."],
  ["tools/product_gate/", "validation", "Product/readability/visual gate support; source contract: docs/ai-pipeline/quality-validation.md."],
  ["tools/assets/source/", "assets", "Source-first asset search/import support; source contract: asset pipeline skills and tools/README.md."],
  ["tools/assets/intake/", "assets", "Asset intake/provenance support; source contract: asset pipeline skills and tools/README.md."],
  ["tools/assets/job/", "art", "Generated-art job support; source contract: generated-game-ui-assets skill."],
  ["tools/assets/cutout/", "art", "Source-sheet cutout/matte support; source contract: generated-game-ui-assets skill."],
  ["tools/assets/pack/", "art", "UI atlas packing/review support; source contract: generated-game-ui-assets skill."],
  ["tools/assets/crop/", "art", "Runtime crop planning support; source contract: generated-game-ui-assets skill."],
  ["tools/assets/assemble/", "art", "Runtime asset assembly support; source contract: generated-game-ui-assets skill."],
  ["tools/assets/audit/", "assets", "Asset policy guard support; source contract: asset pipeline skills and AGENTS.md."],
  ["tools/assets/", "assets", "Reusable asset helper; source contract: tools/README.md and asset pipeline skills."],
  ["tools/asset_review/", "assets", "Asset gallery, review, pull, and promotion support; source contract: tools/README.md."],
  ["tools/devapi/", "runtime", "Runtime automation support; source contract: runtime automation skill and template conventions."],
  ["tools/state_codegen/", "runtime", "Schema-first runtime state support; source contract: game-state-management skill."],
  ["tools/lib/", "facade", "Shared small library for agent-facing tools; source contract: tools/README.md."],
  ["tools/", "facade", "Agent-facing utility; source contract: tools/README.md."]
];

function toPosix(value) {
  return value.replaceAll("\\", "/");
}

function repoPath(rel) {
  return path.join(repoRoot, rel);
}

function exists(rel) {
  return fs.existsSync(repoPath(rel));
}

function walk(dir, predicate = () => true) {
  const abs = repoPath(dir);
  if (!fs.existsSync(abs)) return [];
  const out = [];
  const stack = [dir];
  while (stack.length) {
    const current = stack.pop();
    for (const entry of fs.readdirSync(repoPath(current), { withFileTypes: true })) {
      const rel = toPosix(path.join(current, entry.name));
      if (entry.isDirectory()) {
        if (entry.name === ".git" || entry.name === "__pycache__" || entry.name === "node_modules") continue;
        stack.push(rel);
      } else if (predicate(rel)) {
        out.push(rel);
      }
    }
  }
  return out.sort((a, b) => a.localeCompare(b));
}

function unique(list) {
  return [...new Set(list.filter(Boolean))];
}

function stripFrontmatter(text) {
  if (!text.startsWith("---")) return text;
  const end = text.indexOf("\n---", 3);
  return end >= 0 ? text.slice(end + 4) : text;
}

function stripMarkdown(line) {
  return line
    .replace(/`([^`]+)`/g, "$1")
    .replace(/\[([^\]]+)\]\([^)]+\)/g, "$1")
    .replace(/[*_>#]/g, "")
    .replace(/\s+/g, " ")
    .trim();
}

function titleFromMarkdown(rel, text) {
  const match = text.match(/^#\s+(.+)$/m);
  if (match) return stripMarkdown(match[1]);
  if (rel.endsWith("/SKILL.md")) return rel.split("/").at(-2);
  return path.posix.basename(rel, ".md");
}

function descriptionFromMarkdown(text) {
  const lines = stripFrontmatter(text).split(/\r?\n/);
  let inFence = false;
  let paragraph = [];
  for (const raw of lines) {
    const line = raw.trim();
    if (line.startsWith("```")) {
      inFence = !inFence;
      continue;
    }
    if (inFence) continue;
    if (!line) {
      if (paragraph.length) break;
      continue;
    }
    if (line.startsWith("#")) continue;
    if (/^[-*]\s+/.test(line) && !paragraph.length) continue;
    if (/^\|/.test(line)) continue;
    paragraph.push(stripMarkdown(line));
    if (paragraph.join(" ").length > 260) break;
  }
  const joined = paragraph.join(" ").replace(/\s+/g, " ").trim();
  return joined || "Markdown source without a short prose description.";
}

function headingsFromMarkdown(text) {
  return [...text.matchAll(/^#{2,3}\s+(.+)$/gm)].map((m) => stripMarkdown(m[1])).slice(0, 6);
}

function classifyPath(rel) {
  const lower = rel.toLowerCase();
  if (lower.startsWith(".codex/skills/")) return "skills";
  if (lower === "agents.md" || lower === "claude.md" || lower === "readme.md") return "hot";
  if (lower.startsWith("tasks/")) return "tasks";
  if (lower.includes("quality-validation") || lower.includes("product_gate") || lower.includes("gate")) return "validation";
  if (lower.includes("profiling") || lower.includes("ai_profile")) return "profile";
  if (lower.startsWith("gamedesign/")) {
    if (lower.includes("asset") || lower.includes("art") || lower.includes("visual") || lower.includes("ui_")) return "art";
    return "design";
  }
  if (lower.startsWith("template/")) return "runtime";
  if (lower.startsWith("external/neotolis-engine/")) return "engine";
  if (lower.startsWith("tools/bootstrap/")) return "export";
  if (lower.startsWith("tools/assets/")) return lower.includes("/job/") || lower.includes("/cutout/") || lower.includes("/pack/") || lower.includes("/crop/") || lower.includes("/assemble/") ? "art" : "assets";
  if (lower.startsWith("tools/asset_review/")) return "assets";
  if (lower.startsWith("tools/devapi/") || lower.startsWith("tools/state_codegen/")) return "runtime";
  if (lower.startsWith("docs/ai-pipeline/agent-workflow") || lower.startsWith("docs/ai-pipeline/subagent") || lower.startsWith("docs/ai-pipeline/orchestration")) return "tasks";
  if (lower.startsWith("docs/ai-pipeline/architecture-map")) return "hot";
  if (lower.startsWith("tools/")) return "facade";
  return "hot";
}

function nodeKind(rel) {
  if (rel.endsWith("/SKILL.md")) return "skill";
  if (rel.startsWith("tasks/")) return "task-doc";
  if (rel.startsWith("tools/")) return "tool-doc";
  if (rel.startsWith("gamedesign/")) return "design-doc";
  if (rel.startsWith("template/")) return "runtime-doc";
  if (rel.startsWith("external/")) return "engine-doc";
  return "doc";
}

function resolveMdLink(fromRel, rawTarget, docSet) {
  const target = rawTarget.split("#")[0].split("?")[0].trim();
  if (!target || target.startsWith("http://") || target.startsWith("https://") || !target.endsWith(".md")) return null;
  let resolved = target;
  if (!resolved.startsWith("/")) resolved = path.posix.normalize(path.posix.join(path.posix.dirname(fromRel), resolved));
  resolved = resolved.replace(/^\/+/, "");
  return docSet.has(resolved) ? resolved : null;
}

function markdownLinks(rel, text, docSet) {
  const links = new Set();
  for (const match of text.matchAll(/\[[^\]]+\]\(([^)]+)\)/g)) {
    const resolved = resolveMdLink(rel, match[1], docSet);
    if (resolved && resolved !== rel) links.add(resolved);
  }
  for (const match of text.matchAll(/`([^`]+?\.md)`/g)) {
    const resolved = resolveMdLink(rel, match[1], docSet);
    if (resolved && resolved !== rel) links.add(resolved);
  }
  return [...links];
}

function collectMarkdownSources() {
  const skillDocs = walk(".codex/skills", (rel) => rel.endsWith("/SKILL.md"));
  const knowledgeDocs = walk("gamedesign/knowledge", (rel) => rel.endsWith(".md"));
  const sourceDocs = walk("gamedesign/sources", (rel) => rel.endsWith(".md"));
  const activeTasks = walk("tasks/active", (rel) => rel.endsWith(".md"));
  const epics = walk("tasks/epics", (rel) => rel.endsWith(".md"));
  const docs = unique([...explicitDocPaths, ...skillDocs, ...knowledgeDocs, ...sourceDocs, ...activeTasks, ...epics]).filter(exists);
  const docSet = new Set(docs);
  return docs.map((rel) => {
    const text = fs.readFileSync(repoPath(rel), "utf8");
    return {
      id: "doc:" + rel,
      path: rel,
      href: hrefFor(rel),
      title: titleFromMarkdown(rel, text),
      description: descriptionFromMarkdown(text),
      headings: headingsFromMarkdown(text),
      group: classifyPath(rel),
      kind: nodeKind(rel),
      links: markdownLinks(rel, text, docSet)
    };
  });
}

function moduleForTool(rel) {
  for (const [prefix, group] of domainNotes.map(([prefix, group]) => [prefix, group])) {
    if (rel.startsWith(prefix)) return group;
  }
  return classifyPath(rel);
}

function toolType(rel) {
  if (rel.includes("/lib/") || rel.endsWith(".json")) return "support";
  if (rel.includes("/public/")) return "browser";
  if (rel.endsWith(".test.mjs")) return "test";
  if (rel.endsWith(".ps1") || rel.endsWith(".cmd")) return "host";
  if (rel.endsWith(".py") || rel.endsWith(".mjs")) return "cli";
  if (rel.endsWith(".c")) return "native source";
  if (rel.endsWith(".exe")) return "binary";
  return "tool";
}

function isPublicTool(rel) {
  if (rel.includes("/lib/") || rel.includes("/public/") || rel.endsWith(".test.mjs") || rel.includes("/test_")) return false;
  return exactToolNotes.has(rel) || rel.endsWith(".mjs") || rel.endsWith(".py") || rel.endsWith(".ps1") || rel.endsWith(".cmd");
}

function descriptionForTool(rel) {
  if (exactToolNotes.has(rel)) return exactToolNotes.get(rel);
  const domain = domainNotes.find(([prefix]) => rel.startsWith(prefix));
  const basename = path.posix.basename(rel);
  const domainText = domain ? domain[2] : "Agent-facing support utility; source contract: tools/README.md.";
  return `${domainText} File role is inferred from its path and name: ${basename}.`;
}

function collectTools() {
  const extensions = new Set([".mjs", ".js", ".py", ".ps1", ".cmd", ".c", ".exe", ".json", ".css"]);
  return walk("tools", (rel) => {
    const ext = path.posix.extname(rel);
    if (!extensions.has(ext)) return false;
    if (rel.includes("__pycache__")) return false;
    if (rel.includes("/node_modules/")) return false;
    return true;
  }).map((rel) => ({
    path: rel,
    href: hrefFor(rel),
    group: moduleForTool(rel),
    type: toolType(rel),
    surface: isPublicTool(rel) ? "public" : "internal",
    description: descriptionForTool(rel),
    source: sourceDocForGroup(moduleForTool(rel))
  }));
}

function sourceDocForGroup(group) {
  const map = {
    hot: "ai_studio/README.md",
    skills: ".codex/skills/ai-pipeline-maintenance/SKILL.md",
    tasks: "ai_studio/taskboard/README.md",
    facade: "tools/README.md",
    validation: "docs/ai-pipeline/quality-validation.md",
    design: "gamedesign/README.md",
    assets: ".codex/skills/game-asset-pipeline/SKILL.md",
    art: ".codex/skills/generated-game-ui-assets/SKILL.md",
    runtime: "template/CONVENTIONS.md",
    engine: "external/neotolis-engine/README.md",
    profile: "docs/ai-pipeline/profiling-reuse.md",
    export: "tools/bootstrap/TEMPLATE.md"
  };
  return map[group] || "tools/README.md";
}

function hrefFor(rel) {
  return toPosix(path.posix.relative(outDir, rel)) || path.posix.basename(rel);
}

function escapeHtml(value) {
  return String(value || "").replace(/[&<>"']/g, (ch) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#39;"
  })[ch]);
}

function hashNumber(value) {
  let hash = 2166136261;
  for (let i = 0; i < value.length; i++) {
    hash ^= value.charCodeAt(i);
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

function buildGraph(markdownDocs, tools) {
  const moduleById = new Map(modules.map((m) => [m.id, m]));
  const nodes = modules.map((m) => ({
    id: "module:" + m.id,
    label: m.title,
    title: m.title,
    description: m.description,
    group: m.id,
    kind: "module",
    module: m.id,
    path: "",
    href: "",
    headings: [],
    x: m.x,
    y: m.y,
    homeX: m.x,
    homeY: m.y,
    color: m.color,
    weight: 2.8
  }));

  for (const doc of markdownDocs) {
    const owner = moduleById.get(doc.group) || moduleById.get("hot");
    const h = hashNumber(doc.path);
    const angle = (h % 628) / 100;
    const ring = 110 + (h % 90);
    const x = Math.max(70, Math.min(graphWidth - 70, owner.x + Math.cos(angle) * ring));
    const y = Math.max(70, Math.min(graphHeight - 70, owner.y + Math.sin(angle) * ring));
    nodes.push({
      id: doc.id,
      label: compactLabel(doc.title, doc.path),
      title: doc.title,
      description: doc.description,
      group: doc.group,
      kind: doc.kind,
      module: doc.group,
      path: doc.path,
      href: doc.href,
      headings: doc.headings,
      links: doc.links,
      x,
      y,
      homeX: x,
      homeY: y,
      color: owner.color,
      weight: doc.kind === "skill" ? 1.7 : 1
    });
  }

  const edges = moduleEdges.map(([from, to, label, description]) => ({
    id: `contract:${from}:${to}`,
    from: "module:" + from,
    to: "module:" + to,
    label,
    description,
    type: "contract",
    group: from
  }));

  for (const doc of markdownDocs) {
    edges.push({
      id: "owns:" + doc.group + ":" + doc.path,
      from: "module:" + doc.group,
      to: doc.id,
      label: "source",
      description: "Markdown source belongs to this architecture module.",
      type: "source",
      group: doc.group
    });
    for (const target of doc.links) {
      edges.push({
        id: "link:" + doc.path + ":" + target,
        from: doc.id,
        to: "doc:" + target,
        label: "md link",
        description: `${doc.path} links to ${target}.`,
        type: "md-link",
        group: doc.group
      });
    }
  }

  return { nodes, edges, modules, moduleEdges, markdownDocs, tools, graphWidth, graphHeight, generatedAt: new Date().toISOString() };
}

function itemTitleFromPath(value, fallback = "") {
  const raw = value || fallback || "";
  const parts = raw.split("/");
  return parts[parts.length - 1] || fallback || raw;
}

function docItemForPath(docsByPath, rel, role = "module source", tags = []) {
  const d = docsByPath.get(rel);
  return {
    kind: "doc",
    path: rel,
    title: d ? d.title : itemTitleFromPath(rel),
    href: d ? d.href : (exists(rel) ? hrefFor(rel) : ""),
    description: d ? d.description : "File is not currently indexed as Markdown.",
    role,
    tags
  };
}

function toolItemForPath(toolsByPath, rel, role = "public tool", tags = []) {
  const t = toolsByPath.get(rel);
  return {
    kind: "tool",
    path: rel,
    title: rel,
    href: t ? t.href : (exists(rel) ? hrefFor(rel) : ""),
    description: t ? t.description : "Tool file is not currently indexed.",
    role,
    tags
  };
}

function contractItemForEdge(edge) {
  return {
    kind: "contract",
    path: edge.from + " -> " + edge.to,
    title: edge.label,
    href: "",
    description: edge.description,
    role: "module contract",
    tags: ["contract", edge.from, edge.to]
  };
}

function buildRefactorGroups(data) {
  const docsByPath = new Map(data.markdownDocs.map((d) => [d.path, d]));
  const toolsByPath = new Map(data.tools.map((t) => [t.path, t]));
  const out = {};
  for (const module of data.modules) {
    const docs = data.markdownDocs.filter((d) => d.group === module.id);
    const skills = docs.filter((d) => d.kind === "skill");
    const toolsForModule = data.tools.filter((t) => t.group === module.id);
    const publicTools = toolsForModule.filter((t) => t.surface === "public");
    const internalTools = toolsForModule.filter((t) => t.surface !== "public");
    const contracts = data.moduleEdges
      .map((e) => ({ from: e[0], to: e[1], label: e[2], description: e[3] }))
      .filter((e) => e.from === module.id || e.to === module.id);
    out[module.id] = [
      {
        id: "source-docs",
        title: "Source Docs",
        icon: "doc",
        color: module.color,
        description: "Markdown sources currently classified under this module.",
        tags: ["source", "docs"],
        items: docs.map((d) => docItemForPath(docsByPath, d.path, "module source", ["source", d.kind]))
      },
      {
        id: "skills",
        title: "Skills",
        icon: "agent",
        color: "#0f766e",
        description: "Repeatable agent procedures currently classified under this module.",
        tags: ["skill", "procedure"],
        items: skills.map((d) => docItemForPath(docsByPath, d.path, "agent procedure", ["skill", "procedure"]))
      },
      {
        id: "public-tools",
        title: "Public Tools",
        icon: "tool",
        color: "#b45309",
        description: "Agent-callable command surface currently classified under this module.",
        tags: ["public-api", "tool"],
        items: publicTools.map((t) => toolItemForPath(toolsByPath, t.path, "public tool", ["public-api", t.type]))
      },
      {
        id: "internal-helpers",
        title: "Internal Helpers",
        icon: "group",
        color: "#64748b",
        description: "Support files that should stay behind a public surface.",
        tags: ["internal", "support"],
        items: internalTools.map((t) => toolItemForPath(toolsByPath, t.path, "internal support", ["internal", t.type]))
      },
      {
        id: "contracts",
        title: "Contracts",
        icon: "shield",
        color: "#b91c1c",
        description: "Explicit module-level links that refactoring should preserve or change deliberately.",
        tags: ["contract", "boundary"],
        items: contracts.map(contractItemForEdge)
      }
    ].filter((group) => group.items.length);
  }
  return out;
}

function explorerLeafFromSpec(spec, data) {
  const docsByPath = new Map(data.markdownDocs.map((d) => [d.path, d]));
  const toolsByPath = new Map(data.tools.map((t) => [t.path, t]));
  const moduleById = new Map(data.modules.map((m) => [m.id, m]));
  const kind = spec.kind || "doc";
  const pathValue = spec.path || "";
  const doc = docsByPath.get(pathValue);
  const tool = toolsByPath.get(pathValue);
  const module = spec.moduleId ? moduleById.get(spec.moduleId) : null;
  const color = spec.color || (module && module.color) || (kind === "tool" ? "#b45309" : "#64748b");
  const title = spec.title || (doc && doc.title) || (tool && tool.path) || itemTitleFromPath(pathValue, spec.id);
  const href = spec.href || (doc && doc.href) || (tool && tool.href) || (pathValue && exists(pathValue) ? hrefFor(pathValue) : "");
  const description = spec.description || "";
  const tags = spec.tags || [];
  return {
    id: spec.id || pathValue || title,
    title,
    subtitle: spec.subtitle || pathValue || spec.subtitle || "",
    kind,
    color,
    description,
    path: pathValue,
    href,
    tags,
    moduleId: spec.moduleId || "",
    groupId: spec.groupId || "",
    item: pathValue ? {
      kind,
      path: pathValue,
      title,
      href,
      description,
      role: spec.role || "",
      tags
    } : null,
    children: []
  };
}

function explorerGroupNode(moduleId, group) {
  return {
    id: "group:" + moduleId + ":" + group.id,
    title: group.title,
    subtitle: group.items.length + " items",
    kind: "section",
    color: group.color,
    description: group.description,
    href: "",
    tags: group.tags,
    moduleId,
    groupId: group.id,
    children: group.items.map((item, index) => ({
      id: "item:" + moduleId + ":" + group.id + ":" + index,
      title: itemTitleFromPath(item.path, item.title),
      subtitle: item.path || item.title || "",
      kind: item.kind,
      color: group.color,
      description: item.description,
      path: item.path || "",
      href: item.href,
      tags: item.tags || [],
      moduleId,
      groupId: group.id,
      item,
      children: []
    }))
  };
}

function explorerModuleNode(module, data) {
  return {
    id: "module:" + module.id,
    title: module.title,
    subtitle: module.kind,
    kind: "module",
    color: module.color,
    description: module.description,
    href: "",
    tags: [],
    moduleId: module.id,
    children: (data.refactorGroups[module.id] || []).map((group) => explorerGroupNode(module.id, group))
  };
}

function generatedExplorerChildren(kind, data) {
  if (kind === "full-inventory") {
    return [{
      id: "not-refactored:full-inventory",
      title: "Full Current Inventory",
      subtitle: "architecture-map-full.html",
      kind: "doc",
      color: "#64748b",
      description: "Complete generated map of all current markdown sources, tools, and inferred connections.",
      href: "architecture-map-full.html",
      tags: ["reference", "all-current"],
      children: []
    }];
  }
  if (kind === "module-backlog") {
    return [{
      id: "not-refactored:modules",
      title: "Current Module Backlog",
      subtitle: "current modules",
      kind: "folder",
      color: "#64748b",
      description: "Raw current modules that still need review before promotion into the main ai_studio tree.",
      href: "",
      tags: ["current", "not-migrated"],
      children: data.modules
        .filter((module) => module.id !== "hot")
        .map((module) => explorerModuleNode(module, data))
    }];
  }
  return [];
}

function validateStudioTreeDescriptions(spec, pathParts = []) {
  const here = [...pathParts, spec.id || spec.title || "(unnamed)"];
  if (!String(spec.description || "").trim()) {
    throw new Error(`Missing description in ${studioTreePath}: ${here.join(" > ")}`);
  }
  for (const child of spec.children || []) {
    validateStudioTreeDescriptions(child, here);
  }
}

function resolveExplorerNode(spec, data) {
  const node = explorerLeafFromSpec(spec, data);
  const explicitChildren = (spec.children || []).map((child) => resolveExplorerNode(child, data));
  const generatedChildren = (spec.generatedChildren || []).flatMap((kind) => generatedExplorerChildren(kind, data));
  node.children = [...explicitChildren, ...generatedChildren];
  return node;
}

function buildStudioTree(data) {
  const sourcePath = repoPath(studioTreePath);
  const source = JSON.parse(fs.readFileSync(sourcePath, "utf8"));
  validateStudioTreeDescriptions(source.root);
  return resolveExplorerNode(source.root, data);
}

function compactLabel(title, rel) {
  if (rel.endsWith("/SKILL.md")) return rel.split("/").at(-2);
  if (title.length <= 34) return title;
  return title.slice(0, 31).trimEnd() + "...";
}

function escapeScriptJson(value) {
  return JSON.stringify(value).replaceAll("<", "\\u003c").replaceAll(">", "\\u003e").replaceAll("&", "\\u0026");
}

function renderFullHtml(data) {
  const dataJson = escapeScriptJson(data);
  return `<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AI Game Studio Pipeline Architecture Graph</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #eef2f7;
      --panel: #ffffff;
      --soft: #f8fafc;
      --ink: #172033;
      --muted: #657086;
      --line: #d6deeb;
      --shadow: 0 14px 36px rgba(23, 32, 51, .10);
      --selected: #111827;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      background: var(--bg);
      color: var(--ink);
      font: 14px/1.45 system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }
    a { color: #2563eb; text-decoration: none; }
    a:hover { text-decoration: underline; }
    code { font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace; font-size: .94em; }
    h1, h2, h3, p { margin: 0; }
    h1 { font-size: 28px; line-height: 1.08; letter-spacing: 0; }
    h2 { font-size: 18px; line-height: 1.2; letter-spacing: 0; }
    h3 { font-size: 14px; line-height: 1.2; letter-spacing: 0; }
    .shell { width: min(1680px, calc(100% - 28px)); margin: 0 auto; padding: 18px 0 40px; }
    header { display: grid; gap: 12px; margin-bottom: 12px; }
    .lead { max-width: 1160px; color: var(--muted); font-size: 15px; }
    .toolbar {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: 8px;
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 10px;
      box-shadow: var(--shadow);
      position: sticky;
      top: 8px;
      z-index: 20;
    }
    input, select, button {
      height: 34px;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #fff;
      color: var(--ink);
      padding: 0 10px;
      font: inherit;
    }
    input { flex: 1 1 330px; min-width: 240px; }
    button { cursor: pointer; background: #1f2937; border-color: #1f2937; color: #fff; }
    button.secondary { background: #fff; color: var(--ink); border-color: var(--line); }
    .range { display: inline-flex; align-items: center; gap: 6px; color: var(--muted); font-size: 12px; }
    .layout { display: grid; grid-template-columns: minmax(0, 1fr) 400px; gap: 14px; align-items: start; }
    .graph-card, .panel, .section {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      box-shadow: var(--shadow);
    }
    .graph-card { overflow: hidden; }
    .graph-head {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      gap: 10px;
      padding: 12px 14px;
      border-bottom: 1px solid var(--line);
      background: linear-gradient(180deg, #ffffff, #f8fbff);
    }
    .metrics { display: flex; flex-wrap: wrap; gap: 8px; }
    .metric {
      display: inline-flex;
      align-items: baseline;
      gap: 5px;
      padding: 5px 8px;
      border: 1px solid var(--line);
      border-radius: 999px;
      background: #fff;
      color: var(--muted);
      font-size: 12px;
    }
    .metric b { color: var(--ink); font-size: 14px; }
    .legend {
      display: flex;
      flex-wrap: wrap;
      gap: 7px;
      align-items: center;
    }
    .legend-item, .chip {
      display: inline-flex;
      align-items: center;
      min-height: 24px;
      gap: 6px;
      padding: 3px 8px;
      border: 1px solid #dce5f3;
      border-radius: 999px;
      background: #f3f6fb;
      color: #334155;
      font-size: 12px;
      white-space: nowrap;
    }
    .swatch { width: 11px; height: 11px; border-radius: 3px; background: var(--c); flex: 0 0 auto; }
    .shape-dot { width: 10px; height: 10px; border-radius: 50%; border: 2px solid currentColor; }
    .shape-square { width: 11px; height: 11px; border-radius: 3px; border: 2px solid currentColor; }
    .viewport {
      position: relative;
      height: min(76vh, 900px);
      min-height: 620px;
      overflow: auto;
      background:
        radial-gradient(circle at 1px 1px, rgba(100, 116, 139, .14) 1px, transparent 0) 0 0 / 24px 24px,
        linear-gradient(180deg, #f9fbff 0%, #eef4fb 100%);
    }
    .graph-layer {
      position: relative;
      width: ${graphWidth}px;
      height: ${graphHeight}px;
      transform-origin: 0 0;
    }
    .edge-layer {
      position: absolute;
      inset: 0;
      width: ${graphWidth}px;
      height: ${graphHeight}px;
      pointer-events: none;
      overflow: visible;
    }
    .edge { fill: none; stroke: #94a3b8; stroke-width: 1.6; opacity: .46; }
    .edge.contract { stroke-width: 2.5; opacity: .62; }
    .edge.source { stroke-dasharray: 4 5; opacity: .34; }
    .edge.md-link { stroke-width: 1.2; opacity: .30; }
    .edge.active { opacity: .95; stroke: #111827; }
    .edge.dimmed { opacity: .08; }
    .edge-label { fill: #475569; font-size: 11px; paint-order: stroke; stroke: #fff; stroke-width: 4px; stroke-linejoin: round; pointer-events: none; }
    .node-layer { position: absolute; inset: 0; }
    .node {
      position: absolute;
      transform: translate3d(-50%, -50%, 0);
      border: 1px solid color-mix(in srgb, var(--c) 42%, #cbd5e1);
      background: rgba(255, 255, 255, .95);
      box-shadow: 0 8px 22px rgba(15, 23, 42, .10);
      cursor: grab;
      user-select: none;
      touch-action: none;
      transition: opacity .16s ease, box-shadow .16s ease, border-color .16s ease;
    }
    .node:active { cursor: grabbing; }
    .node.module {
      width: 188px;
      min-height: 82px;
      border-width: 2px;
      border-radius: 8px;
      padding: 10px;
      display: grid;
      gap: 6px;
      background: #fff;
    }
    .node.skill {
      width: 156px;
      min-height: 58px;
      border-radius: 28px;
      padding: 9px 12px;
      display: grid;
      gap: 3px;
    }
    .node.doc, .node.task-doc, .node.tool-doc, .node.design-doc, .node.runtime-doc, .node.engine-doc {
      width: 138px;
      min-height: 52px;
      border-radius: 18px;
      padding: 8px 10px;
      display: grid;
      gap: 3px;
    }
    .node .top { display: flex; align-items: center; justify-content: space-between; gap: 7px; }
    .node .label { font-weight: 700; font-size: 12px; line-height: 1.15; overflow-wrap: anywhere; }
    .node.module .label { font-size: 14px; }
    .node .kind { color: var(--muted); font-size: 10px; text-transform: uppercase; letter-spacing: .05em; }
    .node .path { color: #64748b; font-size: 10px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .node.selected { border-color: var(--selected); box-shadow: 0 0 0 4px color-mix(in srgb, var(--c) 20%, transparent), 0 14px 34px rgba(15, 23, 42, .20); z-index: 5; }
    .node.neighbor { box-shadow: 0 0 0 3px color-mix(in srgb, var(--c) 16%, transparent), 0 10px 24px rgba(15, 23, 42, .16); }
    .node.dimmed { opacity: .16; }
    .node.hidden { display: none; }
    .panel {
      position: sticky;
      top: 82px;
      padding: 14px;
      display: grid;
      gap: 13px;
      max-height: calc(100vh - 100px);
      overflow: auto;
    }
    .panel-block { display: grid; gap: 7px; }
    .muted { color: var(--muted); font-size: 12px; }
    .source-link {
      display: inline-flex;
      align-items: center;
      width: fit-content;
      padding: 4px 8px;
      border-radius: 6px;
      background: #eef4ff;
      border: 1px solid #c9dcff;
      font-size: 12px;
    }
    .list { display: grid; gap: 7px; }
    .small-card {
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--soft);
      padding: 9px;
      display: grid;
      gap: 4px;
    }
    .section { padding: 16px; margin-top: 14px; }
    .section-head {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      gap: 10px;
      align-items: flex-start;
      margin-bottom: 12px;
    }
    .source-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 10px; }
    .baseline-grid { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 10px; }
    .inventory-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(310px, 1fr)); gap: 10px; }
    details.group {
      border: 1px solid var(--line);
      border-radius: 8px;
      overflow: hidden;
      background: #fff;
    }
    details.group + details.group { margin-top: 9px; }
    summary {
      cursor: pointer;
      padding: 11px 13px;
      background: var(--soft);
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
    }
    table { width: 100%; border-collapse: collapse; }
    th, td { border-top: 1px solid var(--line); padding: 8px 10px; text-align: left; vertical-align: top; }
    th { background: #f2f5fb; color: var(--muted); font-size: 12px; font-weight: 600; }
    .file { font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace; font-size: 12px; overflow-wrap: anywhere; }
    .footer-note { color: var(--muted); font-size: 12px; margin-top: 10px; }
    @media (max-width: 1160px) {
      .layout { grid-template-columns: 1fr; }
      .panel { position: static; max-height: none; }
      .viewport { height: 70vh; }
      .baseline-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    }
    @media (max-width: 720px) {
      .shell { width: min(100% - 18px, 1680px); }
      .toolbar { position: static; }
      input, select, button { width: 100%; }
      .viewport { min-height: 560px; }
      .baseline-grid { grid-template-columns: 1fr; }
      table, tbody, tr, td { display: block; width: 100%; }
      thead { display: none; }
      tr { border-top: 1px solid var(--line); padding: 7px 0; }
      td { border-top: 0; padding: 6px 10px; }
    }
  </style>
</head>
<body>
  <div class="shell">
    <header>
      <h1>AI Game Studio Pipeline Architecture Graph</h1>
      <p class="lead">
        Это Obsidian-like граф архитектуры pipeline: крупные узлы — модули, малые узлы — markdown-источники и skills.
        Описания в карточках берутся из самих <code>.md</code>: заголовок, первый осмысленный абзац, локальные ссылки и разделы.
        Узлы можно двигать, граф можно ставить на паузу, фильтровать и использовать как карту для декомпозиции.
      </p>
      <div class="toolbar">
        <input id="search" type="search" placeholder="Поиск: модуль, skill, markdown, утилита, связь">
        <select id="moduleFilter" aria-label="Фильтр модуля"></select>
        <select id="kindFilter" aria-label="Фильтр типа узла">
          <option value="">Все узлы</option>
          <option value="module">Только модули</option>
          <option value="skill">Skills</option>
          <option value="doc">Markdown docs</option>
        </select>
        <button id="runBtn" type="button">Pause layout</button>
        <button id="resetBtn" type="button" class="secondary">Reset layout</button>
        <label class="range">Zoom <input id="zoom" type="range" min="70" max="130" value="100"></label>
      </div>
    </header>

    <main class="layout">
      <section class="graph-card">
        <div class="graph-head">
          <div>
            <h2>Граф модулей и markdown-источников</h2>
            <p class="muted">Перетаскивай узлы. Клик показывает описание из <code>.md</code>, входящие/исходящие связи и утилиты домена.</p>
          </div>
          <div class="metrics">
            <span class="metric"><b id="moduleCount">0</b> modules</span>
            <span class="metric"><b id="docCount">0</b> markdown</span>
            <span class="metric"><b id="skillCount">0</b> skills</span>
            <span class="metric"><b id="toolCount">0</b> tools</span>
            <span class="metric"><b id="edgeCount">0</b> links</span>
          </div>
          <div class="legend" id="legend"></div>
          <div class="legend">
            <span class="legend-item"><span class="shape-square"></span> module</span>
            <span class="legend-item"><span class="shape-dot"></span> markdown / skill</span>
            <span class="legend-item">solid: contract</span>
            <span class="legend-item">dashed: source</span>
            <span class="legend-item">thin: md link</span>
          </div>
        </div>
        <div class="viewport" id="viewport">
          <div class="graph-layer" id="graphLayer">
            <svg class="edge-layer" id="edgeLayer" viewBox="0 0 ${graphWidth} ${graphHeight}" aria-hidden="true"></svg>
            <div class="node-layer" id="nodeLayer"></div>
          </div>
        </div>
      </section>

      <aside class="panel" id="detail"></aside>
    </main>

    <section class="section">
      <div class="section-head">
        <div>
          <h2>Основа для архитектурного рефакторинга</h2>
          <p class="muted">Эта карта нужна не для красоты. Это рабочий инвентарь: что существует, кто владеет областью, где источник правды и какие связи нельзя ломать.</p>
        </div>
      </div>
      <div class="baseline-grid">
        <article class="small-card">
          <h3>1. Инвентарь</h3>
          <p class="muted">Каждый узел связан с реальным Markdown или модулем. Если чего-то нет на карте, это кандидат на каталогизацию или удаление.</p>
        </article>
        <article class="small-card">
          <h3>2. Ownership</h3>
          <p class="muted">Цвет показывает домен-владельца. Новая утилита должна попасть в один домен, а не висеть отдельным скриптом без роли.</p>
        </article>
        <article class="small-card">
          <h3>3. Контракты</h3>
          <p class="muted">Толстые связи между модулями — архитектурные контракты. Рефакторинг должен сохранять или явно менять эти связи.</p>
        </article>
        <article class="small-card">
          <h3>4. План исправлений</h3>
          <p class="muted">Дальше можно идти модуль за модулем: public API, internal helpers, валидаторы, дубли правил, мёртвые файлы, переносы.</p>
        </article>
      </div>
    </section>

    <section class="section">
      <div class="section-head">
        <div>
          <h2>Что уже есть по модулям</h2>
          <p class="muted">Сводка строится из текущих файлов. Она показывает, где система уже насыщена, а где домен пока держится только на документах или разрозненных tools.</p>
        </div>
      </div>
      <div class="inventory-grid" id="moduleInventory"></div>
    </section>

    <section class="section">
      <div class="section-head">
        <div>
          <h2>Markdown sources grouped by module</h2>
          <p class="muted">Это не ручные описания: карточки ниже построены из заголовка и первого абзаца соответствующих <code>.md</code>.</p>
        </div>
        <span class="chip" id="visibleSourceCount">0 visible</span>
      </div>
      <div class="source-grid" id="sourceCards"></div>
    </section>

    <section class="section">
      <div class="section-head">
        <div>
          <h2>Utility catalog grouped by architecture module</h2>
          <p class="muted">Утилиты сгруппированы по доменам графа. Для ключевых public CLI указано точное назначение; для support-файлов описание выводится из доменного markdown-контракта и имени файла.</p>
        </div>
        <span class="chip" id="visibleToolCount">0 visible</span>
      </div>
      <div id="toolGroups"></div>
      <p class="footer-note">Generated from local Markdown and tool files at ${data.generatedAt}. Rebuild: <code>node tools/architecture_map/build_architecture_map.mjs</code>.</p>
    </section>
  </div>

  <script id="graph-data" type="application/json">${dataJson}</script>
  <script>
  (() => {
    const data = JSON.parse(document.getElementById("graph-data").textContent);
    const moduleById = new Map(data.modules.map((m) => [m.id, m]));
    const nodes = data.nodes.map((n) => Object.assign({ vx: 0, vy: 0, pinned: false }, n));
    const nodeById = new Map(nodes.map((n) => [n.id, n]));
    const edges = data.edges.filter((e) => nodeById.has(e.from) && nodeById.has(e.to));
    const nodeLayer = document.getElementById("nodeLayer");
    const edgeLayer = document.getElementById("edgeLayer");
    const graphLayer = document.getElementById("graphLayer");
    const viewport = document.getElementById("viewport");
    const search = document.getElementById("search");
    const moduleFilter = document.getElementById("moduleFilter");
    const kindFilter = document.getElementById("kindFilter");
    const zoomInput = document.getElementById("zoom");
    const detail = document.getElementById("detail");
    let selectedId = "module:hot";
    let running = true;
    let dragging = null;
    let zoom = 1;
    const nodeEls = new Map();
    const edgeEls = new Map();
    const labelEls = new Map();

    function esc(value) {
      return String(value || "").replace(/[&<>"']/g, (ch) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[ch]));
    }
    function icon(name) {
      const paths = {
        doc: '<path d="M6 2h7l5 5v15H6z"></path><path d="M13 2v6h6"></path>',
        agent: '<path d="M12 3a4 4 0 0 1 4 4v2a4 4 0 0 1-8 0V7a4 4 0 0 1 4-4z"></path><path d="M5 21a7 7 0 0 1 14 0"></path>',
        route: '<path d="M4 6h6a4 4 0 0 1 0 8H8"></path><path d="M8 10l-4 4 4 4"></path><path d="M14 18h6"></path>',
        tool: '<path d="M14.7 6.3a4 4 0 0 0-5.4 5.4L4 17v3h3l5.3-5.3a4 4 0 0 0 5.4-5.4z"></path>',
        shield: '<path d="M12 3l8 4v5c0 5-3.4 8.4-8 9-4.6-.6-8-4-8-9V7z"></path>',
        sync: '<path d="M20 6v5h-5"></path><path d="M4 18v-5h5"></path><path d="M19 11a7 7 0 0 0-12-4"></path><path d="M5 13a7 7 0 0 0 12 4"></path>',
        group: '<path d="M4 4h7v7H4z"></path><path d="M13 4h7v7h-7z"></path><path d="M4 13h7v7H4z"></path><path d="M13 13h7v7h-7z"></path>',
        remove: '<path d="M3 6h18"></path><path d="M8 6V4h8v2"></path><path d="M7 6l1 15h8l1-15"></path>'
      };
      return '<span class="icon"><svg viewBox="0 0 24 24" aria-hidden="true">' + (paths[name] || paths.group) + '</svg></span>';
    }
    function tags(values) {
      return '<div class="tags">' + values.map((v) => '<span class="tag">' + esc(v) + '</span>').join("") + '</div>';
    }
    function docItem(path, role, tagList = []) {
      const d = docsByPath.get(path);
      return {
        kind: "doc",
        path,
        title: d ? d.title : path,
        href: d ? d.href : path,
        description: d ? d.description : "Markdown source is not currently indexed.",
        role,
        tags: tagList
      };
    }
    function toolItem(path, role, tagList = []) {
      const t = toolsByPath.get(path);
      return {
        kind: "tool",
        path,
        title: path,
        href: t ? t.href : path,
        description: t ? t.description : "Tool file is not currently indexed.",
        role,
        tags: tagList
      };
    }
    function contractItem(edge) {
      return {
        kind: "contract",
        path: edge.from + " -> " + edge.to,
        title: edge.label,
        href: "",
        description: edge.description,
        role: "module contract",
        tags: ["contract", edge.from, edge.to]
      };
    }
    function matchQuery(node, q) {
      if (!q) return true;
      return [node.title, node.label, node.path, node.description, node.kind, node.group].join(" ").toLowerCase().includes(q);
    }
    function selectedNeighbors() {
      const ids = new Set([selectedId]);
      edges.forEach((e) => {
        if (e.from === selectedId) ids.add(e.to);
        if (e.to === selectedId) ids.add(e.from);
      });
      return ids;
    }
    function visibleKind(node) {
      const k = kindFilter.value;
      if (!k) return true;
      if (k === "doc") return node.kind !== "module" && node.kind !== "skill";
      return node.kind === k;
    }
    function moduleVisible(node) {
      return !moduleFilter.value || node.group === moduleFilter.value || node.id === "module:" + moduleFilter.value;
    }
    function shouldHide(node) {
      return !visibleKind(node) || !moduleVisible(node);
    }
    function isDimmed(node, q) {
      if (shouldHide(node)) return true;
      if (!q) return false;
      if (matchQuery(node, q)) return false;
      return !edges.some((e) => {
        if (e.from === node.id && matchQuery(nodeById.get(e.to), q)) return true;
        if (e.to === node.id && matchQuery(nodeById.get(e.from), q)) return true;
        return false;
      });
    }
    function renderLegend() {
      const root = document.getElementById("legend");
      root.innerHTML = data.modules.map((m) => '<span class="legend-item"><span class="swatch" style="--c:' + esc(m.color) + '"></span>' + esc(m.title) + '</span>').join("");
      moduleFilter.innerHTML = '<option value="">Все модули</option>' + data.modules.map((m) => '<option value="' + esc(m.id) + '">' + esc(m.title) + '</option>').join("");
    }
    function renderNodes() {
      nodeLayer.innerHTML = "";
      for (const node of nodes) {
        const el = document.createElement("div");
        el.className = "node " + node.kind;
        el.style.setProperty("--c", node.color);
        el.dataset.id = node.id;
        el.tabIndex = 0;
        const pathLine = node.path ? '<div class="path">' + esc(node.path) + '</div>' : '<div class="path">' + esc(node.group) + '</div>';
        el.innerHTML = '<div class="top"><span class="kind">' + esc(node.kind) + '</span><span class="swatch" style="--c:' + esc(node.color) + '"></span></div><div class="label">' + esc(node.label) + '</div>' + pathLine;
        el.addEventListener("pointerdown", (event) => startDrag(event, node));
        el.addEventListener("click", (event) => {
          event.stopPropagation();
          selectedId = node.id;
          updateAll();
        });
        el.addEventListener("keydown", (event) => {
          if (event.key === "Enter" || event.key === " ") {
            selectedId = node.id;
            updateAll();
          }
        });
        nodeLayer.appendChild(el);
        nodeEls.set(node.id, el);
      }
    }
    function renderEdges() {
      edgeLayer.innerHTML = '<defs><marker id="arrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth"><path d="M0,0 L0,6 L8,3 z" fill="#94a3b8"></path></marker></defs>';
      for (const edge of edges) {
        const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
        path.setAttribute("class", "edge " + edge.type);
        path.setAttribute("marker-end", edge.type === "contract" ? "url(#arrow)" : "");
        edgeLayer.appendChild(path);
        edgeEls.set(edge.id, path);
        const label = document.createElementNS("http://www.w3.org/2000/svg", "text");
        label.setAttribute("class", "edge-label");
        label.setAttribute("text-anchor", "middle");
        label.textContent = edge.label;
        edgeLayer.appendChild(label);
        labelEls.set(edge.id, label);
      }
    }
    function pointFromEvent(event) {
      const rect = viewport.getBoundingClientRect();
      return {
        x: (event.clientX - rect.left + viewport.scrollLeft) / zoom,
        y: (event.clientY - rect.top + viewport.scrollTop) / zoom
      };
    }
    function startDrag(event, node) {
      if (event.button !== 0) return;
      const p = pointFromEvent(event);
      dragging = { node, dx: p.x - node.x, dy: p.y - node.y };
      node.pinned = true;
      node.vx = 0;
      node.vy = 0;
      event.currentTarget.setPointerCapture(event.pointerId);
    }
    window.addEventListener("pointermove", (event) => {
      if (!dragging) return;
      const p = pointFromEvent(event);
      dragging.node.x = Math.max(40, Math.min(data.graphWidth - 40, p.x - dragging.dx));
      dragging.node.y = Math.max(40, Math.min(data.graphHeight - 40, p.y - dragging.dy));
      updatePositions();
    });
    window.addEventListener("pointerup", () => {
      dragging = null;
    });
    function physicsStep() {
      const active = nodes.filter((n) => !shouldHide(n));
      for (let i = 0; i < active.length; i++) {
        const a = active[i];
        for (let j = i + 1; j < active.length; j++) {
          const b = active[j];
          let dx = a.x - b.x;
          let dy = a.y - b.y;
          let d2 = dx * dx + dy * dy + 0.01;
          if (d2 > 90000) continue;
          const d = Math.sqrt(d2);
          const force = (a.kind === "module" || b.kind === "module" ? 900 : 520) / d2;
          dx /= d;
          dy /= d;
          if (!a.pinned) { a.vx += dx * force; a.vy += dy * force; }
          if (!b.pinned) { b.vx -= dx * force; b.vy -= dy * force; }
        }
      }
      for (const edge of edges) {
        const a = nodeById.get(edge.from);
        const b = nodeById.get(edge.to);
        if (!a || !b || shouldHide(a) || shouldHide(b)) continue;
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const dist = Math.max(1, Math.sqrt(dx * dx + dy * dy));
        const target = edge.type === "contract" ? 250 : edge.type === "source" ? 125 : 170;
        const strength = edge.type === "contract" ? 0.0035 : edge.type === "source" ? 0.0022 : 0.0015;
        const pull = (dist - target) * strength;
        const fx = (dx / dist) * pull;
        const fy = (dy / dist) * pull;
        if (!a.pinned) { a.vx += fx; a.vy += fy; }
        if (!b.pinned) { b.vx -= fx; b.vy -= fy; }
      }
      for (const node of active) {
        const module = moduleById.get(node.group);
        const homeX = node.kind === "module" ? node.homeX : module ? module.x : node.homeX;
        const homeY = node.kind === "module" ? node.homeY : module ? module.y : node.homeY;
        const gravity = node.kind === "module" ? 0.0035 : 0.0009;
        if (!node.pinned) {
          node.vx += (homeX - node.x) * gravity;
          node.vy += (homeY - node.y) * gravity;
          node.vx *= 0.86;
          node.vy *= 0.86;
          node.x = Math.max(40, Math.min(data.graphWidth - 40, node.x + node.vx));
          node.y = Math.max(40, Math.min(data.graphHeight - 40, node.y + node.vy));
        }
      }
    }
    function updatePositions() {
      const q = search.value.trim().toLowerCase();
      const neighbors = selectedNeighbors();
      for (const node of nodes) {
        const el = nodeEls.get(node.id);
        if (!el) continue;
        el.style.left = node.x + "px";
        el.style.top = node.y + "px";
        el.classList.toggle("selected", node.id === selectedId);
        el.classList.toggle("neighbor", neighbors.has(node.id) && node.id !== selectedId);
        el.classList.toggle("dimmed", isDimmed(node, q) && node.id !== selectedId);
        el.classList.toggle("hidden", shouldHide(node));
      }
      for (const edge of edges) {
        const a = nodeById.get(edge.from);
        const b = nodeById.get(edge.to);
        const path = edgeEls.get(edge.id);
        const label = labelEls.get(edge.id);
        if (!a || !b || !path || !label) continue;
        const hidden = shouldHide(a) || shouldHide(b);
        const active = edge.from === selectedId || edge.to === selectedId;
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const curve = edge.type === "contract" ? Math.max(-80, Math.min(80, dy * .18)) : 0;
        path.setAttribute("d", "M " + a.x + " " + a.y + " C " + (a.x + dx * .44) + " " + (a.y + curve) + ", " + (a.x + dx * .56) + " " + (b.y - curve) + ", " + b.x + " " + b.y);
        path.classList.toggle("active", active);
        path.classList.toggle("dimmed", hidden || (!active && q && (isDimmed(a, q) || isDimmed(b, q))));
        label.setAttribute("x", String((a.x + b.x) / 2));
        label.setAttribute("y", String((a.y + b.y) / 2 - 7));
        label.style.display = active || edge.type === "contract" ? "block" : "none";
        label.classList.toggle("dimmed", hidden);
      }
    }
    function frame() {
      if (running && !dragging) physicsStep();
      updatePositions();
      requestAnimationFrame(frame);
    }
    function relatedEdges(nodeId) {
      return edges.filter((e) => e.from === nodeId || e.to === nodeId);
    }
    function renderDetail() {
      const node = nodeById.get(selectedId) || nodeById.get("module:hot");
      const related = relatedEdges(node.id);
      const groupDocs = data.markdownDocs.filter((d) => d.group === node.group).slice(0, 7);
      const groupTools = data.tools.filter((t) => t.group === node.group).slice(0, 8);
      const headings = node.headings && node.headings.length ? node.headings.map((h) => '<span class="chip">' + esc(h) + '</span>').join("") : '<span class="muted">No H2/H3 headings captured.</span>';
      const source = node.path ? '<a class="source-link" href="' + esc(node.href) + '">' + esc(node.path) + '</a>' : '<span class="chip">architecture module</span>';
      const nodeModule = moduleById.get(node.group);
      detail.innerHTML =
        '<div class="panel-block"><h2>' + esc(node.title) + '</h2><p class="muted">' + esc(node.kind) + ' / ' + esc(nodeModule ? nodeModule.title : node.group) + '</p>' + source + '</div>' +
        '<div class="panel-block"><h3>Описание из Markdown</h3><p>' + esc(node.description) + '</p></div>' +
        '<div class="panel-block"><h3>Разделы</h3><div class="legend">' + headings + '</div></div>' +
        '<div class="panel-block"><h3>Связи</h3><div class="list">' + (related.length ? related.slice(0, 12).map((e) => edgeCard(e, node.id)).join("") : '<p class="muted">No direct graph links.</p>') + '</div></div>' +
        '<div class="panel-block"><h3>Markdown этого модуля</h3><div class="list">' + groupDocs.map((d) => '<div class="small-card"><a class="file" href="' + esc(d.href) + '">' + esc(d.path) + '</a><p class="muted">' + esc(d.description) + '</p></div>').join("") + '</div></div>' +
        '<div class="panel-block"><h3>Утилиты домена</h3><div class="list">' + (groupTools.length ? groupTools.map((t) => '<div class="small-card"><a class="file" href="' + esc(t.href) + '">' + esc(t.path) + '</a><p class="muted">' + esc(t.description) + '</p></div>').join("") : '<p class="muted">No tools classified here.</p>') + '</div></div>';
    }
    function edgeCard(edge, current) {
      const other = nodeById.get(edge.from === current ? edge.to : edge.from);
      return '<div class="small-card"><div><span class="chip">' + esc(edge.type) + '</span> <span class="chip">' + esc(edge.label) + '</span></div><a href="#" data-node="' + esc(other.id) + '">' + esc(other.title) + '</a><p class="muted">' + esc(edge.description) + '</p></div>';
    }
    detail.addEventListener("click", (event) => {
      const link = event.target.closest("[data-node]");
      if (!link) return;
      event.preventDefault();
      selectedId = link.dataset.node;
      updateAll();
    });
    function renderSourceCards() {
      const root = document.getElementById("sourceCards");
      const q = search.value.trim().toLowerCase();
      const mod = moduleFilter.value;
      const rows = data.markdownDocs.filter((d) => {
        if (mod && d.group !== mod) return false;
        if (!q) return true;
        return [d.title, d.path, d.description, d.kind, d.group].join(" ").toLowerCase().includes(q);
      });
      root.innerHTML = rows.map((d) => {
        const rowModule = moduleById.get(d.group);
        return '<article class="small-card"><div class="legend"><span class="swatch" style="--c:' + esc(rowModule ? rowModule.color : "#64748b") + '"></span><span class="chip">' + esc(d.kind) + '</span></div><h3>' + esc(d.title) + '</h3><a class="file" href="' + esc(d.href) + '">' + esc(d.path) + '</a><p class="muted">' + esc(d.description) + '</p></article>';
      }).join("");
      document.getElementById("visibleSourceCount").textContent = rows.length + " visible";
    }
    function renderModuleInventory() {
      const root = document.getElementById("moduleInventory");
      root.innerHTML = data.modules.map((module) => {
        const docs = data.markdownDocs.filter((d) => d.group === module.id);
        const skills = docs.filter((d) => d.kind === "skill");
        const tools = data.tools.filter((t) => t.group === module.id);
        const publicTools = tools.filter((t) => t.surface === "public");
        const related = edges.filter((e) => e.from === "module:" + module.id || e.to === "module:" + module.id);
        const source = docs[0];
        return '<article class="small-card">' +
          '<div class="legend"><span class="swatch" style="--c:' + esc(module.color) + '"></span><span class="chip">' + esc(module.kind) + '</span></div>' +
          '<h3>' + esc(module.title) + '</h3>' +
          '<p class="muted">' + esc(module.description) + '</p>' +
          '<div class="legend">' +
            '<span class="chip">' + docs.length + ' md</span>' +
            '<span class="chip">' + skills.length + ' skills</span>' +
            '<span class="chip">' + publicTools.length + '/' + tools.length + ' public/tools</span>' +
            '<span class="chip">' + related.length + ' links</span>' +
          '</div>' +
          (source ? '<a class="file" href="' + esc(source.href) + '">' + esc(source.path) + '</a>' : '<span class="muted">No markdown source.</span>') +
        '</article>';
      }).join("");
    }
    function renderToolGroups() {
      const root = document.getElementById("toolGroups");
      const q = search.value.trim().toLowerCase();
      const mod = moduleFilter.value;
      let visible = 0;
      root.innerHTML = "";
      for (const module of data.modules) {
        const rows = data.tools.filter((t) => {
          if (t.group !== module.id) return false;
          if (mod && t.group !== mod) return false;
          if (!q) return true;
          return [t.path, t.description, t.type, t.surface, t.source, module.title].join(" ").toLowerCase().includes(q);
        });
        if (!rows.length) continue;
        visible += rows.length;
        const details = document.createElement("details");
        details.className = "group";
        details.open = Boolean(mod || q) || ["facade", "tasks", "validation", "assets", "art"].includes(module.id);
        details.innerHTML = '<summary><strong>' + esc(module.title) + '</strong><span class="chip">' + rows.length + ' entries</span></summary><table><thead><tr><th>Path</th><th>Surface</th><th>Type</th><th>Description</th><th>Markdown source</th></tr></thead><tbody></tbody></table>';
        const tbody = details.querySelector("tbody");
        rows.forEach((t) => {
          const tr = document.createElement("tr");
          tr.innerHTML = '<td><a class="file" href="' + esc(t.href) + '">' + esc(t.path) + '</a></td><td><span class="chip">' + esc(t.surface) + '</span></td><td><span class="chip">' + esc(t.type) + '</span></td><td>' + esc(t.description) + '</td><td><span class="file">' + esc(t.source) + '</span></td>';
          tbody.appendChild(tr);
        });
        root.appendChild(details);
      }
      document.getElementById("visibleToolCount").textContent = visible + " visible";
    }
    function updateAll() {
      updatePositions();
      renderDetail();
      renderSourceCards();
      renderToolGroups();
    }
    document.getElementById("runBtn").addEventListener("click", () => {
      running = !running;
      document.getElementById("runBtn").textContent = running ? "Pause layout" : "Resume layout";
    });
    document.getElementById("resetBtn").addEventListener("click", () => {
      nodes.forEach((n) => {
        n.x = n.homeX;
        n.y = n.homeY;
        n.vx = 0;
        n.vy = 0;
        n.pinned = false;
      });
      search.value = "";
      moduleFilter.value = "";
      kindFilter.value = "";
      selectedId = "module:hot";
      running = true;
      document.getElementById("runBtn").textContent = "Pause layout";
      updateAll();
    });
    zoomInput.addEventListener("input", () => {
      zoom = Number(zoomInput.value) / 100;
      graphLayer.style.transform = "scale(" + zoom + ")";
      graphLayer.style.width = data.graphWidth + "px";
      graphLayer.style.height = data.graphHeight + "px";
    });
    search.addEventListener("input", updateAll);
    moduleFilter.addEventListener("change", () => {
      if (moduleFilter.value) selectedId = "module:" + moduleFilter.value;
      updateAll();
    });
    kindFilter.addEventListener("change", updateAll);
    viewport.addEventListener("click", () => {
      if (!dragging) return;
      dragging = null;
    });

    renderLegend();
    renderNodes();
    renderEdges();
    renderModuleInventory();
    document.getElementById("moduleCount").textContent = data.modules.length;
    document.getElementById("docCount").textContent = data.markdownDocs.length;
    document.getElementById("skillCount").textContent = data.markdownDocs.filter((d) => d.kind === "skill").length;
    document.getElementById("toolCount").textContent = data.tools.length;
    document.getElementById("edgeCount").textContent = edges.length;
    updateAll();
    requestAnimationFrame(frame);
  })();
  </script>
</body>
</html>
`;
}

function renderRefactorHtml(data) {
  const dataJson = escapeScriptJson(data);
  return `<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AI Pipeline Core Refactor Map</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f2f5f8;
      --panel: #ffffff;
      --soft: #f8fafc;
      --ink: #172033;
      --muted: #667085;
      --line: #d8e1ee;
      --shadow: 0 14px 34px rgba(23, 32, 51, .10);
      --danger: #b91c1c;
      --warn: #b45309;
      --ok: #15803d;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      background: var(--bg);
      color: var(--ink);
      font: 14px/1.45 system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }
    a { color: #2563eb; text-decoration: none; }
    a:hover { text-decoration: underline; }
    code { font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace; font-size: .94em; }
    h1, h2, h3, p { margin: 0; }
    h1 { font-size: 28px; line-height: 1.08; letter-spacing: 0; }
    h2 { font-size: 18px; line-height: 1.2; letter-spacing: 0; }
    h3 { font-size: 14px; line-height: 1.2; letter-spacing: 0; }
    .shell { width: min(1760px, calc(100% - 24px)); margin: 0 auto; padding: 12px 0 22px; }
    header { display: grid; gap: 12px; margin-bottom: 12px; }
    .lead { color: var(--muted); max-width: 1120px; font-size: 15px; }
    .navline { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
    .navline a, .chip {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      min-height: 25px;
      border: 1px solid #dce5f3;
      border-radius: 999px;
      background: #fff;
      padding: 3px 9px;
      color: #334155;
      font-size: 12px;
      white-space: nowrap;
    }
    .toolbar {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: 8px;
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 10px;
      box-shadow: var(--shadow);
      position: sticky;
      top: 8px;
      z-index: 20;
    }
    input, select, button {
      height: 34px;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #fff;
      color: var(--ink);
      padding: 0 10px;
      font: inherit;
    }
    input { flex: 1 1 300px; min-width: 240px; }
    button { cursor: pointer; background: #1f2937; color: #fff; border-color: #1f2937; }
    button.secondary { background: #fff; color: var(--ink); border-color: var(--line); }
    .layout { display: grid; grid-template-columns: minmax(0, 1fr); gap: 14px; align-items: start; }
    .graph-only-layout { min-height: calc(100vh - 24px); }
    .graph-card, .panel, .section, .tree-card, .drill-card, .explorer-card, .relation-drawer {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      box-shadow: var(--shadow);
    }
    .tree-card, .drill-card, .explorer-card {
      padding: 16px;
      margin-bottom: 14px;
    }
    .graph-card { overflow: hidden; }
    .relation-drawer {
      margin: 14px 0;
      overflow: hidden;
    }
    .relation-drawer > summary {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      padding: 13px 16px;
      cursor: pointer;
      list-style: none;
      border-bottom: 1px solid transparent;
    }
    .relation-drawer[open] > summary { border-bottom-color: var(--line); }
    .relation-drawer > summary::-webkit-details-marker { display: none; }
    .relation-title { display: grid; gap: 3px; }
    .graph-controls {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
      width: 100%;
    }
    .graph-head {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: flex-start;
      gap: 10px;
      padding: 12px 14px;
      border-bottom: 1px solid var(--line);
      background: linear-gradient(180deg, #ffffff, #f8fbff);
    }
    .metrics, .legend, .actions { display: flex; flex-wrap: wrap; gap: 7px; align-items: center; }
    .metric {
      display: inline-flex;
      align-items: baseline;
      gap: 5px;
      border: 1px solid var(--line);
      border-radius: 999px;
      background: #fff;
      color: var(--muted);
      padding: 4px 8px;
      font-size: 12px;
    }
    .metric b { color: var(--ink); font-size: 14px; }
    .swatch { width: 11px; height: 11px; border-radius: 3px; background: var(--c); flex: 0 0 auto; }
    .chip.ok { color: var(--ok); background: #edf9f0; border-color: #b8e4c2; }
    .chip.warn { color: var(--warn); background: #fff7e9; border-color: #efd1a0; }
    .chip.bad { color: var(--danger); background: #fff0f0; border-color: #efbdbd; }
    .chip.info { color: #2563eb; background: #eef4ff; border-color: #c9dcff; }
    .boot-error {
      display: none;
      margin: 10px 14px;
      border: 1px solid #efbdbd;
      border-radius: 8px;
      background: #fff0f0;
      color: #8f1515;
      padding: 10px 12px;
      font-size: 12px;
      white-space: pre-wrap;
    }
    .viewport {
      position: relative;
      height: calc(100vh - 186px);
      min-height: 620px;
      overflow: hidden;
      cursor: grab;
      touch-action: none;
      user-select: none;
      background:
        radial-gradient(circle at 1px 1px, rgba(100, 116, 139, .16) 1px, transparent 0) 0 0 / 24px 24px,
        linear-gradient(180deg, #fbfdff 0%, #eef5fb 100%);
    }
    .viewport.panning { cursor: grabbing; }
    .viewport.drilling {
      cursor: default;
      overflow: auto;
      touch-action: auto;
      user-select: text;
    }
    .viewport.drilling .graph-layer { display: none; }
    .graph-layer {
      position: absolute;
      left: 0;
      top: 0;
      width: 1360px;
      height: 760px;
      transform-origin: 0 0;
    }
    .drill-layer {
      display: none;
      position: absolute;
      inset: 0;
      min-height: 100%;
      padding: 14px;
      overflow: auto;
      background:
        radial-gradient(circle at 1px 1px, rgba(100, 116, 139, .14) 1px, transparent 0) 0 0 / 22px 22px,
        linear-gradient(180deg, #ffffff 0%, #f4f8fc 100%);
    }
    .viewport.drilling .drill-layer { display: block; }
    .edge-layer {
      position: absolute;
      inset: 0;
      width: 1360px;
      height: 760px;
      pointer-events: none;
      overflow: visible;
    }
    .edge {
      fill: none;
      stroke: #9aa9bd;
      stroke-width: 2.1;
      opacity: .42;
      marker-end: url(#drillArrow);
    }
    .expand-line {
      fill: none;
      stroke: var(--c);
      stroke-width: 1.7;
      stroke-dasharray: 5 5;
      opacity: .50;
    }
    .edge.active { opacity: .95; stroke: #111827; stroke-width: 3; }
    .edge.dimmed { opacity: .10; }
    .edge-label {
      fill: #475569;
      font-size: 11px;
      paint-order: stroke;
      stroke: #fff;
      stroke-width: 4px;
      stroke-linejoin: round;
      pointer-events: none;
    }
    .edge-label.dimmed { opacity: .12; }
    .node-layer { position: absolute; inset: 0; }
    .module-node {
      position: absolute;
      width: 214px;
      min-height: 116px;
      transform: translate3d(-50%, -50%, 0);
      border: 2px solid color-mix(in srgb, var(--c) 45%, #cbd5e1);
      border-radius: 8px;
      background: rgba(255, 255, 255, .96);
      box-shadow: 0 10px 25px rgba(15, 23, 42, .11);
      padding: 11px;
      cursor: grab;
      user-select: none;
      touch-action: none;
      display: grid;
      gap: 7px;
    }
    .module-node:active { cursor: grabbing; }
    .module-node.selected {
      border-color: #111827;
      box-shadow: 0 0 0 4px color-mix(in srgb, var(--c) 20%, transparent), 0 16px 38px rgba(15, 23, 42, .22);
      z-index: 4;
    }
    .module-node.expanded {
      background: linear-gradient(180deg, #fff, color-mix(in srgb, var(--c) 6%, #fff));
      width: 560px;
      min-height: 360px;
      max-height: 520px;
      overflow: auto;
      z-index: 8;
    }
    .module-node.neighbor { box-shadow: 0 0 0 3px color-mix(in srgb, var(--c) 16%, transparent), 0 12px 30px rgba(15, 23, 42, .16); }
    .module-node.dimmed { opacity: .22; }
    .module-node.hidden { display: none; }
    .module-node .top { display: flex; justify-content: space-between; gap: 8px; align-items: center; }
    .module-node .kind { color: var(--muted); font-size: 10px; text-transform: uppercase; letter-spacing: .05em; }
    .module-node .title {
      display: -webkit-box;
      font-weight: 750;
      font-size: 15px;
      line-height: 1.15;
      -webkit-line-clamp: 2;
      -webkit-box-orient: vertical;
      overflow: hidden;
      overflow-wrap: anywhere;
      white-space: normal;
    }
    .module-node .desc { color: #465569; font-size: 12px; }
    .module-node .counts { display: flex; flex-wrap: wrap; gap: 5px; }
    .node-sections {
      display: grid;
      gap: 8px;
      border-top: 1px solid var(--line);
      padding-top: 8px;
    }
    .node-section {
      border: 1px solid color-mix(in srgb, var(--section-color) 28%, #dce5f3);
      border-left: 4px solid var(--section-color);
      border-radius: 8px;
      background: rgba(255, 255, 255, .80);
      padding: 7px;
      display: grid;
      gap: 6px;
    }
    .node-section-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
    }
    .node-section-title {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      font-weight: 750;
      font-size: 12px;
    }
    .node-file-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 5px;
    }
    .node-file {
      display: grid;
      gap: 3px;
      border: 1px solid #e1e8f2;
      border-radius: 6px;
      background: #f8fafc;
      padding: 6px;
      min-width: 0;
    }
    .node-file-top {
      display: flex;
      align-items: center;
      gap: 5px;
      min-width: 0;
    }
    .node-file-name {
      font-weight: 650;
      font-size: 11px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .node-file-path {
      color: #64748b;
      font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
      font-size: 10px;
      line-height: 1.2;
      overflow-wrap: anywhere;
    }
    .node-file .tag { font-size: 9px; min-height: 17px; padding: 1px 5px; }
    .expand-toggle {
      width: fit-content;
      height: 28px;
      padding: 0 8px;
      border-radius: 6px;
      background: #fff;
      color: var(--ink);
      border-color: color-mix(in srgb, var(--c) 40%, var(--line));
      font-size: 12px;
    }
    .child-node {
      position: absolute;
      width: 178px;
      min-height: 86px;
      transform: translate3d(-50%, -50%, 0);
      border: 1px solid color-mix(in srgb, var(--c) 48%, #cbd5e1);
      border-top: 4px solid var(--c);
      border-radius: 8px;
      background: rgba(255, 255, 255, .96);
      box-shadow: 0 9px 20px rgba(15, 23, 42, .10);
      padding: 9px;
      display: grid;
      gap: 5px;
      cursor: pointer;
      z-index: 3;
    }
    .child-node.selected {
      border-color: #111827;
      box-shadow: 0 0 0 4px color-mix(in srgb, var(--c) 18%, transparent), 0 12px 26px rgba(15, 23, 42, .18);
    }
    .child-node.dimmed { opacity: .24; }
    .child-node .top { display: flex; align-items: center; gap: 7px; }
    .child-node .title { font-weight: 700; font-size: 13px; line-height: 1.15; }
    .child-node .desc { color: #526070; font-size: 11px; }
    .child-node.file-node {
      width: 154px;
      min-height: 62px;
      border-top-width: 3px;
      padding: 7px;
      z-index: 2;
    }
    .child-node.file-node .title { font-size: 11px; }
    .path-mini {
      color: #64748b;
      font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
      font-size: 10px;
      line-height: 1.2;
      overflow-wrap: anywhere;
    }
    .icon {
      width: 20px;
      height: 20px;
      border-radius: 6px;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      color: var(--c);
      background: color-mix(in srgb, var(--c) 10%, #fff);
      border: 1px solid color-mix(in srgb, var(--c) 34%, #dbe4f0);
      flex: 0 0 auto;
    }
    .icon svg { width: 13px; height: 13px; stroke: currentColor; fill: none; stroke-width: 2; stroke-linecap: round; stroke-linejoin: round; }
    .tags { display: flex; flex-wrap: wrap; gap: 4px; }
    .tag {
      display: inline-flex;
      min-height: 19px;
      align-items: center;
      border-radius: 999px;
      border: 1px solid color-mix(in srgb, var(--c) 30%, #dbe4f0);
      background: color-mix(in srgb, var(--c) 8%, #fff);
      color: #405066;
      padding: 2px 6px;
      font-size: 10px;
      white-space: nowrap;
    }
    .panel {
      position: sticky;
      top: 82px;
      padding: 14px;
      display: grid;
      gap: 13px;
      max-height: calc(100vh - 100px);
      overflow: auto;
    }
    .block { display: grid; gap: 7px; }
    .muted { color: var(--muted); font-size: 12px; }
    .file {
      font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
      font-size: 12px;
      overflow-wrap: anywhere;
    }
    .list { display: grid; gap: 7px; }
    .mini-card {
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--soft);
      padding: 9px;
      display: grid;
      gap: 5px;
    }
    .section { padding: 16px; margin-top: 14px; }
    .section-head {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      gap: 10px;
      align-items: flex-start;
      margin-bottom: 12px;
    }
    .tree-shell { display: grid; gap: 8px; }
    .tree-root, .tree-branch {
      border: 1px solid var(--line);
      border-radius: 8px;
      background: #fff;
      overflow: hidden;
    }
    .tree-root { border-color: #cbd5e1; }
    .tree-branch {
      margin: 8px 0 0 28px;
      border-left: 4px solid var(--c);
    }
    .tree-summary {
      list-style: none;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      padding: 10px 12px;
      background: color-mix(in srgb, var(--c, #172033) 5%, #fff);
    }
    .tree-summary::-webkit-details-marker { display: none; }
    .tree-summary-main {
      display: flex;
      align-items: center;
      gap: 8px;
      min-width: 0;
    }
    .tree-summary-title { display: grid; gap: 2px; min-width: 0; }
    .tree-summary-title strong { line-height: 1.15; }
    .tree-children {
      border-left: 1px solid #d9e2ef;
      margin-left: 20px;
      padding: 0 0 10px 10px;
    }
    .tree-items {
      display: grid;
      gap: 6px;
      padding: 8px 10px 10px;
    }
    .tree-file {
      display: grid;
      grid-template-columns: 22px minmax(0, 1fr) minmax(170px, .55fr);
      gap: 8px;
      align-items: start;
      border: 1px solid #e1e8f2;
      border-radius: 7px;
      background: #f8fafc;
      padding: 8px;
      cursor: pointer;
    }
    .tree-file:hover, .tree-file.active {
      border-color: color-mix(in srgb, var(--c) 44%, #cbd5e1);
      background: color-mix(in srgb, var(--c) 7%, #fff);
    }
    .tree-file-main { display: grid; gap: 4px; min-width: 0; }
    .tree-file-title {
      display: flex;
      flex-wrap: wrap;
      gap: 6px;
      align-items: center;
      min-width: 0;
    }
    .tree-file-title strong { font-size: 13px; line-height: 1.2; }
    .tree-path {
      font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
      font-size: 11px;
      color: #475569;
      overflow-wrap: anywhere;
    }
    .tree-role {
      display: grid;
      gap: 4px;
      color: #465569;
      font-size: 12px;
      min-width: 0;
    }
    .tree-empty {
      color: var(--muted);
      font-size: 12px;
      padding: 8px;
    }
    .drill-shell { display: grid; gap: 12px; }
    .drill-nav {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: #f8fafc;
      padding: 10px;
    }
    .crumbs { display: flex; flex-wrap: wrap; gap: 6px; align-items: center; color: var(--muted); font-size: 12px; }
    .crumb {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      border: 1px solid #dce5f3;
      border-radius: 999px;
      background: #fff;
      padding: 3px 8px;
      color: #334155;
    }
    .drill-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 10px;
    }
    .drill-node, .drill-file {
      min-width: 0;
      text-align: left;
      border: 1px solid color-mix(in srgb, var(--c) 30%, #dce5f3);
      border-left: 5px solid var(--c);
      border-radius: 8px;
      background: linear-gradient(180deg, #fff, color-mix(in srgb, var(--c) 5%, #fff));
      color: var(--ink);
      padding: 11px;
      display: grid;
      gap: 8px;
      box-shadow: 0 8px 18px rgba(15, 23, 42, .07);
    }
    .drill-node {
      cursor: pointer;
      min-height: 142px;
    }
    .drill-node:hover, .drill-node:focus-visible {
      outline: 0;
      border-color: color-mix(in srgb, var(--c) 58%, #94a3b8);
      box-shadow: 0 0 0 4px color-mix(in srgb, var(--c) 14%, transparent), 0 13px 26px rgba(15, 23, 42, .12);
      transform: translateY(-1px);
    }
    .drill-top { display: flex; justify-content: space-between; gap: 8px; align-items: flex-start; }
    .drill-title { display: flex; gap: 8px; align-items: center; min-width: 0; }
    .drill-title strong { font-size: 14px; line-height: 1.2; overflow-wrap: anywhere; }
    .drill-desc { color: #465569; font-size: 12px; }
    .drill-file {
      grid-template-columns: 24px minmax(0, 1fr);
      min-height: 0;
    }
    .drill-file-body { display: grid; gap: 5px; min-width: 0; }
    .drill-path {
      font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
      font-size: 12px;
      overflow-wrap: anywhere;
    }
    .drill-empty {
      border: 1px dashed var(--line);
      border-radius: 8px;
      background: #f8fafc;
      color: var(--muted);
      padding: 12px;
    }
    .explorer-layout { display: block; }
    .explorer-sub {
      color: var(--muted);
      font-size: 11px;
      font-weight: 500;
    }
    .graph-tree-board {
      min-height: 100%;
      border: 1px solid var(--line);
      border-radius: 8px;
      background:
        radial-gradient(circle at 1px 1px, rgba(100, 116, 139, .14) 1px, transparent 0) 0 0 / 22px 22px,
        linear-gradient(180deg, #ffffff 0%, #f4f8fc 100%);
      padding: 14px;
      overflow: auto;
    }
    .drill-layer.graph-tree-board {
      border: 0;
      border-radius: 0;
      background: transparent;
      padding: 14px;
      overflow: hidden;
    }
    .tree-level { display: grid; gap: 12px; }
    .tree-map-stage {
      display: grid;
      grid-template-columns: minmax(280px, 380px) minmax(360px, 1fr);
      gap: 36px;
      align-items: start;
    }
    .tree-current-column { position: relative; min-width: 0; }
    .tree-current-column.has-children:after {
      content: "";
      position: absolute;
      top: 50%;
      right: -36px;
      width: 36px;
      border-top: 2px solid #d9e2ef;
    }
    .tree-level-head {
      display: grid;
      gap: 10px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: rgba(255, 255, 255, .90);
      padding: 10px;
    }
    .tree-toolbar {
      display: flex;
      justify-content: space-between;
      gap: 10px;
      flex-wrap: wrap;
      align-items: center;
    }
    .tree-breadcrumb {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: 6px;
      color: var(--muted);
      font-size: 12px;
    }
    .tree-breadcrumb span {
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }
    .tree-breadcrumb span + span:before {
      content: "/";
      color: #94a3b8;
    }
    .tree-current-card {
      display: grid;
      gap: 9px;
      border-left: 5px solid var(--c);
      border-radius: 8px;
      background: #fff;
      padding: 11px;
      box-shadow: 0 6px 16px rgba(15, 23, 42, .08);
    }
    .tree-current-top {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      flex-wrap: wrap;
      align-items: flex-start;
    }
    .tree-current-title {
      display: flex;
      align-items: center;
      gap: 8px;
      min-width: 0;
    }
    .tree-current-title h3 {
      margin: 0;
      overflow-wrap: anywhere;
    }
    .tree-current-meta {
      display: flex;
      flex-wrap: wrap;
      gap: 7px;
      align-items: center;
    }
    .tree-children-stack {
      display: grid;
      gap: 10px;
      position: relative;
      min-width: 0;
    }
    .tree-children-stack.has-children:before {
      content: "";
      position: absolute;
      left: -18px;
      top: 30px;
      bottom: 30px;
      border-left: 2px solid #d9e2ef;
    }
    .graph-tree-node {
      position: relative;
      border: 1px solid color-mix(in srgb, var(--c) 34%, #dce5f3);
      border-left: 5px solid var(--c);
      border-radius: 8px;
      background: rgba(255, 255, 255, .96);
      box-shadow: 0 5px 14px rgba(15, 23, 42, .08);
      color: var(--ink);
      cursor: pointer;
      display: grid;
      gap: 8px;
      min-height: 118px;
      padding: 11px;
      text-align: left;
    }
    .graph-tree-node:before {
      content: "";
      position: absolute;
      left: -23px;
      top: 50%;
      width: 18px;
      border-top: 2px solid #d9e2ef;
    }
    .graph-tree-node:hover, .graph-tree-node:focus-visible {
      outline: 0;
      border-color: color-mix(in srgb, var(--c) 58%, #94a3b8);
      box-shadow: 0 0 0 4px color-mix(in srgb, var(--c) 12%, transparent), 0 10px 24px rgba(15, 23, 42, .12);
      transform: translateY(-1px);
    }
    .graph-tree-node.selected {
      border-color: #111827;
      box-shadow: 0 0 0 4px color-mix(in srgb, var(--c) 16%, transparent), 0 14px 30px rgba(15, 23, 42, .18);
    }
    .tree-node-top {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 8px;
    }
    .tree-node-title {
      display: flex;
      align-items: center;
      gap: 7px;
      min-width: 0;
      font-weight: 750;
    }
    .tree-node-title span:last-child { overflow-wrap: anywhere; }
    .tree-node-desc { color: #465569; font-size: 12px; }
    .drill-graph-shell {
      display: grid;
      gap: 10px;
      min-height: 100%;
    }
    .drill-graph-layout {
      display: grid;
      grid-template-columns: 280px minmax(0, 1fr);
      gap: 12px;
      align-items: stretch;
      min-height: calc(100% - 58px);
    }
    .drill-graph-viewport {
      position: relative;
      min-height: 720px;
      overflow: hidden;
      border-radius: 8px;
      background:
        radial-gradient(circle at 1px 1px, rgba(100, 116, 139, .16) 1px, transparent 0) 0 0 / 24px 24px,
        linear-gradient(180deg, #fbfdff 0%, #eef5fb 100%);
      cursor: grab;
      touch-action: none;
    }
    .drill-graph-viewport.panning {
      cursor: grabbing;
    }
    .drill-graph-pan {
      position: absolute;
      left: 0;
      top: 0;
      transform-origin: 0 0;
      will-change: transform;
    }
    .drill-hierarchy-panel {
      position: sticky;
      top: 58px;
      align-self: start;
      max-height: calc(100vh - 260px);
      overflow: auto;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: rgba(255, 255, 255, .94);
      box-shadow: 0 8px 20px rgba(15, 23, 42, .08);
      padding: 10px;
      display: grid;
      gap: 10px;
    }
    .hierarchy-list {
      display: grid;
      gap: 5px;
    }
    .hierarchy-node {
      min-height: 34px;
      width: 100%;
      display: grid;
      grid-template-columns: 18px minmax(0, 1fr) auto;
      align-items: center;
      gap: 7px;
      border: 1px solid #dce5f3;
      border-left: 4px solid var(--c);
      border-radius: 7px;
      background: #fff;
      color: var(--ink);
      padding: 5px 7px;
      text-align: left;
    }
    .hierarchy-node.current {
      border-color: #111827;
      background: color-mix(in srgb, var(--c) 7%, #fff);
    }
    .hierarchy-node.leaf {
      cursor: default;
      background: #f8fafc;
    }
    .hierarchy-node span {
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .hierarchy-node.depth-1 { margin-left: 10px; width: calc(100% - 10px); }
    .hierarchy-node.depth-2 { margin-left: 20px; width: calc(100% - 20px); }
    .hierarchy-node.depth-3 { margin-left: 30px; width: calc(100% - 30px); }
    .hierarchy-children {
      display: grid;
      gap: 5px;
      border-top: 1px solid var(--line);
      padding-top: 8px;
    }
    .drill-graph-toolbar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      flex-wrap: wrap;
      position: sticky;
      top: 0;
      z-index: 12;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: rgba(255, 255, 255, .94);
      padding: 10px;
      box-shadow: 0 8px 20px rgba(15, 23, 42, .08);
    }
    .drill-graph-canvas {
      position: relative;
      min-height: 720px;
      overflow: visible;
      border: 1px solid var(--line);
      border-radius: 8px;
      background:
        radial-gradient(circle at 1px 1px, rgba(100, 116, 139, .16) 1px, transparent 0) 0 0 / 24px 24px,
        linear-gradient(180deg, #fbfdff 0%, #eef5fb 100%);
    }
    .drill-edge-layer {
      position: absolute;
      inset: 0;
      width: 100%;
      height: 100%;
      pointer-events: none;
      overflow: visible;
    }
    .drill-edge {
      fill: none;
      stroke: #94a3b8;
      stroke-width: 2;
      opacity: .55;
      marker-end: url(#drillArrow);
    }
    .drill-graph-node {
      position: absolute;
      width: 260px;
      height: auto;
      min-height: 118px;
      transform: translate3d(-50%, -50%, 0);
      border: 1px solid color-mix(in srgb, var(--c) 42%, #cbd5e1);
      border-top: 4px solid var(--c);
      border-radius: 8px;
      background: rgba(255, 255, 255, .96);
      box-shadow: 0 10px 25px rgba(15, 23, 42, .10);
      color: var(--ink);
      cursor: pointer;
      display: grid;
      gap: 6px;
      padding: 12px;
      text-align: left;
      z-index: 2;
    }
    .drill-graph-node.center {
      width: 320px;
      min-height: 146px;
      border-color: #111827;
      box-shadow: 0 0 0 4px color-mix(in srgb, var(--c) 18%, transparent), 0 16px 38px rgba(15, 23, 42, .18);
      z-index: 3;
    }
    .drill-graph-node.leaf {
      border-top-style: solid;
      background: rgba(248, 250, 252, .96);
      cursor: default;
    }
    .drill-graph-node.desc-open {
      width: 340px;
      z-index: 20;
    }
    .drill-graph-node:hover, .drill-graph-node:focus-visible {
      outline: 0;
      border-color: color-mix(in srgb, var(--c) 65%, #64748b);
      box-shadow: 0 0 0 4px color-mix(in srgb, var(--c) 13%, transparent), 0 14px 30px rgba(15, 23, 42, .14);
      transform: translate3d(-50%, -50%, 0) translateY(-1px);
    }
    .drill-node-top {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
      min-width: 0;
    }
    .drill-node-title {
      display: flex;
      align-items: flex-start;
      gap: 7px;
      min-width: 0;
      width: 100%;
      font-size: 14px;
      font-weight: 750;
      line-height: 1.18;
    }
    .drill-node-title .icon { flex: 0 0 auto; }
    .drill-graph-node.center .drill-node-title { font-size: 15px; }
    .drill-graph-node.title-fit-sm .drill-node-title { font-size: 13px; }
    .drill-graph-node.title-fit-xs .drill-node-title { font-size: 12px; }
    .drill-graph-node.title-fit-xxs .drill-node-title { font-size: 11px; }
    .drill-node-title span:last-child {
      display: -webkit-box;
      -webkit-line-clamp: 2;
      -webkit-box-orient: vertical;
      overflow: hidden;
      overflow-wrap: anywhere;
      text-overflow: clip;
      white-space: normal;
    }
    .drill-node-desc {
      color: #465569;
      font-size: 12px;
      display: -webkit-box;
      -webkit-line-clamp: 2;
      -webkit-box-orient: vertical;
      overflow: hidden;
    }
    .drill-graph-node.desc-open .drill-node-desc {
      display: block;
      max-height: 180px;
      overflow: auto;
      -webkit-line-clamp: initial;
      -webkit-box-orient: initial;
    }
    .drill-node-more {
      align-items: center;
      background: transparent;
      border: 1px solid #cbd5e1;
      border-radius: 6px;
      color: #334155;
      cursor: pointer;
      display: inline-flex;
      font-size: 11px;
      font-weight: 700;
      justify-content: center;
      min-height: 26px;
      padding: 4px 7px;
      width: fit-content;
    }
    .drill-node-more:hover, .drill-node-more:focus-visible {
      outline: 0;
      border-color: color-mix(in srgb, var(--c) 55%, #111827);
      box-shadow: 0 0 0 3px color-mix(in srgb, var(--c) 16%, transparent);
    }
    .drill-node-subtitle {
      color: #64748b;
      font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
      font-size: 10px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .drill-node-path {
      border: 1px solid #dbe5f1;
      border-radius: 6px;
      background: #fff;
      color: #334155;
      font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
      font-size: 10px;
      overflow: hidden;
      padding: 5px 6px;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .drill-node-actions {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 6px;
    }
    .drill-node-action {
      align-items: center;
      background: #111827;
      border: 1px solid #111827;
      border-radius: 6px;
      color: #fff;
      cursor: pointer;
      display: inline-flex;
      font-size: 11px;
      font-weight: 700;
      justify-content: center;
      min-height: 28px;
      padding: 5px 7px;
      text-decoration: none;
    }
    .drill-node-action.secondary {
      background: #fff;
      color: #334155;
      border-color: #cbd5e1;
    }
    .drill-node-action:hover, .drill-node-action:focus-visible {
      outline: 0;
      border-color: color-mix(in srgb, var(--c) 55%, #111827);
      box-shadow: 0 0 0 3px color-mix(in srgb, var(--c) 16%, transparent);
    }
    .explorer-detail-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(190px, 1fr));
      gap: 8px;
    }
    .explorer-child-list {
      display: grid;
      gap: 7px;
    }
    .explorer-child {
      display: grid;
      gap: 4px;
      border: 1px solid var(--line);
      border-left: 4px solid var(--c);
      border-radius: 7px;
      background: #f8fafc;
      padding: 8px;
      cursor: pointer;
    }
    .explorer-child:hover {
      background: color-mix(in srgb, var(--c) 7%, #fff);
      border-color: color-mix(in srgb, var(--c) 34%, #dce5f3);
    }
    .queue-grid, .protocol-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 10px; }
    .queue-card {
      border: 1px solid var(--line);
      border-top: 4px solid var(--c);
      border-radius: 8px;
      background: #fff;
      padding: 11px;
      display: grid;
      gap: 8px;
    }
    .queue-card.hidden { display: none; }
    table { width: 100%; border-collapse: collapse; }
    th, td { border-top: 1px solid var(--line); padding: 8px 10px; text-align: left; vertical-align: top; }
    th { background: #f2f5fb; color: var(--muted); font-size: 12px; font-weight: 600; }
    @media (max-width: 1160px) {
      .layout { grid-template-columns: 1fr; }
      .drill-graph-layout { grid-template-columns: 1fr; }
      .drill-hierarchy-panel { position: static; max-height: none; }
      .tree-map-stage { grid-template-columns: 1fr; gap: 12px; }
      .tree-current-column:after, .tree-children-stack.has-children:before, .graph-tree-node:before { display: none; }
      .panel { position: static; max-height: none; }
      .viewport { height: 68vh; }
      .tree-file { grid-template-columns: 22px minmax(0, 1fr); }
      .tree-role { grid-column: 2; }
    }
    @media (max-width: 720px) {
      .shell { width: min(100% - 18px, 1620px); }
      .toolbar { position: static; }
      input, select, button { width: 100%; }
      .viewport { min-height: 560px; }
      table, tbody, tr, td { display: block; width: 100%; }
      thead { display: none; }
      tr { border-top: 1px solid var(--line); padding: 7px 0; }
      td { border-top: 0; padding: 6px 10px; }
    }
  </style>
</head>
<body>
  <div class="shell">
      <div class="layout graph-only-layout">
        <section class="graph-card" aria-labelledby="graphTitle">
        <div class="graph-head">
          <div>
            <h1 id="graphTitle">ai_studio/ architecture graph</h1>
            <p class="muted">Стартовая точка рефакторинга: корень ai_studio, домены, вложенные уровни, source docs, tools, контракты и возврат через панель иерархии.</p>
          </div>
          <div class="metrics">
            <span class="metric"><b id="moduleCount">0</b> modules</span>
            <span class="metric"><b id="docCount">0</b> md sources</span>
            <span class="metric"><b id="toolCount">0</b> tools</span>
            <span class="metric"><b id="contractCount">0</b> contracts</span>
          </div>
          <div class="graph-controls">
            <input id="search" type="search" placeholder="Поиск по модулю, source, skill, utility, risk">
            <select id="moduleFilter" aria-label="Module filter"></select>
            <select id="focusMode" aria-label="Focus mode">
              <option value="local">Local graph: выбранный модуль + соседи</option>
              <option value="all">All contracts</option>
              <option value="inventory">Inventory pressure</option>
            </select>
            <button id="resetBtn" type="button">Reset layout</button>
            <button id="fitBtn" type="button" class="secondary">Fit start</button>
            <button id="zoomInBtn" type="button" class="secondary">+</button>
            <button id="zoomOutBtn" type="button" class="secondary">-</button>
            <span class="chip info" id="zoomLabel">100%</span>
          </div>
          <div class="legend" id="legend"></div>
          <div class="legend">
            <span class="chip info">color = owner/group</span>
            <span class="chip info">icon = entity type</span>
            <span class="chip info">tag = refactor decision</span>
            <span class="chip info">click opens nested tree in this viewport</span>
          </div>
        </div>
        <div class="boot-error" id="bootError"></div>
        <div class="viewport" id="viewport">
          <div class="graph-layer" id="graphLayer">
            <svg class="edge-layer" id="edgeLayer" viewBox="0 0 1360 760" aria-hidden="true">
              <defs>
                <marker id="arrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
                  <path d="M0,0 L0,6 L8,3 z" fill="#9aa9bd"></path>
                </marker>
              </defs>
            </svg>
            <div class="node-layer" id="nodeLayer"></div>
          </div>
          <div id="studioExplorerTree" class="drill-layer graph-tree-board" aria-label="Nested module tree"></div>
        </div>
        </section>
      </div>
  </div>

  <script id="graph-data" type="application/json">${dataJson}</script>
  <script>
  (() => {
    const data = JSON.parse(document.getElementById("graph-data").textContent);
    const studioTreeSource = data.studioTree || {
      id: "studio",
      title: "ai_studio/",
      subtitle: "missing tree source",
      kind: "root",
      color: "#172033",
      description: "ai_studio/tree.json was not embedded in this generated map.",
      tags: ["missing-source"],
      children: []
    };
    const refactorGroupData = data.refactorGroups || {};
    const refactorMeta = {
      hot: {
        title: "Core Harness",
        kind: "core",
        description: "Harness, agent-facing contract, AGENTS.md, short routing docs, and the smallest public entry points needed to start work."
      },
      skills: {
        description: "Reusable procedures that must stay outside Core unless they are only routing pointers."
      },
      tasks: {
        description: "Durable work state and orchestration contracts; adjacent to Core, but not the Core itself."
      },
      facade: {
        description: "Agent-facing CLI facades; Core should point here, not embed command procedure."
      },
      validation: {
        description: "Mechanical gates and validators; Core references required gates, but enforcement belongs here."
      }
    };
    const modules = data.modules.map((m) => Object.assign({}, m, refactorMeta[m.id] || {}, {
      x: Math.max(110, Math.min(1250, m.x - 40)),
      y: Math.max(80, Math.min(690, m.y - 20)),
      homeX: Math.max(110, Math.min(1250, m.x - 40)),
      homeY: Math.max(80, Math.min(690, m.y - 20)),
      pinned: false
    }));
    const refactorPositions = {
      hot: [430, 350],
      skills: [720, 110],
      tasks: [735, 285],
      facade: [735, 470],
      validation: [1040, 430],
      profile: [1030, 175],
      export: [1040, 300],
      design: [170, 590],
      assets: [390, 635],
      art: [620, 650],
      runtime: [935, 640],
      engine: [1190, 640]
    };
    modules.forEach((m) => {
      const pos = refactorPositions[m.id];
      if (!pos) return;
      m.x = pos[0];
      m.y = pos[1];
      m.homeX = pos[0];
      m.homeY = pos[1];
    });
    const moduleById = new Map(modules.map((m) => [m.id, m]));
    const edgeData = data.moduleEdges.map((e, index) => ({ id: "e" + index, from: e[0], to: e[1], label: e[2], description: e[3] }));
    const search = document.getElementById("search");
    const moduleFilter = document.getElementById("moduleFilter");
    const focusMode = document.getElementById("focusMode");
    const graphLayer = document.getElementById("graphLayer");
    const nodeLayer = document.getElementById("nodeLayer");
    const edgeLayer = document.getElementById("edgeLayer");
    const viewport = document.getElementById("viewport");
    const detail = document.getElementById("detail");
    const studioExplorerTree = document.getElementById("studioExplorerTree");
    const zoomLabel = document.getElementById("zoomLabel");
    let selected = "hot";
    let selectedGroup = "agent-contract";
    const expanded = new Set();
    let explorerSelected = "studio";
    let explorerView = "studio";
    const explorerHistory = [];
    let drillOpen = true;
    let drillDragging = null;
    let drillPanning = null;
    let drillZoom = 1;
    let drillPanX = 0;
    let drillPanY = 0;
    let suppressExplorerClick = false;
    const drillPositions = new Map();
    let dragging = null;
    let panning = null;
    let suppressGraphClick = false;
    let zoom = 0.88;
    let panX = 36;
    let panY = 22;
    const nodeEls = new Map();
    const edgeEls = new Map();
    const labelEls = new Map();

    function esc(value) {
      return String(value || "").replace(/[&<>"']/g, (ch) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[ch]));
    }
    function icon(name) {
      const paths = {
        doc: '<path d="M6 2h7l5 5v15H6z"></path><path d="M13 2v6h6"></path>',
        agent: '<path d="M12 3a4 4 0 0 1 4 4v2a4 4 0 0 1-8 0V7a4 4 0 0 1 4-4z"></path><path d="M5 21a7 7 0 0 1 14 0"></path>',
        route: '<path d="M4 6h6a4 4 0 0 1 0 8H8"></path><path d="M8 10l-4 4 4 4"></path><path d="M14 18h6"></path>',
        tool: '<path d="M14.7 6.3a4 4 0 0 0-5.4 5.4L4 17v3h3l5.3-5.3a4 4 0 0 0 5.4-5.4z"></path>',
        shield: '<path d="M12 3l8 4v5c0 5-3.4 8.4-8 9-4.6-.6-8-4-8-9V7z"></path>',
        sync: '<path d="M20 6v5h-5"></path><path d="M4 18v-5h5"></path><path d="M19 11a7 7 0 0 0-12-4"></path><path d="M5 13a7 7 0 0 0 12 4"></path>',
        group: '<path d="M4 4h7v7H4z"></path><path d="M13 4h7v7h-7z"></path><path d="M4 13h7v7H4z"></path><path d="M13 13h7v7h-7z"></path>',
        remove: '<path d="M3 6h18"></path><path d="M8 6V4h8v2"></path><path d="M7 6l1 15h8l1-15"></path>'
      };
      return '<span class="icon"><svg viewBox="0 0 24 24" aria-hidden="true">' + (paths[name] || paths.group) + '</svg></span>';
    }
    function tags(values) {
      return '<div class="tags">' + values.map((v) => '<span class="tag">' + esc(v) + '</span>').join("") + '</div>';
    }
    function inventory(id) {
      const docs = data.markdownDocs.filter((d) => d.group === id);
      const skills = docs.filter((d) => d.kind === "skill");
      const tools = data.tools.filter((t) => t.group === id);
      const publicTools = tools.filter((t) => t.surface === "public");
      const internalTools = tools.filter((t) => t.surface !== "public");
      const contracts = edgeData.filter((e) => e.from === id || e.to === id);
      return { docs, skills, tools, publicTools, internalTools, contracts };
    }
    function refactorGroups(id) {
      return refactorGroupData[id] || [];
    }
    function firstGroupId(id) {
      const groups = refactorGroups(id);
      return groups.length ? groups[0].id : null;
    }
    function risks(id) {
      const inv = inventory(id);
      const out = [];
      if (!inv.docs.length) out.push(["bad", "missing source doc"]);
      if (inv.tools.length && !inv.publicTools.length) out.push(["bad", "no public facade"]);
      if (inv.publicTools.length > 10) out.push(["warn", "wide public surface"]);
      if (inv.internalTools.length > Math.max(6, inv.publicTools.length * 3)) out.push(["warn", "heavy internals"]);
      if (!inv.skills.length && ["skills", "tasks", "design", "assets", "art", "runtime", "validation"].includes(id)) out.push(["warn", "no dedicated skill"]);
      if (!out.length) out.push(["ok", "mapped"]);
      return out;
    }
    function migrationStatus(id) {
      if (id === "hot") return ["info", "CORE: keep small"];
      if (["skills", "tasks", "facade"].includes(id)) return ["warn", "core-adjacent: define boundary"];
      if (["validation", "profile", "export"].includes(id)) return ["warn", "platform domain: route from Core"];
      if (id === "engine") return ["info", "external boundary"];
      return ["bad", "move out of Core"];
    }
    function clamp(value, min, max) {
      return Math.max(min, Math.min(max, value));
    }
    function clampZoom(value) {
      return clamp(value, 0.35, 2.2);
    }
    function applyTransform() {
      graphLayer.style.transform = "translate(" + panX + "px, " + panY + "px) scale(" + zoom + ")";
      if (zoomLabel) zoomLabel.textContent = Math.round(zoom * 100) + "%";
    }
    function applyDrillTransform() {
      const pan = studioExplorerTree ? studioExplorerTree.querySelector(".drill-graph-pan") : null;
      if (!pan) return;
      pan.style.transform = "translate(" + drillPanX + "px, " + drillPanY + "px) scale(" + drillZoom + ")";
      if (zoomLabel) zoomLabel.textContent = Math.round(drillZoom * 100) + "%";
    }
    function setZoomAt(clientX, clientY, nextZoom) {
      const rect = viewport.getBoundingClientRect();
      const graphX = (clientX - rect.left - panX) / zoom;
      const graphY = (clientY - rect.top - panY) / zoom;
      zoom = clampZoom(nextZoom);
      panX = clientX - rect.left - graphX * zoom;
      panY = clientY - rect.top - graphY * zoom;
      applyTransform();
    }
    function setDrillZoomAt(clientX, clientY, nextZoom) {
      const board = studioExplorerTree ? studioExplorerTree.querySelector(".drill-graph-viewport") : null;
      if (!board) return;
      const rect = board.getBoundingClientRect();
      const localX = clientX - rect.left;
      const localY = clientY - rect.top;
      const graphX = (localX - drillPanX) / drillZoom;
      const graphY = (localY - drillPanY) / drillZoom;
      drillZoom = clampZoom(nextZoom);
      drillPanX = localX - graphX * drillZoom;
      drillPanY = localY - graphY * drillZoom;
      applyDrillTransform();
    }
    function drillCanvasPoint(event, canvas) {
      const rect = canvas.getBoundingClientRect();
      return {
        x: (event.clientX - rect.left) / drillZoom,
        y: (event.clientY - rect.top) / drillZoom
      };
    }
    function fitStart() {
      zoom = 0.88;
      panX = 36;
      panY = 22;
      applyTransform();
    }
    function chip(cls, text) {
      return '<span class="chip ' + cls + '">' + esc(text) + '</span>';
    }
    function neighborIds(id) {
      const ids = new Set([id]);
      edgeData.forEach((e) => {
        if (e.from === id) ids.add(e.to);
        if (e.to === id) ids.add(e.from);
      });
      return ids;
    }
    function matches(m) {
      const q = search.value.trim().toLowerCase();
      if (!q) return true;
      const inv = inventory(m.id);
      return [
        m.title,
        m.kind,
        m.description,
        inv.docs.map((d) => d.path + " " + d.title).join(" "),
        inv.tools.map((t) => t.path + " " + t.description).join(" "),
        risks(m.id).map((r) => r[1]).join(" ")
      ].join(" ").toLowerCase().includes(q);
    }
    function isVisible(m) {
      return !moduleFilter.value || m.id === moduleFilter.value || neighborIds(moduleFilter.value).has(m.id);
    }
    function isDimmed(m) {
      if (!matches(m)) return true;
      if (focusMode.value === "all") return false;
      if (focusMode.value === "inventory") {
        return risks(m.id).some((r) => r[0] !== "ok") ? false : m.id !== selected;
      }
      return !neighborIds(selected).has(m.id);
    }
    function point(event) {
      const rect = viewport.getBoundingClientRect();
      return { x: (event.clientX - rect.left - panX) / zoom, y: (event.clientY - rect.top - panY) / zoom };
    }
    function renderLegend() {
      document.getElementById("legend").innerHTML = modules.map((m) => '<span class="chip"><span class="swatch" style="--c:' + esc(m.color) + '"></span>' + esc(m.title) + '</span>').join("");
      moduleFilter.innerHTML = '<option value="">Все модули</option>' + modules.map((m) => '<option value="' + esc(m.id) + '">' + esc(m.title) + '</option>').join("");
    }
    function moduleNodeHtml(m) {
      const inv = inventory(m.id);
      return '<div class="top"><span class="kind">' + esc(m.kind) + '</span><span class="swatch" style="--c:' + esc(m.color) + '"></span></div>' +
        '<div class="title">' + esc(m.title) + '</div>' +
        '<div class="desc">' + esc(m.description) + '</div>' +
        '<div class="counts">' +
          '<span class="chip">' + refactorGroups(m.id).length + ' sections</span>' +
          '<span class="chip">' + inv.docs.length + ' md</span>' +
          '<span class="chip">' + inv.publicTools.length + '/' + inv.tools.length + ' tools</span>' +
        '</div>';
    }
    function renderNodes() {
      nodeLayer.innerHTML = "";
      modules.forEach((m) => {
        const el = document.createElement("div");
        el.className = "module-node";
        el.style.setProperty("--c", m.color);
        el.dataset.id = m.id;
        el.dataset.expanded = expanded.has(m.id) ? "1" : "0";
        el.innerHTML = moduleNodeHtml(m);
        el.addEventListener("pointerdown", (event) => {
          if (event.target.closest("button")) return;
          if (event.button !== 0) return;
          const p = point(event);
          dragging = { module: m, dx: p.x - m.x, dy: p.y - m.y, clientX: event.clientX, clientY: event.clientY, moved: false };
          m.pinned = true;
          el.setPointerCapture(event.pointerId);
        });
        el.addEventListener("click", (event) => {
          if (event.target.closest("button")) return;
          if (suppressGraphClick) {
            event.stopPropagation();
            suppressGraphClick = false;
            return;
          }
          event.stopPropagation();
          openModuleDrill(m.id);
        });
        nodeLayer.appendChild(el);
        nodeEls.set(m.id, el);
      });
    }
    function renderEdges() {
      edgeData.forEach((e) => {
        const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
        path.setAttribute("class", "edge");
        edgeLayer.appendChild(path);
        edgeEls.set(e.id, path);
        const label = document.createElementNS("http://www.w3.org/2000/svg", "text");
        label.setAttribute("class", "edge-label");
        label.setAttribute("text-anchor", "middle");
        label.textContent = e.label;
        edgeLayer.appendChild(label);
        labelEls.set(e.id, label);
      });
    }
    function groupMatches(group) {
      const q = search.value.trim().toLowerCase();
      if (!q) return true;
      return [
        group.title,
        group.description,
        group.tags.join(" "),
        group.items.map((item) => [item.title, item.path, item.role, item.description, item.tags.join(" ")].join(" ")).join(" ")
      ].join(" ").toLowerCase().includes(q);
    }
    function childOffset(index, count) {
      const coreOffsets = [
        [-280, -150], [0, -190], [280, -150],
        [-280, 150], [0, 200], [280, 150]
      ];
      if (count <= coreOffsets.length) return coreOffsets[index];
      const angle = -Math.PI / 2 + (index / count) * Math.PI * 2;
      const radius = 260;
      return [Math.cos(angle) * radius, Math.sin(angle) * radius];
    }
    function itemOffset(parent, groupX, groupY, index, count) {
      let dx = groupX - parent.x;
      let dy = groupY - parent.y;
      let len = Math.sqrt(dx * dx + dy * dy) || 1;
      let ux = dx / len;
      let uy = dy / len;
      let px = -uy;
      let py = ux;
      const row = Math.floor(index / 2);
      const side = index % 2 === 0 ? -1 : 1;
      const spread = count === 1 ? 0 : side * 76;
      return [
        groupX + ux * (88 + row * 58) + px * spread,
        groupY + uy * (70 + row * 46) + py * side * 30
      ];
    }
    function itemTitle(item) {
      const raw = item.path || item.title || "";
      const parts = raw.split("/");
      return parts[parts.length - 1] || item.title || raw;
    }
    function visibleGroupsForNode(id) {
      const groups = refactorGroups(id);
      if (id === "hot") return groups;
      return groups.filter((g) => ["source-docs", "skills", "public-tools", "contracts"].includes(g.id)).slice(0, 4);
    }
    function renderInlineItem(item) {
      return '<div class="node-file">' +
        '<div class="node-file-top">' + itemIcon(item.kind) + '<span class="node-file-name">' + esc(itemTitle(item)) + '</span></div>' +
        '<div class="node-file-path">' + esc(item.path || item.title) + '</div>' +
        tags((item.tags || []).slice(0, 3)) +
      '</div>';
    }
    function renderInlineSections(id) {
      const groups = visibleGroupsForNode(id);
      if (!groups.length) return "";
      return '<div class="node-sections">' + groups.map((group) => {
        const items = group.items.slice(0, id === "hot" ? 6 : 4);
        const more = group.items.length > items.length ? '<span class="chip">+' + (group.items.length - items.length) + ' more</span>' : "";
        return '<section class="node-section" style="--section-color:' + esc(group.color) + '">' +
          '<div class="node-section-head">' +
            '<div class="node-section-title">' + icon(group.icon) + '<span>' + esc(group.title) + '</span></div>' +
            '<div class="legend"><span class="chip">' + group.items.length + ' items</span>' + more + '</div>' +
          '</div>' +
          tags(group.tags.slice(0, 4)) +
          '<div class="node-file-grid">' + items.map(renderInlineItem).join("") + '</div>' +
        '</section>';
      }).join("") + '</div>';
    }
    function selectExplorerModule(moduleId, groupId) {
      const root = buildExplorerRoot();
      const wanted = groupId ? "group:" + moduleId + ":" + groupId : "module:" + moduleId;
      const fallback = "module:" + moduleId;
      explorerSelected = findExplorerNode(root, wanted) ? wanted : fallback;
      explorerView = explorerSelected;
      explorerHistory.length = 0;
    }
    function syncDrillToSelected(groupId) {
      selectExplorerModule(selected, groupId || "");
    }
    function setDrillOpen(value) {
      drillOpen = Boolean(value);
      viewport.classList.toggle("drilling", drillOpen);
      if (!drillOpen) {
        explorerHistory.length = 0;
        explorerSelected = "studio";
        explorerView = "studio";
      }
    }
    function openExplorerNode(id, pushHistory) {
      const root = buildExplorerRoot();
      const node = findExplorerNode(root, id);
      if (!node) return;
      if (!drillOpen) {
        explorerHistory.length = 0;
      } else if (pushHistory && explorerView !== id) {
        explorerHistory.push(explorerView);
      }
      explorerSelected = id;
      explorerView = id;
      syncSelectionFromExplorer(node);
      setDrillOpen(true);
      updateAll();
    }
    function openModuleDrill(moduleId) {
      selected = moduleId;
      selectedGroup = firstGroupId(moduleId);
      moduleFilter.value = "";
      openExplorerNode("module:" + moduleId, false);
    }
    function cloneStudioTree() {
      return JSON.parse(JSON.stringify(studioTreeSource));
    }
    function buildExplorerRoot() {
      return cloneStudioTree();
    }
    function findExplorerNode(node, id) {
      if (!node) return null;
      if (node.id === id) return node;
      for (const child of node.children || []) {
        const found = findExplorerNode(child, id);
        if (found) return found;
      }
      return null;
    }
    function findExplorerParent(node, id, parent) {
      if (!node) return null;
      if (node.id === id) return parent || null;
      for (const child of node.children || []) {
        const found = findExplorerParent(child, id, node);
        if (found) return found;
      }
      return null;
    }
    function explorerNodeIcon(node) {
      if (node.kind === "module") return icon("group");
      if (node.kind === "section") return icon("route");
      if (node.kind === "tool") return icon("tool");
      if (node.kind === "contract") return icon("shield");
      if (node.kind === "doc" || node.kind === "target" || node.kind === "current") return icon("doc");
      return icon("group");
    }
    function syncSelectionFromExplorer(node) {
      if (!node) return;
      if (node.moduleId) {
        selected = node.moduleId;
        selectedGroup = node.groupId || firstGroupId(selected);
        moduleFilter.value = "";
      }
    }
    function findExplorerPath(node, id, path) {
      if (!node) return [];
      const nextPath = (path || []).concat(node);
      if (node.id === id) return nextPath;
      for (const child of node.children || []) {
        const found = findExplorerPath(child, id, nextPath);
        if (found.length) return found;
      }
      return [];
    }
    function activateExplorerNode(id) {
      openExplorerNode(id, true);
    }
    function copyText(value, button) {
      const text = String(value || "");
      const done = () => {
        if (!button) return;
        const previous = button.textContent;
        button.textContent = "Copied";
        window.setTimeout(() => { button.textContent = previous; }, 1100);
      };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done).catch(() => {
          fallbackCopyText(text);
          done();
        });
        return;
      }
      fallbackCopyText(text);
      done();
    }
    function fallbackCopyText(value) {
      const input = document.createElement("textarea");
      input.value = value;
      input.setAttribute("readonly", "");
      input.style.position = "fixed";
      input.style.left = "-9999px";
      document.body.appendChild(input);
      input.select();
      document.execCommand("copy");
      input.remove();
    }
    function explorerBack() {
      const root = buildExplorerRoot();
      if (explorerHistory.length) {
        explorerView = explorerHistory.pop();
      } else {
        setDrillOpen(false);
        updateAll();
        return;
      }
      explorerSelected = explorerView;
      syncSelectionFromExplorer(findExplorerNode(root, explorerSelected));
      updateAll();
    }
    function hierarchyDepthClass(index) {
      return "depth-" + Math.min(3, Math.max(0, index));
    }
    function renderHierarchyRow(node, index, currentId) {
      const isCurrent = node.id === currentId;
      const hasChildren = node.children && node.children.length;
      const clickable = !isCurrent && hasChildren;
      const tag = clickable ? "button" : "div";
      const attrs = clickable ? ' type="button" data-explorer-card="' + esc(node.id) + '"' : "";
      return '<' + tag + ' class="hierarchy-node ' + hierarchyDepthClass(index) + (isCurrent ? ' current' : '') + (!hasChildren ? ' leaf' : '') + '" style="--c:' + esc(node.color) + '"' + attrs + '>' +
        explorerNodeIcon(node) +
        '<span>' + esc(node.title) + '</span>' +
        '<small>' + esc(node.kind) + '</small>' +
      '</' + tag + '>';
    }
    function breadcrumbTitle(node) {
      return node.id === "studio" ? "ai_studio" : node.title;
    }
    function renderHierarchyPanel(current, path, visibleChildren) {
      const displayPath = path.length ? path : [current];
      const rows = displayPath.map((node, index) => renderHierarchyRow(node, index, current.id)).join("");
      const childRows = visibleChildren.slice(0, 18).map((node) => renderHierarchyRow(node, Math.min(3, displayPath.length), "")).join("");
      const more = visibleChildren.length > 18 ? '<p class="muted">+' + (visibleChildren.length - 18) + ' hidden by panel limit; use graph or search.</p>' : "";
      const canBack = current.id !== "studio";
      return '<aside class="drill-hierarchy-panel" aria-label="Hierarchy">' +
        '<div class="actions">' + (canBack ? '<button type="button" class="secondary" data-explorer-back="1">Назад</button>' : '') + '<button type="button" class="secondary" data-explorer-close="1">Граф связей</button></div>' +
        '<div class="block"><h3>Иерархия</h3><div class="hierarchy-list">' + rows + '</div></div>' +
        '<div class="hierarchy-children"><h3>Внутри уровня</h3>' + (childRows || '<p class="muted">Внутренних узлов нет.</p>') + more + '</div>' +
      '</aside>';
    }
    function explorerNodeMatches(node, query) {
      if (!query) return true;
      const own = [
        node.title,
        node.subtitle,
        node.kind,
        node.description,
        (node.tags || []).join(" "),
        node.href || ""
      ].join(" ").toLowerCase();
      if (own.includes(query)) return true;
      return (node.children || []).some((child) => explorerNodeMatches(child, query));
    }
    function renderExplorer() {
      if (!studioExplorerTree) return;
      if (!drillOpen) {
        studioExplorerTree.innerHTML = "";
        return;
      }
      const root = buildExplorerRoot();
      if (!findExplorerNode(root, explorerSelected)) explorerSelected = "studio";
      if (!findExplorerNode(root, explorerView)) explorerView = "studio";
      const current = findExplorerNode(root, explorerView) || root;
      const children = current.children || [];
      const query = search.value.trim().toLowerCase();
      const visibleChildren = children.filter((child) => explorerNodeMatches(child, query));
      const path = findExplorerPath(root, current.id, []);
      const displayPath = path.length ? path : [root];
      const pathText = displayPath.map(breadcrumbTitle).join(" / ");
      const viewTitle = current.id === "studio" ? "Main Refactor Tree" : current.title;
      studioExplorerTree.innerHTML =
        '<div class="drill-graph-shell">' +
          '<div class="drill-graph-toolbar">' +
            '<div><h2>' + esc(viewTitle) + '</h2><p class="muted">' + esc(pathText) + '</p></div>' +
            '<div class="legend"><span class="chip">' + visibleChildren.length + ' visible</span><span class="chip">' + children.length + ' children</span><span class="chip">' + esc(current.kind) + '</span></div>' +
          '</div>' +
          '<div class="drill-graph-layout">' +
            renderHierarchyPanel(current, path, visibleChildren) +
            (current.id === "studio" ? renderTopLevelDrillGraph(visibleChildren, children.length) : renderDrillGraph(current, visibleChildren, children.length)) +
          '</div>' +
        '</div>';
      applyDrillTransform();
    }
    function drillPositionKey(viewId, nodeId) {
      return viewId + "::" + nodeId;
    }
    function resolvedDrillPosition(viewId, node, x, y) {
      const key = drillPositionKey(viewId, node.id);
      if (!drillPositions.has(key)) drillPositions.set(key, { x, y });
      return drillPositions.get(key);
    }
    function nodePath(node) {
      if (node.item && node.item.path) return node.item.path;
      if (node.href) return node.href;
      return node.subtitle || node.title || "";
    }
    function renderLeafActions(node) {
      const path = nodePath(node);
      const open = node.href
        ? '<a class="drill-node-action" href="' + esc(node.href) + '" target="_blank" rel="noopener">Open file</a>'
        : "";
      const copy = path
        ? '<button type="button" class="drill-node-action secondary" data-copy-path="' + esc(path) + '">Copy path</button>'
        : "";
      if (!open && !copy) return "";
      return (path ? '<div class="drill-node-path" title="' + esc(path) + '">' + esc(path) + '</div>' : '') +
        '<div class="drill-node-actions">' + open + copy + '</div>';
    }
    function renderDescriptionToggle(node, role, hasChildren) {
      const text = node.description || "";
      const canToggle = text.trim().length > 0;
      return canToggle
        ? '<button type="button" class="drill-node-more" data-toggle-description="1" aria-expanded="false">More</button>'
        : "";
    }
    function drillTitleFitClass(title) {
      const text = String(title || "");
      const longestPart = text.split(/[\s._/\\:-]+/).reduce((max, part) => Math.max(max, part.length), 0);
      if (text.length > 68 || longestPart > 34) return " title-fit-xxs";
      if (text.length > 52 || longestPart > 28) return " title-fit-xs";
      if (text.length > 34 || longestPart > 20) return " title-fit-sm";
      return "";
    }
    function renderDrillGraphNode(node, x, y, role) {
      const hasChildren = node.children && node.children.length;
      const className = "drill-graph-node" + (role === "center" ? " center" : "") + (!hasChildren && role !== "center" ? " leaf" : "") + drillTitleFitClass(node.title);
      const canOpen = role !== "center" && hasChildren;
      const tag = "div";
      const cardAttr = canOpen ? ' role="button" tabindex="0" data-explorer-card="' + esc(node.id) + '"' : "";
      const leafActions = !hasChildren && role !== "center" ? renderLeafActions(node) : "";
      const descToggle = renderDescriptionToggle(node, role, hasChildren);
      return '<' + tag + ' class="' + className + '" style="--c:' + esc(node.color) + '; left:' + Math.round(x) + 'px; top:' + Math.round(y) + 'px" data-drill-id="' + esc(node.id) + '"' + cardAttr + '>' +
        '<div class="drill-node-top">' +
          '<div class="drill-node-title">' + explorerNodeIcon(node) + '<span>' + esc(node.title) + '</span></div>' +
        '</div>' +
        (node.subtitle ? '<div class="drill-node-subtitle">' + esc(node.subtitle) + '</div>' : '') +
        '<p class="drill-node-desc">' + esc(node.description || "No description yet.") + '</p>' +
        descToggle +
        leafActions +
        '<div class="metrics"><span class="metric"><b>' + (node.children ? node.children.length : 0) + '</b> children</span><span class="metric"><b>' + esc(node.kind) + '</b></span></div>' +
      '</' + tag + '>';
    }
    function drillGraphLayout(children) {
      const placements = [];
      const ringCaps = [8, 14, 20, 28, 36];
      let offsetIndex = 0;
      let maxRadius = 0;
      ringCaps.forEach((cap, ring) => {
        const remaining = children.length - offsetIndex;
        if (remaining <= 0) return;
        const count = Math.min(cap, remaining);
        const radius = 230 + ring * 170;
        maxRadius = Math.max(maxRadius, radius);
        for (let i = 0; i < count; i++) {
          const angle = -Math.PI / 2 + (i / count) * Math.PI * 2 + (ring % 2 ? Math.PI / count : 0);
          placements.push({ node: children[offsetIndex + i], ox: Math.cos(angle) * radius, oy: Math.sin(angle) * radius });
        }
        offsetIndex += count;
      });
      const overflow = children.slice(offsetIndex);
      overflow.forEach((node, i) => {
        const ring = ringCaps.length;
        const count = overflow.length;
        const radius = 230 + ring * 170;
        maxRadius = Math.max(maxRadius, radius);
        const angle = -Math.PI / 2 + (i / Math.max(1, count)) * Math.PI * 2;
        placements.push({ node, ox: Math.cos(angle) * radius, oy: Math.sin(angle) * radius });
      });
      maxRadius = Math.max(maxRadius, children.length ? 230 : 0);
      const width = Math.max(1120, Math.ceil(maxRadius * 2 + 560));
      const height = Math.max(720, Math.ceil(maxRadius * 2 + 320));
      const cx = width / 2;
      const cy = height / 2;
      return { width, height, cx, cy, placements: placements.map((p) => Object.assign({}, p, { x: cx + p.ox, y: cy + p.oy })) };
    }
    function topLevelGraphLayout(children) {
      const width = 1120;
      const height = 720;
      const cx = width / 2;
      const cy = height / 2;
      const presets = {
        1: [[cx, cy]],
        2: [[cx - 190, cy], [cx + 190, cy]],
        3: [[cx, cy - 170], [cx - 220, cy + 130], [cx + 220, cy + 130]],
        4: [[cx - 220, cy - 140], [cx + 220, cy - 140], [cx - 220, cy + 140], [cx + 220, cy + 140]]
      };
      const preset = presets[children.length];
      const placements = children.map((node, index) => {
        const pos = preset
          ? preset[index]
          : [cx + Math.cos(-Math.PI / 2 + (index / children.length) * Math.PI * 2) * 280, cy + Math.sin(-Math.PI / 2 + (index / children.length) * Math.PI * 2) * 220];
        return { node, x: pos[0], y: pos[1] };
      });
      return { width, height, placements };
    }
    function renderDrillViewport(layout, canvasHtml) {
      return '<div class="drill-graph-viewport" data-drill-viewport="1">' +
        '<div class="drill-graph-pan" style="width:' + layout.width + 'px; min-height:' + layout.height + 'px">' +
          canvasHtml +
        '</div>' +
      '</div>';
    }
    function renderTopLevelDrillGraph(visibleChildren, totalChildren) {
      const layout = topLevelGraphLayout(visibleChildren);
      const nodes = layout.placements.map((p) => {
        const pos = resolvedDrillPosition(explorerView, p.node, p.x, p.y);
        return renderDrillGraphNode(p.node, pos.x, pos.y, "child");
      }).join("");
      const empty = !visibleChildren.length
        ? '<div class="drill-empty" style="position:absolute;left:32px;top:32px">' + (totalChildren ? 'Поиск не нашел верхних узлов.' : 'Верхний уровень пуст.') + '</div>'
        : "";
      return renderDrillViewport(layout, '<div class="drill-graph-canvas" style="width:' + layout.width + 'px; min-height:' + layout.height + 'px">' +
        '<svg class="drill-edge-layer" viewBox="0 0 ' + layout.width + ' ' + layout.height + '" aria-hidden="true"><defs><marker id="drillArrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth"><path d="M0,0 L0,6 L8,3 z" fill="#94a3b8"></path></marker></defs></svg>' +
        nodes +
        empty +
      '</div>');
    }
    function renderDrillGraph(current, visibleChildren, totalChildren) {
      const layout = drillGraphLayout(visibleChildren);
      const center = resolvedDrillPosition(explorerView, current, layout.cx, layout.cy);
      const edges = visibleChildren.map((child, index) => {
        const p = layout.placements[index];
        const childPos = resolvedDrillPosition(explorerView, child, p.x, p.y);
        return '<path class="drill-edge" data-drill-edge-to="' + esc(child.id) + '" d="M ' + Math.round(center.x) + ' ' + Math.round(center.y) + ' L ' + Math.round(childPos.x) + ' ' + Math.round(childPos.y) + '"></path>';
      }).join("");
      const childNodes = layout.placements.map((p) => {
        const pos = resolvedDrillPosition(explorerView, p.node, p.x, p.y);
        return renderDrillGraphNode(p.node, pos.x, pos.y, "child");
      }).join("");
      const empty = !visibleChildren.length
        ? '<div class="drill-empty" style="position:absolute;left:' + Math.round(layout.cx + 155) + 'px;top:' + Math.round(layout.cy - 18) + 'px">' + (totalChildren ? 'Поиск не нашел внутренних узлов на этом уровне.' : 'У этой ноды нет внутренних узлов.') + '</div>'
        : "";
      return renderDrillViewport(layout, '<div class="drill-graph-canvas" style="width:' + layout.width + 'px; min-height:' + layout.height + 'px">' +
        '<svg class="drill-edge-layer" viewBox="0 0 ' + layout.width + ' ' + layout.height + '" aria-hidden="true"><defs><marker id="drillArrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth"><path d="M0,0 L0,6 L8,3 z" fill="#94a3b8"></path></marker></defs>' + edges.replaceAll('marker-end: url(#arrow)', '') + '</svg>' +
        renderDrillGraphNode(current, center.x, center.y, "center") +
        childNodes +
        empty +
      '</div>');
    }
    function updateDrillEdges() {
      const canvas = studioExplorerTree.querySelector(".drill-graph-canvas");
      if (!canvas) return;
      const center = canvas.querySelector(".drill-graph-node.center");
      if (!center) return;
      const centerX = parseFloat(center.style.left) || 0;
      const centerY = parseFloat(center.style.top) || 0;
      canvas.querySelectorAll(".drill-edge[data-drill-edge-to]").forEach((edge) => {
        const id = edge.getAttribute("data-drill-edge-to");
        const node = Array.from(canvas.querySelectorAll(".drill-graph-node[data-drill-id]")).find((el) => el.dataset.drillId === id);
        if (!node) return;
        const x = parseFloat(node.style.left) || 0;
        const y = parseFloat(node.style.top) || 0;
        edge.setAttribute("d", "M " + centerX + " " + centerY + " L " + x + " " + y);
      });
    }
    function renderExpandedChildren() {
      nodeLayer.querySelectorAll(".child-node").forEach((el) => el.remove());
      edgeLayer.querySelectorAll(".expand-line").forEach((el) => el.remove());
      return;
      expanded.forEach((id) => {
        const parent = moduleById.get(id);
        if (!parent || !isVisible(parent)) return;
        const groups = refactorGroups(id);
        groups.forEach((group, index) => {
          const offset = childOffset(index, groups.length);
          const x = Math.max(80, Math.min(1280, parent.x + offset[0]));
          const y = Math.max(60, Math.min(715, parent.y + offset[1]));
          const line = document.createElementNS("http://www.w3.org/2000/svg", "path");
          line.setAttribute("class", "expand-line");
          line.setAttribute("style", "--c:" + group.color);
          line.setAttribute("d", "M " + parent.x + " " + parent.y + " L " + x + " " + y);
          edgeLayer.appendChild(line);

          const el = document.createElement("div");
          el.className = "child-node";
          el.style.setProperty("--c", group.color);
          el.style.left = x + "px";
          el.style.top = y + "px";
          el.dataset.module = id;
          el.dataset.group = group.id;
          el.innerHTML =
            '<div class="top">' + icon(group.icon) + '<span class="title">' + esc(group.title) + '</span></div>' +
            '<div class="desc">' + esc(group.description) + '</div>' +
            tags(group.tags.slice(0, 4)) +
            '<div class="counts"><span class="chip">' + group.items.length + ' items</span></div>';
          el.classList.toggle("selected", id === selected && group.id === selectedGroup);
          el.classList.toggle("dimmed", isDimmed(parent) || !groupMatches(group));
          el.addEventListener("click", (event) => {
            event.stopPropagation();
            selected = id;
            selectedGroup = group.id;
            moduleFilter.value = "";
            updateAll();
          });
          nodeLayer.appendChild(el);

          group.items.forEach((item, itemIndex) => {
            const itemPos = itemOffset(parent, x, y, itemIndex, group.items.length);
            const itemX = clamp(itemPos[0], 70, 1290);
            const itemY = clamp(itemPos[1], 50, 720);
            const itemLine = document.createElementNS("http://www.w3.org/2000/svg", "path");
            itemLine.setAttribute("class", "expand-line");
            itemLine.setAttribute("style", "--c:" + group.color);
            itemLine.setAttribute("d", "M " + x + " " + y + " L " + itemX + " " + itemY);
            edgeLayer.appendChild(itemLine);

            const itemEl = document.createElement("div");
            itemEl.className = "child-node file-node";
            itemEl.style.setProperty("--c", group.color);
            itemEl.style.left = itemX + "px";
            itemEl.style.top = itemY + "px";
            itemEl.dataset.module = id;
            itemEl.dataset.group = group.id;
            itemEl.dataset.path = item.path || "";
            itemEl.innerHTML =
              '<div class="top">' + itemIcon(item.kind) + '<span class="title">' + esc(itemTitle(item)) + '</span></div>' +
              '<div class="path-mini">' + esc(item.path || item.title) + '</div>' +
              tags((item.tags || []).slice(0, 3));
            itemEl.classList.toggle("dimmed", isDimmed(parent) || !groupMatches(group));
            itemEl.addEventListener("click", (event) => {
              event.stopPropagation();
              selected = id;
              selectedGroup = group.id;
              moduleFilter.value = "";
              updateAll();
            });
            nodeLayer.appendChild(itemEl);
          });
        });
      });
    }
    function updatePositions() {
      const local = neighborIds(selected);
      modules.forEach((m) => {
        const el = nodeEls.get(m.id);
        const nextExpanded = expanded.has(m.id) ? "1" : "0";
        if (el.dataset.expanded !== nextExpanded) {
          el.dataset.expanded = nextExpanded;
          el.innerHTML = moduleNodeHtml(m);
        }
        el.style.left = m.x + "px";
        el.style.top = m.y + "px";
        el.classList.toggle("selected", m.id === selected);
        el.classList.toggle("expanded", expanded.has(m.id));
        el.classList.toggle("neighbor", local.has(m.id) && m.id !== selected);
        el.classList.toggle("dimmed", isDimmed(m));
        el.classList.toggle("hidden", !isVisible(m));
        const button = el.querySelector("[data-expand]");
        if (button) button.textContent = expanded.has(m.id) ? "Collapse" : "Open inside";
      });
      edgeData.forEach((e) => {
        const a = moduleById.get(e.from);
        const b = moduleById.get(e.to);
        const path = edgeEls.get(e.id);
        const label = labelEls.get(e.id);
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const curve = Math.max(-70, Math.min(70, dy * .20));
        path.setAttribute("d", "M " + a.x + " " + a.y + " C " + (a.x + dx * .45) + " " + (a.y + curve) + ", " + (a.x + dx * .55) + " " + (b.y - curve) + ", " + b.x + " " + b.y);
        const active = e.from === selected || e.to === selected;
        const hidden = !isVisible(a) || !isVisible(b);
        path.classList.toggle("active", active);
        path.classList.toggle("dimmed", hidden || (focusMode.value === "local" && !active));
        label.setAttribute("x", String((a.x + b.x) / 2));
        label.setAttribute("y", String((a.y + b.y) / 2 - 8));
        label.classList.toggle("dimmed", hidden || (focusMode.value === "local" && !active));
        label.style.display = active || focusMode.value === "all" ? "block" : "none";
      });
      renderExpandedChildren();
    }
    window.addEventListener("pointermove", (event) => {
      if (drillDragging) {
        const canvas = drillDragging.canvas;
        const pointer = drillCanvasPoint(event, canvas);
        const x = pointer.x - drillDragging.dx;
        const y = pointer.y - drillDragging.dy;
        if (Math.abs(event.clientX - drillDragging.clientX) > 4 || Math.abs(event.clientY - drillDragging.clientY) > 4) {
          drillDragging.moved = true;
        }
        drillDragging.el.style.left = x + "px";
        drillDragging.el.style.top = y + "px";
        drillPositions.set(drillPositionKey(drillDragging.viewId, drillDragging.id), { x, y });
        updateDrillEdges();
        return;
      }
      if (drillPanning) {
        if (Math.abs(event.clientX - drillPanning.clientX) > 4 || Math.abs(event.clientY - drillPanning.clientY) > 4) {
          drillPanning.moved = true;
        }
        drillPanX = drillPanning.panX + event.clientX - drillPanning.clientX;
        drillPanY = drillPanning.panY + event.clientY - drillPanning.clientY;
        applyDrillTransform();
        return;
      }
      if (panning) {
        panX = panning.panX + event.clientX - panning.clientX;
        panY = panning.panY + event.clientY - panning.clientY;
        applyTransform();
        return;
      }
      if (!dragging) return;
      const p = point(event);
      if (Math.abs(event.clientX - dragging.clientX) > 4 || Math.abs(event.clientY - dragging.clientY) > 4) {
        dragging.moved = true;
      }
      dragging.module.x = p.x - dragging.dx;
      dragging.module.y = p.y - dragging.dy;
      updatePositions();
    });
    window.addEventListener("pointerup", () => {
      if (drillDragging && drillDragging.moved) {
        suppressExplorerClick = true;
        window.setTimeout(() => { suppressExplorerClick = false; }, 250);
      }
      drillDragging = null;
      if (drillPanning) {
        const board = studioExplorerTree ? studioExplorerTree.querySelector(".drill-graph-viewport.panning") : null;
        if (board) board.classList.remove("panning");
      }
      drillPanning = null;
      if (dragging && dragging.moved) {
        suppressGraphClick = true;
        window.setTimeout(() => { suppressGraphClick = false; }, 250);
      }
      dragging = null;
      panning = null;
      viewport.classList.remove("panning");
    });
    if (studioExplorerTree) {
      studioExplorerTree.addEventListener("pointerdown", (event) => {
        const node = event.target.closest(".drill-graph-node");
        if (node && event.button === 0) {
          const interactive = event.target.closest("a,button,input,select,textarea");
          if (interactive && !interactive.classList.contains("drill-graph-node")) return;
          event.preventDefault();
          event.stopPropagation();
          const canvas = node.closest(".drill-graph-canvas");
          if (!canvas) return;
          const pointer = drillCanvasPoint(event, canvas);
          const x = parseFloat(node.style.left) || 0;
          const y = parseFloat(node.style.top) || 0;
          drillDragging = {
            id: node.dataset.drillId,
            viewId: explorerView,
            el: node,
            canvas,
            dx: pointer.x - x,
            dy: pointer.y - y,
            clientX: event.clientX,
            clientY: event.clientY,
            moved: false
          };
          node.setPointerCapture(event.pointerId);
          return;
        }
        if (event.button !== 0) return;
        const interactive = event.target.closest("a,button,input,select,textarea");
        if (interactive) return;
        const board = event.target.closest(".drill-graph-viewport");
        if (!board) return;
        event.preventDefault();
        drillPanning = {
          clientX: event.clientX,
          clientY: event.clientY,
          panX: drillPanX,
          panY: drillPanY,
          moved: false
        };
        board.classList.add("panning");
        board.setPointerCapture(event.pointerId);
      });
      studioExplorerTree.addEventListener("wheel", (event) => {
        if (!drillOpen) return;
        const board = event.target.closest(".drill-graph-viewport");
        if (!board) return;
        if (event.target.closest(".drill-node-desc")) return;
        event.preventDefault();
        const factor = Math.exp(-event.deltaY * 0.0012);
        setDrillZoomAt(event.clientX, event.clientY, drillZoom * factor);
      }, { passive: false });
    }
    viewport.addEventListener("pointerdown", (event) => {
      if (drillOpen) return;
      if (event.button !== 0) return;
      if (event.target.closest && event.target.closest(".module-node,.child-node,button,a,input,select")) return;
      panning = { clientX: event.clientX, clientY: event.clientY, panX, panY };
      viewport.classList.add("panning");
      viewport.setPointerCapture(event.pointerId);
    });
    viewport.addEventListener("wheel", (event) => {
      if (drillOpen) return;
      event.preventDefault();
      const factor = Math.exp(-event.deltaY * 0.0012);
      setZoomAt(event.clientX, event.clientY, zoom * factor);
    }, { passive: false });
    if (detail) {
      detail.addEventListener("click", (event) => {
        const groupButton = event.target.closest("[data-group]");
        if (groupButton) {
          event.preventDefault();
          selectedGroup = groupButton.dataset.group;
          selectExplorerModule(selected, selectedGroup);
          updateAll();
          return;
        }
        const expandButton = event.target.closest("[data-detail-expand]");
        if (!expandButton) return;
        event.preventDefault();
        openModuleDrill(expandButton.dataset.detailExpand);
      });
    }
    if (studioExplorerTree) {
      studioExplorerTree.addEventListener("keydown", (event) => {
        if (event.key !== "Enter" && event.key !== " ") return;
        const card = event.target.closest("[data-explorer-card]");
        if (!card || !studioExplorerTree.contains(card)) return;
        if (event.target.closest("[data-toggle-description],[data-copy-path],a,button")) return;
        event.preventDefault();
        activateExplorerNode(card.dataset.explorerCard);
      });
      studioExplorerTree.addEventListener("click", (event) => {
        if (suppressExplorerClick) {
          event.preventDefault();
          event.stopPropagation();
          suppressExplorerClick = false;
          return;
        }
        const close = event.target.closest("[data-explorer-close]");
        if (close) {
          event.preventDefault();
          event.stopPropagation();
          setDrillOpen(false);
          updateAll();
          return;
        }
        const back = event.target.closest("[data-explorer-back]");
        if (back) {
          event.preventDefault();
          event.stopPropagation();
          explorerBack();
          return;
        }
        const copy = event.target.closest("[data-copy-path]");
        if (copy) {
          event.preventDefault();
          event.stopPropagation();
          copyText(copy.dataset.copyPath, copy);
          return;
        }
        const toggleDescription = event.target.closest("[data-toggle-description]");
        if (toggleDescription) {
          event.preventDefault();
          event.stopPropagation();
          const node = toggleDescription.closest(".drill-graph-node");
          const expanded = !node.classList.contains("desc-open");
          node.classList.toggle("desc-open", expanded);
          toggleDescription.setAttribute("aria-expanded", expanded ? "true" : "false");
          toggleDescription.textContent = expanded ? "Less" : "More";
          return;
        }
        const card = event.target.closest("[data-explorer-card]");
        if (!card) return;
        event.preventDefault();
        activateExplorerNode(card.dataset.explorerCard);
      });
    }
    function listItems(items, mapper, empty) {
      if (!items.length) return '<p class="muted">' + esc(empty) + '</p>';
      return items.map(mapper).join("");
    }
    function itemIcon(kind) {
      if (kind === "tool") return icon("tool");
      if (kind === "contract") return icon("shield");
      return icon("doc");
    }
    function renderRefactorItem(item) {
      const title = item.href
        ? '<a class="file" href="' + esc(item.href) + '">' + esc(item.path) + '</a>'
        : '<span class="file">' + esc(item.path) + '</span>';
      return '<div class="mini-card">' +
        '<div class="legend">' + itemIcon(item.kind) + '<strong>' + esc(item.title) + '</strong></div>' +
        title +
        '<p class="muted">' + esc(item.role) + '</p>' +
        '<p class="muted">' + esc(item.description) + '</p>' +
        tags(item.tags || []) +
      '</div>';
    }
    function renderGroupDetail(moduleId) {
      const groups = refactorGroups(moduleId);
      const active = groups.find((g) => g.id === selectedGroup) || groups[0];
      if (!active) return "";
      selectedGroup = active.id;
      return '<div class="block">' +
        '<div class="legend">' + groups.map((g) => '<button type="button" class="' + (g.id === active.id ? '' : 'secondary') + '" data-group="' + esc(g.id) + '">' + esc(g.title) + '</button>').join("") + '</div>' +
        '<div class="mini-card" style="border-top:4px solid ' + esc(active.color) + '">' +
          '<div class="legend">' + icon(active.icon) + '<h3>' + esc(active.title) + '</h3>' + tags(active.tags) + '</div>' +
          '<p class="muted">' + esc(active.description) + '</p>' +
        '</div>' +
        '<div class="list">' + listItems(active.items, renderRefactorItem, 'No items in this group.') + '</div>' +
      '</div>';
    }
    function renderDetail() {
      if (!detail) return;
      const m = moduleById.get(selected) || modules[0];
      const inv = inventory(m.id);
      detail.innerHTML =
        '<div class="block"><div class="legend"><span class="swatch" style="--c:' + esc(m.color) + '"></span><span class="chip">' + esc(m.kind) + '</span>' + chip(migrationStatus(m.id)[0], migrationStatus(m.id)[1]) + risks(m.id).map((r) => chip(r[0], r[1])).join("") + '</div><h2>' + esc(m.title) + '</h2><p class="muted">' + esc(m.description) + '</p></div>' +
        '<div class="block"><div class="actions"><button type="button" data-detail-expand="' + esc(m.id) + '">' + (expanded.has(m.id) ? "Collapse on graph" : "Expand on graph") + '</button></div></div>' +
        '<div class="block"><h3>Текущий инвентарь</h3><div class="legend"><span class="chip">' + inv.docs.length + ' markdown</span><span class="chip">' + inv.skills.length + ' skills</span><span class="chip">' + inv.publicTools.length + ' public tools</span><span class="chip">' + inv.internalTools.length + ' internal tools</span><span class="chip">' + inv.contracts.length + ' contracts</span></div></div>' +
        renderGroupDetail(m.id) +
        '<div class="block"><h3>Source docs</h3><div class="list">' + listItems(inv.docs.slice(0, 8), (d) => '<div class="mini-card"><a class="file" href="' + esc(d.href) + '">' + esc(d.path) + '</a><p class="muted">' + esc(d.description) + '</p></div>', 'No markdown source classified here.') + '</div></div>' +
        '<div class="block"><h3>Public tools</h3><div class="list">' + listItems(inv.publicTools.slice(0, 10), (t) => '<div class="mini-card"><a class="file" href="' + esc(t.href) + '">' + esc(t.path) + '</a><p class="muted">' + esc(t.description) + '</p></div>', 'No public tools classified here.') + '</div></div>' +
        '<div class="block"><h3>Contracts</h3><div class="list">' + listItems(inv.contracts, (e) => '<div class="mini-card"><div class="legend"><span class="chip">' + esc(e.from) + ' -> ' + esc(e.to) + '</span><span class="chip">' + esc(e.label) + '</span></div><p class="muted">' + esc(e.description) + '</p></div>', 'No direct module contracts.') + '</div></div>' +
        '<div class="block"><h3>Next refactor actions</h3><div class="list">' + refactorActions(m.id).map((x) => '<div class="mini-card">' + esc(x) + '</div>').join("") + '</div></div>';
    }
    function refactorActions(id) {
      const inv = inventory(id);
      const actions = id === "hot" ? [
        "Define exact Core contract: harness, agent role, AGENTS.md, short routing, minimal public commands.",
        "Remove domain-specific procedure from Core docs; replace with links to owned modules.",
        "Keep Core docs small enough to load every session.",
        "Make every outgoing Core edge explicit: route to skill, task state, facade, validation, or export.",
        "After each move, rebuild maps and check that Core did not absorb new domain logic."
      ] : [
        "Decide whether this module is a separate domain, core-adjacent support, or deletion candidate.",
        "Confirm module owner and source-of-truth Markdown.",
        "Define the minimal public facade agents should call.",
        "Move support helpers behind that facade or mark them internal.",
        "Add or route a validator for mandatory rules.",
        "Run generator and validate docs/taskboard after changes."
      ];
      if (inv.tools.length && !inv.publicTools.length) actions.unshift("Create or choose a public CLI facade before moving files.");
      if (inv.publicTools.length > 10) actions.unshift("Reduce public surface: merge overlapping commands or document clear routes.");
      if (inv.docs.length > 12) actions.unshift("Split docs into index + owned references; remove duplicated procedure from hot docs.");
      return actions;
    }
    function renderQueue() {
      const root = document.getElementById("queueGrid");
      const visibleQueue = document.getElementById("visibleQueue");
      if (!root || !visibleQueue) return;
      const q = search.value.trim().toLowerCase();
      let visible = 0;
      root.innerHTML = modules.map((m) => {
        const inv = inventory(m.id);
        const riskText = risks(m.id).map((r) => r[1]).join(" ");
        const hay = [m.title, m.description, riskText, inv.docs.map((d) => d.path).join(" "), inv.tools.map((t) => t.path).join(" ")].join(" ").toLowerCase();
        const hidden = q && !hay.includes(q);
        if (!hidden) visible++;
        const firstDoc = inv.docs[0];
        return '<article class="queue-card ' + (hidden ? 'hidden' : '') + '" style="--c:' + esc(m.color) + '">' +
          '<div class="legend"><span class="swatch" style="--c:' + esc(m.color) + '"></span>' + chip(migrationStatus(m.id)[0], migrationStatus(m.id)[1]) + risks(m.id).map((r) => chip(r[0], r[1])).join("") + '</div>' +
          '<h3>' + esc(m.title) + '</h3>' +
          '<p class="muted">' + esc(m.description) + '</p>' +
          '<div class="legend"><span class="chip">' + inv.docs.length + ' md</span><span class="chip">' + inv.skills.length + ' skills</span><span class="chip">' + inv.publicTools.length + '/' + inv.tools.length + ' public/tools</span><span class="chip">' + inv.contracts.length + ' contracts</span></div>' +
          (firstDoc ? '<a class="file" href="' + esc(firstDoc.href) + '">' + esc(firstDoc.path) + '</a>' : '<span class="muted">No source doc</span>') +
          '<div class="actions"><button type="button" class="secondary" data-select="' + esc(m.id) + '">Open module</button></div>' +
        '</article>';
      }).join("");
      visibleQueue.textContent = visible + " visible";
    }
    const queueGrid = document.getElementById("queueGrid");
    if (queueGrid) {
      queueGrid.addEventListener("click", (event) => {
        const btn = event.target.closest("[data-select]");
        if (!btn) return;
        selected = btn.dataset.select;
        selectedGroup = firstGroupId(selected);
        syncDrillToSelected("");
        expanded.add(selected);
        moduleFilter.value = "";
        updateAll();
        viewport.scrollIntoView({ behavior: "smooth", block: "start" });
      });
    }
    function updateAll() {
      updatePositions();
      renderDetail();
      renderQueue();
      renderExplorer();
    }
    document.getElementById("resetBtn").addEventListener("click", () => {
      modules.forEach((m) => { m.x = m.homeX; m.y = m.homeY; m.pinned = false; });
      selected = "hot";
      selectedGroup = "agent-contract";
      syncDrillToSelected("");
      moduleFilter.value = "";
      search.value = "";
      focusMode.value = "local";
      expanded.clear();
      explorerHistory.length = 0;
      explorerSelected = "studio";
      explorerView = "studio";
      setDrillOpen(true);
      fitStart();
      updateAll();
    });
    document.getElementById("fitBtn").addEventListener("click", () => {
      fitStart();
    });
    document.getElementById("zoomInBtn").addEventListener("click", () => {
      const rect = viewport.getBoundingClientRect();
      setZoomAt(rect.left + rect.width / 2, rect.top + rect.height / 2, zoom * 1.18);
    });
    document.getElementById("zoomOutBtn").addEventListener("click", () => {
      const rect = viewport.getBoundingClientRect();
      setZoomAt(rect.left + rect.width / 2, rect.top + rect.height / 2, zoom / 1.18);
    });
    window.addEventListener("keydown", (event) => {
      const tag = event.target && event.target.tagName ? event.target.tagName.toLowerCase() : "";
      if (tag === "input" || tag === "select" || tag === "textarea") return;
      const rect = viewport.getBoundingClientRect();
      if (event.key === "+" || event.key === "=") {
        setZoomAt(rect.left + rect.width / 2, rect.top + rect.height / 2, zoom * 1.16);
      } else if (event.key === "-") {
        setZoomAt(rect.left + rect.width / 2, rect.top + rect.height / 2, zoom / 1.16);
      } else if (event.key === "0") {
        fitStart();
      }
    });
    search.addEventListener("input", updateAll);
    focusMode.addEventListener("change", updateAll);
    moduleFilter.addEventListener("change", () => {
      if (moduleFilter.value) {
        selected = moduleFilter.value;
        selectedGroup = firstGroupId(selected);
        syncDrillToSelected("");
      }
      updateAll();
    });
    try {
      renderLegend();
      renderNodes();
      renderEdges();
      applyTransform();
      setDrillOpen(true);
      document.getElementById("moduleCount").textContent = modules.length;
      document.getElementById("docCount").textContent = data.markdownDocs.length;
      document.getElementById("toolCount").textContent = data.tools.length;
      document.getElementById("contractCount").textContent = edgeData.length;
      updateAll();
    } catch (error) {
      const bootError = document.getElementById("bootError");
      if (bootError) {
        bootError.style.display = "block";
        bootError.textContent = "Architecture map failed to initialize: " + (error && error.message ? error.message : String(error));
      }
      throw error;
    }
  })();
  </script>
</body>
</html>
`;
}

const markdownDocs = collectMarkdownSources();
const tools = collectTools();
const data = buildGraph(markdownDocs, tools);
data.refactorGroups = buildRefactorGroups(data);
data.studioTree = buildStudioTree(data);
fs.mkdirSync(path.dirname(repoPath(refactorOutPath)), { recursive: true });
fs.writeFileSync(repoPath(fullOutPath), renderFullHtml(data), "utf8");
fs.writeFileSync(repoPath(refactorOutPath), renderRefactorHtml(data), "utf8");
console.log(`wrote ${refactorOutPath}`);
console.log(`wrote ${fullOutPath}`);
console.log(`sources=${markdownDocs.length} tools=${tools.length} nodes=${data.nodes.length} edges=${data.edges.length}`);
