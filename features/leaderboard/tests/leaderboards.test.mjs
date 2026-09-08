import assert from "node:assert/strict";
import test from "node:test";

import {
  consoleChecklist,
  generateHeader,
  mergeLeaderboardsBlock,
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

function twoBoards() {
  const manifest = valid();
  manifest.boards[0].backends.crazygames = "none";
  const second = structuredClone(manifest.boards[0]);
  second.id = "laps";
  second.portal_ids.yandex = "lap_times";
  second.portal_ids.playgama = { id: "lap_times" };
  manifest.boards.push(second);
  return manifest;
}

test("multiple boards may share a family with distinct portal ids", () => {
  const manifest = twoBoards();
  for (const id of ["distance", "wins"]) {
    const board = structuredClone(manifest.boards[1]);
    board.id = id;
    board.portal_ids.yandex = id;
    board.portal_ids.playgama = { id };
    manifest.boards.push(board);
  }
  assert.deepEqual(validateManifest(manifest), []);
});

test("one target cannot mix backend families, including disabled boards", () => {
  for (const [target, family] of [["local", "none"], ["yandex", "http"]]) {
    const manifest = twoBoards();
    manifest.boards[1].backends[target] = family;
    assert.match(validateManifest(manifest).join("\n"), new RegExp(`${target}:.*one backend family`));
  }
});

test("portal completion ids must identify one board per target", () => {
  for (const target of ["yandex", "playgama"]) {
    const manifest = twoBoards();
    manifest.boards[1].portal_ids[target] = { id: "planets" };
    assert.match(validateManifest(manifest).join("\n"), new RegExp(`${target}:.*duplicate portal id`));
  }
});

test("CrazyGames has only one effective portal board regardless of configured names", () => {
  const manifest = twoBoards();
  for (const board of manifest.boards) board.backends.crazygames = "portal";
  manifest.boards[1].portal_ids.crazygames = "laps";
  assert.match(validateManifest(manifest).join("\n"), /crazygames:.*one portal board/);
});

test("malformed portal ids fail validation before C generation", () => {
  for (const portalId of ["", 12, [], {}, { id: 12 }, { id: "" }, "bad\nname", "bad\0name", { id: "ok", isMain: "yes" }]) {
    const manifest = valid();
    manifest.boards[0].portal_ids.yandex = portalId;
    assert.match(validateManifest(manifest).join("\n"), /portal_ids\.yandex.*must/);
  }
});

test("quoted portal ids retain their bytes in generated C string literals", () => {
  const manifest = valid();
  manifest.boards[0].portal_ids.yandex = 'lap"time\\best';
  assert.deepEqual(validateManifest(manifest), []);
  assert.ok(generateHeader(manifest, "yandex").includes('.portal_id = "lap\\"time\\\\best",'));
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

const handAuthored = `{
  "readme": [
    "a hand written note"
  ],

  "platforms": {},

  "advertisement": {
    "interstitial": {
      "placements": [
        { "id": "pause_resume" }
      ]
    }
  }
}
`;

test("only the leaderboards block is written into a hand-authored config", () => {
  const merged = mergeLeaderboardsBlock(handAuthored, playgamaLeaderboards(valid()));
  assert.match(merged, /"leaderboards": \[/);
  assert.deepEqual(JSON.parse(merged).leaderboards, [{ id: "planets", isMain: true }]);
  /* the hand-authored parts survive byte for byte, blank lines and all; only
     the brace that now precedes a new key gains its comma */
  const head = handAuthored.slice(0, handAuthored.indexOf('"advertisement"'));
  assert.ok(merged.startsWith(head), "the readme and its blank lines are untouched");
  assert.ok(merged.includes('        { "id": "pause_resume" }\n'), "placements keep their own formatting");
});

test("rewriting the block twice changes nothing the second time", () => {
  const once = mergeLeaderboardsBlock(handAuthored, playgamaLeaderboards(valid()));
  assert.equal(mergeLeaderboardsBlock(once, playgamaLeaderboards(valid())), once);
});

test("a game with no playgama board leaves the config without the block", () => {
  const manifest = valid();
  manifest.boards[0].backends.playgama = "none";
  const once = mergeLeaderboardsBlock(handAuthored, playgamaLeaderboards(valid()));
  const removed = mergeLeaderboardsBlock(once, playgamaLeaderboards(manifest));
  assert.equal(JSON.parse(removed).leaderboards, undefined);
  assert.equal(removed, handAuthored);
});
