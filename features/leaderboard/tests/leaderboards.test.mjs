import assert from "node:assert/strict";
import test from "node:test";

import {
  consoleChecklist,
  generateHeader,
  playgamaLeaderboards,
  validateManifest,
} from "../lib/leaderboards.mjs";

const valid = () => ({
  schema: "ai_studio.leaderboards.v1",
  boards: [
    {
      id: "planets",
      metric: "planets_devoured",
      sort: "desc",
      scopes: ["all_time", "utc_day"],
      portal_ids: { yandex: "planets", crazygames: "main", playgama: { id: "planets", isMain: true } },
      backends: {
        yandex: "portal",
        crazygames: "portal",
        playgama: "portal",
        poki: "http",
        itch: "http",
        local: "http",
      },
    },
  ],
});

test("a complete manifest validates", () => {
  assert.deepEqual(validateManifest(valid()), []);
});

test("a family of portal without a portal id is refused", () => {
  const manifest = valid();
  delete manifest.boards[0].portal_ids.yandex;
  const errors = validateManifest(manifest);
  assert.equal(errors.length, 1);
  assert.match(errors[0], /portal_ids\.yandex is missing/);
});

test("a publish target left out has no silent default", () => {
  const manifest = valid();
  delete manifest.boards[0].backends.itch;
  assert.match(validateManifest(manifest).join("\n"), /itch has no backend family/);
});

test("an unknown publish target is refused", () => {
  const manifest = valid();
  manifest.boards[0].backends.newgrounds = "portal";
  assert.match(validateManifest(manifest).join("\n"), /unknown publish target "newgrounds"/);
});

test("a portal without a leaderboard API cannot be the portal family", () => {
  const manifest = valid();
  manifest.boards[0].backends.poki = "portal";
  assert.match(validateManifest(manifest).join("\n"), /poki has no leaderboard API/);
});

test("a scope no target can serve is refused at authoring time", () => {
  const manifest = valid();
  for (const target of ["poki", "itch", "local"]) manifest.boards[0].backends[target] = "none";
  assert.match(validateManifest(manifest).join("\n"), /can serve the utc_day scope/);
});

test("the day scope surviving on one target only is normal", () => {
  const manifest = valid();
  manifest.boards[0].backends.poki = "none";
  assert.deepEqual(validateManifest(manifest), []);
});

test("every problem is reported, not just the first", () => {
  const manifest = valid();
  manifest.boards[0].sort = "sideways";
  delete manifest.boards[0].metric;
  assert.equal(validateManifest(manifest).length, 2);
});

test("the generated table carries the portal id of its own target", () => {
  const yandex = generateHeader(valid(), "yandex");
  assert.match(yandex, /\.portal_id = "planets"/);
  assert.match(yandex, /GAME_LEADERBOARD_HAS_PORTAL 1/);
  const local = generateHeader(valid(), "local");
  assert.match(local, /\.portal_id = NULL/);
  assert.match(local, /GAME_LEADERBOARD_HAS_HTTP 1/);
});

test("generation is reproducible", () => {
  assert.equal(generateHeader(valid(), "yandex"), generateHeader(valid(), "yandex"));
});

test("the checklist names the boards a human must create", () => {
  const lines = consoleChecklist(valid()).join("\n");
  assert.match(lines, /yandex: create a leaderboard with technical name "planets"/);
  assert.match(lines, /crazygames: request the leaderboard/);
});

test("the playgama block carries isMain only where the manifest says so", () => {
  assert.deepEqual(playgamaLeaderboards(valid()), [{ id: "planets", isMain: true }]);
  const manifest = valid();
  manifest.boards[0].backends.playgama = "none";
  assert.deepEqual(playgamaLeaderboards(manifest), []);
});
