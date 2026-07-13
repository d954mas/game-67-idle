import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import test from "node:test";

const root = resolve(import.meta.dirname, "../../..");
const assetsRoot = join(root, "ai_studio", "assets");
const owners = ["catalog", "manifests", "sources", "intake", "licenses", "previews"];
const internalOwnerImport = /\/(?:catalog|intake|licenses|manifests|previews|sources)\/(?!ops\.mjs$)/;

function importSpecifiers(source) {
  return [...source.matchAll(/\bimport\s+(?:(?:[^"'`;]|\r|\n)+?\sfrom\s+)?["']([^"']+)["']/g)].map((match) => match[1]);
}

test("owner facade guard covers single-line and multiline imports", () => {
  assert.deepEqual(importSpecifiers('import { query } from "../catalog/query.mjs";'), ["../catalog/query.mjs"]);
  assert.deepEqual(importSpecifiers('import {\n  query,\n}\nfrom "../catalog/query.mjs";'), ["../catalog/query.mjs"]);
  assert.match(importSpecifiers('import { query } from "../catalog/query.mjs";')[0], internalOwnerImport);
  assert.doesNotMatch(importSpecifiers('import { query } from "../catalog/ops.mjs";')[0], internalOwnerImport);
});

test("asset storage responsibilities have explicit compact public owners", () => {
  for (const owner of owners) {
    const opsPath = join(assetsRoot, owner, "ops.mjs");
    assert.equal(existsSync(opsPath), true, `${owner}/ops.mjs must exist`);
    const ops = readFileSync(opsPath, "utf8");
    assert.ok(ops.split(/\r?\n/).length <= 40, `${owner}/ops.mjs must stay a compact router`);
    assert.doesNotMatch(ops, /node:sqlite|DatabaseSync/, `${owner}/ops.mjs must not load implementation detail`);
  }
});

test("catalog implementation is split at tested store, source-record, query, and preview boundaries", () => {
  for (const file of ["index.mjs", "store.mjs", "source_records.mjs", "query.mjs"]) {
    const path = join(assetsRoot, "catalog", file);
    assert.equal(existsSync(path), true, `catalog/${file} must exist`);
    assert.ok(readFileSync(path, "utf8").split(/\r?\n/).length < 600, `catalog/${file} must stay bounded`);
  }
  assert.equal(existsSync(join(assetsRoot, "previews", "status.mjs")), true);
});

test("canonical preview HDR keeps its approved bytes at the new owner path", () => {
  const bytes = readFileSync(join(assetsRoot, "previews", "studio_env.hdr"));
  assert.equal(createHash("sha256").update(bytes).digest("hex"), "6249d61cb8e5534e6396a888b45732ab93716820a13b4ace7cd53d1565c90361");
});

test("preview renderer keeps its temporary workspace inside the repository", () => {
  const renderer = join(assetsRoot, "previews", "render_library_previews.mjs");
  const source = readFileSync(renderer, "utf8");
  assert.match(source, /const ROOT = resolve\(HERE, "\.\.", "\.\.", "\.\."\);/);
  const resolvedRoot = resolve(dirname(renderer), "..", "..", "..");
  assert.equal(resolvedRoot, root);
  for (const path of [join(resolvedRoot, "tmp", "lib_previews_out"), join(resolvedRoot, "tmp", "lib_previews_manifest.txt")]) {
    const rel = relative(root, path);
    assert.equal(rel === ".." || rel.startsWith(`..${process.platform === "win32" ? "\\\\" : "/"}`), false, `${path} escapes the repository`);
  }
});

test("temporary backlog storage is gone and live asset surfaces use owner facades", () => {
  assert.equal(existsSync(join(assetsRoot, "backlog", "storage")), false);
  const violations = [];
  const tracked = execFileSync("git", ["ls-files", "-z", "--", ".claude", ".codex", "ai_studio", "games", "templates"], { cwd: root })
    .toString("utf8").split("\0").filter(Boolean);
  for (const rel of tracked) {
    if (rel.startsWith("ai_studio/taskboard/items/")) continue;
    const path = join(root, rel);
    if (!existsSync(path) || statSync(path).size > 2_000_000) continue;
    const text = readFileSync(path, "utf8");
    if (rel === "ai_studio/assets/tests/ownership.test.mjs") continue;
    const retiredPath = ["assets", "backlog", "storage"].join("/");
    if (text.includes(retiredPath) || text.includes(["backlog", "storage"].join("/"))) violations.push(rel);
  }
  assert.deepEqual(violations, []);

  for (const path of [
    join(assetsRoot, "gallery", "api.mjs"),
    join(assetsRoot, "gallery", "build_review.mjs"),
    join(assetsRoot, "gallery", "promote.mjs"),
    join(assetsRoot, "gallery", "pull.mjs"),
    join(assetsRoot, "items_viewer", "ops.mjs"),
    join(root, "games", "new_game.mjs"),
    join(root, "templates", "new_template.mjs"),
  ]) {
    const source = readFileSync(path, "utf8");
    for (const specifier of importSpecifiers(source)) assert.doesNotMatch(specifier, internalOwnerImport, path);
  }
});
