import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { parseArgs, pickHero } from "../record_gallery.mjs";

function gallery(payload) {
  const dir = mkdtempSync(join(tmpdir(), "record-gallery-"));
  writeFileSync(
    join(dir, "index.html"),
    `<!doctype html><script>window.__VIEWER__=${JSON.stringify(payload)};</script>`,
    "utf8",
  );
  return dir;
}

test("parseArgs rejects unknown and missing-value options", () => {
  assert.throws(() => parseArgs(["--url"]), /missing value/);
  assert.throws(() => parseArgs(["--bad", "x"]), /unknown option/);

  const args = parseArgs([
    "--url", "http://127.0.0.1:8910",
    "--out", "tmp/out.mp4",
    "--port", "9333",
    "--gallery", "tmp/gallery",
    "--chrome", "C:/Chrome/chrome.exe",
    "--pack", "food-kit",
    "--asset", "chair",
  ]);

  assert.equal(args.url, "http://127.0.0.1:8910");
  assert.equal(args.out, "tmp/out.mp4");
  assert.equal(args.port, 9333);
  assert.equal(args.gallery, "tmp/gallery");
  assert.equal(args.chrome, "C:/Chrome/chrome.exe");
  assert.equal(args.pack, "food-kit");
  assert.equal(args.asset, "chair");
});

test("pickHero uses requested pack and requested asset when available", () => {
  const dir = gallery({
    packs: [{ pack: "food-kit", title: "Food Kit", count: 10, coverImg: "cover.png" }],
    assets: [{ id: "chair", pack: "food-kit", model: "chair.glb" }],
  });
  try {
    const { pack, asset } = pickHero(dir, "food-kit", "chair");
    assert.equal(pack.pack, "food-kit");
    assert.equal(asset.id, "chair");
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("pickHero falls back to a strong pack and model asset", () => {
  const dir = gallery({
    packs: [
      { pack: "small", title: "Small", count: 2, coverImg: "small.png" },
      { pack: "space-kit", title: "Space Habitat", count: 12, coverImg: "space.png" },
    ],
    assets: [
      { id: "no-model", pack: "space-kit" },
      { id: "station", pack: "space-kit", model: "station.glb" },
    ],
  });
  try {
    const { pack, asset } = pickHero(dir, "", "");
    assert.equal(pack.pack, "space-kit");
    assert.equal(asset.id, "station");
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("pickHero reports missing payload or empty gallery clearly", () => {
  const missing = mkdtempSync(join(tmpdir(), "record-gallery-"));
  const empty = gallery({ packs: [], assets: [] });
  try {
    writeFileSync(join(missing, "index.html"), "<!doctype html>", "utf8");
    assert.throws(() => pickHero(missing, "", ""), /could not find __VIEWER__/);
    assert.throws(() => pickHero(empty, "", ""), /gallery has no packs/);
  } finally {
    rmSync(missing, { recursive: true, force: true });
    rmSync(empty, { recursive: true, force: true });
  }
});
