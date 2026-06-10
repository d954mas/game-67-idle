#!/usr/bin/env node
import { createServer } from 'node:http';
import { spawn } from 'node:child_process';
import { createReadStream, openSync } from 'node:fs';
import { mkdir, readFile, rm, stat, writeFile } from 'node:fs/promises';
import { inflateSync } from 'node:zlib';
import { dirname, extname, isAbsolute, join, normalize, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = dirname(fileURLToPath(import.meta.url));
const root = resolve(scriptDir, '../../..');

const options = parseArgs(process.argv.slice(2));
const webRoot = resolve(root, options.webRoot ?? 'build/game_67_idle/wasm-qa');
const iteration = options.iteration ?? (options.iteration7 === 'true' ? '7' : null);
const outRoot = resolve(root, options.outRoot ?? (iteration ? `build/captures/iteration${iteration}` : 'build/captures/iteration6'));
const httpPort = Number(options.httpPort ?? 8076);
const cdpPort = Number(options.cdpPort ?? 9336);
const waitMs = Number(options.waitMs ?? 3500);
const actionWaitMs = Number(options.actionWaitMs ?? 300);
const jobWaitMs = Number(options.jobWaitMs ?? 6500);
const lifecycleWaitMs = Number(options.lifecycleWaitMs ?? 1000);
const captureProgressed = !['0', 'false', 'no'].includes(String(options.progressed ?? options.progress ?? 'true').toLowerCase());
const checkLifecycle = ['1', 'true', 'yes'].includes(String(options.lifecycle ?? 'false').toLowerCase());
const url = `http://127.0.0.1:${httpPort}/index.html`;
const origin = `http://127.0.0.1:${httpPort}`;
const profileDir = resolve(root, `tmp/web-visual-qa-chrome-profile-${Date.now()}`);

const requiredArtifacts = [
  'index.html',
  'index.js',
  'index.wasm',
  'assets/game_67_idle.ntpack',
];

const viewports = [
  {
    name: 'desktop',
    metrics: { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false },
  },
  {
    name: 'mobile_portrait',
    metrics: { width: 390, height: 844, deviceScaleFactor: 1, mobile: true },
  },
];

const mime = new Map([
  ['.html', 'text/html; charset=utf-8'],
  ['.js', 'text/javascript; charset=utf-8'],
  ['.wasm', 'application/wasm'],
  ['.png', 'image/png'],
  ['.jpg', 'image/jpeg'],
  ['.jpeg', 'image/jpeg'],
  ['.ntpack', 'application/octet-stream'],
]);

function parseArgs(args) {
  const parsed = {};
  for (let i = 0; i < args.length; i += 1) {
    const arg = args[i];
    if (!arg.startsWith('--')) {
      throw new Error(`Unexpected argument: ${arg}`);
    }
    const eq = arg.indexOf('=');
    if (eq >= 0) {
      parsed[arg.slice(2, eq)] = arg.slice(eq + 1);
      continue;
    }
    const key = arg.slice(2);
    const value = args[i + 1];
    if (!value || value.startsWith('--')) {
      parsed[key] = 'true';
    } else {
      parsed[key] = value;
      i += 1;
    }
  }
  return parsed;
}

async function exists(path) {
  try {
    await stat(path);
    return true;
  } catch {
    return false;
  }
}

async function findChrome() {
  if (options.chrome) return options.chrome;
  if (process.env.CHROME_PATH) return process.env.CHROME_PATH;

  const candidates = [
    'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
    'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
    'chrome.exe',
    'chromium.exe',
    'msedge.exe',
    'google-chrome',
    'chromium',
    'chromium-browser',
  ];

  for (const candidate of candidates) {
    if (!isAbsolute(candidate)) return candidate;
    if (await exists(candidate)) return candidate;
  }

  throw new Error('Chrome/Chromium was not found. Set CHROME_PATH or pass --chrome <path>.');
}

function sleep(ms) {
  return new Promise((resolveSleep) => setTimeout(resolveSleep, ms));
}

function checkStaticPath(requestPath) {
  const decoded = decodeURIComponent(requestPath);
  const relativeRequest = decoded === '/' ? '/index.html' : decoded;
  const filePath = normalize(join(webRoot, relativeRequest));
  const rel = relative(webRoot, filePath);
  if (rel.startsWith('..') || isAbsolute(rel)) return null;
  return filePath;
}

async function startServer() {
  const server = createServer(async (req, res) => {
    try {
      const requestPath = new URL(req.url ?? '/', url).pathname;
      const filePath = checkStaticPath(requestPath);
      if (!filePath) {
        res.writeHead(403);
        res.end('forbidden');
        return;
      }
      await stat(filePath);
      res.writeHead(200, { 'content-type': mime.get(extname(filePath)) ?? 'application/octet-stream' });
      createReadStream(filePath).pipe(res);
    } catch {
      res.writeHead(404);
      res.end('not found');
    }
  });

  await new Promise((resolveListen) => server.listen(httpPort, '127.0.0.1', resolveListen));
  return server;
}

function spawnChrome(chrome) {
  return spawn(chrome, [
    '--headless=new',
    '--no-sandbox',
    '--disable-background-networking',
    '--disable-dev-shm-usage',
    '--disable-gpu',
    '--disable-sync',
    '--metrics-recording-only',
    '--no-first-run',
    '--no-default-browser-check',
    '--remote-allow-origins=*',
    `--remote-debugging-port=${cdpPort}`,
    `--user-data-dir=${profileDir}`,
    'about:blank',
  ], {
    cwd: root,
    stdio: [
      'ignore',
      openSync(join(outRoot, 'chrome_stdout.txt'), 'w'),
      openSync(join(outRoot, 'chrome_stderr.txt'), 'w'),
    ],
    windowsHide: true,
  });
}

async function getPageTarget(chromeProc) {
  for (let i = 0; i < 100; i += 1) {
    if (chromeProc.exitCode !== null) {
      throw new Error(`Chrome exited early with code ${chromeProc.exitCode}`);
    }
    try {
      const res = await fetch(`http://127.0.0.1:${cdpPort}/json/list`);
      if (res.ok) {
        const targets = await res.json();
        const page = targets.find((target) => target.type === 'page');
        if (page?.webSocketDebuggerUrl) return page;
      }
    } catch {
      await sleep(100);
    }
  }
  throw new Error('Chrome CDP did not start');
}

async function connectCdp(chromeProc) {
  if (typeof WebSocket === 'undefined') {
    throw new Error('This script requires a Node runtime with global WebSocket support.');
  }

  const pageTarget = await getPageTarget(chromeProc);
  const debuggerUrl = pageTarget.webSocketDebuggerUrl.replace('ws://localhost:', 'ws://127.0.0.1:');
  const ws = new WebSocket(debuggerUrl);
  await new Promise((resolveOpen, rejectOpen) => {
    ws.addEventListener('open', resolveOpen, { once: true });
    ws.addEventListener('error', () => rejectOpen(new Error('WebSocket open failed')), { once: true });
  });

  let id = 0;
  const pending = new Map();
  const events = [];
  ws.addEventListener('message', async (event) => {
    let raw = event.data;
    if (raw instanceof ArrayBuffer) raw = Buffer.from(raw).toString('utf8');
    else if (ArrayBuffer.isView(raw)) raw = Buffer.from(raw.buffer, raw.byteOffset, raw.byteLength).toString('utf8');
    else if (typeof raw?.text === 'function') raw = await raw.text();
    else raw = String(raw);

    const message = JSON.parse(raw);
    if (message.id && pending.has(message.id)) {
      const { resolve: resolvePending, reject } = pending.get(message.id);
      pending.delete(message.id);
      if (message.error) reject(new Error(message.error.message));
      else resolvePending(message.result);
      return;
    }
    events.push(message);
  });

  function send(method, params = {}) {
    id += 1;
    ws.send(JSON.stringify({ id, method, params }));
    return new Promise((resolveSend, rejectSend) => {
      const timer = setTimeout(() => {
        pending.delete(id);
        rejectSend(new Error(`CDP timeout: ${method}`));
      }, 15000);
      pending.set(id, {
        resolve: (value) => {
          clearTimeout(timer);
          resolveSend(value);
        },
        reject: (error) => {
          clearTimeout(timer);
          rejectSend(error);
        },
      });
    });
  }

  return { ws, send, events };
}

async function waitForCanvas(send) {
  for (let i = 0; i < 60; i += 1) {
    const result = await send('Runtime.evaluate', {
      returnByValue: true,
      expression: `(() => {
        const canvas = document.querySelector('canvas');
        return Boolean(canvas && canvas.clientWidth > 0 && canvas.clientHeight > 0);
      })()`,
    });
    if (result.result.value) return;
    await sleep(100);
  }
}

async function navigateViewport(send, viewport) {
  await send('Page.enable');
  await send('Runtime.enable');
  await send('Log.enable');
  await send('Emulation.setDeviceMetricsOverride', viewport.metrics);
  await send('Page.navigate', { url });
  await sleep(waitMs);
  await waitForCanvas(send);
}

async function readLayout(send) {
  const layoutResult = await send('Runtime.evaluate', {
    returnByValue: true,
    expression: `(() => {
      const canvas = document.querySelector('canvas');
      const html = document.documentElement;
      const body = document.body;
      const rect = canvas?.getBoundingClientRect();
      const spinner = document.querySelector('#spinner');
      const crash = document.querySelector('#crash-overlay');
      const pageScrollWidth = Math.max(html?.scrollWidth ?? 0, body?.scrollWidth ?? 0);
      return {
        href: location.href,
        title: document.title,
        innerWidth: window.innerWidth,
        innerHeight: window.innerHeight,
        devicePixelRatio: window.devicePixelRatio,
        pageScrollWidth,
        pageScrollHeight: Math.max(html?.scrollHeight ?? 0, body?.scrollHeight ?? 0),
        bodyText: body?.innerText ?? '',
        canvasCount: document.querySelectorAll('canvas').length,
        canvasClientWidth: canvas?.clientWidth ?? 0,
        canvasClientHeight: canvas?.clientHeight ?? 0,
        canvasWidth: canvas?.width ?? 0,
        canvasHeight: canvas?.height ?? 0,
        canvasLeft: rect?.left ?? null,
        canvasRight: rect?.right ?? null,
        canvasTop: rect?.top ?? null,
        canvasBottom: rect?.bottom ?? null,
        spinnerDisplay: spinner ? getComputedStyle(spinner).display : null,
        crashDisplay: crash ? getComputedStyle(crash).display : null,
        crashText: crash?.textContent ?? '',
        moduleCalledRun: Boolean(window.Module?.calledRun),
      };
    })()`,
  });
  return layoutResult.result.value;
}

async function readStorageSummary(send) {
  const result = await send('Runtime.evaluate', {
    returnByValue: true,
    expression: `(() => {
      const entries = {};
      for (let i = 0; i < localStorage.length; i += 1) {
        const key = localStorage.key(i);
        entries[key] = localStorage.getItem(key);
      }
      const parsed = {};
      for (const [key, value] of Object.entries(entries)) {
        try {
          parsed[key] = JSON.parse(value);
        } catch {
          parsed[key] = null;
        }
      }
      const qaState = window.__game67QaState ?? parsed['game_67_idle.qa_state']?.state ?? null;
      const gameEntry = Object.entries(parsed).find(([, value]) => value?.schema === 'game_67_idle.state' && value?.state);
      const state = qaState ?? gameEntry?.[1]?.state ?? null;
      return {
        keys: Object.keys(entries).sort(),
        gameStateKey: qaState ? 'window.__game67QaState' : (gameEntry?.[0] ?? null),
        state: state ? {
          meme_coins: state.meme_coins,
          status: state.status,
          click_power: state.click_power,
          first_upgrade_owned: state.first_upgrade_owned,
          active_job_id: state.active_job_id,
          active_job_elapsed_ms: state.active_job_elapsed_ms,
          active_job_duration_ms: state.active_job_duration_ms,
          income_per_second: state.income_per_second,
          comfort: state.comfort,
          visual_stage: state.visual_stage,
          feedback_code: state.feedback_code,
          first_job_active: state.first_job_active,
          first_job_ready: state.first_job_ready,
        } : null,
      };
    })()`,
  });
  return result.result.value;
}

async function readLiveQaSnapshot(send) {
  const result = await send('Runtime.evaluate', {
    returnByValue: true,
    expression: `(() => {
      const api = window.Game67QA;
      if (api && typeof api.snapshot === 'function') {
        try {
          return api.snapshot();
        } catch (error) {
          return { error: String(error?.message ?? error) };
        }
      }
      if (window.__game67QaState) {
        return {
          schema: 'game_67_idle.qa_snapshot',
          gameSchema: 'game_67_idle.state',
          source: 'legacy-window-state',
          state: JSON.parse(JSON.stringify(window.__game67QaState)),
        };
      }
      return null;
    })()`,
  });
  return result.result.value;
}

async function readLifecycleState(send) {
  const result = await send('Runtime.evaluate', {
    returnByValue: true,
    expression: `(() => {
      const entries = {};
      for (let i = 0; i < localStorage.length; i += 1) {
        const key = localStorage.key(i);
        entries[key] = localStorage.getItem(key);
      }
      return {
        lifecycle: window.Game67Lifecycle ? JSON.parse(JSON.stringify(window.Game67Lifecycle)) : null,
        localStorageKeys: Object.keys(entries).sort(),
        autosavePresent: Object.prototype.hasOwnProperty.call(entries, 'game_67_idle.autosave.game'),
      };
    })()`,
  });
  return result.result.value;
}

async function readProgressionState(send) {
  const liveSnapshot = await readLiveQaSnapshot(send);
  if (liveSnapshot?.gameSchema === 'game_67_idle.state' && liveSnapshot?.state) {
    return {
      source: 'web-qa-snapshot',
      state: liveSnapshot.state,
      liveSnapshot,
      storage: null,
    };
  }

  const storage = await readStorageSummary(send);
  if (storage?.state) {
    return {
      source: 'localStorage',
      state: storage.state,
      liveSnapshot,
      storage,
    };
  }

  return {
    source: null,
    state: null,
    liveSnapshot,
    storage,
  };
}

async function captureCurrent(send, viewport, stateName, screenshotName, metadata = {}) {
  const layout = await readLayout(send);
  const screenshot = await send('Page.captureScreenshot', {
    format: 'png',
    fromSurface: true,
    captureBeyondViewport: false,
  });
  const screenshotBytes = Buffer.from(screenshot.data, 'base64');
  const screenshotPath = join(outRoot, screenshotName);
  await writeFile(screenshotPath, screenshotBytes);
  const png = analyzePng(screenshotBytes);
  const horizontalOverflow =
    layout.pageScrollWidth > layout.innerWidth + 1 ||
    (layout.canvasLeft !== null && layout.canvasLeft < -1) ||
    (layout.canvasRight !== null && layout.canvasRight > layout.innerWidth + 1);

  return {
    name: viewport.name,
    stateName,
    screenshotPath,
    screenshotBytes: screenshotBytes.length,
    png,
    layout,
    ...metadata,
    checks: {
      hasCanvas: layout.canvasCount > 0,
      canvasHasSize: layout.canvasClientWidth >= 32 && layout.canvasClientHeight >= 32,
      screenshotIsNotTiny: png.width >= viewport.metrics.width && png.height >= viewport.metrics.height && screenshotBytes.length >= 4096,
      screenshotIsNotBlank: !png.blank,
      noMobileHorizontalOverflow: viewport.metrics.mobile ? !horizontalOverflow : true,
      noCrashOverlay: layout.crashDisplay === null || layout.crashDisplay === 'none',
    },
  };
}

async function captureViewport(send, viewport) {
  await navigateViewport(send, viewport);
  return captureCurrent(send, viewport, 'initial', `wasm_qa_${viewport.name}.png`);
}

function gameplayRects(layout) {
  const fbW = layout.canvasWidth || layout.canvasClientWidth || layout.innerWidth;
  const fbH = layout.canvasHeight || layout.canvasClientHeight || layout.innerHeight;
  const margin = Math.max(18, fbW * 0.048);
  const bottomSafe = Math.max(28, fbH * 0.034);
  const tabH = Math.min(60, Math.max(54, fbH * 0.080));
  const cardH = Math.min(74, Math.max(66, fbH * 0.088));
  const gap = Math.max(10, fbH * 0.012);
  const buttonSize = Math.min(fbW * 0.53, fbH * 0.205);
  const cardW = fbW - (margin * 2);
  const cardY = fbH - bottomSafe - tabH - margin - (cardH * 2) - gap;
  let buttonY = fbH * 0.39;
  const maxButtonY = cardY - gap - buttonSize;
  if (buttonY > maxButtonY) buttonY = maxButtonY;
  if (buttonY < fbH * 0.35) buttonY = fbH * 0.35;

  return {
    do67: { x: (fbW - buttonSize) * 0.5, y: buttonY, w: buttonSize, h: buttonSize },
    upgrade: { x: margin, y: cardY, w: cardW, h: cardH },
    job: { x: margin, y: cardY + cardH + gap, w: cardW, h: cardH },
  };
}

function rectCenter(layout, rect) {
  const scaleX = layout.canvasClientWidth / (layout.canvasWidth || layout.canvasClientWidth || 1);
  const scaleY = layout.canvasClientHeight / (layout.canvasHeight || layout.canvasClientHeight || 1);
  return {
    x: (layout.canvasLeft ?? 0) + (rect.x + rect.w * 0.5) * scaleX,
    y: (layout.canvasTop ?? 0) + (rect.y + rect.h * 0.5) * scaleY,
  };
}

async function tapPoint(send, point) {
  await send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: point.x, y: point.y, button: 'none' });
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', x: point.x, y: point.y, button: 'left', buttons: 1, clickCount: 1 });
  await sleep(40);
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: point.x, y: point.y, button: 'left', buttons: 0, clickCount: 1 });
  await sleep(actionWaitMs);
}

