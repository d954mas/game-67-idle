import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { findStudioRoot } from "./lib/studio_root.mjs";

test("studio root probe supports public/template and private game depths", (t) => {
  const root = mkdtempSync(join(tmpdir(), "studio-root-probe-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "external", "neotolis-engine"), { recursive: true });

  assert.equal(findStudioRoot(join(root, "templates", "template")), root);
  assert.equal(findStudioRoot(join(root, "games", "public-game")), root);
  assert.equal(findStudioRoot(join(root, "games", "private", "private-game")), root);
});

test("studio root probe fails closed without the engine checkout marker", (t) => {
  const root = mkdtempSync(join(tmpdir(), "studio-root-missing-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.throws(
    () => findStudioRoot(join(root, "games", "private", "private-game")),
    /cannot locate the studio repo root.*external\/neotolis-engine/i,
  );
});
