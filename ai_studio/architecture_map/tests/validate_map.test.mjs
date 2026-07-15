import assert from "node:assert/strict";
import test from "node:test";
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
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
import { createArchitectureMapApi } from "../api.mjs";
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

function tempRoot(t, prefix) {
  const root = mkdtempSync(join(tmpdir(), prefix));
  t.after(() => {
    fixtureFiles.delete(root);
    rmSync(root, { recursive: true, force: true });
  });
  return root;
}

function report(options) {
  return createValidationReport({
    ...options,
    excludedPrefixes: options.excludedPrefixes || [],
    trackedPaths: options.trackedPaths || [...(fixtureFiles.get(options.repoRoot) || [])],
  });
}

function collectNodes(node, out = []) {
  out.push(node);
  for (const child of node.children || []) collectNodes(child, out);
  return out;
}

function mockRes() {
  const res = { statusCode: null, headers: null, body: "" };
  res.writeHead = (status, headers) => {
    res.statusCode = status;
    res.headers = headers;
  };
  res.end = (chunk) => {
    if (chunk) res.body += chunk;
  };
  return res;
}

function loadRepoTreeRoot() {
  return loadArchitectureTree(repoRoot, "ai_studio/tree.json").root;
}

test("map validation reports missing, duplicate, and unmapped files", (t) => {
  const root = tempRoot(t, "architecture-map-");
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

test("map validation reports unmapped scanned files outside ai_studio", (t) => {
  const root = tempRoot(t, "architecture-map-");
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

test("mapped directories cover their child files", (t) => {
  const root = tempRoot(t, "architecture-map-");
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

test("a module subtree covers implementation, tests, and validators without authored leaves", (t) => {
  const root = tempRoot(t, "architecture-map-");
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
            id: "sample-module",
            kind: "module",
            path: "ai_studio/sample",
            owner: "Sample",
            entry: "ai_studio/sample/README.md",
            verifyDomain: "harness",
            description: "Sample ownership boundary.",
          },
        ],
      },
    }),
  );
  write(root, "ai_studio/sample/README.md", "# sample");
  write(root, "ai_studio/sample/tests/sample.test.mjs", "import test from 'node:test';");
  write(root, "ai_studio/sample/validation/sample_check.mjs", "console.log('validate');");

  const validation = report({
    repoRoot: root,
    scanRoots: ["ai_studio"],
  });

  assert.deepEqual(validation.issues.unmappedInAiStudio, [{ path: "ai_studio/tree.json" }]);
  const nodes = collectNodes(loadArchitectureTree(root, "ai_studio/tree.json").root);
  assert.equal(nodes.some((node) => node.kind === "test" || node.kind === "validation"), false);
});

test("direct-files covers root files but leaves new child directories unmapped", (t) => {
  const root = tempRoot(t, "architecture-map-direct-");
  write(root, "features/README.md", "# features");
  write(root, "features/verify.mjs", "export {};");
  write(root, "features/known/README.md", "# known");
  write(root, "features/new-pack/README.md", "# new");
  write(root, "ai_studio/tree.json", JSON.stringify({
    schema: 2,
    root: {
      id: "studio",
      title: "Studio",
      kind: "root",
      owner: "Lead",
      description: "Studio ownership.",
      entry: "ai_studio/tree.json",
      verifyDomain: "harness",
      children: [{
        id: "features",
        title: "Features",
        kind: "workspace",
        path: "features",
        coverage: "direct-files",
        owner: "Features",
        description: "Feature pack container.",
        entry: "features/README.md",
        verifyDomain: "features",
        children: [{
          id: "known",
          title: "Known",
          kind: "module",
          path: "features/known",
          owner: "Known",
          description: "Known feature pack.",
          entry: "features/known/README.md",
          verifyDomain: "features",
        }],
      }],
    },
  }));

  const validation = report({
    repoRoot: root,
    scanRoots: [{ path: "features", mode: "root-files-and-child-directories" }],
  });
  assert.deepEqual(validation.issues.unmappedOutsideAiStudio, [{ path: "features/new-pack" }]);
});

test("a new top-level Studio module remains unmapped", (t) => {
  const root = tempRoot(t, "architecture-map-studio-root-");
  write(root, "ai_studio/README.md", "# Studio");
  write(root, "ai_studio/known/README.md", "# known");
  write(root, "ai_studio/new-module/README.md", "# new");
  write(root, "ai_studio/tree.json", JSON.stringify({
    schema: 1,
    root: {
      id: "studio",
      title: "Studio",
      kind: "root",
      path: "ai_studio",
      coverage: "direct-files",
      owner: "Lead",
      description: "Studio ownership.",
      entry: "ai_studio/README.md",
      verifyDomain: "harness",
      children: [{
        id: "known",
        title: "Known",
        kind: "module",
        path: "ai_studio/known",
        owner: "Known",
        description: "Known module.",
        entry: "ai_studio/known/README.md",
        verifyDomain: "harness",
      }],
    },
  }));

  const validation = report({ repoRoot: root, scanRoots: ["ai_studio"] });
  assert.deepEqual(validation.issues.unmappedInAiStudio, [
    { path: "ai_studio/new-module/README.md" },
  ]);
});