async function dispatchPageLifecycle(send, eventName) {
  await send('Runtime.evaluate', {
    returnByValue: true,
    expression: `(() => {
      const event = new Event(${JSON.stringify(eventName)});
      if (${JSON.stringify(eventName)} === 'visibilitychange') {
        document.dispatchEvent(event);
      } else {
        window.dispatchEvent(event);
      }
      return true;
    })()`,
  });
}

async function clickGameplay(send, target) {
  const layout = await readLayout(send);
  const rects = gameplayRects(layout);
  const rect = rects[target];
  if (!rect) throw new Error(`Unknown gameplay target: ${target}`);
  const point = rectCenter(layout, rect);
  await tapPoint(send, point);
  return { target, point, rect };
}

async function runLifecyclePauseCheck(send, viewport) {
  if (!checkLifecycle) {
    return null;
  }
  const before = await readProgressionState(send);
  await dispatchPageLifecycle(send, 'blur');
  await sleep(lifecycleWaitMs);
  const during = await readProgressionState(send);
  const blurredLifecycle = await readLifecycleState(send);
  await dispatchPageLifecycle(send, 'focus');
  await sleep(actionWaitMs);
  const focusedLifecycle = await readLifecycleState(send);

  const beforeElapsed = Number(before.state?.active_job_elapsed_ms ?? 0);
  const duringElapsed = Number(during.state?.active_job_elapsed_ms ?? 0);
  const elapsedDelta = duringElapsed - beforeElapsed;
  const checks = {
    lifecycleInstalled: blurredLifecycle.lifecycle?.installed === true,
    blurPaused: blurredLifecycle.lifecycle?.paused === true,
    focusResumed: focusedLifecycle.lifecycle?.paused === false,
    jobTimerPaused: elapsedDelta <= 250,
    autosavePresentAfterBlur: blurredLifecycle.autosavePresent === true,
  };

  return {
    viewport: viewport.name,
    waitMs: lifecycleWaitMs,
    before,
    during,
    blurredLifecycle,
    focusedLifecycle,
    elapsedDelta,
    checks,
    warnings: Object.entries(checks)
      .filter(([, passed]) => !passed)
      .map(([name]) => `${viewport.name}.lifecycle.${name} failed`),
  };
}

