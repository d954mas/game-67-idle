import { test } from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { detectOrigin, kindForExt, libraryKind, main, parseArgs, renderHtml, safeJson, escHtml, resolveScanRoot } from "../build_review.mjs";

test("kindForExt maps primary extensions, ignores sidecars", () => {
  assert.equal(kindForExt(".obj"), "model");
  assert.equal(kindForExt(".png"), "image");
  assert.equal(kindForExt(".ttf"), "font");
  assert.equal(kindForExt(".wav"), "audio");
  assert.equal(kindForExt(".mtl"), null);
  assert.equal(kindForExt(".txt"), null);
});

test("libraryKind refines rasters to ui vs texture by path", () => {
  assert.equal(libraryKind(".obj", "assets/source/x.obj"), "model");
  assert.equal(libraryKind(".ttf", "assets/fonts/x.ttf"), "font");
  assert.equal(libraryKind(".png", "assets/ui/button.png"), "ui");
  assert.equal(libraryKind(".png", "assets/icons/coin.png"), "ui");
  assert.equal(libraryKind(".png", "assets/hud/bar.png"), "ui");
  assert.equal(libraryKind(".png", "assets/textures/wood.png"), "texture");
  assert.equal(libraryKind(".mtl", "a.mtl"), null);
});

test("detectOrigin: ai by gen path; sourced by vendor/source; license alone is NOT sourced", () => {
  assert.equal(detectOrigin("assets/generated/sprite.png", []), "ai");
  assert.equal(detectOrigin("assets/imagegen/x.png", []), "ai");
  assert.equal(detectOrigin("assets/source/models/kenney/desk.obj", []), "sourced");
  assert.equal(detectOrigin("assets/models/quaternius/robot.glb", []), "sourced");
  // a bare license file must not flip the user's own art to "sourced"
  assert.equal(detectOrigin("assets/art/MyFont.ttf", ["assets/art/LICENSE.txt"]), "unknown");
  assert.equal(detectOrigin("assets/art/hero.png", []), "unknown");
});

test("renderHtml escapes untrusted names/paths/title in all three sinks", () => {
  const cards = [{
    id: "x</script>", name: "</script><img src=x onerror=alert(1)>", relpath: "a/b",
    kind: "texture", origin: "sourced", source: "k", license: "CC0", tags: ["<svg>"],
    preview: "", fontUrl: "", audioUrl: "",
  }];
  const html = renderHtml({ mode: "review", title: "t<script>evil", cards, meta: "m<b>" });
  assert.ok(!html.includes("</script><img"), "no raw </script> breakout in embedded DATA");
  assert.ok(!/<img src=x onerror=/.test(html), "no unescaped <img> in client markup template");
  assert.match(html, /t&lt;script&gt;evil/, "title escaped in <title>/<h1>");
  assert.ok(safeJson({ a: "</script>" }).includes("\\u003c"), "safeJson neutralizes <");
});

test("escHtml escapes the five HTML entities and handles nullish", () => {
  assert.equal(escHtml(`<>&"'`), "&lt;&gt;&amp;&quot;&#39;");
  assert.equal(escHtml(null), "");
  assert.equal(escHtml(undefined), "");
});

test("parseArgs covers gallery builder CLI contract", () => {
  assert.equal(parseArgs([]).mode, "library");

  const args = parseArgs([
    "--mode", "scan",
    "--game", "demo",
    "--base", "clean-seed",
    "--library", "C:/assets",
    "--repo", "C:/repo",
    "--out", "tmp/out",
    "--path", "demo/assets",
    "--ref",
  ]);

  assert.equal(args.mode, "scan");
  assert.equal(args.game, "demo");
  assert.equal(args.base, "clean-seed");
  assert.equal(args.library, "C:/assets");
  assert.equal(args.repo, "C:/repo");
  assert.equal(args.out, "tmp/out");
  assert.equal(args.path, "demo/assets");
  assert.equal(args.ref, true);

  assert.throws(() => parseArgs(["--mode"]), /missing value/);
  assert.throws(() => parseArgs(["--bad", "x"]), /unknown option/);
  assert.throws(() => parseArgs(["--mode", "broken"]), /unknown mode/);
});

test("resolveScanRoot keeps scan mode inside the repository", () => {
  const repo = resolve("C:/repo");

  assert.equal(resolveScanRoot(repo), resolve(repo, "assets"));
  assert.equal(resolveScanRoot(repo, "templates/template/assets"), resolve(repo, "templates/template/assets"));
  assert.equal(resolveScanRoot(repo, resolve(repo, "games/demo/assets")), resolve(repo, "games/demo/assets"));
  assert.throws(() => resolveScanRoot(repo, "../outside/assets"), /inside --repo/);
  assert.throws(() => resolveScanRoot(repo, "C:/outside/assets"), /inside --repo/);
});

test("gallery output copies the shared HDR from the owned previews store", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-gallery-hdr-"));
  const out = join(root, "out");
  mkdirSync(join(root, "assets"), { recursive: true });
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const originalLog = console.log;
  console.log = () => {};
  try {
    await main(["--mode", "scan", "--repo", root, "--out", out]);
  } finally {
    console.log = originalLog;
  }
  assert.equal(existsSync(join(out, "studio_env.hdr")), true);
});
