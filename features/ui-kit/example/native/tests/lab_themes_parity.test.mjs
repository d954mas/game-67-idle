import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { join } from "node:path";
import test from "node:test";
import assert from "node:assert/strict";

// A lab theme is one sheet expressed twice: themes/<id>.json draws the art,
// src/lab_theme.c colours the styles. This is the same seam the kit guards for
// its own presets, applied to the copies a consumer makes.

const LAB = fileURLToPath(new URL("../", import.meta.url));
const KIT = join(LAB, "..", "..");
const source = readFileSync(join(LAB, "src", "lab_theme.c"), "utf8");
const studioB = JSON.parse(readFileSync(join(KIT, "tokens", "studio_b.json"), "utf8"));
const themes = ["forest", "ember", "night"].map((id) => ({
  sheet: JSON.parse(readFileSync(join(LAB, "themes", `${id}.json`), "utf8")),
  block: tokenBlock(id.toUpperCase()),
}));

function tokenBlock(symbol) {
  const start = source.indexOf(`static const ui_tokens_t ${symbol} = {`);
  assert.notEqual(start, -1, `lab_theme.c has no ${symbol} theme`);
  const end = source.indexOf("\n};", start);
  return source.slice(start, end + 3);
}

function field(block, name) {
  const match = new RegExp(`\\.${name}\\s*=\\s*([^,\\s]+)`).exec(block);
  assert.ok(match, `no .${name}`);
  return match[1].trim();
}

function packed(hex) {
  const text = hex.replace("#", "");
  const [r, g, b] = [0, 2, 4].map((at) => parseInt(text.slice(at, at + 2), 16));
  const a = text.length >= 8 ? parseInt(text.slice(6, 8), 16) : 0xff;
  const value = (((a << 24) | (b << 16) | (g << 8) | r) >>> 0).toString(16).toUpperCase();
  return `0x${value.padStart(8, "0")}U`;
}

const geometryMacro = (() => {
  const start = source.indexOf("#define LAB_THEME_GEOMETRY");
  assert.notEqual(start, -1, "lab_theme.c has no LAB_THEME_GEOMETRY");
  const end = source.indexOf("\n\n", start);
  return source.slice(start, end);
})();

for (const { sheet, block } of themes) {
  test(`${sheet.id}: every colour reaches lab_theme.c unchanged`, () => {
    for (const [name, hex] of Object.entries(sheet.colors)) {
      assert.equal(field(block, name), packed(hex), `colour '${name}' disagrees`);
    }
  });

  test(`${sheet.id}: geometry and type ramp are Studio B's, shared through one macro`, () => {
    assert.match(block, /LAB_THEME_GEOMETRY,/);
    assert.deepEqual(sheet.type, studioB.type);
    assert.deepEqual(sheet.geometry, studioB.geometry);
    assert.deepEqual(sheet.canvas, studioB.canvas);
    assert.deepEqual(sheet.art, studioB.art);
  });
}

test("the geometry macro carries Studio B's numbers", () => {
  const scalars = {
    t_display: studioB.type.display,
    t_title: studioB.type.title,
    t_body: studioB.type.body,
    t_num: studioB.type.num,
    t_badge: studioB.type.badge,
    t_row: studioB.type.row,
    t_row_sub: studioB.type.row_sub,
    rim: studioB.geometry.rim,
    lift: studioB.geometry.lift,
    gap: studioB.geometry.gap,
    pad: studioB.geometry.pad,
    hit: studioB.geometry.hit,
    panel_min_w: studioB.geometry.panel_min_w,
    panel_max_w: studioB.geometry.panel_max_w,
    ref_short: studioB.canvas.ref_short,
  };
  for (const [name, value] of Object.entries(scalars)) {
    assert.equal(Number.parseFloat(field(geometryMacro, name)), value, `token '${name}' disagrees`);
  }
  assert.equal(Number.parseFloat(field(geometryMacro, "slice9_scale")), 1 / studioB.art.export_scale);
});

test("the pack builder's slice9 macros are Studio B's art tokens times its export scale", () => {
  const builder = readFileSync(join(LAB, "src", "build_packs.c"), "utf8");
  const scale = Number.parseInt(/#define UI_KIT_EXPORT_SCALE (\d+)/.exec(builder)[1], 10);
  assert.equal(scale, studioB.art.export_scale);
  const macros = {
    PANEL_BORDER: studioB.art.slice9.panel[0],
    BUTTON_BORDER_X: studioB.art.slice9.button[0],
    BUTTON_BORDER_TOP: studioB.art.slice9.button[2],
    BUTTON_BORDER_BOTTOM: studioB.art.slice9.button[3],
    TILE_BORDER: studioB.art.slice9.tile[0],
    BAR_BORDER: studioB.art.slice9.slider_track[0],
  };
  for (const [name, designPx] of Object.entries(macros)) {
    const match = new RegExp(`#define ${name} \\((\\d+) \\* UI_KIT_EXPORT_SCALE\\)`).exec(builder);
    assert.ok(match, `no ${name} macro`);
    assert.equal(Number.parseInt(match[1], 10), designPx, `${name} disagrees with Studio B's art.slice9`);
  }
});

test("the pack builder and the theme table name the same theme folders", () => {
  const builder = readFileSync(join(LAB, "src", "build_packs.c"), "utf8");
  const packed = /THEMES\[\] = \{([^}]*)\}/.exec(builder)[1].match(/"([a-z]+)"/g).map((s) => s.replaceAll('"', ""));
  const table = [...source.matchAll(/\{"([a-z]+)", "[^"]+", /g)].map((m) => m[1]);
  assert.deepEqual(packed, table);
});
