import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import {
  auditGameReleaseAssets,
  builderAssetPaths,
  packedAssetPaths,
  standaloneAssetPaths,
  webStagedAssetPaths,
} from "../game_release.mjs";

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

function standaloneFixture(t) {
  const item = fixture(t);
  const standalonePath = "assets/ui/loading_slime.png";
  const standaloneBytes = Buffer.from("loading-slime-bytes");
  write(join(item.root, "assets", "release_inputs.json"), JSON.stringify({
    schema: "ai_studio.game_release_assets.v1",
    inputs: [item.path],
    standalone_inputs: [standalonePath],
  }) + "\n");
  write(join(item.root, ...standalonePath.split("/")), standaloneBytes);
  const dollar = "$";
  write(join(item.root, "cmake", "GamePlatform.cmake"),
    "set(GAME_LOADING_SLIME_SOURCE \"" + dollar + "{CMAKE_CURRENT_SOURCE_DIR}/assets/ui/loading_slime.png\")\n"
    + "set(GAME_LOADING_SLIME_OUTPUT \"" + dollar + "{GAME_OUTPUT_DIR}/assets/ui/loading_slime.png\")\n"
    + "configure_file(\"" + dollar + "{GAME_LOADING_SLIME_SOURCE}\" \"" + dollar + "{GAME_LOADING_SLIME_OUTPUT}\" COPYONLY)\n");
  write(join(item.root, "assets", "packs", "loading", "assets.jsonl"), JSON.stringify({
    ...item.record,
    asset_id: "fixture__loading_slime__cc0-1-0",
    source_resource: "ui/loading_slime.png",
    sha256: createHash("sha256").update(standaloneBytes).digest("hex"),
    bytes: standaloneBytes.length,
  }) + "\n");
  return {
    ...item,
    standalonePath,
    tracked: [
      ...item.tracked,
      standalonePath,
      "assets/packs/loading/assets.jsonl",
      "cmake/GamePlatform.cmake",
    ],
  };
}

// A restricted input is gitignored by design, so on a fresh clone (CI) the
// file cannot exist; the committed reconstruction record is its contract.
function restrictedFixture(t, overrides = {}, { presentFile = false } = {}) {
  const root = mkdtempSync(join(tmpdir(), "game-release-restricted-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const bytes = Buffer.from("restricted-bytes");
  const path = "assets/restricted/audio/music/loop.mp3";
  write(join(root, "assets", "release_inputs.json"), `${JSON.stringify({
    schema: "ai_studio.game_release_assets.v1",
    inputs: [path],
  })}\n`);
  if (presentFile) write(join(root, ...path.split("/")), bytes);
  write(join(root, "src", "build_packs.c"), `nt_atlas_add(atlas, "${path}", &options);\n`);
  const record = {
    asset_id: "fixture__loop__restricted",
    source_resource: "restricted/audio/music/loop.mp3",
    origin: "sourced",
    license: "Vendor-Commercial-Game-Use",
    license_url: "https://example.test/license",
    license_kind: "vendor",
    publish: "false",
    redistribution_allowed: "true",
    commercial_use: "true",
    modification_allowed: "true",
    provenance: "fixture restricted source",
    source_file_sha256: createHash("sha256").update("source").digest("hex"),
    transform: "ffmpeg -i <source_file> loop.mp3",
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

test("game release asset audit accepts standalone inputs only when CMake stages them", (t) => {
  const item = standaloneFixture(t);
  assert.deepEqual(standaloneAssetPaths(item.root), [item.standalonePath]);
  assert.deepEqual(webStagedAssetPaths(item.root), [item.standalonePath]);
  assert.deepEqual(auditGameReleaseAssets(item.root, { trackedPaths: item.tracked }), {
    ok: true, packed: 1, issues: [],
  });

  write(join(item.root, "cmake", "GamePlatform.cmake"), "# no staged binary assets\n");
  assert.match(
    auditGameReleaseAssets(item.root, { trackedPaths: item.tracked }).issues.join("\n"),
    /standalone input is not staged by the local web build/,
  );
});

test("standalone release inputs reject unsafe duplicate and packed paths", (t) => {
  const item = fixture(t);
  const contractPath = join(item.root, "assets", "release_inputs.json");
  write(contractPath, JSON.stringify({
    schema: "ai_studio.game_release_assets.v1",
    inputs: [item.path],
    standalone_inputs: ["assets/../escape.png"],
  }));
  assert.throws(() => standaloneAssetPaths(item.root), /unique safe binary asset paths/);

  write(contractPath, JSON.stringify({
    schema: "ai_studio.game_release_assets.v1",
    inputs: [item.path],
    standalone_inputs: ["assets/ui/other.png", "assets/ui/other.png"],
  }));
  assert.throws(() => standaloneAssetPaths(item.root), /unique safe binary asset paths/);

  write(contractPath, JSON.stringify({
    schema: "ai_studio.game_release_assets.v1",
    inputs: [item.path],
    standalone_inputs: [item.path],
  }));
  assert.throws(() => standaloneAssetPaths(item.root), /must not overlap packed inputs/);
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

test("a restricted input missing from a fresh clone audits by its reconstruction record", (t) => {
  const clean = restrictedFixture(t);
  assert.deepEqual(auditGameReleaseAssets(clean.root, { trackedPaths: clean.tracked }), {
    ok: true, packed: 1, issues: [],
  });
  const bare = restrictedFixture(t, { transform: "", source_file_sha256: "" });
  assert.match(
    auditGameReleaseAssets(bare.root, { trackedPaths: bare.tracked }).issues.join("\n"),
    /reconstruction record/,
  );
  // Where the file does exist, the content checks still bind it to the record.
  const stale = restrictedFixture(t, { sha256: "0".repeat(64) }, { presentFile: true });
  assert.match(
    auditGameReleaseAssets(stale.root, { trackedPaths: stale.tracked }).issues.join("\n"),
    /sha256 mismatch/,
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
