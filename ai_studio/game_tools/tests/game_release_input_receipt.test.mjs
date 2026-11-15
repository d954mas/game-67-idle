import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { test } from "node:test";

import { readPackInputReceipt, stagePackInputReceipt } from "../lib/game_release_input_receipt.mjs";

function glb(value) {
  const json = Buffer.from(JSON.stringify(value));
  const padding = Buffer.alloc((4 - json.length % 4) % 4, " ");
  const header = Buffer.alloc(20);
  header.write("glTF", 0);
  header.writeUInt32LE(2, 4);
  header.writeUInt32LE(20 + json.length + padding.length, 8);
  header.writeUInt32LE(json.length + padding.length, 12);
  header.write("JSON", 16);
  return Buffer.concat([header, json, padding]);
}

test("pack input receipt binds discovered source paths to all staged packs", (t) => {
  const root = mkdtempSync(join(tmpdir(), "pack-input-receipt-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const gameDir = join(root, "game");
  const studioRoot = join(root, "studio");
  const artifactDir = join(root, "web", "bin");
  const trace = join(root, "native", "pack-inputs.txt");
  const fingerprint = "a".repeat(64);
  function put(path, bytes) {
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, bytes);
  }
  const model = join(gameDir, "assets", "models", "lin.glb");
  const music = join(artifactDir, "assets", "music.ntpack");
  const embedded = glb({ asset: { version: "2.0" }, images: [{ uri: "data:image/png;base64,AA==" }] });
  put(model, embedded);
  put(join(artifactDir, "assets", "game.ntpack"), "game pack");
  put(music, "music pack");
  put(trace, "assets/models/lin.glb\r\nassets/models/lin.glb\n");
  const font = "ai_studio/assets/canvas/site/fonts/Inter/Inter-SemiBold.ttf";
  const notice = "ai_studio/assets/canvas/site/fonts/Inter/OFL.txt";
  put(join(studioRoot, font), "inter bits");
  put(join(studioRoot, notice), "font license");
  put(join(gameDir, "cmake", "GameAssets.cmake"), `set(GAME_UI_FONT "\${GAME_REPO_ROOT}/${font}")`);
  put(join(gameDir, "assets", "release_inputs.json"), JSON.stringify({
    schema: "ai_studio.game_release_assets.v2", standalone_inputs: ["web/loading-title.webp"],
    studio_fonts: [{ path: font, license: "OFL-1.1", license_file: notice, sha256: createHash("sha256").update("inter bits").digest("hex") }],
  }));
  put(join(gameDir, "web", "loading-title.webp"), "cover");
  const stagedCover = join(artifactDir, "loading-title.webp");
  put(stagedCover, "cover");
  const step = { gameDir, studioRoot, artifactDir, trace, fingerprint };
  stagePackInputReceipt(step);
  assert.deepEqual(readPackInputReceipt(step), ["assets/models/lin.glb"]);
  put(stagedCover, "stale cover");
  assert.throws(() => readPackInputReceipt(step), /standalone asset differs/);
  put(stagedCover, "cover");
  put(join(studioRoot, font), "changed font");
  assert.throws(() => readPackInputReceipt(step), /Studio UI font source changed/);
  put(join(studioRoot, font), "inter bits");
  assert.throws(() => readPackInputReceipt({ ...step, fingerprint: "b".repeat(64) }), /fingerprint/);
  put(model, "replaced model");
  assert.throws(() => readPackInputReceipt(step), /source.*changed/);
  put(model, embedded);
  put(music, "replaced pack");
  assert.throws(() => readPackInputReceipt(step), /pack.*changed/);
  put(model, glb({ asset: { version: "2.0" }, images: [{ uri: "external.png" }] }));
  assert.throws(() => stagePackInputReceipt(step), /external asset/);
  put(trace, "assets/../outside.glb\n");
  assert.throws(() => stagePackInputReceipt(step), /unsafe/);
});
