import assert from "node:assert/strict";
import test from "node:test";
import { mkdtempSync, mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

import { createValidationReport, hasStrictFailures } from "../validate_map.mjs";

function write(root, rel, text = "") {
  const abs = join(root, rel);
  mkdirSync(join(abs, ".."), { recursive: true });
  writeFileSync(abs, text, "utf8");
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

  const report = createValidationReport({
    repoRoot: root,
    scanRoots: ["ai_studio"],
  });

  assert.equal(report.summary.missingInRepo, 1);
  assert.equal(report.summary.duplicateMappings, 1);
  assert.equal(report.summary.unmappedInAiStudio, 1);
  assert.equal(report.summary.missingDescriptions, 1);
  assert.equal(hasStrictFailures(report), true);
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

  const report = createValidationReport({
    repoRoot: root,
    scanRoots: ["ai_studio", "AGENTS.md"],
  });

  assert.equal(report.summary.unmappedOutsideAiStudio, 1);
  assert.deepEqual(report.issues.unmappedOutsideAiStudio, [{ path: "AGENTS.md" }]);
  assert.equal(hasStrictFailures(report), true);
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

  const report = createValidationReport({
    repoRoot: root,
    scanRoots: ["ai_studio"],
  });

  assert.equal(report.summary.unmappedInAiStudio, 1, "tree.json is intentionally still outside the test map");
  assert.ok(!report.issues.unmappedInAiStudio.some((item) => item.path.endsWith("public/app.js")));
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

  const report = createValidationReport({
    repoRoot: root,
    scanRoots: ["ai_studio"],
  });

  assert.equal(report.summary.unmappedInAiStudio, 0);
  assert.ok(!report.issues.unmappedInAiStudio.some((item) => item.path.endsWith("sample.test.mjs")));
  assert.ok(!report.issues.unmappedInAiStudio.some((item) => item.path.endsWith("sample_check.mjs")));
});