async function clearWebStorage(send) {
  const attempts = [];
  try {
    await send('Storage.clearDataForOrigin', {
      origin,
      storageTypes: 'local_storage,indexeddb,websql,cache_storage,service_workers',
    });
    attempts.push({ method: 'Storage.clearDataForOrigin', ok: true });
  } catch (error) {
    attempts.push({ method: 'Storage.clearDataForOrigin', ok: false, error: error.message });
  }
  return attempts;
}

function stateMatches(state, expected) {
  if (!state) return false;
  return Object.entries(expected).every(([key, value]) => {
    if (typeof value === 'number') return Number(state[key]) === value;
    if (value && typeof value === 'object' && typeof value.min === 'number') {
      return Number(state[key]) >= value.min;
    }
    return state[key] === value;
  });
}

function progressionMetadata(storage, expectedState) {
  const actualStateAvailable = Boolean(storage.state);
  return {
    actualState: storage,
    storageState: storage.storage ?? storage,
    liveSnapshot: storage.liveSnapshot ?? null,
    expectedState,
    actualStateAvailable,
    progressionVerifiedBy: actualStateAvailable ? storage.source : 'visual-capture-only',
    progressionVerified: actualStateAvailable ? stateMatches(storage.state, expectedState) : null,
  };
}

async function runProgressedViewport(send, viewport) {
  const storageReset = await clearWebStorage(send);
  await navigateViewport(send, viewport);

  const captures = [];
  const actions = [];

  for (let i = 0; i < 5; i += 1) {
    actions.push(await clickGameplay(send, 'do67'));
  }
  await sleep(actionWaitMs);
  let storage = await readProgressionState(send);
  captures.push(await captureCurrent(
    send,
    viewport,
    'after_5_taps',
    `wasm_qa_${viewport.name}_after_5_taps.png`,
    progressionMetadata(storage, { meme_coins: 5, status: 1, first_upgrade_owned: false }),
  ));

  actions.push(await clickGameplay(send, 'upgrade'));
  await sleep(actionWaitMs);
  storage = await readProgressionState(send);
  captures.push(await captureCurrent(
    send,
    viewport,
    'after_first_upgrade',
    `wasm_qa_${viewport.name}_after_first_upgrade.png`,
    progressionMetadata(storage, { meme_coins: 0, status: 2, first_upgrade_owned: true, click_power: 2 }),
  ));

  actions.push(await clickGameplay(send, 'job'));
  const lifecycleCheckResult = await runLifecyclePauseCheck(send, viewport);
  await sleep(jobWaitMs);
  actions.push(await clickGameplay(send, 'job'));
  await sleep(actionWaitMs);
  storage = await readProgressionState(send);
  captures.push(await captureCurrent(
    send,
    viewport,
    'after_job_claim',
    `wasm_qa_${viewport.name}_after_job_claim.png`,
    progressionMetadata(storage, { meme_coins: 8, status: 3, first_upgrade_owned: true, income_per_second: 1, comfort: 2 }),
  ));

  const warnings = captures
    .filter((capture) => capture.progressionVerified === false)
    .map((capture) => `${viewport.name}.${capture.stateName} did not match expected ${capture.progressionVerifiedBy} summary`);
  if (lifecycleCheckResult) {
    warnings.push(...lifecycleCheckResult.warnings);
  }
  const todos = captures
    .filter((capture) => capture.progressionVerified === null)
    .map((capture) => `${viewport.name}.${capture.stateName}: visual capture exists, but web QA snapshot/localStorage verification was unavailable.`);

  return {
    viewport: viewport.name,
    method: 'web-cdp-canvas-clicks',
    storageReset,
    actionWaitMs,
    jobWaitMs,
    actions,
    lifecycleCheck: lifecycleCheckResult,
    captures,
    warnings,
    todos,
  };
}

