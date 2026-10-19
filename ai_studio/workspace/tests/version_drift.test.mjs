import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";

import { versionDriftReport } from "../version_drift.mjs";

const ENGINE = "1".repeat(40);

function writeJson(root, rel, value) {
  const path = join(root, rel);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify(value), "utf8");
}

function game(root, id, { engine = ENGINE, features = [], lifecycle } = {}) {
  writeJson(root, `games/${id}/game.json`, {
    schema: "ai_studio.game.v1", id, title: id, storageNamespace: id, ...(lifecycle ? { lifecycle } : {}),
  });
  writeJson(root, `games/${id}/dependencies.json`, {
    schema: "ai_studio.game.dependencies.v3",
    engine: { source: "external/neotolis-engine", version: engine.slice(0, 8), revision: engine, compatibility: "test" },
    features: features.map(([featureId, version]) => ({ id: featureId, source: `features/${featureId}`, version, compatibility: "test" })),
    compatibility: "test",
  });
}

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "version-drift-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  writeJson(root, "features/ui-kit/feature.json", { id: "ui-kit", version: "3.1.0" });
  return root;
}

test("a recorded version is drift unless it is what the Studio checkout builds", (t) => {
  const root = fixture(t);
  game(root, "current-game", { features: [["ui-kit", "3.1.0"]] });
  game(root, "behind-game", { engine: "2".repeat(40), features: [["ui-kit", "1.7.0"]] });

  const report = versionDriftReport(root, { engineRevision: ENGINE });
  assert.deepEqual(report, [
    { game: "behind-game", drift: [
      { id: "engine", recorded: "22222222", current: "11111111" },
      { id: "ui-kit", recorded: "1.7.0", current: "3.1.0" },
    ] },
    { game: "current-game", drift: [] },
  ]);
});

test("frozen games are skipped unless named", (t) => {
  const root = fixture(t);
  const lifecycle = { state: "frozen", studioCommit: "a".repeat(40), since: "2026-10-19" };
  game(root, "old-game", { features: [["ui-kit", "1.7.0"]], lifecycle });

  assert.deepEqual(versionDriftReport(root, { engineRevision: ENGINE }), []);
  assert.equal(versionDriftReport(root, { engineRevision: ENGINE, game: "old-game" })[0].drift.length, 1);
});
