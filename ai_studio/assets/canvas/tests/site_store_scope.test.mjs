import test from "node:test";
import assert from "node:assert/strict";
import {
  appendStoreQuery,
  canvasApiUrl,
  canvasRefBase,
  canvasStoreHeaders,
  decodeLastProject,
  encodeLastProject,
  projectCacheKey,
  projectFileUrl,
  projectKey,
  projectStoreId,
  setStoreParams,
  storeIdFromParams,
} from "../site/store_scope.js";

test("site store scope keeps visible API URLs unqualified and sends private store headers", () => {
  assert.equal(projectStoreId({ id: "p1" }), "studio");
  assert.equal(appendStoreQuery("/projects/p1", "studio"), "/projects/p1");
  assert.equal(appendStoreQuery("/projects/p1?select=e1", "game:secret-game"), "/projects/p1?select=e1&store=game%3Asecret-game");
  assert.equal(canvasApiUrl("/projects/p1/files/a.png", "game:secret-game"), "/api/canvas/projects/p1/files/a.png");
  assert.deepEqual(canvasStoreHeaders("studio", { "content-type": "application/json" }), { "content-type": "application/json" });
  assert.deepEqual(canvasStoreHeaders("game:secret-game"), { "x-ai-studio-store": "game:secret-game" });
});

test("site store scope reads and writes deep-link store params", () => {
  assert.equal(storeIdFromParams(new URLSearchParams("project=p1")), "studio");
  assert.equal(storeIdFromParams(new URLSearchParams("game=secret-game&project=p1")), "game:secret-game");
  assert.equal(storeIdFromParams(new URLSearchParams("store=game%3Asecret-game&project=p1")), "game:secret-game");

  const params = setStoreParams(new URLSearchParams("project=p1&game=old"), "game:secret-game");
  assert.equal(params.toString(), "project=p1&store=game%3Asecret-game");
  assert.equal(setStoreParams(new URLSearchParams("project=p1&store=game%3Ax"), "studio").toString(), "project=p1");
});

test("site store scope stores last project as v2 pair and migrates legacy public ids", () => {
  assert.deepEqual(decodeLastProject("old-public-id"), { storeId: "studio", projectId: "old-public-id" });
  const encoded = encodeLastProject({ id: "private-id", storeId: "game:secret-game" });
  assert.deepEqual(decodeLastProject(encoded), { storeId: "game:secret-game", projectId: "private-id" });
});

test("site store scope separates file URLs, image-cache keys, project keys, and private refs by store", () => {
  const publicProject = { id: "same-id", title: "Public", storeId: "studio" };
  const privateProject = { id: "same-id", title: "Secret title", storeId: "game:secret-game" };

  assert.equal(projectKey(publicProject), "studio:same-id");
  assert.equal(projectKey(privateProject), "game:secret-game:same-id");
  assert.equal(projectFileUrl(publicProject, "files/a.png"), "/api/canvas/projects/same-id/files/a.png");
  assert.equal(projectFileUrl(privateProject, "files/a.png"), "/api/canvas/projects/same-id/files/a.png?store=game%3Asecret-game");
  assert.notEqual(projectCacheKey(publicProject, "files/a.png"), projectCacheKey(privateProject, "files/a.png"));

  assert.deepEqual(canvasRefBase(publicProject), { uri: "canvas://same-id", private: false, title: "Public" });
  assert.deepEqual(canvasRefBase(privateProject), { uri: "canvas://game/secret-game/same-id", private: true, title: "Secret title" });
});