async function runReloadLifecycleScenario(send, viewport) {
  if (!checkLifecycle) {
    return null;
  }
  const storageReset = await clearWebStorage(send);
  await navigateViewport(send, viewport);

  const actions = [];
  for (let i = 0; i < 5; i += 1) {
    actions.push(await clickGameplay(send, 'do67'));
  }
  await sleep(actionWaitMs);
  actions.push(await clickGameplay(send, 'upgrade'));
  await sleep(actionWaitMs);

  const beforePagehide = await readProgressionState(send);
  await dispatchPageLifecycle(send, 'pagehide');
  await sleep(actionWaitMs);
  const afterPagehideLifecycle = await readLifecycleState(send);

  await send('Page.reload', { ignoreCache: true });
  await sleep(waitMs);
  await waitForCanvas(send);
  await sleep(actionWaitMs);
  const afterReload = await readProgressionState(send);

  const expectedReloadState = {
    meme_coins: 0,
    status: 2,
    first_upgrade_owned: true,
    click_power: 2,
  };
  const pagehideEvents = afterPagehideLifecycle.lifecycle?.events ?? [];
  const checks = {
    beforePagehideStateReady: stateMatches(beforePagehide.state, expectedReloadState),
    lifecycleInstalled: afterPagehideLifecycle.lifecycle?.installed === true,
    pagehideFlushEvent: pagehideEvents.some((event) => event.source === 'pagehide' && event.flush === true),
    autosavePresentAfterPagehide: afterPagehideLifecycle.autosavePresent === true,
    reloadRestoredState: stateMatches(afterReload.state, expectedReloadState),
  };

  return {
    viewport: viewport.name,
    method: 'web-cdp-pagehide-reload',
    storageReset,
    actions,
    beforePagehide,
    afterPagehideLifecycle,
    afterReload,
    expectedReloadState,
    checks,
    warnings: Object.entries(checks)
      .filter(([, passed]) => !passed)
      .map(([name]) => `${viewport.name}.reloadLifecycle.${name} failed`),
  };
}

