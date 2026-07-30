import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import { auditGameReleaseAssets, builderAssetPaths, packedAssetPaths } from "../game_release.mjs";

function write(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, value);
}

function fixture(t, overrides = {}) {
  const root = mkdtempSync(join(tmpdir(), "game-release-assets-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const bytes = Buffer.from("asset-bytes");
  const path = "assets/ui/panel.png";
  write(join(root, "assets", "release_inputs.json"), `${JSON.stringify({
    schema: "ai_studio.game_release_assets.v1",
    inputs: [path],
  })}\n`);
  write(join(root, ...path.split("/")), bytes);
  write(join(root, "src", "build_packs.c"), `nt_atlas_add(atlas, "${path}", &options);\n`);
  const record = {
    asset_id: "fixture__panel__cc0-1-0",
    source_resource: "ui/panel.png",
    origin: "sourced",
    license: "CC0-1.0",
    license_url: "https://creativecommons.org/publicdomain/zero/1.0/",
    license_kind: "cc",
    publish: "true",
    redistribution_allowed: "true",
    commercial_use: "true",
    modification_allowed: "true",
    provenance: "fixture source",
    sha256: createHash("sha256").update(bytes).digest("hex"),
    bytes: bytes.length,
    ...overrides,
  };
  write(join(root, "assets", "packs", "fixture", "assets.jsonl"), `${JSON.stringify(record)}\n`);
  return {
    root,
    path,
    record,
    tracked: [
      "assets/release_inputs.json",
      "assets/packs/fixture/assets.jsonl",
      "src/build_packs.c",
      path,
    ],
  };
}

test("game release asset audit validates packed tracked bytes and metadata", (t) => {
  const item = fixture(t);
  assert.deepEqual(packedAssetPaths(item.root), [item.path]);
  assert.deepEqual(builderAssetPaths(item.root), [item.path]);
  assert.deepEqual(auditGameReleaseAssets(item.root, { trackedPaths: item.tracked }), {
    ok: true, packed: 1, issues: [],
  });
});

test("game release asset audit requires exact agreement with pack builder inputs", (t) => {
  const builderOnly = fixture(t);
  write(
    join(builderOnly.root, "src", "build_packs.c"),
    `nt_atlas_add(atlas, "${builderOnly.path}", &options);\nnt_atlas_add(atlas, "assets/ui/extra.png", &options);\n`,
  );
  assert.match(
    auditGameReleaseAssets(builderOnly.root, { trackedPaths: builderOnly.tracked }).issues.join("\n"),
    /pack builder input is missing from release input contract/,
  );

  const contractOnly = fixture(t);
  write(join(contractOnly.root, "src", "build_packs.c"), "/* no binary inputs */\n");
  assert.match(
    auditGameReleaseAssets(contractOnly.root, { trackedPaths: contractOnly.tracked }).issues.join("\n"),
    /release input is not consumed by the pack builder/,
  );
});

test("game release asset audit fails on missing records stale hashes and release-forbidden licenses", (t) => {
  const missing = fixture(t);
  assert.match(
    auditGameReleaseAssets(missing.root, { packedPaths: ["assets/ui/missing.png"], trackedPaths: [] }).issues.join("\n"),
    /missing/,
  );
  const stale = fixture(t, { sha256: "0".repeat(64) });
  assert.match(auditGameReleaseAssets(stale.root, { trackedPaths: stale.tracked }).issues.join("\n"), /sha256 mismatch/);
  const forbidden = fixture(t, {
    license: "Proprietary", license_url: "", publish: "false", redistribution_allowed: "false",
  });
  assert.match(
    auditGameReleaseAssets(forbidden.root, { trackedPaths: forbidden.tracked }).issues.join("\n"),
    /redistribution|not publishable|license/i,
  );
});

test("game release asset audit rejects untracked packed inputs", (t) => {
  const item = fixture(t);
  assert.match(auditGameReleaseAssets(item.root, { trackedPaths: [] }).issues.join("\n"), /not tracked/);
});

test("game release asset contract rejects path escapes and audit rejects duplicate or stale metadata", (t) => {
  const escaped = fixture(t);
  write(join(escaped.root, "assets", "release_inputs.json"), JSON.stringify({
    schema: "ai_studio.game_release_assets.v1", inputs: ["assets/../outside.png"],
  }));
  assert.throws(() => packedAssetPaths(escaped.root), /safe binary asset paths/);

  const duplicate = fixture(t);
  write(
    join(duplicate.root, "assets", "packs", "second", "assets.jsonl"),
    `${JSON.stringify(duplicate.record)}\n`,
  );
  assert.match(
    auditGameReleaseAssets(duplicate.root, {
      trackedPaths: [...duplicate.tracked, "assets/packs/second/assets.jsonl"],
    }).issues.join("\n"),
    /duplicate asset metadata/,
  );

  const stale = fixture(t);
  write(join(stale.root, "assets", "unused.png"), Buffer.from("unused"));
  write(
    join(stale.root, "assets", "packs", "fixture", "assets.jsonl"),
    `${JSON.stringify(stale.record)}\n${JSON.stringify({
      ...stale.record, asset_id: "fixture__unused__cc0-1-0", source_resource: "unused.png",
      sha256: createHash("sha256").update("unused").digest("hex"), bytes: 6,
    })}\n`,
  );
  assert.match(
    auditGameReleaseAssets(stale.root, {
      trackedPaths: [...stale.tracked, "assets/unused.png"],
    }).issues.join("\n"),
    /stale asset metadata/,
  );
});

test("game release asset audit rejects non-regular inputs and malformed source paths", (t) => {
  const nonRegular = fixture(t);
  rmSync(join(nonRegular.root, ...nonRegular.path.split("/")));
  mkdirSync(join(nonRegular.root, ...nonRegular.path.split("/")));
  assert.match(
    auditGameReleaseAssets(nonRegular.root, { trackedPaths: nonRegular.tracked }).issues.join("\n"),
    /regular non-symlink file/,
  );

  const malformed = fixture(t);
  write(
    join(malformed.root, "assets", "packs", "fixture", "assets.jsonl"),
    `${JSON.stringify({ ...malformed.record, source_resource: "assets/ui/panel.png" })}\n`,
  );
  assert.throws(
    () => auditGameReleaseAssets(malformed.root, { trackedPaths: malformed.tracked }),
    /relative to assets/,
  );
});
