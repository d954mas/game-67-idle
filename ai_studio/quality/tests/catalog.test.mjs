import test from "node:test";
import assert from "node:assert/strict";
import { loadQualityCatalog } from "../catalog.mjs";

test("quality catalog exposes rule groups and numbered checks", () => {
  const catalog = loadQualityCatalog(process.cwd());

  assert.equal(catalog.groups.length, 6);
  assert.equal(catalog.totalChecks, 9);

  const clarity = catalog.groups.find((group) => group.slug === "player_clarity");
  assert.equal(clarity.title, "Player Clarity Rules");
  assert.equal(clarity.prefix, "QCLR");
  assert.equal(clarity.checks.length, 3);

  const responsive = clarity.checks.find((check) => check.id === "QCLR_002");
  assert.equal(responsive.name, "Responsive Viewports");
  assert.equal(responsive.number, "002");
  assert.equal(responsive.path, "ai_studio/quality/rules/player_clarity/checks/QCLR_002_responsive_viewports.md");
  assert.match(responsive.whatItChecks, /viewport ratios keep/i);
  assert.match(responsive.useWhen, /crop, hide, overlap, or misplace/i);
  assert.match(responsive.evidence, /Screenshot or runtime UI bounds/i);
  assert.equal(responsive.copyText, "QCLR_002");

  const technical = catalog.groups.find((group) => group.slug === "technical");
  assert.equal(technical.checks[0].id, "QTECH_001");
});
