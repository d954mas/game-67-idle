import assert from "node:assert/strict";
import test from "node:test";
import { createYandexPlatformAdapter } from "../web/adapters/yandex.js";
import { createPlaygamaPlatformAdapter } from "../web/adapters/playgama.js";
import { createCrazygamesPlatformAdapter } from "../web/adapters/crazygames.js";
import { createPokiPlatformAdapter } from "../web/adapters/poki.js";

function yandex(storage) {
  return createYandexPlatformAdapter({ host: { YaGames: { init: async () => ({
    getPlayer: async () => storage,
  }) } } });
}

test("storage distinguishes a missing key from a failed read", async () => {
  const missing = yandex({ getData: async () => ({}) });
  assert.deepEqual(await missing.loadData("save"), { status: "missing" });
  const failed = yandex({ getData: async () => { throw new Error("offline"); } });
  assert.deepEqual(await failed.loadData("save"), { status: "failed" });
});

test("storage acknowledges only writes accepted by the backend", async () => {
  const accepted = yandex({ setData: async (_data, flush) => { assert.equal(flush, true); } });
  assert.deepEqual(await accepted.saveData("save", "document"), { status: "acknowledged" });
  const failed = yandex({ setData: async () => { throw new Error("quota"); } });
  assert.deepEqual(await failed.saveData("save", "document"), { status: "failed" });
});

test("storage preserves found values and reports absent APIs as unavailable", async () => {
  const backend = yandex({ getData: async () => ({ save: "document" }) });
  assert.deepEqual(await backend.loadData("save"), { status: "found", value: "document" });
  assert.deepEqual(await backend.saveData("save", "document"), { status: "unavailable" });
  const poki = createPokiPlatformAdapter({ host: {} });
  assert.deepEqual(await poki.loadData("save"), { status: "unavailable" });
  assert.deepEqual(await poki.saveData("save", "document"), { status: "unavailable" });
});

test("a transient player lookup failure does not poison later storage reads", async () => {
  let calls = 0;
  const backend = createYandexPlatformAdapter({ host: { YaGames: { init: async () => ({
    getPlayer: async () => {
      if (++calls === 1) throw new Error("offline");
      return { getData: async () => ({ save: "recovered" }) };
    },
  }) } } });
  assert.deepEqual(await backend.loadData("save"), { status: "failed" });
  assert.deepEqual(await backend.loadData("save"), { status: "found", value: "recovered" });
});

test("Playgama preserves failed and missing storage outcomes", async () => {
  const storage = { get: async () => null, set: async () => { throw new Error("quota"); } };
  const backend = createPlaygamaPlatformAdapter({ host: { bridge: {
    initialize: async () => {}, storage,
  } } });
  assert.deepEqual(await backend.loadData("save"), { status: "missing" });
  storage.get = async () => { throw new Error("offline"); };
  assert.deepEqual(await backend.loadData("save"), { status: "failed" });
  assert.deepEqual(await backend.saveData("save", "document"), { status: "failed" });
});

test("CrazyGames storage propagates synchronous failures", async () => {
  const backend = createCrazygamesPlatformAdapter({ host: { CrazyGames: { SDK: {
    init: async () => {}, data: {
      getItem() { throw new Error("disabled"); },
      setItem() { throw new Error("quota"); },
    },
  } } } });
  assert.deepEqual(await backend.loadData("save"), { status: "failed" });
  assert.deepEqual(await backend.saveData("save", "document"), { status: "failed" });
});
