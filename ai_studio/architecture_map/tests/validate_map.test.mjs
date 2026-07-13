import assert from "node:assert/strict";
import test from "node:test";
import { mkdtempSync, mkdirSync, writeFileSync } from "node:fs";
import { execFileSync, spawnSync } from "node:child_process";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";

import {
  collectScannedPaths,
  createValidationReport,
  descriptionPolicyViolations,
  hasStrictFailures,
  listTrackedPaths,
} from "../validate_map.mjs";
import { loadArchitectureTree } from "../tree_loader.mjs";

const fixtureFiles = new Map();
const repoRoot = resolve(fileURLToPath(new URL("../../..", import.meta.url)));

function write(root, rel, text = "") {
  const abs = join(root, rel);
  mkdirSync(join(abs, ".."), { recursive: true });
  writeFileSync(abs, text, "utf8");
  if (!fixtureFiles.has(root)) fixtureFiles.set(root, new Set());
  fixtureFiles.get(root).add(rel.replaceAll("\\", "/"));
}

function report(options) {
  return createValidationReport({
    ...options,
    trackedPaths: options.trackedPaths || [...(fixtureFiles.get(options.repoRoot) || [])],
  });
}

function collectNodes(node, out = []) {
  out.push(node);
  for (const child of node.children || []) collectNodes(child, out);
  return out;
}

function loadRepoTreeRoot() {
  return loadArchitectureTree(repoRoot, "ai_studio/tree.json").root;
}

test("map validation reports missing, duplicate, and unmapped files", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-map-"));
  mkdirSync(join(root, "ai_studio"), { recursive: true });
  write(
    root,
    "ai_studio/tree.json",
    JSON.stringify({
      schema: 1,
      root: {
        id: "studio",
        title: "studio",
        children: [
          { id: "tree", kind: "doc", path: "ai_studio/tree.json", description: "Map source." },
          { id: "readme", kind: "doc", path: "ai_studio/README.md", description: "Mapped readme." },
          { id: "readme-copy", kind: "doc", path: "ai_studio/README.md", description: "Duplicate mapping." },
          { id: "missing", kind: "doc", path: "ai_studio/missing.md", description: "Missing file." },
          { id: "blank-description", kind: "doc", path: "ai_studio/blank.md", description: "" },
        ],
      },
    }),
  );
  write(root, "ai_studio/README.md", "# mapped");
  write(root, "ai_studio/blank.md", "# mapped but blank description");
  write(root, "ai_studio/unmapped.md", "# not in map");

  const validation = report({
    repoRoot: root,
    scanRoots: ["ai_studio"],
  });

  assert.equal(validation.summary.missingInRepo, 1);
  assert.equal(validation.summary.duplicateMappings, 1);
  assert.equal(validation.summary.unmappedInAiStudio, 1);
  assert.equal(validation.summary.missingDescriptions, 1);
  assert.equal(hasStrictFailures(validation), true);
});

test("map validation reports unmapped scanned files outside ai_studio", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-map-"));
  mkdirSync(join(root, "ai_studio"), { recursive: true });
  write(
    root,
    "ai_studio/tree.json",
    JSON.stringify({
      schema: 1,
      root: {
        id: "studio",
        title: "studio",
        children: [
          { id: "tree", kind: "doc", path: "ai_studio/tree.json", description: "Map source." },
        ],
      },
    }),
  );
  write(root, "AGENTS.md", "# Agents");

  const validation = report({
    repoRoot: root,
    scanRoots: ["ai_studio", "AGENTS.md"],
  });

  assert.equal(validation.summary.unmappedOutsideAiStudio, 1);
  assert.deepEqual(validation.issues.unmappedOutsideAiStudio, [{ path: "AGENTS.md" }]);
  assert.equal(hasStrictFailures(validation), true);
});

test("mapped directories cover their child files", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-map-"));
  mkdirSync(join(root, "ai_studio"), { recursive: true });
  write(
    root,
    "ai_studio/tree.json",
    JSON.stringify({
      schema: 1,
      root: {
        id: "studio",
        title: "studio",
        children: [
          { id: "public", kind: "dir", path: "ai_studio/taskboard/public", description: "Mapped browser assets." },
        ],
      },
    }),
  );
  write(root, "ai_studio/taskboard/public/app.js", "console.log('mapped');");

  const validation = report({
    repoRoot: root,
    scanRoots: ["ai_studio"],
  });

  assert.equal(validation.summary.unmappedInAiStudio, 1, "tree.json is intentionally still outside the test map");
  assert.ok(!validation.issues.unmappedInAiStudio.some((item) => item.path.endsWith("public/app.js")));
});

