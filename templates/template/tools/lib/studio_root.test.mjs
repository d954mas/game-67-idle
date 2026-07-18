import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { findStudioRoot } from "./studio_root.mjs";

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "studio-root-probe-"));
  mkdirSync(join(root, "external", "neotolis-engine"), { recursive: true });
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return root;
}

test("findStudioRoot probes public and private game depths", (t) => {
  const root = fixture(t);
  assert.equal(findStudioRoot(join(root, "games", "public-game")), root);
  assert.equal(findStudioRoot(join(root, "games", "private", "private-game")), root);
});

test("findStudioRoot fails closed without an engine marker", (t) => {
  const root = mkdtempSync(join(tmpdir(), "studio-root-missing-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  assert.throws(() => findStudioRoot(join(root, "games", "missing-game")), /cannot find Studio root/);
});
