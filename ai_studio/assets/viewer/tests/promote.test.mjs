import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, mkdir, writeFile, readFile, rm } from "node:fs/promises";
import { existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { execFile } from "node:child_process";
import { promisify } from "node:util";

const exec = promisify(execFile);
const TOOL = "ai_studio/assets/viewer/promote.mjs";

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
    assert.equal(existsSync(join(library, "packs")), false);
    assert.equal(existsSync(join(library, "restricted")), false);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("--apply writes pack manifest + license + log with correct schema", async () => {
  const { root, repo, library, manifestPath } = await setup();
  try {
    await run(["--manifest", manifestPath, "--ids", "m1", "--repo", repo, "--library", library, "--source", "kenney", "--license", "CC0-1.0", "--apply"]);
    const id = "kenney__desk__cc0-1-0";
    assert.equal(existsSync(join(library, "packs", "kenney", "files", id, "desk.obj")), true);
    const row = JSON.parse((await readFile(join(library, "packs", "kenney", "assets.jsonl"), "utf8")).trim());
    assert.equal(row.asset_id, id);
    assert.equal(row.kind, "model");
    assert.equal(row.origin, "sourced");
    assert.equal(row.license, "CC0-1.0");
    assert.equal(row.commercial_use, "true");
    assert.match(row.sha256, /^[0-9a-f]{64}$/);
    assert.equal(existsSync(join(library, "packs", "kenney", "licenses", `${id}.md`)), true);
    assert.equal(existsSync(join(library, "packs", "kenney", "licenses", `${id}-License.txt`)), true);
    const log = await readFile(join(library, "intake-log.md"), "utf8");
    assert.match(log, /promoted kenney__desk__cc0-1-0/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("custom license defaults to restricted publish=false unless explicitly proven", async () => {
  const { root, repo, library, manifestPath } = await setup();
  try {
    await run([
      "--manifest", manifestPath,
      "--ids", "m1",
      "--repo", repo,
      "--library", library,
      "--source", "vendor",
      "--license", "Vendor Custom",
      "--license-url", "https://example.test/license",
      "--apply",
    ]);
    const id = "vendor__desk__vendor-custom";
    const row = JSON.parse((await readFile(join(library, "restricted", "packs", "vendor", "assets.jsonl"), "utf8")).trim());
    assert.equal(row.asset_id, id);
    assert.equal(row.license_kind, "custom");
    assert.equal(row.redistribution_allowed, "false");
    assert.equal(row.publish, "false");
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("CC-BY promotion can be added during development without final attribution metadata", async () => {
  const { root, repo, library, manifestPath } = await setup();
  try {
    await run(
      ["--manifest", manifestPath, "--ids", "m1", "--repo", repo, "--library", library, "--source", "artist", "--license", "CC-BY-4.0"],
    );
    assert.equal(existsSync(join(library, "packs")), false, "dry-run only");
    await run([
      "--manifest", manifestPath,
      "--ids", "m1",
      "--repo", repo,
      "--library", library,
      "--source", "artist",
      "--license", "CC-BY-4.0",
      "--author-vendor", "Example Artist",
      "--source-page-url", "https://example.test/asset",
      "--apply",
    ]);
    const id = "artist__desk__cc-by-4-0";
    const row = JSON.parse((await readFile(join(library, "packs", "artist", "assets.jsonl"), "utf8")).trim());
    assert.equal(row.attribution_required, "true");
    assert.equal(row.notice_required, "true");
    assert.equal(row.credit_text, "Example Artist - https://example.test/asset");
    assert.equal(row.publish, "true");
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("custom publish=true requires explicit redistribution, commercial, and modification rights", async () => {
  const { root, repo, library, manifestPath } = await setup();
  try {
    await expectFail(
      [
        "--manifest", manifestPath, "--ids", "m1", "--repo", repo, "--library", library,
        "--source", "vendor", "--license", "Vendor Custom", "--license-url", "https://example.test/license",
        "--publish", "true",
      ],
      /redistribution_allowed=true|commercial_use=true|modification_allowed=true/,
    );
    await run([
      "--manifest", manifestPath,
      "--ids", "m1",
      "--repo", repo,
      "--library", library,
      "--source", "vendor",
      "--license", "Vendor Custom",
      "--license-url", "https://example.test/license",
      "--publish", "true",
      "--redistribution-allowed", "true",
      "--commercial-use", "true",
      "--modification-allowed", "true",
      "--apply",
    ]);
    const id = "vendor__desk__vendor-custom";
    const row = JSON.parse((await readFile(join(library, "packs", "vendor", "assets.jsonl"), "utf8")).trim());
    assert.equal(row.publish, "true");
    assert.equal(row.redistribution_allowed, "true");
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
