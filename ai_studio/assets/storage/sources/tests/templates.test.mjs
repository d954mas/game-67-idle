import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { listRegisteredTemplates, registerTemplateAssetSource, templateRegistryPath } from "../templates.mjs";

function tempRoot() {
  return mkdtempSync(join(tmpdir(), "ai-studio-templates-registry-"));
}

test("listRegisteredTemplates falls back to the repository template source", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.deepEqual(listRegisteredTemplates(root), [{
    id: "template",
    title: "Template",
    folder: "template",
    assets: "template/assets",
    status: "active",
  }]);
});

test("registerTemplateAssetSource creates and lists a template asset source", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const registered = registerTemplateAssetSource(root, { id: "mobile-template", title: "Mobile Template" });

  assert.equal(registered.assets, "mobile-template/assets");
  assert.equal(templateRegistryPath(root), "ai_studio/assets/storage/sources/templates.json");
  assert.deepEqual(listRegisteredTemplates(root), [
    {
      id: "mobile-template",
      title: "Mobile Template",
      folder: "mobile-template",
      assets: "mobile-template/assets",
      status: "active",
    },
    {
      id: "template",
      title: "Template",
      folder: "template",
      assets: "template/assets",
      status: "active",
    },
  ]);

  const parsed = JSON.parse(readFileSync(join(root, "ai_studio", "assets", "storage", "sources", "templates.json"), "utf8"));
  assert.equal(parsed.templates.length, 2);
});

test("registerTemplateAssetSource rejects non-kebab ids", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.throws(() => registerTemplateAssetSource(root, { id: "Bad Template" }), /lowercase kebab-case/);
});
