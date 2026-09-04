import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { join } from "node:path";
import test from "node:test";
import assert from "node:assert/strict";

// The token sheet is read by the art generator; ui_tokens.c is read by the
// runtime. They are the same design decision expressed twice, and a repaint that
// lands in only one of them ships art that does not match its own styles. This
// is the seam that catches it.

const FEATURE = fileURLToPath(new URL("../", import.meta.url));
const sheet = JSON.parse(readFileSync(join(FEATURE, "tokens", "studio_default.json"), "utf8"));
const source = readFileSync(join(FEATURE, "src", "ui_tokens.c"), "utf8");

function field(name) {
  const match = new RegExp(`\\.${name}\\s*=\\s*([^,]+),`).exec(source);
  assert.ok(match, `ui_tokens.c has no .${name}`);
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

test("every colour token is packed into ui_tokens.c unchanged", () => {
  for (const [name, hex] of Object.entries(sheet.colors)) {
    assert.equal(field(name), packed(hex), `colour '${name}' disagrees`);
  }
});

test("the type ramp and geometry reach ui_tokens.c unchanged", () => {
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
    short_edge_max: sheet.canvas.short_edge_max,
  };
  for (const [name, value] of Object.entries(scalars)) {
    assert.equal(Number.parseFloat(field(name)), value, `token '${name}' disagrees`);
  }
});

test("the runtime slice9 scale is the reciprocal of the art export scale", () => {
  assert.equal(Number.parseFloat(field("slice9_scale")), 1 / sheet.art.export_scale);
});

test("every slice9 border contains its corner radius", () => {
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
  // The button's bottom border must also clear the lift ledge, or the press
  // shadow gets stretched instead of held.
  assert.ok(
    sheet.art.slice9.button[3] >= radius.button + sheet.geometry.lift,
    "button bottom border does not contain radius plus lift",
  );
});
