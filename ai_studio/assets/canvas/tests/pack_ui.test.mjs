// T0332 v2 phase C -- pure pack-mode UI helpers (axes JSON smart-quote normalization +
// line/column parse errors, a sheet-count estimate) live inside site/inspector.js alongside
// T0272's own aspect-ratio-lock math (see aspect_lock.test.mjs's own precedent: inspector.js
// is DOM page code but has no top-level document/window access, only inside function bodies,
// so importing it in plain Node to reach these pure exports is safe). Run:
//   node --test ai_studio/assets/canvas/tests/pack_ui.test.mjs
import { strict as assert } from "node:assert";
import test from "node:test";
import { estimatePackSheetCount, normalizeSmartQuotes, PACK_AXES_SKELETON, parseAxesJson } from "../site/inspector.js";

// ---- normalizeSmartQuotes -------------------------------------------------------

test("normalizeSmartQuotes: curly double quotes (typographic + guillemets) straighten to ASCII", () => {
  assert.equal(normalizeSmartQuotes("“material”"), '"material"');
  assert.equal(normalizeSmartQuotes("«material»"), '"material"');
  assert.equal(normalizeSmartQuotes("„material“"), '"material"');
});

test("normalizeSmartQuotes: curly single quotes straighten to ASCII", () => {
  assert.equal(normalizeSmartQuotes("‘stone’"), "'stone'");
  assert.equal(normalizeSmartQuotes("‚stone‘"), "'stone'");
});

test("normalizeSmartQuotes: plain ASCII text is unchanged; null/undefined -> empty string", () => {
  assert.equal(normalizeSmartQuotes('{"a": ["b"]}'), '{"a": ["b"]}');
  assert.equal(normalizeSmartQuotes(null), "");
  assert.equal(normalizeSmartQuotes(undefined), "");
});

// ---- parseAxesJson ---------------------------------------------------------------

test("parseAxesJson: a well-formed axes object round-trips", () => {
  const parsed = parseAxesJson('{"material": ["stone", "wood"], "grade": ["rusty", "plain"]}');
  assert.deepEqual(parsed, { material: ["stone", "wood"], grade: ["rusty", "plain"] });
});

test("parseAxesJson: smart quotes are normalized BEFORE parsing (a curly-quoted axes blob still parses)", () => {
  const parsed = parseAxesJson("{“material”: [“stone”, “wood”]}");
  assert.deepEqual(parsed, { material: ["stone", "wood"] });
});

test("parseAxesJson: a JSON syntax error surfaces a line/column pointer whenever V8's own message names one", () => {
  // A missing comma between array elements: V8 already names "(line X column Y)" directly in
  // the SyntaxError message on modern Node — reused verbatim, not a hand-rolled offset walk.
  assert.throws(() => parseAxesJson('{"material": ["stone" "wood"]}'), /Invalid JSON at line \d+, column \d+/);
});

test("parseAxesJson: a JSON syntax error with no locatable position/line (e.g. a trailing comma in an array, on some V8 versions) still surfaces the raw message, prefixed", () => {
  assert.throws(() => parseAxesJson('{\n  "material": ["stone",]\n}'), /Invalid JSON: /);
});

test("parseAxesJson: a syntactically valid but non-object value (array/string/number) is a loud shape error", () => {
  assert.throws(() => parseAxesJson("[1, 2, 3]"), /Axes must be a JSON object/);
  assert.throws(() => parseAxesJson('"just a string"'), /Axes must be a JSON object/);
  assert.throws(() => parseAxesJson("42"), /Axes must be a JSON object/);
});

test("PACK_AXES_SKELETON itself is valid, parseable axes JSON (the empty-textarea prefill must not be a broken example)", () => {
  const parsed = parseAxesJson(PACK_AXES_SKELETON);
  assert.ok(parsed && typeof parsed === "object");
  assert.ok(Object.keys(parsed).length > 0);
});

// ---- estimatePackSheetCount ------------------------------------------------------

test("estimatePackSheetCount: product of every axis EXCEPT vary (mirrors expand_jobs.py's own combos)", () => {
  const pack = { axes: { grade: ["rusty", "plain", "gilded", "mythic"], material: ["stone", "wood"] }, vary: "grade" };
  assert.equal(estimatePackSheetCount(pack), 2, "only 'material' (2 values) is a big axis; 'grade' is vary, excluded");
});

test("estimatePackSheetCount: no axes at all -> 1 sheet (itertools.product() of zero lists -> one empty combo)", () => {
  assert.equal(estimatePackSheetCount({ axes: {}, vary: "" }), 1);
});

test("estimatePackSheetCount: vary not matching any axis key -> every axis counts as 'big' (no exclusion)", () => {
  assert.equal(estimatePackSheetCount({ axes: { a: ["1", "2"], b: ["1", "2", "3"] }, vary: "nope" }), 6);
});

test("estimatePackSheetCount: null/undefined/non-object pack -> 0", () => {
  assert.equal(estimatePackSheetCount(null), 0);
  assert.equal(estimatePackSheetCount(undefined), 0);
  assert.equal(estimatePackSheetCount("not a pack"), 0);
});
