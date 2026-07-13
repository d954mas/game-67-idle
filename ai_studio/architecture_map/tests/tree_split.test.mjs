import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import test from "node:test";
import { mkdtempSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
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

test("nested parts recursively materialize with exact order and fields", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-nested-split-"));
  write(root, "ai_studio/tree.json", JSON.stringify({
    schema: 1,
    index: true,
    root: { id: "studio", title: "Studio", parts: ["parts/owner.json"] },
  }));
  write(root, "ai_studio/parts/owner.json", JSON.stringify({
    id: "owner",
    title: "Owner",
    kind: "module",
    custom: { keep: [1, 2, 3] },
    parts: ["owner/first.json", "owner/second.json"],
  }));
  write(root, "ai_studio/parts/owner/first.json", JSON.stringify({ id: "first", kind: "doc", path: "one" }));
  write(root, "ai_studio/parts/owner/second.json", JSON.stringify({
    id: "second",
    kind: "group",
    parts: ["second/leaf.json"],
  }));
  write(root, "ai_studio/parts/owner/second/leaf.json", JSON.stringify({ id: "leaf", kind: "doc", path: "two" }));

  assert.deepEqual(loadArchitectureTree(root, "ai_studio/tree.json"), {
    schema: 1,
    root: {
      id: "studio",
      title: "Studio",
      children: [{
        id: "owner",
        title: "Owner",
        kind: "module",
        custom: { keep: [1, 2, 3] },
        children: [
          { id: "first", kind: "doc", path: "one" },
          { id: "second", kind: "group", children: [{ id: "leaf", kind: "doc", path: "two" }] },
        ],
      }],
    },
  });
});

test("nested ref failures name the referrer and requested path", () => {
  const root = mkdtempSync(join(tmpdir(), "architecture-nested-errors-"));
  write(root, "ai_studio/tree.json", JSON.stringify({ root: { id: "studio", parts: ["parts/owner.json"] } }));
  write(root, "ai_studio/parts/owner.json", JSON.stringify({ id: "owner", parts: ["missing.json"] }));
  assert.throws(
    () => loadArchitectureTree(root, "ai_studio/tree.json"),
    /architecture tree ref "missing\.json" from "ai_studio\/parts\/owner\.json".*not found/i,
  );

  write(root, "ai_studio/parts/owner.json", JSON.stringify({ id: "owner", parts: ["broken.json"] }));
  write(root, "ai_studio/parts/broken.json", "{ nope");
  assert.throws(
    () => loadArchitectureTree(root, "ai_studio/tree.json"),
    /architecture tree ref "broken\.json" from "ai_studio\/parts\/owner\.json".*malformed json/i,
  );

  write(root, "ai_studio/parts/owner.json", JSON.stringify({ id: "owner", parts: ["cycle.json"] }));
  write(root, "ai_studio/parts/cycle.json", JSON.stringify({ id: "cycle", parts: ["owner.json"] }));
  assert.throws(
    () => loadArchitectureTree(root, "ai_studio/tree.json"),
    /architecture tree ref cycle: ai_studio\/parts\/owner\.json -> ai_studio\/parts\/cycle\.json -> ai_studio\/parts\/owner\.json/i,
  );

  write(root, "ai_studio/parts/owner.json", JSON.stringify({ id: "owner", parts: [42] }));
  assert.throws(
    () => loadArchitectureTree(root, "ai_studio/tree.json"),
    /architecture tree ref from "ai_studio\/parts\/owner\.json" must be a non-empty string/i,
  );
  write(root, "ai_studio/parts/owner.json", JSON.stringify({ id: "owner", parts: [], children: [] }));
  assert.throws(
    () => loadArchitectureTree(root, "ai_studio/tree.json"),
    /cannot contain both parts and children/i,
  );

  write(root, "ai_studio/parts/owner.json", JSON.stringify({ id: "owner", parts: ["../../../../outside.json"] }));
  assert.throws(
    () => loadArchitectureTree(root, "ai_studio/tree.json"),
    /architecture tree ref "\.\.\/\.\.\/\.\.\/\.\.\/outside\.json" from "ai_studio\/parts\/owner\.json".*escapes repository root/i,
  );
});

test("repo owner storage remains recursively split and context-bounded", () => {
  for (const name of ["module-assets", "module-hot"]) {
    const owner = JSON.parse(readFileSync(join(repoRoot, `ai_studio/architecture_map/tree/${name}.json`), "utf8"));
    assert.ok(Array.isArray(owner.parts) && owner.parts.length > 1, `${name} owner must route nested parts`);
    assert.ok(!("children" in owner), `${name} owner must not remain a child monolith`);
  }
  const sizes = [];
  const walk = (path) => {
    for (const entry of readdirSync(path, { withFileTypes: true })) {
      const child = join(path, entry.name);
      if (entry.isDirectory()) walk(child);
      else if (entry.name.endsWith(".json")) sizes.push(statSync(child).size);
    }
  };
  walk(join(repoRoot, "ai_studio/architecture_map/tree/module-assets"));
  walk(join(repoRoot, "ai_studio/architecture_map/tree/module-hot"));
  assert.ok(Math.max(...sizes) <= 19_000, `largest recursive owner part exceeds 19 KB: ${Math.max(...sizes)}`);
});

test("repo merged model keeps the accepted pre-split hash", () => {
  const merged = loadArchitectureTree(repoRoot, "ai_studio/tree.json");
  const hash = createHash("sha256").update(JSON.stringify(merged)).digest("hex");
  assert.equal(hash, "338a294b6cab2cc66c2de6853714136bae10e247851e16d3438bb0f2144e1e8b");
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
  assert.deepEqual(payload, loadArchitectureTree(repoRoot, "ai_studio/tree.json"), "API and loader materialize the identical model");
  const visit = (value) => {
    if (!value || typeof value !== "object") return;
    assert.ok(!Object.hasOwn(value, "parts"), "API payload must not expose ref markers");
    for (const child of Array.isArray(value) ? value : Object.values(value)) visit(child);
  };
  visit(payload);
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
