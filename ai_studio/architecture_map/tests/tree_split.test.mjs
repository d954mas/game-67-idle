import assert from "node:assert/strict";
import test from "node:test";
import { mkdtempSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";

import { loadArchitectureTree, mergeArchitectureTree } from "../tree_loader.mjs";
import { createArchitectureMapApi } from "../api.mjs";

const repoRoot = resolve(fileURLToPath(new URL("../../..", import.meta.url)));

function write(root, rel, text) {
  const abs = join(root, rel);
  mkdirSync(dirname(abs), { recursive: true });
  writeFileSync(abs, text, "utf8");
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

test("mergeArchitectureTree returns a single-file tree unchanged", () => {
  const single = {
    schema: 1,
    root: { id: "studio", title: "studio", children: [{ id: "a", path: "ai_studio/a.md" }] },
  };
  const merged = mergeArchitectureTree(single, () => {
    throw new Error("loadPart must not be called for a single-file tree");
  });
  assert.deepEqual(merged, single);
});

test("split index + parts round-trips to the original single-file tree", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-split-"));
  const single = {
    schema: 1,
    description: "sample tree",
    root: {
      id: "studio",
      title: "AI Studio",
      kind: "root",
      tags: ["a", "b"],
      children: [
        { id: "one", kind: "doc", path: "ai_studio/one.md", description: "One." },
        {
          id: "two",
          kind: "module",
          children: [{ id: "two-child", kind: "doc", path: "ai_studio/two/child.md", description: "Child." }],
        },
        { id: "three", kind: "group", path: "ai_studio/three", description: "Three." },
      ],
    },
  };

  // Split: one part file per top-level child, index lists them in order.
  const parts = [];
  single.root.children.forEach((child, i) => {
    const rel = `parts/part-${i}.json`;
    write(root, `ai_studio/${rel}`, `${JSON.stringify(child, null, 2)}\n`);
    parts.push(rel);
  });
  const { children, ...rootMeta } = single.root;
  const index = { schema: single.schema, description: single.description, index: true, root: { ...rootMeta, parts } };
  write(root, "ai_studio/tree.json", `${JSON.stringify(index, null, 2)}\n`);

  const merged = loadArchitectureTree(root, "ai_studio/tree.json");
  assert.deepEqual(merged, single);
});

const requiredTopLevelIds = [
  "studio-readme",
  "workspace-folders",
  "studio-shell",
  "module:hot",
  "runtime-automation",
  "architecture-map",
];

test("the repo index merges required ownership roots without exposing root.parts", () => {
  const merged = loadArchitectureTree(repoRoot, "ai_studio/tree.json");
  assert.ok(Array.isArray(merged.root.children));
  const ids = merged.root.children.map((child) => child.id);
  assert.equal(new Set(ids).size, ids.length, "top-level ownership ids must be unique");
  for (const id of requiredTopLevelIds) assert.ok(ids.includes(id), `missing required ownership root ${id}`);
  assert.ok(!("parts" in merged.root), "merged root must not expose the parts index");
  assert.equal(merged.root.id, "studio");
});

test("GET /api/architecture-tree serves the merged tree", async () => {
  const handle = createArchitectureMapApi(repoRoot);
  const res = mockRes();
  const handled = await handle({ method: "GET" }, res, new URL("http://x/api/architecture-tree"));
  assert.equal(handled, true);
  assert.equal(res.statusCode, 200);
  const payload = JSON.parse(res.body);
  const ids = payload.root.children.map((child) => child.id);
  for (const id of requiredTopLevelIds) assert.ok(ids.includes(id), `API omitted required ownership root ${id}`);
  assert.ok(!("parts" in payload.root));
});

test("GET /api/architecture-validation serves a live report", async () => {
  const handle = createArchitectureMapApi(repoRoot);
  const res = mockRes();
  const handled = await handle({ method: "GET" }, res, new URL("http://x/api/architecture-validation"));
  assert.equal(handled, true);
  assert.equal(res.statusCode, 200);
  const payload = JSON.parse(res.body);
  assert.equal(payload.schema, 1);
  assert.ok(payload.summary && typeof payload.summary.mappedPaths === "number");
  assert.ok(payload.issues, "report exposes issue buckets");
});

test("unrelated API routes are not handled", async () => {
  const handle = createArchitectureMapApi(repoRoot);
  const res = mockRes();
  const handled = await handle({ method: "GET" }, res, new URL("http://x/api/board"));
  assert.equal(handled, false);
  assert.equal(res.statusCode, null);
});
