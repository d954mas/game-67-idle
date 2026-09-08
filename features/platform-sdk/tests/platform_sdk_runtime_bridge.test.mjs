import assert from "node:assert/strict";
import test from "node:test";

import { createPokiPlatformAdapter } from "../web/adapters/poki.js";
import { createYandexPlatformAdapter } from "../web/adapters/yandex.js";

test("Poki reports the documented start edge and later close edge with one request id", async () => {
  let resolveBreak;
  const lifecycle = { events: [], adVisible(id, visible) { this.events.push([id, visible]); } };
  const host = { setTimeout: () => 1, clearTimeout() {}, PokiSDK: {
    init: async () => {},
    commercialBreak(onStart) {
      onStart();
      return new Promise((resolve) => { resolveBreak = resolve; });
    },
  } };
  const adapter = createPokiPlatformAdapter({ host, lifecycle });
  await adapter.ready();

  const pending = adapter.showInterstitial("level_break", 41);
  await new Promise(setImmediate);
  assert.deepEqual(lifecycle.events, [[41, true]]);

  resolveBreak();
  assert.deepEqual(await pending, { supported: true, shown: true });
  assert.deepEqual(lifecycle.events, [[41, true], [41, false]]);
});

test("Yandex closes visibility using the original request id", async () => {
  let callbacks;
  const lifecycle = { events: [], adVisible(id, visible) { this.events.push([id, visible]); } };
  const host = { setTimeout: () => 1, clearTimeout() {}, YaGames: { init: async () => ({
    adv: { showFullscreenAdv({ callbacks: value }) { callbacks = value; } },
  }) } };
  const adapter = createYandexPlatformAdapter({ host, lifecycle });
  await adapter.ready();

  const pending = adapter.showInterstitial("level_break", 73);
  await new Promise(setImmediate);
  callbacks.onOpen();
  callbacks.onClose(true);

  assert.deepEqual(await pending, { supported: true, shown: true });
  assert.deepEqual(lifecycle.events, [[73, true], [73, false]]);
});