test("test and validation nodes still map files", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-map-"));
  mkdirSync(join(root, "ai_studio"), { recursive: true });
  write(
    root,
    "ai_studio/tree.json",
    JSON.stringify({
      schema: 1,
      root: {
        id: "studio",
        title: "studio",
        children: [
          { id: "tree", kind: "doc", path: "ai_studio/tree.json", description: "Map source." },
          {
            id: "sample-test",
            kind: "test",
            path: "ai_studio/tests/sample.test.mjs",
            description: "Test file filtered from the default graph view.",
          },
          {
            id: "sample-validator",
            kind: "validation",
            path: "ai_studio/validation/sample_check.mjs",
            description: "Validation file filtered from the default graph view.",
          },
        ],
      },
    }),
  );
  write(root, "ai_studio/tests/sample.test.mjs", "import test from 'node:test';");
  write(root, "ai_studio/validation/sample_check.mjs", "console.log('validate');");

  const validation = report({
    repoRoot: root,
    scanRoots: ["ai_studio"],
  });

  assert.equal(validation.summary.unmappedInAiStudio, 0);
  assert.ok(!validation.issues.unmappedInAiStudio.some((item) => item.path.endsWith("sample.test.mjs")));
  assert.ok(!validation.issues.unmappedInAiStudio.some((item) => item.path.endsWith("sample_check.mjs")));
});

test("child-directory scan roots report new folders without scanning their files", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-map-"));
  mkdirSync(join(root, "ai_studio"), { recursive: true });
  write(
    root,
    "ai_studio/tree.json",
    JSON.stringify({
      schema: 1,
      root: {
        id: "studio",
        title: "studio",
        children: [
          {
            id: "templates-root",
            kind: "dir",
            path: "templates",
            coverage: "self",
            description: "Template container.",
            children: [
              {
                id: "templates-base",
                kind: "dir",
                path: "templates/base",
                description: "Mapped template folder.",
              },
            ],
          },
        ],
      },
    }),
  );
  write(root, "templates/base/README.md", "# mapped template");
  write(root, "templates/new-template/README.md", "# unmapped template");

  const validation = report({
    repoRoot: root,
    scanRoots: [{ path: "templates", mode: "child-directories" }],
  });

  assert.equal(validation.summary.scannedPaths, 2);
  assert.equal(validation.summary.unmappedOutsideAiStudio, 1);
  assert.deepEqual(validation.issues.unmappedOutsideAiStudio, [{ path: "templates/new-template" }]);
});

test("root-file and child-directory scan roots report workspace root commands", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-map-"));
  mkdirSync(join(root, "ai_studio"), { recursive: true });
  write(
    root,
    "ai_studio/tree.json",
    JSON.stringify({
      schema: 1,
      root: {
        id: "studio",
        title: "studio",
        children: [
          {
            id: "templates-root",
            kind: "dir",
            path: "templates",
            coverage: "self",
            description: "Template container.",
            children: [
              {
                id: "templates-base",
                kind: "dir",
                path: "templates/base",
                description: "Mapped template folder.",
              },
            ],
          },
        ],
      },
    }),
  );
  write(root, "templates/base/README.md", "# mapped template");
  write(root, "templates/new_template.mjs", "console.log('new template');");
  write(root, "templates/new-template/README.md", "# unmapped template");

  const validation = report({
    repoRoot: root,
    scanRoots: [{ path: "templates", mode: "root-files-and-child-directories" }],
  });

  assert.deepEqual(collectScannedPaths(root, [{ path: "templates", mode: "root-files-and-child-directories" }], {
    trackedPaths: [...fixtureFiles.get(root)],
  }), [
    "templates/base",
    "templates/new_template.mjs",
    "templates/new-template",
  ]);
  assert.deepEqual(validation.issues.unmappedOutsideAiStudio, [
    { path: "templates/new_template.mjs" },
    { path: "templates/new-template" },
  ]);
});

test("scan roots skip explicit excluded prefixes", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-map-"));
  write(root, "games/public-game/README.md", "# public game");
  write(root, "games/secret-game/README.md", "# private game");

  assert.deepEqual(
    collectScannedPaths(root, [{ path: "games", mode: "root-files-and-child-directories" }], {
      excludedPrefixes: ["games/secret-game"],
      trackedPaths: [...fixtureFiles.get(root)],
    }),
    ["games/public-game"],
  );
});

