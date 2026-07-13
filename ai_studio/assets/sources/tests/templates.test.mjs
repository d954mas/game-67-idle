import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { listRegisteredTemplates, registerTemplateAssetSource, templateRegistryPath } from "../templates.mjs";

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "catalog-templates-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return root;
}

test("template asset source is a workspace catalog adapter", (t) => {
  const root = fixture(t);
  assert.deepEqual(registerTemplateAssetSource(root, { id: "demo-template", title: "Demo" }), {
    id: "demo-template", title: "Demo", folder: "templates/demo-template", assets: "templates/demo-template/assets", status: "active",
  });
  assert.equal(templateRegistryPath(root), "ai_studio/workspace/catalog.json");
  assert.deepEqual(listRegisteredTemplates(root).map((template) => template.id), ["demo-template"]);
});

test("template asset source enforces derived roots and strict ids", (t) => {
  const root = fixture(t);
  assert.throws(() => registerTemplateAssetSource(root, { id: "Bad" }), /lowercase kebab-case/);
  assert.throws(() => registerTemplateAssetSource(root, { id: "demo", folder: "../escape" }), /folder must be templates\/demo/);
});
