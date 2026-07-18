import assert from "node:assert/strict";
import test from "node:test";

import {
  filterHomeProjects, homeCreationStoreId, ownerGameOptions, projectLifecycleLabel,
} from "../site/home_filters.mjs";
import { ALL_STORES_ID } from "../site/store_scope.js";

const PROJECTS = [
  { id: "active-a", storeId: "studio", ownership: { kind: "game", gameId: "game-a" } },
  { id: "archived-a", storeId: "studio", archived: true, ownership: { kind: "game", gameId: "game-a" } },
  { id: "private-b", storeId: "game:game-b" },
  { id: "unowned", storeId: "studio" },
];

test("Home owner filter keeps one active+archived project stream", () => {
  assert.deepEqual(ownerGameOptions(PROJECTS, ALL_STORES_ID), ["game-a", "game-b"]);
  assert.deepEqual(ownerGameOptions(PROJECTS, "studio"), ["game-a"]);
  assert.deepEqual(
    filterHomeProjects(PROJECTS, { storeId: "studio", ownerGame: "game-a" }).map((project) => project.id),
    ["active-a", "archived-a"],
  );
});

test("Home lifecycle labels expose archived state as text", () => {
  assert.equal(projectLifecycleLabel(PROJECTS[0]), "Active");
  assert.equal(projectLifecycleLabel(PROJECTS[1]), "Archived");
});

test("Home creation stays in a private owner's unique store and refuses ambiguous All stores", () => {
  assert.equal(homeCreationStoreId(PROJECTS, { ownerGame: "game-b" }), "game:game-b");
  assert.equal(homeCreationStoreId(PROJECTS, { ownerGame: "game-a" }), "studio");
  assert.equal(homeCreationStoreId(PROJECTS, { storeId: "game:game-b", ownerGame: "game-b" }), "game:game-b");
  assert.equal(homeCreationStoreId([
    ...PROJECTS,
    { id: "private-a", storeId: "game:game-a", ownership: { kind: "game", gameId: "game-a" } },
  ], { ownerGame: "game-a" }), null);
});