test("validation report excludes local private game mounts from game scans", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-map-"));
  write(
    root,
    "ai_studio/tree.json",
    JSON.stringify({
      schema: 1,
      root: {
        id: "studio",
        title: "studio",
        children: [
          {
            id: "games-root",
            kind: "dir",
            path: "games",
            coverage: "self",
            description: "Public games container.",
          },
        ],
      },
    }),
  );
  write(root, "games/public-game/README.md", "# public game");
  write(root, "games/secret-game/README.md", "# private game");
  write(
    root,
    "ai_studio/workspace/catalog.json",
    JSON.stringify({
      schema: "ai_studio.workspace.catalog.v1",
      mounts: [{ kind: "game", root: "games/public-game", visibility: "public", gitRoot: "", commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [] }],
    }),
  );
  write(root, "games/public-game/game.json", JSON.stringify({ schema: "ai_studio.game.v1", id: "public-game", title: "Public Game", storageNamespace: "public-game" }));
  write(root, "games/public-game/dependencies.json", JSON.stringify({ schema: "ai_studio.game.dependencies.v2", engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test" }));
  write(root, "games/secret-game/game.json", JSON.stringify({ schema: "ai_studio.game.v1", id: "secret-game", title: "Secret Game", storageNamespace: "secret-game" }));
  write(root, "games/secret-game/dependencies.json", JSON.stringify({ schema: "ai_studio.game.dependencies.v2", engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test" }));
  write(
    root,
    "ai_studio/workspace/catalog.local.json",
    JSON.stringify({
      schema: "ai_studio.workspace.catalog.v1",
      mounts: [{ kind: "game", root: "games/secret-game", visibility: "private", gitRoot: "games/secret-game", commitPolicy: "nested-private", enabledStores: ["assets"], aliases: [] }],
    }),
  );

  const validation = report({
    repoRoot: root,
    scanRoots: [{ path: "games", mode: "root-files-and-child-directories" }],
  });
  const reportText = JSON.stringify(validation);

  assert.equal(reportText.includes("secret-game"), false);
  assert.deepEqual(validation.issues.unmappedOutsideAiStudio, [
    { path: "games/public-game" },
  ]);
});

test("repo tree maps workspace and experimental extension ownership without listing implementation files", () => {
  const nodes = collectNodes(loadRepoTreeRoot());
  const byId = new Map(nodes.map((node) => [node.id, node]));
  const byPath = new Map(nodes.filter((node) => node.path).map((node) => [node.path, node]));
  const paths = nodes.map((node) => node.path).filter(Boolean);

  assert.equal(byId.get("workspace:templates")?.path, "templates");
  assert.equal(byId.get("workspace:templates")?.coverage, "self");
  assert.equal(byId.get("workspace:templates-readme")?.path, "templates/README.md");
  assert.equal(byId.get("workspace:template:template")?.path, "templates/template");
  assert.equal(byId.get("workspace:features")?.path, "features");
  assert.equal(byId.get("workspace:features")?.kind, "group");
  assert.equal(byId.get("workspace:features")?.coverage, "self");
  assert.equal(byId.get("workspace:features-readme")?.path, "features/README.md");
  assert.equal(byId.get("game-design:knowledge-base")?.path, "ai_studio/game_design/knowledge_base");
  assert.equal(byPath.get("games")?.id, "workspace:games");
  assert.equal(byPath.get("games")?.coverage, "self");
  assert.equal(byId.get("workspace:games-readme")?.path, "games/README.md");
  assert.equal(byId.get("workspace:extensions")?.path, "extensions");
  assert.equal(byId.get("workspace:extensions")?.coverage, "self");
  assert.equal(byId.get("workspace:extensions-readme")?.path, "extensions/README.md");
  assert.equal(byId.get("workspace:extensions:experimental")?.path, "extensions/experimental");
  assert.equal(
    byId.get("workspace:extensions:experimental:skeletal-animation")?.path,
    "extensions/experimental/skeletal_animation",
  );

  assert.deepEqual(paths.filter((path) => path.startsWith("templates/template/")), ["templates/template/template.json"]);
  assert.deepEqual(paths.filter((path) => path.startsWith("features/") && path !== "features/README.md" && path.split("/").length > 2), []);
  assert.deepEqual(paths.filter((path) => path.startsWith("games/") && path.split("/").length > 2), []);
});

test("coverage truth comes from git ls-files while untracked hygiene stays optional", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-map-git-"));
  write(root, "ai_studio/tree.json", JSON.stringify({
    schema: 1,
    root: {
      id: "studio",
      title: "studio",
      children: [{ id: "tracked", path: "ai_studio/tracked.mjs", description: "Tracked source ownership." }],
    },
  }));
  write(root, "ai_studio/tracked.mjs", "export const tracked = true;");
  write(root, "ai_studio/generated-local.mjs", "export const generated = true;");
  write(root, "ai_studio/untracked.custom", "arbitrary extension");
  write(root, "ai_studio/.venv/ignored.py", "ignored generated environment");
  write(root, ".gitignore", "ai_studio/.venv/\n");
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["add", "ai_studio/tree.json", "ai_studio/tracked.mjs"], { cwd: root, stdio: "ignore" });

  assert.deepEqual(listTrackedPaths(root), ["ai_studio/tracked.mjs", "ai_studio/tree.json"]);
  const validation = createValidationReport({ repoRoot: root, scanRoots: ["ai_studio"], includeHygiene: true });
  assert.equal(validation.summary.unmappedInAiStudio, 1, "only tracked tree.json is unmapped");
  assert.deepEqual(validation.hygiene.untrackedPaths, [
    "ai_studio/generated-local.mjs",
    "ai_studio/untracked.custom",
  ]);
  assert.equal(hasStrictFailures(validation), true);
  assert.equal(validation.issues.unmappedInAiStudio.some((item) => item.path.includes("generated-local")), false);
});

test("description policy rejects long and operational text", () => {
  assert.deepEqual(descriptionPolicyViolations("Owns architecture validation."), []);
  assert.ok(descriptionPolicyViolations("x".repeat(241)).includes("too-long"));
  assert.ok(descriptionPolicyViolations("Run node tool.mjs --strict.").includes("command-or-flag"));
  assert.ok(descriptionPolicyViolations("Serves GET /api/example.").includes("route"));
  assert.ok(descriptionPolicyViolations("Regression tests for click and hover cases.").includes("test-case-detail"));
  assert.ok(descriptionPolicyViolations("Persists a sidebar toggle in localStorage.").includes("ui-micro-behavior"));
});

test("invalid architectural descriptions are reported and fail strict validation", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-map-description-"));
  write(root, "ai_studio/tree.json", JSON.stringify({
    schema: 1,
    root: {
      id: "studio",
      title: "studio",
      children: [{
        id: "operational",
        path: "ai_studio/tool.mjs",
        description: "Run node tool.mjs --strict and inspect GET /api/tool.",
      }],
    },
  }));
  write(root, "ai_studio/tool.mjs", "export const tool = true;");
  const validation = report({ repoRoot: root, scanRoots: ["ai_studio"] });
  assert.equal(validation.summary.invalidDescriptions, 1);
  assert.deepEqual(validation.issues.invalidDescriptions[0].violations, ["command-or-flag", "route"]);
  assert.equal(hasStrictFailures(validation), true);
});

test("repo descriptions are concise architectural locators and required agent/runtime ownership is mapped", () => {
  const nodes = collectNodes(loadRepoTreeRoot()).filter((node) => node.id !== "studio");
  const byPath = new Map(nodes.filter((node) => node.path).map((node) => [node.path, node]));
  assert.ok(byPath.has(".codex/agents"), "Codex role catalog ownership must be locatable");
  assert.ok(byPath.has(".claude/agents"), "Claude role catalog ownership must be locatable");
  assert.ok(byPath.has("ai_studio/runtime_automation"), "runtime proof ownership must be locatable");
  for (const node of nodes) {
    assert.ok(node.title || node.path, `${node.id} needs a human locator`);
    assert.ok(node.role || node.kind === "module" || node.kind === "shell", `${node.id} needs an ownership role`);
    assert.deepEqual(descriptionPolicyViolations(node.description), [], `${node.id}: ${node.description}`);
  }
});

test("validate_map module import has no CLI side effects", () => {
  const moduleUrl = new URL("../validate_map.mjs", import.meta.url).href;
  const result = spawnSync(process.execPath, ["--input-type=module", "-e", `await import(${JSON.stringify(moduleUrl)}); console.log("imported")`], {
    encoding: "utf8",
  });
  assert.equal(result.status, 0, result.stderr);
  assert.equal(result.stdout.trim(), "imported");
  assert.equal(result.stderr, "");
});

test("CLI help documents every strict description gate", () => {
  const cliPath = fileURLToPath(new URL("../validate_map.mjs", import.meta.url));
  const result = spawnSync(process.execPath, [cliPath, "--help"], { encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /descriptions are missing or violate/);
  assert.match(result.stdout, /--hygiene adds a separate non-gating report/);
});

test("the tracked repository architecture report is strict-clean", () => {
  const validation = createValidationReport({ repoRoot });
  assert.deepEqual(
    validation.scanRoots.find((root) => typeof root === "object" && root.path === "extensions"),
    { path: "extensions", mode: "root-files-and-child-directories" },
  );
  assert.equal(hasStrictFailures(validation), false, JSON.stringify(validation.issues, null, 2));
});