function analyzePng(buffer) {
  const signature = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
  if (buffer.length < 33 || !buffer.subarray(0, 8).equals(signature)) {
    throw new Error('Screenshot is not a PNG.');
  }

  let offset = 8;
  let width = 0;
  let height = 0;
  let bitDepth = 0;
  let colorType = 0;
  const idat = [];

  while (offset + 8 <= buffer.length) {
    const length = buffer.readUInt32BE(offset);
    const type = buffer.subarray(offset + 4, offset + 8).toString('ascii');
    const data = buffer.subarray(offset + 8, offset + 8 + length);
    if (type === 'IHDR') {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      bitDepth = data[8];
      colorType = data[9];
    } else if (type === 'IDAT') {
      idat.push(data);
    } else if (type === 'IEND') {
      break;
    }
    offset += 12 + length;
  }

  if (bitDepth !== 8) throw new Error(`Unsupported PNG bit depth: ${bitDepth}`);

  const bppByColorType = new Map([
    [0, 1],
    [2, 3],
    [4, 2],
    [6, 4],
  ]);
  const bytesPerPixel = bppByColorType.get(colorType);
  if (!bytesPerPixel) throw new Error(`Unsupported PNG color type: ${colorType}`);

  const inflated = inflateSync(Buffer.concat(idat));
  const stride = width * bytesPerPixel;
  let inputOffset = 0;
  let previous = Buffer.alloc(stride);
  let unique = new Set();
  let count = 0;
  let mean = 0;
  let m2 = 0;

  for (let y = 0; y < height; y += 1) {
    const filter = inflated[inputOffset];
    inputOffset += 1;
    const row = Buffer.from(inflated.subarray(inputOffset, inputOffset + stride));
    inputOffset += stride;
    defilter(row, previous, bytesPerPixel, filter);

    const sampleEvery = Math.max(1, Math.floor(width * height / 60000));
    for (let x = 0; x < width; x += sampleEvery) {
      const p = x * bytesPerPixel;
      const r = row[p];
      const g = colorType === 0 ? r : row[p + 1];
      const b = colorType === 0 ? r : row[p + 2];
      unique.add(`${r},${g},${b}`);
      const luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b;
      count += 1;
      const delta = luminance - mean;
      mean += delta / count;
      m2 += delta * (luminance - mean);
      if (unique.size > 256 && count > 1000) break;
    }

    previous = row;
  }

  const variance = count > 1 ? m2 / (count - 1) : 0;
  return {
    width,
    height,
    bitDepth,
    colorType,
    uniqueSampleColors: unique.size,
    luminanceVariance: Number(variance.toFixed(2)),
    blank: unique.size <= 3 || variance < 3,
  };
}

