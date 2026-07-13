import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import { validateFeatureContracts } from "./validate_contracts.mjs";

const ROOT_FEATURES = [
  "audio-core", "game-events", "game-state", "items-core", "platform-sdk", "progression-core",
];
const POINTER_FEATURES = ["resource_panel", "settings"];
const ROUTER = [
  "## Purpose\nOwn the reusable purpose.",
  "## Public surface\nExpose the documented API.",
  "## Validation\nRun the owning tests.",
  "## Compatibility\nPATCH preserves compatibility; MINOR adds compatible surface; MAJOR permits breaking changes.",
  "## Extension points\nExtend only through documented seams.",
].join("\n\n");
const SKILL_ROUTERS = {
  ".codex/skills/nt-game-feature/SKILL.md": ["features/README.md", "templates/template/src/features/README.md", "README.md", "INSTALL.md", "feature.json"],
  ".codex/skills/nt-game-state-management/SKILL.md": ["features/game-state/README.md", "features/game-state/INSTALL.md", "features/game-state/feature.json"],
  ".codex/skills/nt-game-items/SKILL.md": ["features/items-core/README.md", "features/items-core/INSTALL.md", "features/items-core/feature.json"],
};

function write(root, rel, value) {
  const path = join(root, rel);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, typeof value === "string" ? value : `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "feature-contracts-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  write(root, "external/neotolis-engine/engine/core/nt_core.h", [
    "#define NT_VERSION_MAJOR 0", "#define NT_VERSION_MINOR 1", "#define NT_VERSION_PATCH 0", "",
  ].join("\n"));
  for (const id of ROOT_FEATURES) {
    write(root, `features/${id}/feature.json`, {
      schema: "ai_studio.feature.v1", id, version: "1.0.0",
      manuals: { install: `features/${id}/INSTALL.md` },
    });
    write(root, `features/${id}/README.md`, `# ${id}\n\n${ROUTER}\n`);
    write(root, `features/${id}/INSTALL.md`, `# Install ${id}\n`);
  }
  for (const id of POINTER_FEATURES) {
    const base = `templates/template/src/features/${id}`;
    write(root, `${base}/feature.json`, {
      schema: "ai_studio.feature.v1", id, version: "1.0.0",
      manuals: { install: `${base}/INSTALL.md` },
    });
    write(root, `${base}/README.md`, `# ${id}\n\n${ROUTER}\n`);
    write(root, `${base}/INSTALL.md`, `# Install ${id}\n`);
  }
  write(root, "templates/template/game-dependencies.json", {
    schema: "ai_studio.game.dependencies.seed.v2",
    engine: { source: "external/neotolis-engine", version: "0.1.0", compatibility: "tested" },
    features: ROOT_FEATURES.map((id) => ({
      id, source: `features/${id}`, version: "1.0.0", compatibility: "tested",
    })),
    compatibility: "tested",
  });
  for (const [path, references] of Object.entries(SKILL_ROUTERS)) {
    write(root, path, `# Router\n\n${references.join("\n")}\n`);
  }
  return root;
}

test("feature contracts inventory root modules and template pointers with exact SemVer", (t) => {
  const root = fixture(t);
  const result = validateFeatureContracts(root);
  assert.deepEqual(result.rootFeatures, ROOT_FEATURES);
  assert.deepEqual(result.pointerFeatures, POINTER_FEATURES);
});

test("feature contracts reject non-exact SemVer and incomplete owning routers", (t) => {
  const root = fixture(t);
  const manifest = {
    schema: "ai_studio.feature.v1", id: "game-state", version: "1.0",
    manuals: { install: "features/game-state/INSTALL.md" },
  };
  write(root, "features/game-state/feature.json", manifest);
  assert.throws(() => validateFeatureContracts(root), /game-state.*exact SemVer/i);

  manifest.version = "1.0.0";
  write(root, "features/game-state/feature.json", manifest);
  write(root, "features/game-state/README.md", "# Game State\n\n## Purpose\nOwn the state contract.\n");
  assert.throws(() => validateFeatureContracts(root), /game-state.*Public surface/i);
});

test("feature contract router sections require substantive bodies and explicit SemVer rules", (t) => {
  const root = fixture(t);
  write(root, "features/game-state/README.md", `# Game State\n\n${ROUTER.replace("Expose the documented API.", "")}\n`);
  assert.throws(() => validateFeatureContracts(root), /game-state.*Public surface.*must not be empty/i);

  write(root, "features/game-state/README.md", `# Game State\n\n${ROUTER.replace("PATCH preserves compatibility; MINOR adds compatible surface; MAJOR permits breaking changes.", "Versioned contract.")}\n`);
  assert.throws(() => validateFeatureContracts(root), /game-state.*Compatibility.*PATCH.*MINOR.*MAJOR/i);
});

test("template dependency seed versions must match owning feature metadata", (t) => {
  const root = fixture(t);
  const seed = {
    schema: "ai_studio.game.dependencies.seed.v2",
    engine: { source: "external/neotolis-engine", version: "0.1.0", compatibility: "tested" },
    features: ROOT_FEATURES.map((id) => ({
      id, source: `features/${id}`, version: id === "items-core" ? "1.1.0" : "1.0.0", compatibility: "tested",
    })),
    compatibility: "tested",
  };
  write(root, "templates/template/game-dependencies.json", seed);
  assert.throws(() => validateFeatureContracts(root), /items-core.*1\.1\.0.*1\.0\.0/i);
});

test("inventory discovers new manifests instead of silently relying on a fixed list", (t) => {
  const root = fixture(t);
  write(root, "features/new-core/feature.json", {
    schema: "ai_studio.feature.v1", id: "new-core", version: "1.0.0",
    manuals: { install: "features/new-core/INSTALL.md" },
  });
  write(root, "features/new-core/README.md", `# new-core\n\n${ROUTER}\n`);
  write(root, "features/new-core/INSTALL.md", "# Install\n");
  assert.throws(() => validateFeatureContracts(root), /missing reusable feature 'new-core'/i);
});

test("root feature inventory rejects immediate child directories without metadata", (t) => {
  const root = fixture(t);
  write(root, "features/new-core/README.md", "# new-core\n");
  assert.throws(() => validateFeatureContracts(root), /features\/new-core\/feature\.json/i);
});

test("template engine version must match the authoritative public engine header", (t) => {
  const root = fixture(t);
  const seed = {
    schema: "ai_studio.game.dependencies.seed.v2",
    engine: { source: "external/neotolis-engine", version: "0.2.0", compatibility: "tested" },
    features: ROOT_FEATURES.map((id) => ({
      id, source: `features/${id}`, version: "1.0.0", compatibility: "tested",
    })),
    compatibility: "tested",
  };
  write(root, "templates/template/game-dependencies.json", seed);
  assert.throws(() => validateFeatureContracts(root), /engine version 0\.2\.0.*nt_core\.h 0\.1\.0/i);
});

test("matching skills must route to every owning feature contract", (t) => {
  const root = fixture(t);
  write(root, ".codex/skills/nt-game-items/SKILL.md", "# Items router\n\nfeatures/items-core/README.md\n");
  assert.throws(() => validateFeatureContracts(root), /nt-game-items.*INSTALL\.md/i);
});
