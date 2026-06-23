import { test } from "node:test";
import assert from "node:assert/strict";
import { detectOrigin, kindForExt, libraryKind, renderHtml, safeJson, escHtml } from "./build_review.mjs";

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
