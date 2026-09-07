import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { runInNewContext } from "node:vm";

import { createCrazygamesPlatformAdapter } from "../web/adapters/crazygames.js";
import { createMockPlatformAdapter } from "../web/adapters/mock.js";
import { createPlaygamaPlatformAdapter } from "../web/adapters/playgama.js";
import { createPokiPlatformAdapter } from "../web/adapters/poki.js";
import { createYandexPlatformAdapter } from "../web/adapters/yandex.js";
import {
  inspectPlatformSdkArtifact,
  platformSdkBundlePrefix,
  sdkForTarget,
  stagePlatformSdkWebAssets,
} from "../scripts/artifact_tools.mjs";
import { scorecardFromNdjson } from "../scripts/scorecard.mjs";
import { createBuildPlan } from "../../../templates/template/tools/build_web.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const TargetPlatform = Object.freeze({
  LOCAL: "local",
  ITCH: "itch",
  POKI: "poki",
  YANDEX: "yandex",
  PLAYGAMA: "playgama",
  CRAZYGAMES: "crazygames",
});
const PLATFORM_BACKEND_METHODS = Object.freeze([
  "destroy",
  "fetchEntries",
  "gameLoadingProgress",
  "gameLoadingFinished",
  "gameReady",
  "gameplayStart",
  "gameplayStop",
  "getLocale",
  "getPlayer",
  "hideBanner",
  "leaderboardCaps",
  "loadData",
  "login",
  "measure",
  "ready",
  "saveData",
  "showBanner",
  "showInterstitial",
  "showLeaderboard",
  "showRewarded",
  "submitScore",
]);

function packagedPlatformPrefix(adapter) {
  const modules = [
    ["platform-sdk-adapter.js", readFileSync(join(HERE, `../web/adapters/${adapter}.js`), "utf8")],
    ["platform-sdk.js", readFileSync(join(HERE, "../web/platform-sdk.js"), "utf8")],
  ];
  const body = modules.map(([label, source]) => {
    const expectedImports = label === "platform-sdk.js" ? new Set([
      'import { createPlatformSdkAdapter } from "./platform-sdk-adapter.js";',
    ]) : new Set();
    const lines = [];
    for (const line of source.split(/\r?\n/)) {
      if (/^\s*import\s/.test(line)) {
        assert.equal(expectedImports.delete(line.trim()), true, label);
        continue;
      }
      lines.push(line.replace(/^(\s*)export\s+((?:async\s+)?function|const|let|var|class)\b/, "$1$2"));
    }
    assert.equal(expectedImports.size, 0, label);
    return lines.join("\n").trimEnd();
  }).join("\n\n");
  return `(function () {\n${body}\n}());\n`;
}

class FakeElement {
  constructor(tagName) {
    this.tagName = tagName;
    this.children = [];
    this.dataset = {};
    this.listeners = new Map();
    this.parentNode = null;
    this.style = {};
    this.textContent = "";
    this.type = "";
  }

  appendChild(child) {
    child.parentNode = this;
    this.children.push(child);
    return child;
  }

  append(...children) {
    for (const child of children) this.appendChild(child);
  }

  remove() {
    if (!this.parentNode) return;
    this.parentNode.children = this.parentNode.children.filter((child) => child !== this);
    this.parentNode = null;
  }

  addEventListener(type, callback) {
    const list = this.listeners.get(type) || [];
    list.push(callback);
    this.listeners.set(type, list);
  }

  click() {
    for (const callback of this.listeners.get("click") || []) callback({ type: "click" });
  }

  findByAction(action) {
    if (this.dataset.platformSdkAction === action) return this;
    for (const child of this.children) {
      const found = child.findByAction(action);
      if (found) return found;
    }
    return null;
  }
}

class FakeDocument {
  constructor() {
    this.body = new FakeElement("body");
    this.head = new FakeElement("head");
    this.hidden = false;
    this.listeners = new Map();
  }

  createElement(tagName) {
    return new FakeElement(tagName);
  }

  addEventListener(type, callback) {
    const list = this.listeners.get(type) || [];
    list.push(callback);
    this.listeners.set(type, list);
  }

  removeEventListener(type, callback) {
    const list = this.listeners.get(type) || [];
    this.listeners.set(
      type,
      list.filter((entry) => entry !== callback),
    );
  }

  dispatch(type) {
    for (const callback of this.listeners.get(type) || []) callback({ type });
  }
}

function createHost(target = TargetPlatform.LOCAL) {
  const document = new FakeDocument();
  const storage = new Map();
  const warnings = [];
  return {
    document,
    target,
    warnings,
    console: {
      warn(...args) {
        warnings.push(args);
      },
    },
    localStorage: {
      getItem(key) {
        return storage.has(key) ? storage.get(key) : null;
      },
      setItem(key, value) {
        storage.set(key, String(value));
      },
      removeItem(key) {
        storage.delete(key);
      },
    },
  };
}

function createMockBackend(target) {
  const host = createHost(target);
  const backend = createMockPlatformAdapter({ host, target });
  return { backend, host };
}

async function flushMicrotasks() {
  for (let i = 0; i < 8; i += 1) await Promise.resolve();
}

test("build tooling maps publish targets to exactly one platform SDK adapter", () => {
  assert.equal(sdkForTarget(TargetPlatform.LOCAL), "mock");
  assert.equal(sdkForTarget(TargetPlatform.ITCH), "mock");
  assert.equal(sdkForTarget(TargetPlatform.POKI), "poki");
  assert.equal(sdkForTarget(TargetPlatform.YANDEX), "yandex");
  assert.equal(sdkForTarget(TargetPlatform.PLAYGAMA), "playgama");
});

test("every platform adapter owns exactly the complete backend method contract", () => {
  for (const [target, factory] of [
    [TargetPlatform.LOCAL, createMockPlatformAdapter],
    [TargetPlatform.POKI, createPokiPlatformAdapter],
    [TargetPlatform.YANDEX, createYandexPlatformAdapter],
    [TargetPlatform.PLAYGAMA, createPlaygamaPlatformAdapter],
  ]) {
    const adapter = factory({
      emitVisibilityChange() {},
      host: createHost(target),
      target,
    });
    for (const method of PLATFORM_BACKEND_METHODS) {
      assert.equal(typeof adapter[method], "function", `${target}.${method}`);
    }
    assert.deepEqual(Object.keys(adapter).sort(), [...PLATFORM_BACKEND_METHODS].sort(), target);
    adapter.destroy();
  }
});