test("unknown coverage and escaping locator fields fail strict validation", (t) => {
  const root = tempRoot(t, "architecture-map-schema-");
  write(root, "ai_studio/module/README.md", "# module");
  write(root, "ai_studio/tree.json", JSON.stringify({
    schema: 2,
    root: {
      id: "studio",
      title: "Studio",
      kind: "root",
      owner: "Lead",
      description: "Studio ownership.",
      entry: "../outside.md",
      verifyDomain: "not-a-domain",
      children: [{
        id: "module",
        title: "Module",
        kind: "module",
        path: "ai_studio/module",
        coverage: "recursive-ish",
        owner: "Module",
        description: "Module ownership.",
        entry: "ai_studio/module/README.md",
        verifyDomain: "harness",
        tags: ["legacy"],
      }],
    },
  }));
  const validation = report({ repoRoot: root, scanRoots: ["ai_studio"] });
  assert.deepEqual(validation.issues.invalidNodes, [
    {
      id: "studio",
      path: "",
      violations: ["invalid-verify-domain", "invalid-entry"],
    },
    {
      id: "module",
      path: "ai_studio/module",
      violations: ["invalid-coverage", "legacy-tags"],
    },
  ]);
  assert.equal(hasStrictFailures(validation), true);
});

test("missing authored entry, contract, and store locators fail strict validation", (t) => {
  const root = tempRoot(t, "architecture-map-locators-");
  write(root, "ai_studio/tree.json", JSON.stringify({
    schema: 1,
    root: {
      id: "studio",
      title: "Studio",
      kind: "root",
      owner: "Lead",
      description: "Studio ownership.",
      entry: "ai_studio/tree.json",
      verifyDomain: "harness",
      children: [{
        id: "module",
        title: "Module",
        kind: "module",
        path: "ai_studio/module",
        owner: "Module",
        description: "Module ownership.",
        entry: "ai_studio/module/MISSING.md",
        contract: "ai_studio/module/MISSING.contract.json",
        store: "ai_studio/module/MISSING-store",
        boundary: "An external conceptual boundary is not a filesystem locator.",
        verifyDomain: "harness",
      }],
    },
  }));
  write(root, "ai_studio/module/present.mjs", "export {};");

  const validation = report({ repoRoot: root, scanRoots: [] });
  assert.deepEqual(validation.issues.missingLocators, [
    { id: "module", title: "Module", field: "entry", locator: "ai_studio/module/MISSING.md" },
    { id: "module", title: "Module", field: "contract", locator: "ai_studio/module/MISSING.contract.json" },
    { id: "module", title: "Module", field: "store", locator: "ai_studio/module/MISSING-store" },
  ]);
  assert.equal(validation.summary.missingLocators, 3);
  assert.equal(hasStrictFailures(validation), true);
});

