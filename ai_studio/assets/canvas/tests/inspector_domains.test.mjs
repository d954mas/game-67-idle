import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";

const canvasRoot = path.resolve(import.meta.dirname, "..");
const runtimePath = path.join(canvasRoot, "site", "inspector", "runtime.js");
const domainPath = path.join(canvasRoot, "site", "inspector", "recipe_pack.js");
const cardDomainPath = path.join(canvasRoot, "site", "inspector", "generation_cards.js");

test("inspector recipe and pack rendering is owned by a real domain module", () => {
  const runtime = fs.readFileSync(runtimePath, "utf8");
  const domain = fs.readFileSync(domainPath, "utf8");

  assert.match(domain, /export function renderRecipe\(/);
  assert.match(domain, /function renderPackAxesField\(/);
  assert.match(domain, /generateFromRecipeAction/);
  assert.doesNotMatch(runtime, /function renderPackAxesField\(/);
  assert.doesNotMatch(runtime, /function renderRecipe\(/);
  const cardDomain = fs.readFileSync(cardDomainPath, "utf8");
  assert.match(cardDomain, /export function renderStyle\(/);
  assert.match(cardDomain, /export function renderAnim\(/);
  assert.match(cardDomain, /generateAnimFromCardAction/);
  assert.doesNotMatch(runtime, /function renderStyle\(/);
  assert.doesNotMatch(runtime, /function renderAnim\(/);
});

test("inspector runtime is a coordinator rather than the full rendering monolith", () => {
  const lines = fs.readFileSync(runtimePath, "utf8").split(/\r?\n/).length;
  assert.ok(lines < 3000, `expected inspector runtime below 3000 lines, got ${lines}`);
});