function defilter(row, previous, bytesPerPixel, filter) {
  for (let i = 0; i < row.length; i += 1) {
    const left = i >= bytesPerPixel ? row[i - bytesPerPixel] : 0;
    const up = previous[i] ?? 0;
    const upLeft = i >= bytesPerPixel ? previous[i - bytesPerPixel] : 0;
    if (filter === 1) row[i] = (row[i] + left) & 0xff;
    else if (filter === 2) row[i] = (row[i] + up) & 0xff;
    else if (filter === 3) row[i] = (row[i] + Math.floor((left + up) / 2)) & 0xff;
    else if (filter === 4) row[i] = (row[i] + paeth(left, up, upLeft)) & 0xff;
    else if (filter !== 0) throw new Error(`Unsupported PNG filter: ${filter}`);
  }
}

function paeth(left, up, upLeft) {
  const estimate = left + up - upLeft;
  const distanceLeft = Math.abs(estimate - left);
  const distanceUp = Math.abs(estimate - up);
  const distanceUpLeft = Math.abs(estimate - upLeft);
  if (distanceLeft <= distanceUp && distanceLeft <= distanceUpLeft) return left;
  if (distanceUp <= distanceUpLeft) return up;
  return upLeft;
}

function failReport(message, details = {}) {
  return { ok: false, message, details };
}

