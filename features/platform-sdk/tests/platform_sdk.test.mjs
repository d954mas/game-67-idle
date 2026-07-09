import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { createPlatformSdkWebBackend } from "../web/platform-sdk-core.js";
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

const HERE = dirname(fileURLToPath(import.meta.url));
const TargetPlatform = Object.freeze({
  LOCAL: "local",
  ITCH: "itch",
  POKI: "poki",
  YANDEX: "yandex",
  PLAYGAMA: "playgama",
});

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
  const backend = createPlatformSdkWebBackend({
    adapterFactory: createMockPlatformAdapter,
    config: { target },
    host,
  });
  return { backend, host };
}

test("build tooling maps publish targets to exactly one platform SDK adapter", () => {
  assert.equal(sdkForTarget(TargetPlatform.LOCAL), "mock");
  assert.equal(sdkForTarget(TargetPlatform.ITCH), "mock");
  assert.equal(sdkForTarget(TargetPlatform.POKI), "poki");
  assert.equal(sdkForTarget(TargetPlatform.YANDEX), "yandex");
  assert.equal(sdkForTarget(TargetPlatform.PLAYGAMA), "playgama");
});

test("template CMake isolates web presets by publish target", () => {
  const cmake = readFileSync(join(HERE, "../../../templates/template/CMakeLists.txt"), "utf8");

  assert.match(cmake, /set\(GAME_PUBLISH_TARGET "local" CACHE STRING/);
  assert.match(cmake, /if\(EMSCRIPTEN AND NOT GAME_PUBLISH_TARGET STREQUAL "local"\)/);
  assert.match(cmake, /set\(NT_PRESET_NAME "\$\{NT_PRESET_NAME\}-\$\{GAME_PUBLISH_TARGET\}"\)/);
  assert.equal(cmake.includes("GAME_BUILD_VARIANT"), false);
});

test("web builds use a checkout-local Emscripten cache by default", () => {
  const cmake = readFileSync(join(HERE, "../../../templates/template/CMakeLists.txt"), "utf8");
  const buildWeb = readFileSync(join(HERE, "../../../templates/template/tools/build_web.sh"), "utf8");

  assert.match(cmake, /set\(GAME_EMSCRIPTEN_CACHE_DIR "\$\{_game_default_em_cache\}" CACHE PATH/);
  assert.match(cmake, /RULE_LAUNCH_COMPILE "\$\{_game_emcache_launcher\}"/);
  assert.match(cmake, /RULE_LAUNCH_LINK "\$\{_game_emcache_launcher\}"/);
  assert.match(buildWeb, /export EM_CACHE="\$GAME_DIR\/build\/emscripten-cache"/);
});

test("web backend exposes only thin adapter methods and lifecycle calls stay direct", async () => {
  const host = createHost(TargetPlatform.LOCAL);
  const calls = [];
  const backend = createPlatformSdkWebBackend({
    adapterFactory: () => ({
      ready() {
        calls.push("ready");
        return true;
      },
      gameLoadingFinished() {
        calls.push("gameLoadingFinished");
      },
      gameReady() {
        calls.push("gameReady");
      },
    }),
    config: { target: TargetPlatform.LOCAL, platformSdk: "mock" },
    host,
  });

  assert.equal(Object.hasOwn(backend, "whenReady"), false);
  assert.equal(Object.hasOwn(backend, "onPause"), false);
  assert.equal(Object.hasOwn(backend, "onResume"), false);
  assert.equal(Object.hasOwn(backend, "getRuntimeState"), false);

  assert.equal(await backend.ready(), true);
  assert.equal(await backend.ready(), true);
  await backend.gameLoadingFinished();
  await backend.gameLoadingFinished();
  await backend.gameReady();
  await backend.gameReady();

  assert.deepEqual(calls, ["ready", "gameLoadingFinished", "gameLoadingFinished", "gameReady", "gameReady"]);
  assert.equal(Object.hasOwn(backend, "track"), false);
  assert.equal(Object.hasOwn(host, "__platformSdkEvents"), false);
});

test("web backend reports selected SDK readiness without owning boot policy", async () => {
  const host = createHost(TargetPlatform.POKI);
  const calls = [];
  const backend = createPlatformSdkWebBackend({
    adapterFactory: () => ({
      ready() {
        calls.push("ready");
        return false;
      },
    }),
    config: { target: TargetPlatform.POKI, platformSdk: "poki" },
    host,
  });

  assert.equal(await backend.ready(), false);
  assert.equal(await backend.ready(), false);
  assert.deepEqual(calls, ["ready"]);
  assert.equal(Object.hasOwn(host, "__platformSdkEvents"), false);
});

test("web backend forwards loading progress without analytics events", async () => {
  const host = createHost(TargetPlatform.POKI);
  const progress = [];
  const backend = createPlatformSdkWebBackend({
    adapterFactory: () => ({
      gameLoadingProgress(value) {
        progress.push(value);
      },
    }),
    config: { target: TargetPlatform.POKI, platformSdk: "poki" },
    host,
  });

  await backend.gameLoadingProgress(0.35);

  assert.deepEqual(progress, [0.35]);
  assert.equal(Object.hasOwn(host, "__platformSdkEvents"), false);
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
  assert.deepEqual(progress, []);

  await Promise.resolve();
  await Promise.resolve();
  resolveInit();
  await Promise.all([p1, p2, p3]);
  assert.deepEqual(progress, [1]);

  await adapter.gameLoadingProgress(0.75);
  await adapter.gameLoadingProgress(1.0);
  assert.deepEqual(progress, [1]);
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

test("web backend delegates gameplay calls without owning input state or guards", async () => {
  const host = createHost(TargetPlatform.LOCAL);
  let starts = 0;
  let stops = 0;
  const backend = createPlatformSdkWebBackend({
    adapterFactory: () => ({
      gameplayStart() {
        starts += 1;
      },
      gameplayStop() {
        stops += 1;
      },
    }),
    config: { target: TargetPlatform.LOCAL, platformSdk: "mock" },
    host,
  });

  assert.equal(Object.hasOwn(backend, "hasInput"), false);
  assert.equal(Object.hasOwn(backend, "hasGameplayStarted"), false);
  assert.equal(Object.hasOwn(backend, "markInput"), false);

  await backend.gameplayStart({ source: "test" });
  await backend.gameplayStart({ source: "duplicate" });
  assert.equal(starts, 2);
  assert.equal(host.warnings.length, 0);

  await backend.gameplayStop({ source: "test" });
  assert.equal(stops, 1);
  assert.equal(Object.hasOwn(host, "__platformSdkEvents"), false);
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

test("thin web backend can forward callbacks only when an adapter provides them", async () => {
  const host = createHost(TargetPlatform.LOCAL);
  let visibilityChanges = 0;
  const backend = createPlatformSdkWebBackend({
    adapterFactory: ({ emitVisibilityChange }) => ({
      destroy() {},
      ready() {
        emitVisibilityChange(true);
        return true;
      },
    }),
    config: { target: TargetPlatform.LOCAL, platformSdk: "mock" },
    host,
    callbacks: {
      onVisibilityChange(hidden) {
        assert.equal(hidden, true);
        visibilityChanges += 1;
      },
    },
  });

  await backend.ready();
  backend.destroy();
  await backend.ready();
  assert.equal(visibilityChanges, 1);
});

test("local mock visibility listener is removed on destroy", () => {
  const host = createHost(TargetPlatform.LOCAL);
  let visibilityChanges = 0;
  const backend = createPlatformSdkWebBackend({
    adapterFactory: createMockPlatformAdapter,
    config: { target: TargetPlatform.LOCAL, platformSdk: "mock" },
    host,
    callbacks: {
      onVisibilityChange() {
        visibilityChanges += 1;
      },
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
  const core = readFileSync(join(HERE, "../web/platform-sdk-core.js"), "utf8");
  const mock = readFileSync(join(HERE, "../web/adapters/mock.js"), "utf8");
  const poki = readFileSync(join(HERE, "../web/adapters/poki.js"), "utf8");

  assert.equal(source.includes("globalThis.PlatformSdk"), false);
  assert.equal(source.includes("PlatformSdkReady"), false);
  assert.equal(source.includes("__platformSdkWebBackend"), false);
  assert.equal(source.includes("platform-sdk-web-backend-ready"), false);
  assert.equal(source.includes("CustomEvent"), false);
  assert.equal(core.includes("TargetPlatform"), false);
  assert.equal(core.includes("PlatformSdkId"), false);
  assert.equal(core.includes("TARGET_TO_SDK"), false);
  assert.equal(core.includes("resolvePlatformSdk"), false);
  assert.equal(core.includes("pauseCallbacks"), false);
  assert.equal(core.includes("resumeCallbacks"), false);
  assert.equal(core.includes("gameplayActive"), false);
  assert.equal(mock.includes("createOverlay"), false);
  assert.equal(mock.includes("platformSdkOverlay"), false);
  assert.equal(poki.includes("gameplayActive"), false);
  assert.equal(poki.includes("sdk.gameLoadingProgress({ percentageDone"), true);
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
  assert.equal(source.includes("platformSdkInternalBackend.ready()"), true);
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
{"seq":2,"tick":3,"type":"items.txn","op":"add","def_id":"tmpl.xp","container":"purse","entry_key":"purse/tmpl.xp","requested_delta":"8","applied_delta":"8","before_count":"0","after_count":"8","reason":"loot:demo_idle","time_ms":10000}
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
