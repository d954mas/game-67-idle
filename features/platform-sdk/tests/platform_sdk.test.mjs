import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { runInNewContext } from "node:vm";

import { createMockPlatformAdapter } from "../web/adapters/mock.js";
import { createPlaygamaPlatformAdapter } from "../web/adapters/playgama.js";
import { createPokiPlatformAdapter } from "../web/adapters/poki.js";
import { createYandexPlatformAdapter } from "../web/adapters/yandex.js";
import {
  inspectPlatformSdkArtifact,
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
});
const PLATFORM_BACKEND_METHODS = Object.freeze([
  "destroy",
  "gameLoadingProgress",
  "gameLoadingFinished",
  "gameReady",
  "gameplayStart",
  "gameplayStop",
  "getLocale",
  "hideBanner",
  "loadData",
  "measure",
  "ready",
  "saveData",
  "showBanner",
  "showInterstitial",
  "showRewarded",
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
    "message:level_pause",
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
    const value = { classList: { add() {} }, style: {} };
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
    ["canvas", { focus() {} }],
  ]);
  const window = {
    matchMedia() {
      return { matches: false };
    },
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
        ? `${packagedPlatformPrefix("poki")}var wasmBinaryFile = 'game.wasm';\n`
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
      writeFileSync(join(dir, "game.js"), `${packagedPlatformPrefix("poki")}var wasmBinaryFile = 'game.wasm';\n`);
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
