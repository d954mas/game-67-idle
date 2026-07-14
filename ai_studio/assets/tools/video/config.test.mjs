import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";

import { corridorKeyDir, videoGenRoot } from "./_lib.mjs";

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "video-config-"));
  mkdirSync(join(root, "ai_studio"), { recursive: true });
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return root;
}

function writeConfig(root, name, data) {
  writeFileSync(join(root, "ai_studio", name), `${JSON.stringify(data)}\n`, "utf8");
}

function isolateEnv(t) {
  const saved = {
    VIDEO_GEN_ROOT: process.env.VIDEO_GEN_ROOT,
    CORRIDOR_KEY_ROOT: process.env.CORRIDOR_KEY_ROOT,
  };
  delete process.env.VIDEO_GEN_ROOT;
  delete process.env.CORRIDOR_KEY_ROOT;
  t.after(() => {
    for (const [key, value] of Object.entries(saved)) {
      if (value === undefined) delete process.env[key];
      else process.env[key] = value;
    }
  });
}

test("Video tools own Studio config interpretation with env over local over committed values", (t) => {
  isolateEnv(t);
  const root = fixture(t);
  writeConfig(root, "studio.config.json", { videoGenRoot: "tmp/video-main", corridorKeyRoot: "tmp/corridor-main" });
  writeConfig(root, "studio.config.local.json", { videoGenRoot: "tmp/video-local", corridorKeyRoot: "tmp/corridor-local" });

  assert.equal(videoGenRoot(root), resolve(root, "tmp/video-local"));
  assert.equal(corridorKeyDir(root), resolve(root, "tmp/corridor-local"));

  process.env.VIDEO_GEN_ROOT = "C:/env/video";
  process.env.CORRIDOR_KEY_ROOT = "C:/env/corridor";
  assert.equal(videoGenRoot(root), resolve("C:/env/video"));
  assert.equal(corridorKeyDir(root), resolve("C:/env/corridor"));

  const source = readFileSync(new URL("./_lib.mjs", import.meta.url), "utf8");
  assert.match(source, /from ["']\.\.\/\.\.\/\.\.\/config\.mjs["']/);
  assert.doesNotMatch(source, /core_harness\/tool_lib\/studio_config/);
});

test("Video tools reject missing configured roots", (t) => {
  isolateEnv(t);
  const root = fixture(t);
  writeConfig(root, "studio.config.json", { schema: "ai_studio.studio_config.v1" });

  assert.throws(() => videoGenRoot(root), /missing videoGenRoot/);
  assert.throws(() => corridorKeyDir(root), /missing corridorKeyRoot/);
});
