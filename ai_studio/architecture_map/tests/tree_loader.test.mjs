import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

import { createArchitectureMapApi } from "../api.mjs";
import { loadArchitectureTree } from "../tree_loader.mjs";

const repoRoot = resolve(fileURLToPath(new URL("../../..", import.meta.url)));

function write(root, rel, text) {
  const abs = join(root, rel);
  mkdirSync(dirname(abs), { recursive: true });
  writeFileSync(abs, text, "utf8");
}

function tempRoot(t, prefix) {
  const root = mkdtempSync(join(tmpdir(), prefix));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return root;
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

test("single-file loader returns the authored tree without ref materialization", (t) => {
  const root = tempRoot(t, "architecture-tree-");
  const tree = {
    schema: 2,
    root: {
      id: "studio",
      title: "Studio",
      kind: "root",
      owner: "Studio Lead",
      description: "Workspace ownership.",
      entry: "ai_studio/README.md",
      verifyDomain: "harness",
      children: [{
        id: "module",
        title: "Module",
        kind: "module",
        path: "ai_studio/module",
        owner: "Module",
        description: "Module boundary.",
        entry: "ai_studio/module/README.md",
        verifyDomain: "harness",
      }],
    },
  };
  write(root, "ai_studio/tree.json", `${JSON.stringify(tree)}\n`);
  assert.deepEqual(loadArchitectureTree(root, "ai_studio/tree.json"), tree);
});

test("single-file loader rejects missing, malformed, escaping, absolute, and ref-based maps", (t) => {
  const root = tempRoot(t, "architecture-tree-errors-");
  assert.throws(() => loadArchitectureTree(root, "ai_studio/tree.json"), /tree .*not found/i);

  write(root, "ai_studio/tree.json", "{ nope");
  assert.throws(() => loadArchitectureTree(root, "ai_studio/tree.json"), /tree .*malformed JSON/i);
  assert.throws(() => loadArchitectureTree(root, "../outside.json"), /escapes repository root/i);
  assert.throws(() => loadArchitectureTree(root, resolve(root, "ai_studio/tree.json")), /must be repository-relative/i);

  write(root, "ai_studio/tree.json", JSON.stringify({ root: { id: "studio", parts: ["part.json"] } }));
  assert.throws(() => loadArchitectureTree(root, "ai_studio/tree.json"), /single-file.*parts/i);
  write(root, "ai_studio/tree.json", JSON.stringify({ root: { id: "studio", children: [{ id: "nested", parts: [] }] } }));
  assert.throws(() => loadArchitectureTree(root, "ai_studio/tree.json"), /single-file.*parts/i);
  write(root, "ai_studio/tree.json", JSON.stringify({ root: { id: "studio", children: [{ id: "nested", ref: "part.json" }] } }));
  assert.throws(() => loadArchitectureTree(root, "ai_studio/tree.json"), /single-file.*ref/i);
});

test("GET architecture tree serves the same single-file model", async () => {
  const handle = createArchitectureMapApi(repoRoot);
  const res = mockRes();
  const handled = await handle({ method: "GET" }, res, new URL("http://x/api/architecture-tree"));
  assert.equal(handled, true);
  assert.equal(res.statusCode, 200);
  assert.deepEqual(JSON.parse(res.body), loadArchitectureTree(repoRoot, "ai_studio/tree.json"));
});

test("GET architecture validation remains live and unrelated routes remain unhandled", async () => {
  const handle = createArchitectureMapApi(repoRoot);
  const validation = mockRes();
  assert.equal(await handle({ method: "GET" }, validation, new URL("http://x/api/architecture-validation")), true);
  assert.equal(validation.statusCode, 200);
  const payload = JSON.parse(validation.body);
  assert.equal(payload.schema, 1);
  assert.ok(payload.summary && payload.issues);

  const unrelated = mockRes();
  assert.equal(await handle({ method: "GET" }, unrelated, new URL("http://x/api/board")), false);
  assert.equal(unrelated.statusCode, null);
});

test("API contains loader errors instead of exposing filesystem data", async (t) => {
  const root = tempRoot(t, "architecture-api-errors-");
  write(root, "ai_studio/tree.json", "{ malformed");
  const handle = createArchitectureMapApi(root);
  const res = mockRes();
  assert.equal(await handle({ method: "GET" }, res, new URL("http://x/api/architecture-tree")), true);
  assert.equal(res.statusCode, 500);
  const payload = JSON.parse(res.body);
  assert.match(payload.error, /malformed JSON/i);
  assert.equal(Object.keys(payload).length, 1);
});
