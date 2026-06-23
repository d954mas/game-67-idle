import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, mkdir, writeFile, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { parseFrontmatter, filterRecords, scanLibrary, parseArgs, recordDecision, FREE_SOURCES, ORIGINS } from "./find_assets.mjs";

test("parseFrontmatter reads scalars, quoted strings, lists, and BOM", () => {
  const fm = parseFrontmatter('﻿---\nasset_id: kenney__sofa__cc0\nkind: model\norigin: sourced\ntitle: "Lounge Sofa"\ntags: [model, furniture, cc0]\n---\nbody');
  assert.equal(fm.asset_id, "kenney__sofa__cc0");
  assert.equal(fm.origin, "sourced");
  assert.equal(fm.title, "Lounge Sofa");
  assert.deepEqual(fm.tags, ["model", "furniture", "cc0"]);
});

test("filterRecords: OR on tags, exact kind+origin, substring query", () => {
  const recs = [
    { asset_id: "a", kind: "model", origin: "sourced", license: "CC0", title: "Sofa", description: "lounge couch", tags: ["furniture", "sofa"] },
    { asset_id: "b", kind: "texture", origin: "ai", license: "n/a", title: "Wood", description: "planks", tags: ["wood", "tileable"] },
    { asset_id: "c", kind: "model", origin: "mine", license: "n/a", title: "Robot", description: "hero mech", tags: ["robot", "character"] },
  ];
  assert.deepEqual(filterRecords(recs, { tags: ["sofa", "wood"] }).map((r) => r.asset_id).sort(), ["a", "b"]);
  assert.deepEqual(filterRecords(recs, { kind: "model" }).map((r) => r.asset_id).sort(), ["a", "c"]);
  assert.deepEqual(filterRecords(recs, { kind: "models" }).map((r) => r.asset_id).sort(), ["a", "c"]);
  assert.deepEqual(filterRecords(recs, { origin: "ai" }).map((r) => r.asset_id), ["b"]);
  assert.deepEqual(filterRecords(recs, { query: "mech" }).map((r) => r.asset_id), ["c"]);
  assert.equal(filterRecords(recs, { tags: ["nothing-here"] }).length, 0);
});

test("scanLibrary normalizes records and defaults unknown origin", async () => {
  const dir = await mkdtemp(join(tmpdir(), "lib-"));
  try {
    await mkdir(join(dir, "catalog", "models"), { recursive: true });
    await writeFile(
      join(dir, "catalog", "models", "kenney__desk__cc0.md"),
      "---\nasset_id: kenney__desk__cc0\ntitle: Desk\nkind: model\nstatus: accepted\nlicense: CC0\norigin: sourced\ntags: [model, furniture]\nresource: files/models/kenney__desk__cc0/\n---\n# Desk\n",
      "utf8",
    );
    // legacy record with no origin -> unknown; README/index skipped
    await writeFile(join(dir, "catalog", "models", "index.md"), "# index\n", "utf8");
    await writeFile(
      join(dir, "catalog", "models", "khronos__box__cc-by.md"),
      "---\nasset_id: khronos__box__cc-by\ntitle: Box\nkind: model\nlicense: CC-BY-4.0\n---\n",
      "utf8",
    );
    const recs = await scanLibrary(dir);
    assert.equal(recs.length, 2);
    const desk = recs.find((r) => r.asset_id === "kenney__desk__cc0");
    assert.equal(desk.origin, "sourced");
    assert.deepEqual(desk.tags, ["model", "furniture"]);
    const box = recs.find((r) => r.asset_id === "khronos__box__cc-by");
    assert.equal(box.origin, "unknown");
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
});

test("free sources + origins are well-formed constants", () => {
  assert.ok(FREE_SOURCES.length >= 5);
  assert.ok(FREE_SOURCES.every((s) => s.name && s.url.startsWith("http")));
  assert.deepEqual(ORIGINS, ["mine", "ai", "sourced", "unknown"]);
});

test("parseFrontmatter handles CRLF and BOM+CRLF (Windows/YandexDisk store)", () => {
  const fm = parseFrontmatter("﻿---\r\nasset_id: a__b__cc0\r\nkind: model\r\norigin: sourced\r\ntags: [model, furniture]\r\n---\r\nbody\r\n");
  assert.equal(fm.asset_id, "a__b__cc0");
  assert.equal(fm.origin, "sourced");
  assert.deepEqual(fm.tags, ["model", "furniture"]);
});

test("parseArgs rejects a flag consumed as a value, and validates --origin", () => {
  assert.throws(() => parseArgs(["--query", "--json"]), /missing value for --query/);
  assert.throws(() => parseArgs(["--origin", "nope"]), /--origin must be one of/);
  const ok = parseArgs(["--tags", "sofa,desk", "--kind", "model", "--origin", "sourced", "--json"]);
  assert.equal(ok.tags, "sofa,desk");
  assert.equal(ok.json, true);
  assert.equal(ok.origin, "sourced");
});

test("recordDecision writes one entry per family (replaces on repeat)", async () => {
  const dir = await mkdtemp(join(tmpdir(), "dec-"));
  const out = join(dir, "decision.json");
  try {
    await recordDecision({ family: "room-furniture", decision: "source+intake", reason: "kenney fits", out });
    let list = JSON.parse(await readFile(out, "utf8"));
    assert.equal(list.length, 1);
    assert.equal(list[0].decision, "source+intake");
    await recordDecision({ family: "room-furniture", decision: "generate", reason: "nothing fit", out });
    list = JSON.parse(await readFile(out, "utf8"));
    assert.equal(list.length, 1);
    assert.equal(list[0].decision, "generate");
    await assert.rejects(() => recordDecision({ family: "x", decision: "bogus", out }), /--decision must be one of/);
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
});
