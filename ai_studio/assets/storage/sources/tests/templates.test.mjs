import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
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
    folder: "templates/template",
    assets: "templates/template/assets",
    status: "active",
  }]);
});

test("registerTemplateAssetSource creates and lists a template asset source", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const registered = registerTemplateAssetSource(root, { id: "mobile-template", title: "Mobile Template" });

  assert.equal(registered.assets, "templates/mobile-template/assets");
  assert.equal(templateRegistryPath(root), "templates/templates.json");
  assert.deepEqual(listRegisteredTemplates(root), [
    {
      id: "mobile-template",
      title: "Mobile Template",
      folder: "templates/mobile-template",
      assets: "templates/mobile-template/assets",
      status: "active",
    },
    {
      id: "template",
      title: "Template",
      folder: "templates/template",
      assets: "templates/template/assets",
      status: "active",
    },
  ]);

  const parsed = JSON.parse(readFileSync(join(root, "templates", "templates.json"), "utf8"));
  assert.equal(parsed.templates.length, 2);
});

test("registerTemplateAssetSource rejects non-kebab ids", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.throws(() => registerTemplateAssetSource(root, { id: "Bad Template" }), /lowercase kebab-case/);
});

test("registerTemplateAssetSource keeps folder and assets inside the repository", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.throws(
    () => registerTemplateAssetSource(root, { id: "bad-template", folder: "../outside" }),
    /template folder must be repo-relative/,
  );
  assert.throws(
    () => registerTemplateAssetSource(root, { id: "bad-template", assets: "C:/outside/assets" }),
    /template assets must be repo-relative/,
  );
});

test("listRegisteredTemplates accepts UTF-8 BOM registry files", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const path = join(root, "templates", "templates.json");
  mkdirSync(join(root, "templates"), { recursive: true });
  writeFileSync(path, `\uFEFF${JSON.stringify({
    schema: "ai_studio.assets.templates.v1",
    templates: [{ id: "template", title: "Template", folder: "templates/template", assets: "templates/template/assets" }],
  })}`, "utf8");

  assert.deepEqual(listRegisteredTemplates(root), [{
    id: "template",
    title: "Template",
    folder: "templates/template",
    assets: "templates/template/assets",
    status: "active",
  }]);
});
