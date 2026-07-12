import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import test from "node:test";

const opsDir = resolve(import.meta.dirname, "..", "ops");
const limits = { core: 8, elements: 12, groups: 8, generation: 18, history: 12, image_pipeline: 22 };
const specialist = /tools\/(?:dual_plate_generate|recipe_generate|anim_generate|prompt_assist|animation_assist)|tools\/video|vitmatte/i;

function importsIn(source) {
  return [...source.matchAll(/import\s*\{([\s\S]*?)\}\s*from\s*"([^"]+)";/g)];
}

function withoutComments(source) {
  return source.replace(/\/\*[\s\S]*?\*\//g, "").replace(/^\s*\/\/.*$/gm, "");
}

test("ops domains keep scoped imports and no obvious unused binding", () => {
  for (const [domain, limit] of Object.entries(limits)) {
    const source = readFileSync(resolve(opsDir, `${domain}.mjs`), "utf8");
    const imports = importsIn(source);
    assert.ok(imports.length <= limit, `${domain} has ${imports.length} imports (limit ${limit})`);
    const body = withoutComments(source.slice(Math.max(...imports.map((match) => match.index + match[0].length))));
    for (const match of imports) {
      const bindings = match[1].split(",").map((value) => value.trim()).filter(Boolean);
      for (const binding of bindings) {
        const parts = binding.split(/\s+as\s+/);
        const local = (parts[1] || parts[0]).trim();
        assert.match(body, new RegExp(`\\b${local}\\b`), `${domain} imports unused ${local}`);
      }
    }
    if (["core", "elements", "groups", "history"].includes(domain)) {
      assert.doesNotMatch(imports.map((match) => match[2]).join("\n"), specialist, `${domain} imports a specialist pipeline`);
    }
  }
});