test("template CMake isolates web presets by publish target", () => {
  const cmake = readFileSync(join(HERE, "../../../templates/template/cmake/GameOptions.cmake"), "utf8");

  assert.match(cmake, /set\(GAME_PUBLISH_TARGET "local" CACHE STRING/);
  assert.match(cmake, /if\(EMSCRIPTEN AND NOT GAME_PUBLISH_TARGET STREQUAL "local"\)/);
  assert.match(cmake, /set\(NT_PRESET_NAME "\$\{NT_PRESET_NAME\}-\$\{GAME_PUBLISH_TARGET\}"\)/);
  assert.equal(cmake.includes("GAME_BUILD_VARIANT"), false);
});

test("web builds use a checkout-local Emscripten cache by default", () => {
  const cmake = readFileSync(join(HERE, "../../../templates/template/cmake/GameOptions.cmake"), "utf8");
  const gameDir = join(HERE, "../../../templates/template");
  const plan = createBuildPlan({
    gameDir,
    args: { preset: "wasm-release", target: "local", debugUi: "default" },
    env: {},
    platform: "linux",
    nativeConfigured: true,
    toolchainExists: false,
  });

  assert.match(cmake, /set\(GAME_EMSCRIPTEN_CACHE_DIR "\$\{_game_default_em_cache\}" CACHE PATH/);
  assert.match(cmake, /RULE_LAUNCH_COMPILE "\$\{_game_emcache_launcher\}"/);
  assert.match(cmake, /RULE_LAUNCH_LINK "\$\{_game_emcache_launcher\}"/);
  assert.equal(plan.env.EM_CACHE, join(gameDir, "build", "emscripten-cache"));
});

test("poki adapter calls the official measure category what action contract", async () => {
  const host = createHost(TargetPlatform.POKI);
  const measures = [];
  host.PokiSDK = {
    init() { return Promise.resolve(); },
    measure(category, what, action) { measures.push([category, what, action]); },
  };
  const adapter = createPokiPlatformAdapter({ host });

  await adapter.measure("collection", "3", "complete");

  assert.deepEqual(measures, [["collection", "3", "complete"]]);
});

test("poki adapter preloads a pending SDK without blocking backend readiness", async () => {
  const host = createHost(TargetPlatform.POKI);
  let releaseInit;
  const pendingInit = new Promise((resolve) => {
    releaseInit = resolve;
  });
  host.PokiSDK = { init: () => pendingInit };
  const adapter = createPokiPlatformAdapter({ host });

  const ready = adapter.ready();
  try {
    assert.equal(ready, true);
    assert.equal(adapter.gameLoadingProgress(0.25), undefined);
  } finally {
    releaseInit();
    await Promise.resolve(ready);
    adapter.destroy();
  }
});

test("poki rewarded reports not-ready when the external SDK is unavailable", async () => {
  const host = createHost(TargetPlatform.POKI);
  const adapter = createPokiPlatformAdapter({ host });

  const resultPromise = adapter.showRewarded("double_reward");
  await Promise.resolve();
  await Promise.resolve();
  assert.equal(host.document.head.children.length, 1);
  host.document.head.children[0].onerror();

  assert.deepEqual(await resultPromise, {
    supported: false,
    shown: false,
    rewarded: false,
    reason: "not_ready",
  });
  adapter.destroy();
});

test("poki rewarded reports not-ready when the loaded SDK lacks rewardedBreak", async () => {
  const host = createHost(TargetPlatform.POKI);
  host.PokiSDK = { init: () => Promise.resolve() };
  const adapter = createPokiPlatformAdapter({ host });

  assert.deepEqual(await adapter.showRewarded("double_reward"), {
    supported: false,
    shown: false,
    rewarded: false,
    reason: "not_ready",
  });
  adapter.destroy();
});

test("web loading-progress bridge stays synchronous and allocation-free", () => {
  const source = readFileSync(join(HERE, "../src/platform_sdk_web.c"), "utf8");
  const begin = source.indexOf("EM_JS(void, platform_sdk_web_backend_game_loading_progress");
  const end = source.indexOf("EM_JS(void, platform_sdk_web_backend_game_loading_finished", begin);
  assert.notEqual(begin, -1);
  assert.notEqual(end, -1);
  assert.equal(source.slice(begin, end).includes("Promise.resolve"), false);
});

test("poki adapter coalesces loading progress queued before SDK init completes", async () => {
  const host = createHost(TargetPlatform.POKI);
  const progress = [];
  let resolveInit;
  host.PokiSDK = {
    init() {
      return new Promise((resolve) => {
        resolveInit = resolve;
      });
    },
    gameLoadingProgress(payload) {
      progress.push(payload.percentageDone);
    },
  };
  const adapter = createPokiPlatformAdapter({ host });

  const p1 = adapter.gameLoadingProgress(0.10);
  const p2 = adapter.gameLoadingProgress(0.45);
  const p3 = adapter.gameLoadingProgress(1.0);
  assert.deepEqual([p1, p2, p3], [undefined, undefined, undefined]);
  assert.deepEqual(progress, []);

  await Promise.resolve();
  await Promise.resolve();
  resolveInit();
  await adapter.gameLoadingFinished();
  assert.deepEqual(progress, [1]);

  await adapter.gameLoadingProgress(0.75);
  await adapter.gameLoadingProgress(1.0);
  assert.deepEqual(progress, [1]);
});

test("poki adapter reuses one payload object across loading progress updates", async () => {
  const host = createHost(TargetPlatform.POKI);
  const payloads = [];
  host.PokiSDK = {
    init() {
      return Promise.resolve();
    },
    gameLoadingProgress(payload) {
      payloads.push(payload);
    },
  };
  const adapter = createPokiPlatformAdapter({ host });

  adapter.ready();
  await adapter.gameLoadingFinished();
  adapter.gameLoadingProgress(0.25);
  adapter.gameLoadingProgress(0.75);

  assert.equal(payloads.length, 3);
  assert.equal(new Set(payloads).size, 1);
  assert.equal(payloads[0].percentageDone, 0.75);
});

test("poki progress throw does not poison SDK readiness or loading completion", async () => {
  const host = createHost(TargetPlatform.POKI);
  let loadingFinished = 0;
  host.PokiSDK = {
    init() {
      return Promise.resolve();
    },
    gameLoadingProgress() {
      throw new Error("progress failed");
    },
    gameLoadingFinished() {
      loadingFinished += 1;
    },
  };
  const adapter = createPokiPlatformAdapter({ host });

  adapter.gameLoadingProgress(0.5);
  await assert.doesNotReject(adapter.gameLoadingFinished());

  assert.equal(loadingFinished, 1);
  adapter.destroy();
});

test("hung platform ads settle with a bounded failed result", async (t) => {
  t.mock.timers.enable({ apis: ["setTimeout"] });
  const failed = { supported: true, shown: false, reason: "failed" };
  const cases = [
    ["poki", () => {
      const host = createHost(TargetPlatform.POKI);
      host.PokiSDK = {
        init() {
          return Promise.resolve();
        },
        commercialBreak() {
          return new Promise(() => {});
        },
      };
      return createPokiPlatformAdapter({ host });
    }],
    ["yandex", () => {
      const host = createHost(TargetPlatform.YANDEX);
      host.YaGames = {
        init() {
          return Promise.resolve({
            adv: {
              showFullscreenAdv() {},
            },
          });
        },
      };
      return createYandexPlatformAdapter({ host });
    }],
    ["playgama", () => {
      const host = createHost(TargetPlatform.PLAYGAMA);
      host.bridge = {
        initialize() {
          return Promise.resolve();
        },
        platform: {},
        advertisement: {
          isInterstitialSupported: true,
          on() {},
          showInterstitial() {},
        },
      };
      return createPlaygamaPlatformAdapter({ host });
    }],
  ];

  for (const [name, createAdapter] of cases) {
    const adapter = createAdapter();
    let outcome;
    void adapter.showInterstitial("level_break").then((result) => {
      outcome = result;
    });

    await flushMicrotasks();
    t.mock.timers.runAll();
    await flushMicrotasks();

    assert.deepEqual(outcome, failed, name);
    adapter.destroy();
  }
});

test("playgama destroy blocks new bridge calls and settles an active ad", async (t) => {
  t.mock.timers.enable({ apis: ["setTimeout"] });
  let showCalls = 0;
  let removedListeners = 0;
  const handlers = new Map();
  const host = createHost(TargetPlatform.PLAYGAMA);
  host.bridge = {
    initialize() {
      return Promise.resolve();
    },
    platform: {
      sendMessage() {},
    },
    advertisement: {
      isInterstitialSupported: true,
      on(name, handler) {
        handlers.set(name, handler);
      },
      off(name, handler) {
        if (handlers.get(name) === handler) handlers.delete(name);
        removedListeners += 1;
      },
      showInterstitial() {
        showCalls += 1;
      },
    },
  };

  const adapter = createPlaygamaPlatformAdapter({ host });
  await adapter.ready();
  adapter.destroy();
  let afterDestroy;
  void adapter.showInterstitial("level_break").then((result) => {
    afterDestroy = result;
  });
  await flushMicrotasks();
  t.mock.timers.runAll();
  await flushMicrotasks();

  assert.equal(showCalls, 0);
  assert.deepEqual(afterDestroy, { supported: false, shown: false, reason: "not_ready" });

  const activeAdapter = createPlaygamaPlatformAdapter({ host });
  let activeResult;
  void activeAdapter.showInterstitial("level_break").then((result) => {
    activeResult = result;
  });
  await flushMicrotasks();
  assert.equal(showCalls, 1);
  activeAdapter.destroy();
  t.mock.timers.runAll();
  await flushMicrotasks();

  assert.ok(activeResult);
  assert.equal(activeResult.shown, false);
  assert.equal(handlers.size, 0);
  assert.equal(removedListeners, 1);
});

test("playgama keeps an earned reward when a late failure event arrives", async () => {
  const handlers = new Map();
  const host = createHost(TargetPlatform.PLAYGAMA);
  host.bridge = {
    initialize() {
      return Promise.resolve();
    },
    platform: {},
    advertisement: {
      isRewardedSupported: true,
      on(name, handler) {
        handlers.set(name, handler);
      },
      off(name) {
        handlers.delete(name);
      },
      showRewarded() {},
    },
  };
  const adapter = createPlaygamaPlatformAdapter({ host });

  const result = adapter.showRewarded("double_reward");
  await flushMicrotasks();
  const handler = handlers.get("rewarded_state_changed");
  assert.equal(typeof handler, "function");
  handler("rewarded");
  handler("failed");

  assert.deepEqual(await result, { supported: true, shown: false, rewarded: true });
  adapter.destroy();
});

test("Yandex and Playgama synchronous SDK throws resolve failed ad results", async () => {
  const yandexHost = createHost(TargetPlatform.YANDEX);
  yandexHost.YaGames = {
    init() {
      return Promise.resolve({
        adv: {
          showFullscreenAdv() {
            throw new Error("fullscreen failed");
          },
        },
      });
    },
  };
  const yandex = createYandexPlatformAdapter({ host: yandexHost });
  assert.deepEqual(await yandex.showInterstitial("level_break"), {
    supported: true,
    shown: false,
    reason: "failed",
  });
  yandex.destroy();

  const playgamaHost = createHost(TargetPlatform.PLAYGAMA);
  playgamaHost.bridge = {
    initialize() {
      return Promise.resolve();
    },
    platform: {},
    advertisement: {
      isInterstitialSupported: true,
      on() {
        throw new Error("listener failed");
      },
      showInterstitial() {},
    },
  };
  const playgama = createPlaygamaPlatformAdapter({ host: playgamaHost });
  assert.deepEqual(await playgama.showInterstitial("level_break"), {
    supported: true,
    shown: false,
    reason: "failed",
  });
  playgama.destroy();
});

test("yandex adapter uses documented loading, gameplay, and ad callbacks", async () => {
  const host = createHost(TargetPlatform.YANDEX);
  const calls = [];
  const ysdk = {
    features: {
      LoadingAPI: {
        ready() {
          calls.push("loading.ready");
        },
      },
      GameplayAPI: {
        start() {
          calls.push("gameplay.start");
        },
        stop() {
          calls.push("gameplay.stop");
        },
      },
    },
    adv: {
      showFullscreenAdv({ callbacks }) {
        calls.push("fullscreen");
        callbacks.onOpen();
        callbacks.onClose(true);
      },
      showRewardedVideo({ callbacks }) {
        calls.push("rewarded");
        callbacks.onOpen();
        callbacks.onRewarded();
        callbacks.onClose(true);
      },
    },
  };
  host.YaGames = {
    init() {
      calls.push("init");
      return Promise.resolve(ysdk);
    },
  };

  const adapter = createYandexPlatformAdapter({ host });

  assert.equal(await adapter.ready(), true);
  await adapter.gameLoadingFinished();
  await adapter.gameplayStart();
  await adapter.gameplayStop();
  assert.deepEqual(await adapter.showInterstitial("level_break"), {
    supported: true,
    shown: true,
  });
  assert.deepEqual(await adapter.showRewarded("double_reward"), {
    supported: true,
    shown: true,
    rewarded: true,
  });
  assert.deepEqual(calls, [
    "init",
    "loading.ready",
    "gameplay.start",
    "gameplay.stop",
    "fullscreen",
    "rewarded",
  ]);
});

function createYandexAuthSdk(calls, { accept, name = "Ada", photo = "https://avatars.example/ada" } = {}) {
  let authorized = false;
  let issued = 0;
  function makePlayer() {
    const authorizedAtIssue = authorized;
    issued += 1;
    return {
      isAuthorized: () => authorizedAtIssue,
      getName: () => name,
      getPhoto: (size) => `${photo}/${size}`,
      setData: () => Promise.resolve(),
      getData: () => Promise.resolve({}),
    };
  }
  return {
    sdk: {
      getPlayer() {
        calls.push("getPlayer");
        return Promise.resolve(makePlayer());
      },
      auth: {
        openAuthDialog() {
          calls.push("openAuthDialog");
          if (!accept) return Promise.reject(new Error("closed"));
          authorized = true;
          return Promise.resolve();
        },
      },
    },
    playersIssued: () => issued,
  };
}

test("yandex login re-reads the player after an accepted dialog", async () => {
  const host = createHost(TargetPlatform.YANDEX);
  const calls = [];
  const fake = createYandexAuthSdk(calls, { accept: true });
  host.YaGames = { init: () => Promise.resolve(fake.sdk) };
  const adapter = createYandexPlatformAdapter({ host });

  assert.equal(await adapter.ready(), true);
  assert.deepEqual(await adapter.getPlayer(), { authorized: false, name: "", avatarUrl: "" });

  const result = await adapter.login();
  assert.equal(result.supported, true);
  assert.equal(result.authorized, true);
  assert.equal(result.reason, "accepted");
  assert.equal(result.name, "Ada");
  assert.equal(result.avatarUrl, "https://avatars.example/ada/medium");
  assert.deepEqual(calls, ["getPlayer", "openAuthDialog", "getPlayer"]);
  assert.equal(fake.playersIssued(), 2, "the anonymous player object is not reused after the dialog");

  assert.deepEqual(await adapter.getPlayer(), { authorized: true, name: "Ada", avatarUrl: "https://avatars.example/ada/medium" });
  assert.equal((await adapter.login()).reason, "accepted");
  assert.deepEqual(calls, ["getPlayer", "openAuthDialog", "getPlayer"], "an authorized player is never shown the dialog again");
});

test("yandex login treats a closed dialog as declined and stays anonymous", async () => {
  const host = createHost(TargetPlatform.YANDEX);
  const calls = [];
  const fake = createYandexAuthSdk(calls, { accept: false });
  host.YaGames = { init: () => Promise.resolve(fake.sdk) };
  const errors = [];
  host.console = { error: (...args) => errors.push(args), warn: (...args) => errors.push(args) };
  const adapter = createYandexPlatformAdapter({ host });

  assert.deepEqual(await adapter.login(), {
    supported: true,
    authorized: false,
    reason: "declined",
    name: "",
    avatarUrl: "",
  });
  assert.deepEqual(await adapter.getPlayer(), { authorized: false, name: "", avatarUrl: "" });
  assert.deepEqual(errors, []);
  assert.equal(calls.filter((c) => c === "openAuthDialog").length, 1);
});

test("yandex login without an auth surface is unsupported", async () => {
  const host = createHost(TargetPlatform.YANDEX);
  host.YaGames = { init: () => Promise.resolve({ getPlayer: () => Promise.resolve({ isAuthorized: () => false }) }) };
  const adapter = createYandexPlatformAdapter({ host });
  const result = await adapter.login();
  assert.equal(result.supported, false);
  assert.equal(result.reason, "unsupported");
});

test("every non-Yandex adapter answers auth unsupported and an anonymous player", async () => {
  for (const [target, factory] of [
    [TargetPlatform.ITCH, createMockPlatformAdapter],
    [TargetPlatform.POKI, createPokiPlatformAdapter],
    [TargetPlatform.PLAYGAMA, createPlaygamaPlatformAdapter],
  ]) {
    const adapter = factory({ emitVisibilityChange() {}, host: createHost(target), target });
    const result = await adapter.login();
    assert.equal(result.supported, false, target);
    assert.equal(result.reason, "unsupported", target);
    assert.deepEqual(await adapter.getPlayer(), { authorized: false, name: "", avatarUrl: "" }, target);
    adapter.destroy();
  }
});

test("local mock login signs in a fake player for the page", async () => {
  const adapter = createMockPlatformAdapter({ host: createHost(TargetPlatform.LOCAL), target: TargetPlatform.LOCAL });
  assert.deepEqual(await adapter.getPlayer(), { authorized: false, name: "", avatarUrl: "" });
  const result = await adapter.login();
  assert.equal(result.supported, true);
  assert.equal(result.authorized, true);
  assert.equal(result.reason, "accepted");
  assert.ok(result.name.length > 0);
  assert.equal((await adapter.getPlayer()).authorized, true);
  adapter.destroy();
});

/* A Yandex SDK with a leaderboard surface and a scriptable player. The clock
   is the adapter's, so both portal quotas are exercised without waiting. */
function createYandexLeaderboardFixture({ authorized = false, entries = null, reject = null } = {}) {
  const host = createHost(TargetPlatform.YANDEX);
  let clock = 1_000_000;
  host.setTimeout = (fn, ms) => {
    clock += Number(ms) || 0;
    fn();
    return 0;
  };
  host.clearTimeout = () => {};
  const calls = [];
  const player = {
    isAuthorized: () => authorized,
    getName: () => "Ada",
    getPhoto: () => "https://avatars.example/ada",
    getUniqueID: () => "me",
    setData: () => Promise.resolve(),
    getData: () => Promise.resolve({}),
  };
  const sdk = {
    getPlayer: () => Promise.resolve(player),
    leaderboards: {
      setScore(name, score, extraData) {
        calls.push({ at: clock, name, score, extraData });
        if (reject && reject(name, score, extraData)) return Promise.reject(reject(name, score, extraData));
        return Promise.resolve();
      },
      getEntries(name, options) {
        calls.push({ at: clock, name, options });
        if (reject && reject(name, options)) return Promise.reject(reject(name, options));
        return Promise.resolve(entries || { entries: [], ranges: [], userRank: 0 });
      },
    },
  };
  host.YaGames = { init: () => Promise.resolve(sdk) };
  const adapter = createYandexPlatformAdapter({ host, now: () => clock });
  return { adapter, calls, advance: (ms) => { clock += ms; } };
}

function yandexEntry(rank, score, id, extraData) {
  return {
    rank,
    score,
    extraData,
    player: { publicName: `Player ${id}`, uniqueID: id, getAvatarSrc: (size) => `https://avatars.example/${id}/${size}` },
  };
}

test("yandex leaderboard submits with extraData and reads rows with name, avatar and payload", async () => {
  const entries = {
    ranges: [{ start: 0, size: 3 }, { start: 40, size: 3 }],
    userRank: 42,
    entries: [
      yandexEntry(1, 900, "a", "skin=1;"),
      yandexEntry(2, 800, "b", ""),
      yandexEntry(3, 700, "c", undefined),
      yandexEntry(41, 120, "x", ""),
      yandexEntry(42, 100, "me", "skin=7;"),
      yandexEntry(43, 90, "y", ""),
    ],
  };
  const { adapter, calls } = createYandexLeaderboardFixture({ authorized: true, entries });
  assert.equal(await adapter.ready(), true);
  assert.deepEqual(adapter.leaderboardCaps("planets"), { canRead: true, canWrite: true, needsLogin: true, nativePopup: false });

  assert.deepEqual(await adapter.submitScore("planets", 0, 100, "skin=7;"), { status: "ok" });
  assert.equal(calls[0].name, "planets");
  assert.equal(calls[0].score, 100);
  assert.equal(calls[0].extraData, "skin=7;");

  const page = await adapter.fetchEntries("planets", 0);
  assert.equal(page.status, "ok");
  assert.deepEqual(calls[1].options, { quantityTop: 20, includeUser: true, quantityAround: 10 });
  assert.equal(page.top.length, 3);
  assert.equal(page.around.length, 3);
  assert.deepEqual(page.top[0], {
    value: 900, rank: 1, you: false, name: "Player a", avatarUrl: "https://avatars.example/a/small", extra: "skin=1;",
  });
  assert.equal(page.top[2].extra, "", "an absent payload is empty, never undefined");
  const mine = page.around.find((row) => row.you);
  assert.equal(mine.rank, 42);
  assert.equal(mine.extra, "skin=7;", "the game's payload survives the extraData round trip");
  assert.deepEqual(page.player, { rank: 42, value: 100 });
  adapter.destroy();
});

test("yandex anonymous write answers needs_login and never reaches the portal; the top still reads", async () => {
  const entries = { ranges: [{ start: 0, size: 1 }], userRank: 0, entries: [yandexEntry(1, 5, "a", "")] };
  const { adapter, calls } = createYandexLeaderboardFixture({ authorized: false, entries });
  await adapter.ready();
  assert.deepEqual(await adapter.submitScore("planets", 0, 100, ""), { status: "needs_login" });
  assert.equal(calls.length, 0, "no setScore call for an anonymous player");

  const page = await adapter.fetchEntries("planets", 0);
  assert.equal(page.status, "ok");
  assert.deepEqual(calls[0].options, { quantityTop: 20, includeUser: false });
  assert.equal(page.top.length, 1);
  assert.equal(page.player, null);
  adapter.destroy();
});

test("yandex setScore is spaced one second apart and a rejected payload retries without it", async () => {
  const { adapter, calls } = createYandexLeaderboardFixture({
    authorized: true,
    reject: (name, score, extraData) => (extraData === "bad" ? new Error("extraData too long") : null),
  });
  await adapter.ready();
  const results = await Promise.all([
    adapter.submitScore("planets", 0, 1, ""),
    adapter.submitScore("planets", 0, 2, ""),
    adapter.submitScore("planets", 0, 3, "bad"),
  ]);
  assert.deepEqual(results, [{ status: "ok" }, { status: "ok" }, { status: "ok" }]);
  const scores = calls.map((call) => call.score);
  assert.deepEqual(scores, [1, 2, 3, 3]);
  for (let i = 1; i < calls.length; i += 1) {
    assert.ok(calls[i].at - calls[i - 1].at >= 1000, `call ${i} respects the one-per-second quota`);
  }
  assert.equal(calls[2].extraData, "bad");
  assert.equal(calls[3].extraData, undefined, "the extra is dropped, the score is not");
  adapter.destroy();
});

test("yandex getEntries refuses the twenty-first read in five minutes and recovers after the window", async () => {
  const { adapter, calls, advance } = createYandexLeaderboardFixture({ authorized: false });
  await adapter.ready();
  for (let i = 0; i < 20; i += 1) {
    assert.equal((await adapter.fetchEntries("planets", 0)).status, "ok", `read ${i + 1}`);
  }
  assert.deepEqual(await adapter.fetchEntries("planets", 0), { status: "rate_limited" });
  assert.equal(calls.length, 20, "the refused read never reached the portal");
  advance(5 * 60 * 1000 + 1);
  assert.equal((await adapter.fetchEntries("planets", 0)).status, "ok");
  adapter.destroy();
});

test("yandex leaderboard errors map to the four refusals without inventing one", async () => {
  const cases = [
    [new Error("Leaderboard not found"), "unsupported"],
    [{ code: "FetchError", message: "Player is not authorized" }, "needs_login"],
    [new Error("network"), "failed"],
  ];
  for (const [error, status] of cases) {
    const { adapter } = createYandexLeaderboardFixture({ authorized: true, reject: () => error });
    await adapter.ready();
    assert.equal((await adapter.fetchEntries("planets", 0)).status, status, String(error.message));
    assert.equal((await adapter.submitScore("planets", 0, 1, "")).status, status, String(error.message));
    adapter.destroy();
  }
});

test("yandex leaderboard answers no capability before the SDK is up", () => {
  const { adapter } = createYandexLeaderboardFixture();
  assert.deepEqual(adapter.leaderboardCaps("planets"), { canRead: false, canWrite: false, needsLogin: false, nativePopup: false });
  adapter.destroy();
});

test("crazygames leaderboard is write-only and submits an AES-GCM score the console key decrypts", async () => {
  const keyBytes = globalThis.crypto.getRandomValues(new Uint8Array(32));
  const leaderboardKey = Buffer.from(keyBytes).toString("base64");
  const host = createHost(TargetPlatform.CRAZYGAMES);
  const submitted = [];
  host.CrazyGames = {
    SDK: {
      environment: "crazygames",
      init: () => Promise.resolve(),
      game: { loadingStart() {}, loadingStop() {}, settings: { muteAudio: false } },
      user: { submitScore: (payload) => { submitted.push(payload); return Promise.resolve(); } },
    },
  };
  const adapter = createCrazygamesPlatformAdapter({ host, config: { leaderboardKey } });
  assert.equal(await adapter.ready(), true);
  assert.deepEqual(adapter.leaderboardCaps("main"), { canRead: false, canWrite: true, needsLogin: false, nativePopup: false });
  assert.deepEqual(await adapter.submitScore("main", 0, 4242, "skin=1;"), { status: "ok" });
  assert.equal(submitted.length, 1);
  assert.equal(submitted[0].score, 4242);

  const combined = Buffer.from(submitted[0].encryptedScore, "base64");
  const iv = combined.subarray(0, 12);
  const cipher = combined.subarray(12);
  const key = await globalThis.crypto.subtle.importKey("raw", keyBytes, { name: "AES-GCM" }, false, ["decrypt"]);
  const plain = await globalThis.crypto.subtle.decrypt({ name: "AES-GCM", iv }, key, cipher);
  assert.equal(new TextDecoder().decode(plain), "4242");

  assert.deepEqual(await adapter.fetchEntries("main", 0), { status: "unsupported" });
  assert.deepEqual(await adapter.showLeaderboard("main"), { status: "unsupported" });
  adapter.destroy();

  const keyless = createCrazygamesPlatformAdapter({ host });
  await keyless.ready();
  assert.equal(keyless.leaderboardCaps("main").canWrite, false, "no console key, nothing to encrypt with");
  assert.deepEqual(await keyless.submitScore("main", 0, 1, ""), { status: "unsupported" });
  keyless.destroy();
});

function createPlaygamaLeaderboardHost(type) {
  const host = createHost(TargetPlatform.PLAYGAMA);
  const calls = [];
  host.bridge = {
    initialize: () => Promise.resolve(),
    platform: { language: "en", sendMessage() {} },
    player: { id: "p7" },
    leaderboards: {
      type,
      setScore(id, score) { calls.push(`setScore:${id}:${score}`); return Promise.resolve(); },
      getEntries(id) {
        calls.push(`getEntries:${id}`);
        return Promise.resolve([
          { id: "p1", name: "One", photo: "https://p.example/1.png", score: 300, rank: 1 },
          { id: "p7", name: "Me", photo: "", score: 200, rank: 2 },
        ]);
      },
      showNativePopup(id) { calls.push(`popup:${id}`); return Promise.resolve(); },
    },
  };
  return { host, calls };
}

test("playgama leaderboard capability follows the host platform's type at run time", async () => {
  const expected = {
    not_available: { canRead: false, canWrite: false, needsLogin: false, nativePopup: false },
    in_game: { canRead: true, canWrite: true, needsLogin: false, nativePopup: false },
    native: { canRead: false, canWrite: true, needsLogin: false, nativePopup: false },
    native_popup: { canRead: false, canWrite: true, needsLogin: false, nativePopup: true },
  };
  for (const [type, caps] of Object.entries(expected)) {
    const { host } = createPlaygamaLeaderboardHost(type);
    const adapter = createPlaygamaPlatformAdapter({ host });
    assert.equal(await adapter.ready(), true);
    assert.deepEqual(adapter.leaderboardCaps("planets"), caps, type);
    adapter.destroy();
  }
});

test("playgama in_game reads entries, native_popup opens the overlay, not_available refuses writes", async () => {
  const inGame = createPlaygamaLeaderboardHost("in_game");
  const a = createPlaygamaPlatformAdapter({ host: inGame.host });
  await a.ready();
  assert.deepEqual(await a.submitScore("planets", 0, 200, "skin=1;"), { status: "ok" });
  const page = await a.fetchEntries("planets", 0);
  assert.equal(page.status, "ok");
  assert.deepEqual(page.top[0], { value: 300, rank: 1, you: false, name: "One", avatarUrl: "https://p.example/1.png", extra: "" });
  assert.equal(page.top[1].you, true);
  assert.deepEqual(page.player, { rank: 2, value: 200 });
  assert.deepEqual(await a.showLeaderboard("planets"), { status: "unsupported" });
  assert.deepEqual(inGame.calls, ["setScore:planets:200", "getEntries:planets"]);
  a.destroy();

  const popup = createPlaygamaLeaderboardHost("native_popup");
  const b = createPlaygamaPlatformAdapter({ host: popup.host });
  await b.ready();
  assert.deepEqual(await b.fetchEntries("planets", 0), { status: "unsupported" });
  assert.deepEqual(await b.showLeaderboard("planets"), { status: "ok" });
  assert.deepEqual(popup.calls, ["popup:planets"]);
  b.destroy();

  const none = createPlaygamaLeaderboardHost("not_available");
  const c = createPlaygamaPlatformAdapter({ host: none.host });
  await c.ready();
  assert.deepEqual(await c.submitScore("planets", 0, 1, ""), { status: "unsupported" });
  assert.deepEqual(none.calls, []);
  c.destroy();
});

test("poki and itch have no board; the local mock serves canned entries around the submitted score", async () => {
  for (const [target, factory] of [
    [TargetPlatform.ITCH, createMockPlatformAdapter],
    [TargetPlatform.POKI, createPokiPlatformAdapter],
  ]) {
    const adapter = factory({ emitVisibilityChange() {}, host: createHost(target), target });
    assert.deepEqual(adapter.leaderboardCaps("planets"), { canRead: false, canWrite: false, needsLogin: false, nativePopup: false }, target);
    assert.deepEqual(await adapter.submitScore("planets", 0, 1, ""), { status: "unsupported" }, target);
    assert.deepEqual(await adapter.fetchEntries("planets", 0), { status: "unsupported" }, target);
    adapter.destroy();
  }

  const local = createMockPlatformAdapter({ host: createHost(TargetPlatform.LOCAL), target: TargetPlatform.LOCAL });
  assert.equal(local.leaderboardCaps("planets").canRead, true);
  assert.deepEqual(await local.submitScore("planets", 0, 850, "skin=3;"), { status: "ok" });
  const page = await local.fetchEntries("planets", 0);
  assert.equal(page.status, "ok");
  assert.ok(page.top.length > 1);
  const mine = page.top.find((row) => row.you);
  assert.equal(mine.value, 850);
  assert.equal(mine.extra, "skin=3;");
  assert.deepEqual(page.player, { rank: mine.rank, value: 850 });
  for (let i = 1; i < page.top.length; i += 1) assert.ok(page.top[i - 1].value >= page.top[i].value, "sorted");
  local.destroy();
});

test("yandex adapter can load the documented custom-domain SDK URL", async () => {
  const host = createHost(TargetPlatform.YANDEX);
  const adapter = createYandexPlatformAdapter({
    host,
    sdkUrl: "https://sdk.games.s3.yandex.net/sdk.js",
  });
  const readyPromise = adapter.ready();

  assert.equal(host.document.head.children.length, 1);
  const script = host.document.head.children[0];
  assert.equal(script.src, "https://sdk.games.s3.yandex.net/sdk.js");

  host.YaGames = {
    init() {
      return Promise.resolve({});
    },
  };
  script.onload();

  assert.equal(await readyPromise, true);
});

test("playgama adapter uses documented bridge lifecycle and gameplay messages", async () => {
  const host = createHost(TargetPlatform.PLAYGAMA);
  const calls = [];
  host.bridge = {
    EVENT_NAME: {
      INTERSTITIAL_STATE_CHANGED: "interstitial_state_changed",
      REWARDED_STATE_CHANGED: "rewarded_state_changed",
    },
    initialize() {
      calls.push("initialize");
      return Promise.resolve();
    },
    platform: {
      language: "en",
      sendMessage(message) {
        calls.push(`message:${message}`);
      },
    },
    advertisement: {
      isInterstitialSupported: true,
      isRewardedSupported: true,
      handlers: new Map(),
      on(name, handler) {
        calls.push(`on:${name}`);
        this.handlers.set(name, handler);
      },
      off(name, handler) {
        calls.push(`off:${name}`);
        if (this.handlers.get(name) === handler) this.handlers.delete(name);
      },
      showInterstitial(placement) {
        calls.push(`interstitial:${placement}`);
        this.handlers.get("interstitial_state_changed")("closed");
      },
      showRewarded(placement) {
        calls.push(`rewarded:${placement}`);
        this.handlers.get("rewarded_state_changed")("rewarded");
        this.handlers.get("rewarded_state_changed")("closed");
      },
    },
  };

  const adapter = createPlaygamaPlatformAdapter({ host });

  assert.equal(await adapter.ready(), true);
  await adapter.gameReady();
  await adapter.gameplayStart();
  await adapter.gameplayStop();
  await adapter.gameplayStart();
  assert.deepEqual(await adapter.showInterstitial("level_break"), {
    supported: true,
    shown: true,
  });
  assert.deepEqual(await adapter.showRewarded("double_reward"), {
    supported: true,
    shown: true,
    rewarded: true,
  });
  assert.deepEqual(calls, [
    "initialize",
    "message:game_ready",
    "message:level_started",
    "message:level_paused",
    "message:level_resumed",
    "on:interstitial_state_changed",
    "interstitial:level_break",
    "off:interstitial_state_changed",
    "on:rewarded_state_changed",
    "rewarded:double_reward",
    "off:rewarded_state_changed",
  ]);
});

test("local JS mock is a method provider and does not render fake ad UI", async () => {
  const { backend, host } = createMockBackend(TargetPlatform.LOCAL);

  assert.deepEqual(await backend.showInterstitial("debug_test"), {
    supported: true,
    shown: true,
    reason: "completed",
  });
  assert.deepEqual(await backend.showRewarded("debug_test"), {
    supported: true,
    shown: true,
    rewarded: true,
  });
  assert.equal(host.document.body.children.length, 0);
  assert.equal(Object.hasOwn(host, "__platformSdkEvents"), false);
});

test("itch mock ad behavior is production-safe unsupported no-op", async () => {
  const { backend, host } = createMockBackend(TargetPlatform.ITCH);

  assert.deepEqual(await backend.showInterstitial("debug_test"), {
    supported: false,
    shown: false,
    reason: "unsupported",
  });
  assert.deepEqual(await backend.showRewarded("debug_test"), {
    supported: false,
    shown: false,
    rewarded: false,
    reason: "unsupported",
  });
  assert.equal(host.document.body.children.length, 0);
});

test("local mock visibility listener is removed on destroy", () => {
  const host = createHost(TargetPlatform.LOCAL);
  let visibilityChanges = 0;
  const backend = createMockPlatformAdapter({
    target: TargetPlatform.LOCAL,
    host,
    emitVisibilityChange() {
      visibilityChanges += 1;
    },
  });

  host.document.dispatch("visibilitychange");
  assert.equal(visibilityChanges, 1);

  backend.destroy();
  host.document.dispatch("visibilitychange");
  assert.equal(visibilityChanges, 1);
});

test("storage and destroy do not emit platform SDK analytics events", async () => {
  const { backend, host } = createMockBackend(TargetPlatform.LOCAL);

  await backend.saveData("slot", { coins: 5 });
  assert.deepEqual(await backend.loadData("slot"), { coins: 5 });
  backend.destroy();

  assert.equal(Object.hasOwn(host, "__platformSdkEvents"), false);
});

test("web runtime does not expose game-facing platform SDK globals", () => {
  const source = readFileSync(join(HERE, "../web/platform-sdk.js"), "utf8");
  const mock = readFileSync(join(HERE, "../web/adapters/mock.js"), "utf8");
  const poki = readFileSync(join(HERE, "../web/adapters/poki.js"), "utf8");

  assert.equal(source.includes("globalThis.PlatformSdk"), false);
  assert.equal(source.includes("PlatformSdkReady"), false);
  assert.equal(source.includes("__platformSdkWebBackend"), false);
  assert.equal(source.includes("platform-sdk-web-backend-ready"), false);
  assert.equal(source.includes("CustomEvent"), false);
  assert.equal(existsSync(join(HERE, "../web/platform-sdk-core.js")), false);
  assert.equal(mock.includes("createOverlay"), false);
  assert.equal(mock.includes("platformSdkOverlay"), false);
  assert.equal(poki.includes("gameplayActive"), false);
});

test("template web shell loads selected platform backend before game.js", () => {
  const shell = readFileSync(join(HERE, "../../../templates/template/web/index.html.in"), "utf8");
  const source = readFileSync(join(HERE, "../web/platform-sdk.js"), "utf8");

  assert.equal(shell.includes('import \'./platform-sdk.js\';'), true);
  assert.equal(shell.includes("gameScript.src = 'game.js';"), true);
  assert.equal(shell.includes('<script type="module" src="platform-sdk.js"></script>'), false);
  assert.equal(shell.includes('<script src="game.js"></script>'), false);
  assert.equal(shell.includes("__platformSdkSetLoadingProgress"), true);
  assert.equal(shell.includes("__platformSdkHideLoadingOverlay"), true);
  assert.equal(shell.includes("statusEl.style.display = 'none'"), false);
  assert.equal(source.includes("Promise.resolve(platformSdkInternalBackend.ready())"), true);
});

test("loading shell does not rewrite DOM for progress inside the same whole percent", () => {
  const shell = readFileSync(join(HERE, "../../../templates/template/web/index.html.in"), "utf8");
  const start = shell.indexOf("(function () {");
  const end = shell.indexOf("window.__PLATFORM_SDK_CONFIG__", start);
  assert.notEqual(start, -1);
  assert.notEqual(end, -1);

  const writes = [];
  function element(id) {
    const value = {
      classList: { add() {} }, style: {}, hidden: true,
      addEventListener() {}, setAttribute() {}, removeAttribute() {}, focus() {},
    };
    value.style = new Proxy({}, {
      set(target, property, next) {
        writes.push(`${id}.style.${String(property)}=${next}`);
        target[property] = next;
        return true;
      },
    });
    Object.defineProperty(value, "textContent", {
      get() {
        return "";
      },
      set(next) {
        writes.push(`${id}.textContent=${next}`);
      },
    });
    return value;
  }
  const elements = new Map([
    ["loading-overlay", element("loading-overlay")],
    ["loading-label", element("loading-label")],
    ["loading-percent", element("loading-percent")],
    ["loading-bar", element("loading-bar")],
    ["runtime-overlay", element("runtime-overlay")],
    ["runtime-title", element("runtime-title")],
    ["runtime-message", element("runtime-message")],
    ["runtime-reload", element("runtime-reload")],
    ["canvas", element("canvas")],
  ]);
  const window = {
    matchMedia() {
      return { matches: false };
    },
    addEventListener() {},
  };
  runInNewContext(`${shell.slice(start, end)}\n}());`, {
    document: {
      activeElement: null,
      getElementById(id) {
        return elements.get(id) || null;
      },
      hasFocus() {
        return true;
      },
    },
    window,
  });

  writes.length = 0;
  window.__platformSdkSetLoadingProgress(0.101);
  writes.length = 0;
  window.__platformSdkSetLoadingProgress(0.109);

  assert.deepEqual(writes, []);
});

test("template web runtime keeps the installed web backend for local mock", () => {
  const debugUi = readFileSync(
    join(HERE, "../../../templates/template/src/ui/platform_sdk_debug.c"),
    "utf8",
  );

  assert.equal(
    debugUi.includes(
      "#if PLATFORM_SDK_TARGET_ID == PLATFORM_SDK_TEMPLATE_TARGET_LOCAL && !defined(__EMSCRIPTEN__)",
    ),
    true,
  );
});

test("production staged artifacts exclude debug labels and unused SDK URLs", () => {
  const dir = mkdtempSync(join(tmpdir(), "platform-sdk-artifact-"));
  try {
    stagePlatformSdkWebAssets({ target: TargetPlatform.LOCAL, outDir: dir, debugUi: false });
    assert.deepEqual(inspectPlatformSdkArtifact({ target: TargetPlatform.LOCAL, artifactDir: dir }), {
      ok: true,
      violations: [],
    });

    stagePlatformSdkWebAssets({ target: TargetPlatform.POKI, outDir: dir, debugUi: false });
    assert.deepEqual(inspectPlatformSdkArtifact({ target: TargetPlatform.POKI, artifactDir: dir }), {
      ok: true,
      violations: [],
    });
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("staged web SDK uses only composition and selected adapter modules", () => {
  const dir = mkdtempSync(join(tmpdir(), "platform-sdk-two-modules-"));
  try {
    stagePlatformSdkWebAssets({ target: TargetPlatform.POKI, outDir: dir });
    assert.equal(existsSync(join(dir, "platform-sdk.js")), true);
    assert.equal(existsSync(join(dir, "platform-sdk-adapter.js")), true);
    assert.equal(existsSync(join(dir, "platform-sdk-core.js")), false);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("release SDK bundles are minified without changing adapter startup", () => {
  for (const adapter of ["mock", "poki", "yandex", "playgama"]) {
    const source = Buffer.from(packagedPlatformPrefix(adapter));
    const release = platformSdkBundlePrefix(adapter);
    assert.ok(release.length < source.length * 0.75, adapter);
    assert.ok(release.toString("utf8").split("\n").length <= 2, adapter);

    const context = { clearTimeout, console, globalThis: null, Promise, setTimeout };
    context.globalThis = context;
    runInNewContext(`${release}globalThis.__releaseBundleReady = !!globalThis.__platformSdkInternalBackend;`, context);
    assert.equal(context.__releaseBundleReady, true, adapter);
    assert.deepEqual(
      Object.keys(context.__platformSdkInternalBackend).sort(),
      [...PLATFORM_BACKEND_METHODS].sort(),
      adapter,
    );
  }
});

test("publish manifests distinguish staged modules from single-JS release packages", () => {
  for (const target of [TargetPlatform.ITCH, TargetPlatform.POKI, TargetPlatform.YANDEX, TargetPlatform.PLAYGAMA]) {
    const manifest = JSON.parse(readFileSync(join(HERE, `../publish-targets/${target}.json`), "utf8"));
    assert.equal(manifest.required_files.includes("platform-sdk.js"), true, target);
    assert.equal(manifest.packaged_required_files.includes("game.js"), true, target);
    assert.equal(manifest.packaged_required_files.some((path) => path.startsWith("platform-sdk")), false, target);
  }
});

test("artifact inspection accepts a package with the exact bundled Poki backend", () => {
  const dir = mkdtempSync(join(tmpdir(), "platform-sdk-packaged-"));
  try {
    const manifest = JSON.parse(readFileSync(join(HERE, "../publish-targets/poki.json"), "utf8"));
    for (const path of manifest.packaged_required_files) {
      const full = join(dir, path);
      mkdirSync(dirname(full), { recursive: true });
      writeFileSync(full, path === "game.js"
        ? `${platformSdkBundlePrefix("poki")}var wasmBinaryFile = 'game.wasm';\n`
        : "fixture");
    }
    assert.deepEqual(inspectPlatformSdkArtifact({
      target: TargetPlatform.POKI,
      artifactDir: dir,
      production: true,
      requireFiles: true,
    }), { ok: true, violations: [] });
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("artifact inspection rejects marker-only and partial platform SDK layouts", () => {
  for (const [label, mutate] of [
    ["marker-only bundle", () => {}],
    ["partial staged modules", (dir) => writeFileSync(join(dir, "platform-sdk.js"), "fixture")],
    ["bundled loader with staged modules", (dir) => {
      writeFileSync(join(dir, "game.js"), `${platformSdkBundlePrefix("poki")}var wasmBinaryFile = 'game.wasm';\n`);
      for (const [from, to] of [
        ["../web/platform-sdk.js", "platform-sdk.js"],
        ["../web/adapters/poki.js", "platform-sdk-adapter.js"],
      ]) {
        writeFileSync(join(dir, to), readFileSync(join(HERE, from)));
      }
    }],
  ]) {
    const dir = mkdtempSync(join(tmpdir(), "platform-sdk-invalid-package-"));
    try {
      const manifest = JSON.parse(readFileSync(join(HERE, "../publish-targets/poki.json"), "utf8"));
      for (const path of manifest.packaged_required_files) {
        const full = join(dir, path);
        mkdirSync(dirname(full), { recursive: true });
        writeFileSync(full, path === "game.js"
          ? "https://game-cdn.poki.com/scripts/v2/poki-sdk.js PokiSDK"
          : "fixture");
      }
      mutate(dir);
      const result = inspectPlatformSdkArtifact({
        target: TargetPlatform.POKI,
        artifactDir: dir,
        production: true,
        requireFiles: true,
      });
      assert.equal(result.ok, false, label);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  }
});

test("debug UI is owned by C and is not staged as a JS artifact", () => {
  const dir = mkdtempSync(join(tmpdir(), "platform-sdk-debug-ui-"));
  try {
    stagePlatformSdkWebAssets({ target: TargetPlatform.LOCAL, outDir: dir, debugUi: true });
    assert.equal(existsSync(join(dir, "platform-sdk-debug-ui.js")), false);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("scorecard summarizes local NDJSON without a portal account", () => {
  const scorecard = scorecardFromNdjson(`
{"schema":"analytics.v1","kind":"header","started_at":1000}
{"seq":0,"tick":1,"type":"game.loading_finished","time_ms":2000}
{"seq":1,"tick":2,"type":"gameplay.start","time_ms":3000}
{"seq":2,"tick":3,"type":"items.txn","op":"add","def_id":"tmpl.xp","container_id":"2","entry_id":"7","requested_delta":"8","applied_delta":"8","before_count":"0","after_count":"8","reason":"loot:demo_idle","time_ms":10000}
{"seq":3,"tick":4,"type":"ad.rewarded.request","placement":"double_reward","time_ms":20000}
{"seq":4,"tick":4,"type":"ad.rewarded.result","supported":true,"shown":true,"rewarded":true,"placement":"double_reward","reason":"completed","time_ms":22000}
{"seq":5,"tick":60,"type":"first_60s.complete","time_ms":59000}
{"seq":6,"tick":75,"type":"gameplay.stop","time_ms":76000}
`);

  assert.equal(scorecard.first60sCompletion, true);
  assert.equal(scorecard.sessionLengthSec, 75);
  assert.equal(scorecard.rewardOrUpgradeInteraction, true);
  assert.equal(scorecard.adBreakOpportunity, true);
  assert.equal(scorecard.continueKillRecommendation, "continue");
});
