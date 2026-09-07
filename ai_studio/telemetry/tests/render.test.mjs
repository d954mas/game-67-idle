import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

import { renderReport, verdict } from "../lib/render.mjs";

const fixture = JSON.parse(readFileSync(fileURLToPath(new URL("./fixtures/report_sample.json", import.meta.url)), "utf8"));

function lines(report, scope) {
  return renderReport(report, scope).split("\n");
}

test("the header names the scope that was asked for", () => {
  const rendered = lines(fixture, { game: "example-game", build: "v11", platform: "poki" });
  assert.equal(rendered[0], "example-game  build=v11  platform=poki");
  assert.match(rendered[1], /sessions 1000 {2}players 820 {2}avg play 214\.5s {2}over 180s 31\.2%/);
  assert.match(rendered[2], /0-60 300 {2}60-180 388 {2}180-600 250 {2}600\+ 62/);
});

test("an omitted build or platform reads as all", () => {
  assert.equal(lines(fixture, { game: "example-game" })[0], "example-game  build=all  platform=all");
});

test("drop-off is the step to the next level and stops at the last one", () => {
  const rows = lines(fixture, { game: "example-game" }).filter((line) => /^\s+\d/.test(line));
  assert.equal(rows.length, 3);
  assert.match(rows[0], /20\.0%$/);
  assert.match(rows[1], /25\.0%$/);
  assert.match(rows[2], /-$/);
  assert.match(rows[2], /\b600\b.*\b300\b.*\b240\b.*78\.9/);
});

test("the verdict reads both halves of the bar", () => {
  assert.match(verdict(fixture), /^PASS:/);
  assert.match(verdict({ ...fixture, play_avg: 150 }), /^BELOW BAR:/);
  assert.match(verdict({ ...fixture, play_over_180_share: 0.2 }), /^BELOW BAR:/);
  assert.match(verdict({ ...fixture, play_avg: 181, play_over_180_share: 0.25 }), /^PASS:/);
});

test("a game with no data still renders", () => {
  const empty = { sessions: 0, players: 0, play_avg: 0, play_over_180_share: 0, play_buckets: {}, levels: [] };
  const rendered = renderReport(empty, { game: "example-game" });
  assert.match(rendered, /sessions 0/);
  assert.match(rendered, /BELOW BAR/);
});
