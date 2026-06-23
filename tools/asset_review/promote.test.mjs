import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, mkdir, writeFile, readFile, rm } from "node:fs/promises";
import { existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { execFile } from "node:child_process";
import { promisify } from "node:util";

const exec = promisify(execFile);
const TOOL = "tools/asset_review/promote.mjs";

async function setup(extraAssets = []) {
  const root = await mkdtemp(join(tmpdir(), "promote-"));
  const repo = join(root, "repo");
  const library = join(root, "library");
  await mkdir(join(repo, "assets", "source", "models", "kenney"), { recursive: true });
  await writeFile(join(repo, "assets", "source", "models", "kenney", "desk.obj"), "obj-bytes");
  await writeFile(join(repo, "assets", "source", "models", "kenney", "License.txt"), "CC0 license");
  await mkdir(join(repo, "assets", "fonts"), { recursive: true });
  await writeFile(join(repo, "assets", "fonts", "Lilita.ttf"), "ttf-bytes");
  const assets = [
    { id: "m1", name: "desk.obj", relpath: "assets/source/models/kenney/desk.obj", kind: "model", origin: "sourced", source: "kenney", licenseFile: "assets/source/models/kenney/License.txt" },
    { id: "f1", name: "Lilita.ttf", relpath: "assets/fonts/Lilita.ttf", kind: "font", origin: "unknown", source: "unknown", licenseFile: "" },
    ...extraAssets,
  ];
  const manifestPath = join(root, "manifest.json");
  await writeFile(manifestPath, JSON.stringify({ game: "t", base: "x", generated: "now", assets }));
  return { root, repo, library, manifestPath };
}

function run(args) {
  return exec(process.execPath, [TOOL, ...args]);
}
async function expectFail(args, re) {
  await assert.rejects(run(args), (e) => re.test(e.stderr || e.message), `expected failure matching ${re}`);
}

test("dry-run writes nothing to the library", async () => {
  const { root, repo, library, manifestPath } = await setup();
  try {
    await run(["--manifest", manifestPath, "--ids", "m1", "--repo", repo, "--library", library, "--source", "kenney", "--license", "CC0-1.0"]);
    assert.equal(existsSync(join(library, "catalog")), false);
    assert.equal(existsSync(join(library, "files")), false);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("--apply writes catalog + license + log with correct schema", async () => {
  const { root, repo, library, manifestPath } = await setup();
  try {
    await run(["--manifest", manifestPath, "--ids", "m1", "--repo", repo, "--library", library, "--source", "kenney", "--license", "CC0-1.0", "--apply"]);
    const id = "kenney__desk__cc0-1-0";
    assert.equal(existsSync(join(library, "files", "models", id, "desk.obj")), true);
    const catalog = await readFile(join(library, "catalog", "models", `${id}.md`), "utf8");
    assert.match(catalog, /^asset_id: kenney__desk__cc0-1-0$/m);
    assert.match(catalog, /^kind: model$/m);
    assert.match(catalog, /^origin: sourced$/m);
    assert.match(catalog, /^license: CC0-1\.0$/m);
    assert.match(catalog, /^commercial_use: true$/m);
    assert.match(catalog, /^shipping_decision: allowed$/m);
    assert.match(catalog, /SHA256: [0-9a-f]{64}/);
    assert.equal(existsSync(join(library, "licenses", id, "license.md")), true);
    assert.equal(existsSync(join(library, "licenses", id, "License.txt")), true); // copied vendor license
    const log = await readFile(join(library, "log.md"), "utf8");
    assert.match(log, /promoted kenney__desk__cc0-1-0/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("refuses to overwrite an existing record without --overwrite", async () => {
  const { root, repo, library, manifestPath } = await setup();
  try {
    const base = ["--manifest", manifestPath, "--ids", "m1", "--repo", repo, "--library", library, "--source", "kenney", "--license", "CC0-1.0", "--apply"];
    await run(base);
    await expectFail(base, /already in library/);
    await run([...base, "--overwrite"]); // succeeds with the flag
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("rejects duplicate asset_ids within one batch", async () => {
  // two distinct files -> same kebab slug + source + license => same asset_id
  const { root, repo, library, manifestPath } = await setup([
    { id: "m2", name: "desk.obj", relpath: "assets/source/models/kenney/sub/desk.obj", kind: "model", origin: "sourced", source: "kenney", licenseFile: "" },
  ]);
  try {
    await mkdir(join(repo, "assets", "source", "models", "kenney", "sub"), { recursive: true });
    await writeFile(join(repo, "assets", "source", "models", "kenney", "sub", "desk.obj"), "obj2");
    await expectFail(
      ["--manifest", manifestPath, "--ids", "m1,m2", "--repo", repo, "--library", library, "--source", "kenney", "--license", "CC0-1.0", "--apply"],
      /duplicate asset_id/,
    );
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("rejects a font + non-font mixed batch under one license", async () => {
  const { root, repo, library, manifestPath } = await setup();
  try {
    await expectFail(
      ["--manifest", manifestPath, "--ids", "m1,f1", "--repo", repo, "--library", library, "--source", "x", "--license", "CC0-1.0", "--apply"],
      /mixes font and non-font/,
    );
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("validation: missing --license, bad --origin, unknown id, font under CC0", async () => {
  const { root, repo, library, manifestPath } = await setup();
  const common = ["--manifest", manifestPath, "--repo", repo, "--library", library, "--source", "kenney"];
  try {
    await expectFail([...common, "--ids", "m1"], /missing --license/);
    await expectFail([...common, "--ids", "m1", "--license", "CC0-1.0", "--origin", "bogus"], /--origin must be/);
    await expectFail([...common, "--ids", "nope", "--license", "CC0-1.0"], /ids not in manifest/);
    await expectFail(["--manifest", manifestPath, "--repo", repo, "--library", library, "--source", "g", "--ids", "f1", "--license", "CC0-1.0"], /not a font license/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});
