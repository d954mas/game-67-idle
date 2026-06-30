import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import {
  buildSourceSnapshot,
  diffSourceSnapshots,
  readSourceSnapshot,
  sourceSnapshotSignature,
  writeSourceSnapshot,
} from "../snapshots.mjs";

function scanSource(path) {
  return { id: "template", type: "template", label: "Template", path };
}

test("source snapshots diff added changed and deleted tracked files", (t) => {
  const root = mkdtempSync(join(tmpdir(), "source-snapshot-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const assets = join(root, "template", "assets");
  mkdirSync(join(assets, "models"), { recursive: true });
  writeFileSync(join(assets, "models", "crate.glb"), "glb", "utf8");
  writeFileSync(join(assets, "packs.json"), "v1", "utf8");
  const src = scanSource(assets);

  const before = buildSourceSnapshot(root, src);
  writeFileSync(join(assets, "models", "crate.glb"), "changed-glb", "utf8");
  writeFileSync(join(assets, "models", "barrel.glb"), "glb", "utf8");
  rmSync(join(assets, "packs.json"), { force: true });
  const after = buildSourceSnapshot(root, src);

  const diff = diffSourceSnapshots(before, after);
  assert.deepEqual(diff.added.map((file) => file.rel), ["models/barrel.glb"]);
  assert.deepEqual(diff.changed.map((file) => file.rel), ["models/crate.glb"]);
  assert.deepEqual(diff.deleted.map((file) => file.rel), ["packs.json"]);
  assert.notDeepEqual(sourceSnapshotSignature(before), sourceSnapshotSignature(after));
});

test("source snapshots persist under tmp as generated data", (t) => {
  const root = mkdtempSync(join(tmpdir(), "source-snapshot-persist-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const assets = join(root, "template", "assets");
  mkdirSync(assets, { recursive: true });
  writeFileSync(join(assets, "button.png"), "png", "utf8");
  const src = scanSource(assets);
  const snapshot = buildSourceSnapshot(root, src);

  writeSourceSnapshot(root, src, snapshot);
  const stored = readSourceSnapshot(root, src);

  assert.equal(stored.sourceKey, snapshot.sourceKey);
  assert.deepEqual(sourceSnapshotSignature(stored), sourceSnapshotSignature(snapshot));
});