async function main() {
  await mkdir(outRoot, { recursive: true });

  const missingArtifacts = [];
  const artifactStats = {};
  for (const artifact of requiredArtifacts) {
    const path = join(webRoot, artifact);
    try {
      const file = await stat(path);
      artifactStats[artifact] = { bytes: file.size, mtime: file.mtime.toISOString() };
      if (file.size <= 0) missingArtifacts.push(artifact);
    } catch {
      artifactStats[artifact] = null;
      missingArtifacts.push(artifact);
    }
  }

  if (missingArtifacts.length > 0) {
    const report = failReport('Missing required web artifacts.', { webRoot, missingArtifacts, artifactStats });
    await writeFile(join(outRoot, 'web_visual_qa_report.json'), JSON.stringify(report, null, 2));
    console.error(JSON.stringify(report, null, 2));
    return 1;
  }

  const indexJs = await readFile(join(webRoot, 'index.js'), 'utf8');
  if (!indexJs.includes('wasm')) {
    const report = failReport('index.js does not look like a WASM loader.', { webRoot, artifactStats });
    await writeFile(join(outRoot, 'web_visual_qa_report.json'), JSON.stringify(report, null, 2));
    console.error(JSON.stringify(report, null, 2));
    return 1;
  }

  await mkdir(profileDir, { recursive: true });
  const chrome = await findChrome();
  const server = await startServer();
  const chromeProc = spawnChrome(chrome);

  try {
    const { ws, send, events } = await connectCdp(chromeProc);
    const captures = [];
    for (const viewport of viewports) {
      captures.push(await captureViewport(send, viewport));
    }
    const progressedScenarios = [];
    if (captureProgressed) {
      for (const viewport of viewports) {
        const progressed = await runProgressedViewport(send, viewport);
        progressedScenarios.push(progressed);
        captures.push(...progressed.captures);
      }
    }
    const reloadLifecycleScenario = captureProgressed
      ? await runReloadLifecycleScenario(send, viewports.find((viewport) => viewport.name === 'mobile_portrait') ?? viewports[0])
      : null;
    ws.close();

    const browserProblems = events.filter((event) =>
      event.method === 'Runtime.exceptionThrown' ||
      (
        event.method === 'Log.entryAdded' &&
        ['error'].includes(event.params?.entry?.level) &&
        !String(event.params?.entry?.url ?? '').endsWith('/favicon.ico')
      )
    );
    const failures = [];
    for (const capture of captures) {
      for (const [name, passed] of Object.entries(capture.checks)) {
        if (!passed) failures.push(`${capture.name}.${name}`);
      }
    }
    const progressionWarnings = progressedScenarios.flatMap((scenario) => scenario.warnings);
    if (reloadLifecycleScenario) {
      progressionWarnings.push(...reloadLifecycleScenario.warnings);
    }
    const progressionTodos = progressedScenarios.flatMap((scenario) => scenario.todos);
    const progressionFailures = [...progressionWarnings, ...progressionTodos];
    failures.push(...progressionFailures);

    const report = {
      ok: failures.length === 0,
      url,
      webRoot,
      outRoot,
      chrome,
      options: {
        waitMs,
        actionWaitMs,
        jobWaitMs,
        lifecycleWaitMs,
        captureProgressed,
        checkLifecycle,
      },
      artifactStats,
      captures,
      progressedScenarios,
      reloadLifecycleScenario,
      progressionWarnings,
      progressionTodos,
      progressionFailures,
      browserProblems,
      failures,
    };

    await writeFile(join(outRoot, 'web_visual_qa_report.json'), JSON.stringify(report, null, 2));
    console.log(JSON.stringify(report, null, 2));
    return report.ok ? 0 : 1;
  } finally {
    chromeProc.kill();
    server.close();
    await sleep(250);
    try {
      await rm(profileDir, { recursive: true, force: true });
    } catch {
      // Chrome can hold Crashpad files briefly after exit; stale profiles are harmless in tmp.
    }
  }
}

main().then((code) => {
  process.exitCode = code;
}).catch(async (error) => {
  const report = failReport(error.message, { stack: error.stack, webRoot, outRoot });
  try {
    await mkdir(outRoot, { recursive: true });
    await writeFile(join(outRoot, 'web_visual_qa_report.json'), JSON.stringify(report, null, 2));
  } catch {
    // The original failure is more useful than a secondary report-write failure.
  }
  console.error(JSON.stringify(report, null, 2));
  process.exitCode = 1;
});