test("child-directory scan roots report new folders without scanning their files", (t) => {
  const root = tempRoot(t, "architecture-map-");
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

test("root-file and child-directory scan roots report workspace root commands", (t) => {
  const root = tempRoot(t, "architecture-map-");
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
  write(root, "templates/helper.mjs", "console.log('template helper');");
  write(root, "templates/new-template/README.md", "# unmapped template");

  const validation = report({
    repoRoot: root,
    scanRoots: [{ path: "templates", mode: "root-files-and-child-directories" }],
  });

  assert.deepEqual(collectScannedPaths(root, [{ path: "templates", mode: "root-files-and-child-directories" }], {
    trackedPaths: [...fixtureFiles.get(root)],
  }), [
    "templates/base",
    "templates/helper.mjs",
    "templates/new-template",
  ]);
  assert.deepEqual(validation.issues.unmappedOutsideAiStudio, [
    { path: "templates/helper.mjs" },
    { path: "templates/new-template" },
  ]);
});

test("scan roots skip explicit excluded prefixes", (t) => {
  const root = tempRoot(t, "architecture-map-");
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

test("validation report excludes local private game mounts from game scans", (t) => {
  const root = tempRoot(t, "architecture-map-");
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
  write(root, "games/private/secret-game/README.md", "# private game");
  write(root, "games/public-game/game.json", JSON.stringify({ schema: "ai_studio.game.v1", id: "public-game", title: "Public Game", storageNamespace: "public-game" }));
  write(root, "games/public-game/dependencies.json", JSON.stringify({ schema: "ai_studio.game.dependencies.v2", engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test" }));
  write(root, "games/private/secret-game/game.json", JSON.stringify({ schema: "ai_studio.game.v1", id: "secret-game", title: "Secret Game", storageNamespace: "secret-game" }));
  write(root, "games/private/secret-game/dependencies.json", JSON.stringify({ schema: "ai_studio.game.dependencies.v2", engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test" }));

  const validation = createValidationReport({
    repoRoot: root,
    scanRoots: [{ path: "games", mode: "root-files-and-child-directories" }],
    trackedPaths: [...fixtureFiles.get(root)],
  });
  const reportText = JSON.stringify(validation);

  assert.equal(reportText.includes("secret-game"), false);
  assert.deepEqual(validation.issues.unmappedOutsideAiStudio, [
    { path: "games/public-game" },
  ]);
});

test("malformed private identity blocks validation without exposing game names", async (t) => {
  const root = tempRoot(t, "architecture-map-private-error-");
  write(root, "ai_studio/tree.json", JSON.stringify({
    schema: 1,
    root: {
      id: "studio",
      title: "Studio",
      kind: "root",
      owner: "Lead",
      description: "Studio ownership.",
      entry: "ai_studio/tree.json",
      verifyDomain: "harness",
    },
  }));
  write(root, "games/private/secret-game/game.json", '{"id":"secret-game"');

  assert.throws(
    () => createValidationReport({ repoRoot: root, trackedPaths: [...fixtureFiles.get(root)] }),
    /private game discovery failed/i,
  );

  const handle = createArchitectureMapApi(root);
  const res = mockRes();
  assert.equal(await handle({ method: "GET" }, res, new URL("http://x/api/architecture-validation")), true);
  assert.equal(res.statusCode, 500);
  const payload = JSON.parse(res.body);
  assert.deepEqual(Object.keys(payload), ["error"]);
  assert.match(payload.error, /private game discovery failed/i);
  assert.equal(res.body.includes("secret-game"), false);
});

test("repo tree maps product boundaries without authored implementation or test leaves", () => {
  const nodes = collectNodes(loadRepoTreeRoot());
  const byId = new Map(nodes.map((node) => [node.id, node]));
  const byPath = new Map(nodes.filter((node) => node.path).map((node) => [node.path, node]));
  const paths = nodes.map((node) => node.path).filter(Boolean);

  assert.equal(byPath.get("templates")?.coverage, "direct-files");
  assert.equal(byPath.get("templates/template")?.coverage, "subtree");
  assert.match(byPath.get("templates/template")?.boundary || "", /Settings.*resource-panel/);
  assert.equal(byPath.get("features")?.coverage, "direct-files");
  assert.equal(byPath.get("games")?.coverage, "direct-files");
  assert.equal(byPath.get("extensions")?.coverage, "direct-files");
  assert.equal(byPath.get("ai_studio/game_design/knowledge_base")?.id, "design-knowledge-base");

  for (const pack of ["audio-core", "game-events", "game-state", "items-core", "platform-sdk", "progression-core"]) {
    assert.equal(byPath.get(`features/${pack}`)?.coverage, "subtree", `${pack} is an explicit feature boundary`);
  }
  assert.equal(paths.some((path) => /(?:^|\/)(?:tests?|fixtures?|generated|history|research|migrations?|implementation)(?:\/|$)/i.test(path)), false);
  assert.equal(nodes.some((node) => node.kind === "test" || node.kind === "validation"), false);
  assert.equal(byId.has("settings"), false);
  assert.equal(byId.has("resource-panel"), false);
});

test("coverage truth comes from git ls-files while untracked hygiene stays optional", (t) => {
  const root = tempRoot(t, "architecture-map-git-");
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
  const validation = createValidationReport({ repoRoot: root, scanRoots: ["ai_studio"], excludedPrefixes: [], includeHygiene: true });
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

test("invalid architectural descriptions are reported and fail strict validation", (t) => {
  const root = tempRoot(t, "architecture-map-description-");
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

test("repo nodes use the compact owner and verification contract", () => {
  const nodes = collectNodes(loadRepoTreeRoot());
  const byPath = new Map(nodes.filter((node) => node.path).map((node) => [node.path, node]));
  assert.ok(byPath.has(".codex/agents"), "Codex role catalog ownership must be locatable");
  assert.ok(byPath.has(".claude/agents"), "Claude role catalog ownership must be locatable");
  assert.ok(byPath.has("ai_studio/runtime_automation"), "runtime proof ownership must be locatable");
  for (const node of nodes) {
    for (const field of ["id", "title", "kind", "owner", "description", "entry", "verifyDomain"]) {
      assert.equal(typeof node[field], "string", `${node.id || "node"} needs ${field}`);
      assert.ok(node[field].trim(), `${node.id || "node"} needs ${field}`);
    }
    for (const legacy of ["tags", "role", "color", "moduleId", "groupId", "item", "href"]) {
      assert.equal(Object.hasOwn(node, legacy), false, `${node.id} retains legacy ${legacy}`);
    }
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
  assert.match(result.stdout, /node, locator, coverage, verification-domain, or description contract/);
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
