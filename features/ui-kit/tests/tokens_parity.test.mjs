import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { join } from "node:path";
import test from "node:test";
import assert from "node:assert/strict";

// The sheets drive generated art while ui_tokens.c drives runtime styles. Each
// preset therefore has one decision expressed twice; this test catches a
// repaint that reaches only one side of that seam.

const FEATURE = fileURLToPath(new URL("../", import.meta.url));
const tokenSource = readFileSync(join(FEATURE, "src", "ui_tokens.c"), "utf8");
const themeSource = readFileSync(join(FEATURE, "src", "ui_theme.c"), "utf8");
const presets = [
  { sheet: "studio_default.json", symbol: "STUDIO_DEFAULT" },
  { sheet: "studio_b.json", symbol: "STUDIO_B" },
].map(({ sheet, symbol }) => ({
  sheet: JSON.parse(readFileSync(join(FEATURE, "tokens", sheet), "utf8")),
  source: tokenBlock(symbol),
}));

function tokenBlock(symbol) {
  const start = tokenSource.indexOf(`static const ui_tokens_t ${symbol} = {`);
  assert.notEqual(start, -1, `ui_tokens.c has no ${symbol} preset`);
  const end = tokenSource.indexOf("\n};", start);
  assert.notEqual(end, -1, `${symbol} has no closing initializer`);
  return tokenSource.slice(start, end + 3);
}

function field(block, name) {
  const match = new RegExp(`\\.${name}\\s*=\\s*([^,]+),`).exec(block);
  assert.ok(match, `preset has no .${name}`);
  return match[1].trim();
}

function packed(hex) {
  // The sheet writes #RRGGBB or #RRGGBBAA; the runtime packs 0xAABBGGRR.
  const text = hex.replace("#", "");
  const [r, g, b] = [0, 2, 4].map((at) => parseInt(text.slice(at, at + 2), 16));
  const a = text.length >= 8 ? parseInt(text.slice(6, 8), 16) : 0xff;
  const value = (((a << 24) | (b << 16) | (g << 8) | r) >>> 0).toString(16).toUpperCase();
  return `0x${value.padStart(8, "0")}U`;
}

for (const { sheet, source: preset } of presets) {
  test(`${sheet.id}: every colour token is packed into ui_tokens.c unchanged`, () => {
    for (const [name, hex] of Object.entries(sheet.colors)) {
      assert.equal(field(preset, name), packed(hex), `colour '${name}' disagrees`);
    }
  });

  test(`${sheet.id}: every colour ui_tokens.c packs comes from the sheet`, () => {
    // The other direction: a colour added to the struct and its preset but not
    // to the sheet is a repaint that only half happens, in the art or the styles.
    for (const [, name] of preset.matchAll(/\.(\w+)\s*=\s*0x[0-9A-Fa-f]{8}U/g)) {
      assert.ok(name in sheet.colors, `sheet has no colour '${name}'`);
    }
  });

  test(`${sheet.id}: type ramp and geometry reach ui_tokens.c unchanged`, () => {
    const scalars = {
      t_display: sheet.type.display,
      t_title: sheet.type.title,
      t_body: sheet.type.body,
      t_num: sheet.type.num,
      t_badge: sheet.type.badge,
      t_row: sheet.type.row,
      t_row_sub: sheet.type.row_sub,
      rim: sheet.geometry.rim,
      lift: sheet.geometry.lift,
      gap: sheet.geometry.gap,
      pad: sheet.geometry.pad,
      hit: sheet.geometry.hit,
      panel_min_w: sheet.geometry.panel_min_w,
      panel_max_w: sheet.geometry.panel_max_w,
      ref_short: sheet.canvas.ref_short,
    };
    for (const [name, value] of Object.entries(scalars)) {
      assert.equal(Number.parseFloat(field(preset, name)), value, `token '${name}' disagrees`);
    }
  });

  test(`${sheet.id}: slice9 scale and borders are compatible with its art`, () => {
    assert.equal(Number.parseFloat(field(preset, "slice9_scale")), 1 / sheet.art.export_scale);
    const radius = sheet.art.radius;
    const pairs = [
      ["panel", radius.panel],
      ["button", radius.button],
      ["tile", radius.tile],
      ["slider_track", radius.bar],
      ["slider_fill", radius.bar],
    ];
    for (const [name, r] of pairs) {
      const [left, right, top, bottom] = sheet.art.slice9[name];
      for (const [side, border] of [["left", left], ["right", right], ["top", top], ["bottom", bottom]]) {
        assert.ok(border >= r, `${name} ${side} border ${border} cuts through radius ${r}`);
      }
    }
    assert.ok(
      sheet.art.slice9.button[3] >= radius.button + sheet.geometry.lift,
      "button bottom border does not contain radius plus lift",
    );
  });
}

test("legacy token initializers fall back to on_panel for action labels", () => {
  const actionText = (tokens) => tokens.on_action || tokens.on_panel;
  const legacy = { on_panel: 0xFF382517, on_action: 0 };
  assert.equal(actionText(legacy), legacy.on_panel);
  for (const { sheet } of presets) {
    assert.equal(actionText({ on_panel: packed(sheet.colors.on_panel), on_action: packed(sheet.colors.on_action) }), packed(sheet.colors.on_action));
  }
  assert.match(themeSource, /t->on_action != 0U \? t->on_action : t->on_panel/);
  assert.match(themeSource, /button_label_action = label_style\(t->t_body, action_text,/);
});
