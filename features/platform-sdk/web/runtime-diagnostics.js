/* Shared browser crash delivery. The engine owns its raw record and this script
   only reads it through localStorage, so reporting never re-enters WASM. */
function installPlatformDiagnostics(root, options) {
  "use strict";

  var config = root.__PLATFORM_SDK_CONFIG__ || {};
  var prefix = String(options && options.storagePrefix || "");
  var runtimeScript = String(options && options.runtimeScript || "");
  var engineKey = prefix + ".v1";
  var webKey = prefix + ".pending.v1";
  var MAX_ENGINE_INPUT = 65536;
  var MAX_OUTGOING = 12000;
  var MAX_STORED = 20000;
  var WEB_TTL_MS = 86400000;
  var MAX_GROW_REPORTS = 4;
  var records = {
    currentEngine: null,
    currentWeb: null,
    previousEngine: null,
    previousWeb: null,
    contextCreation: null,
    contextLoss: null,
    memoryGrows: [],
  };
  var seenRaw = Object.create(null);
  var liveEngine = false;
  var timerActive = false;
  var readinessPolls = 0;
  var intentionalLossUntil = -Infinity;
  var wasmMemory = null;
  var wasmSeenBytes = null;
  var wasmGrows = 0;
  var stage = "loading";
  var startedAt = monotonicNow();
  var expectedRuntime = runtimeUrl(runtimeScript);
  var ownedScripts = [expectedRuntime, runtimeUrl("platform-sdk.js"), runtimeUrl("platform-sdk-adapter.js")];

  function monotonicNow() {
    return root.performance && typeof root.performance.now === "function" ? root.performance.now() : Date.now();
  }

  function byteLength(value) {
    var text = String(value);
    if (typeof root.TextEncoder === "function") return new root.TextEncoder().encode(text).length;
    return unescape(encodeURIComponent(text)).length;
  }

  function capBytes(value, limit, preserveEnds) {
    var text = String(value || "");
    if (byteLength(text) <= limit) return text;
    var marker = "\n[truncated]\n";
    var headLimit = preserveEnds ? Math.min(2048, Math.floor(limit / 3)) : 0;
    var head = preserveEnds ? text.slice(0, headLimit) : "";
    var tail = text;
    while (byteLength(head + marker + tail) > limit && tail.length) {
      tail = tail.slice(Math.ceil(tail.length / 8));
    }
    if (preserveEnds) return head + marker + tail;
    while (byteLength(tail) > limit && tail.length) tail = tail.slice(0, Math.floor(tail.length * 0.75));
    return tail;
  }

  function clean(value) {
    return String(value || "")
      .replace(/(?:https?|wss?|chrome-extension|moz-extension):\/\/[^\s)]+/g, function (text) {
        if (/^(?:chrome|moz)-extension:/.test(text)) return "[extension]";
        try {
          var url = new URL(text);
          return url.origin + url.pathname;
        } catch (_) {
          return "[url]";
        }
      })
      .replace(/\b(?:authorization|cookie|set-cookie)\s*[:=][^\r\n]*/gi, "[redacted header]")
      .replace(/\b(?:Bearer|Basic)\s+[^\s,;]+/gi, "[redacted credential]")
      .replace(/((?:token|password|session)\s*[=:]\s*)[^\s,;]+/gi, "$1[redacted]");
  }

  function validKind(value) {
    return typeof value === "string" && /^[a-z0-9_.-]{1,64}$/.test(value);
  }

  function createError(kind, stack) {
    var error = new Error(kind);
    error.name = "PlatformDiagnostic";
    error.stack = capBytes(clean(stack), MAX_OUTGOING, true);
    return error;
  }

  function engineEnvelope(text) {
    return typeof text === "string" && text.length > 0 && text.length <= MAX_ENGINE_INPUT &&
      /^game: [^\r\n]+\r?\nengine: [^\r\n]+/m.test(text) &&
      /\r?\ntime: [^\r\n]+/.test(text) && /\r?\nreason: [^\r\n]+/.test(text) &&
      /\r?\nstack:\r?\n/.test(text) && /\r?\nlog:\r?\n/.test(text);
  }

  function engineStack(text, memoryLine) {
    var stamped = memoryLine ? text.replace(/(\r?\nengine: [^\r\n]*)/, "$1\n" + memoryLine) : text;
    return capBytes(clean(stamped), MAX_OUTGOING, true);
  }

  // Only a reference to the module's memory is kept; reading its size never calls into WASM.
  function keepWasmMemory(values) {
    if (wasmMemory || !values || typeof root.WebAssembly !== "object") return;
    for (var key in values) {
      if (values[key] instanceof root.WebAssembly.Memory) { wasmMemory = values[key]; noteWasmSize(); return; }
    }
  }

  // A grow allocates a new, larger block and copies the heap into it, the
  // likeliest allocation to fail on a phone; the first few are reported.
  function noteWasmSize() {
    if (!wasmMemory) return;
    var bytes = wasmMemory.buffer.byteLength;
    if (wasmSeenBytes === null || bytes === wasmSeenBytes) { wasmSeenBytes = bytes; return; }
    var from = wasmSeenBytes;
    wasmSeenBytes = bytes;
    wasmGrows += 1;
    if (wasmGrows > MAX_GROW_REPORTS) return;
    var grow = { from_bytes: from, to_bytes: bytes, since_start_ms: Math.round(monotonicNow() - startedAt), stage: stage, count: wasmGrows };
    var stack = webStack("memory.grow", { message: "WebAssembly memory grew from " + from + " to " + bytes + " bytes" }, { grow: grow });
    var record = { raw: "memory.grow " + wasmGrows, error: createError("memory.grow", stack), fatal: false };
    if (seenRaw[record.raw]) return;
    seenRaw[record.raw] = true;
    records.memoryGrows.push(record);
    schedule(0);
  }

  // Emscripten grows from JS (emscripten_resize_heap), so the hook sees it at
  // once; the frame check catches a grow made by the module's own memory.grow.
  function watchWasmGrowth() {
    var api = root.WebAssembly;
    var proto = api && api.Memory && api.Memory.prototype;
    if (proto && typeof proto.grow === "function") {
      var grow = proto.grow;
      // A memory first seen by its own grow takes its size before that grow as the baseline.
      proto.grow = function () {
        var before = null;
        try { before = this.buffer.byteLength; } catch (_) {}
        var result = grow.apply(this, arguments);
        try {
          if (!wasmMemory) { wasmMemory = this; wasmSeenBytes = before; }
          if (this === wasmMemory) noteWasmSize();
        } catch (_) {}
        return result;
      };
    }
    if (typeof root.requestAnimationFrame !== "function") return;
    var frame = function () {
      try { noteWasmSize(); } catch (_) {}
      root.requestAnimationFrame(frame);
    };
    root.requestAnimationFrame(frame);
  }

  // The SDK lifecycle the game drives names the stage a grow happened in.
  function watchStage() {
    var backend = root.__platformSdkInternalBackend;
    if (!backend || backend.__diagnosticsStage) return;
    try {
      backend.__diagnosticsStage = true;
      [["gameLoadingFinished", "loaded"], ["gameplayStart", "gameplay"], ["gameplayStop", "paused"]].forEach(function (pair) {
        var original = backend[pair[0]];
        if (typeof original !== "function") return;
        backend[pair[0]] = function () {
          stage = pair[1];
          return original.apply(this, arguments);
        };
      });
    } catch (_) {}
  }

  function watchWasmMemory() {
    var api = root.WebAssembly;
    if (!api || typeof api.Memory !== "function") return;
    ["instantiate", "instantiateStreaming"].forEach(function (name) {
      var original = api[name];
      if (typeof original !== "function") return;
      api[name] = function (source, imports) {
        try { keepWasmMemory(imports && imports.env); } catch (_) {}
        var pending = original.apply(this, arguments);
        if (pending && typeof pending.then === "function") {
          pending.then(function (result) {
            try { keepWasmMemory((result && result.instance || result).exports); } catch (_) {}
          }, function () {});
        }
        return pending;
      };
    });
  }

  // Values at the moment of the event; an absent source stays null, so "unknown" never reads as zero.
  function memoryState() {
    var state = { wasm_bytes: null, device_gb: null, js_used_bytes: null, js_limit_bytes: null };
    try { if (wasmMemory) state.wasm_bytes = wasmMemory.buffer.byteLength; } catch (_) {}
    try {
      var device = root.navigator && root.navigator.deviceMemory;
      if (typeof device === "number" && Number.isFinite(device)) state.device_gb = device;
    } catch (_) {}
    try {
      var heap = root.performance && root.performance.memory;
      if (heap && Number.isFinite(heap.usedJSHeapSize)) state.js_used_bytes = heap.usedJSHeapSize;
      if (heap && Number.isFinite(heap.jsHeapSizeLimit)) state.js_limit_bytes = heap.jsHeapSizeLimit;
    } catch (_) {}
    return state;
  }

  function memoryLine() {
    var state = memoryState();
    return "memory: " + Object.keys(state).map(function (key) {
      return key + "=" + (state[key] === null ? "-" : state[key]);
    }).join(" ");
  }

  function webStack(kind, cause, extra) {
    var stack = cause && cause.stack ? cause.stack : cause && cause.message ? cause.message : "platform failure";
    var fields = {
      version: 1,
      at: Date.now(),
      kind: String(kind),
      build: String(config.runtimeBuildFingerprint || ""),
      memory: memoryState(),
    };
    for (var key in extra || {}) fields[key] = extra[key];
    var packet = JSON.stringify(fields);
    return capBytes(clean(stack) + "\nPLATFORM_DIAGNOSTICS " + packet, MAX_STORED - 512, true);
  }

  function storeWeb(kind, stack) {
    var stored = JSON.stringify({ version: 1, at: Date.now(), kind: String(kind), stack: stack });
    while (byteLength(stored) > MAX_STORED && stack.length) {
      stack = capBytes(stack, Math.max(0, byteLength(stack) - 512), true);
      stored = JSON.stringify({ version: 1, at: Date.now(), kind: String(kind), stack: stack });
    }
    if (byteLength(stored) > MAX_STORED) return;
    try { root.localStorage.setItem(webKey, stored); } catch (_) {}
  }

  function add(slot, record) {
    if (records[slot] || seenRaw[record.raw]) return false;
    records[slot] = record;
    seenRaw[record.raw] = true;
    schedule(0);
    return true;
  }

  function addEngine(text, slot, memory) {
    if (!engineEnvelope(text)) return false;
    return add(slot, { raw: text, error: createError("engine.crash", engineStack(text, memory)), fatal: true });
  }

  function addWeb(kind, cause, slot, persist) {
    var stack = webStack(kind, cause);
    var causeText = cause && cause.stack ? cause.stack : cause && cause.message ? cause.message : "platform failure";
    if (!add(slot, { raw: String(kind) + "\n" + clean(causeText), error: createError(kind, stack), fatal: slot === "currentWeb" || slot === "previousWeb" })) return false;
    if (persist) storeWeb(kind, stack);
    return true;
  }

  function previousWeb() {
    var raw;
    try { raw = root.localStorage.getItem(webKey); } catch (_) { return; }
    if (typeof raw !== "string" || byteLength(raw) > MAX_STORED) return;
    try {
      var saved = JSON.parse(raw);
      var age = Date.now() - saved.at;
      if (saved.version !== 1 || !Number.isFinite(age) || age < 0 || age > WEB_TTL_MS ||
          !validKind(saved.kind) || typeof saved.stack !== "string") return;
      add("previousWeb", { raw: raw, error: createError(saved.kind, saved.stack), fatal: true });
    } catch (_) {}
  }

  function previousEngine() {
    var raw;
    try { raw = root.localStorage.getItem(engineKey); } catch (_) { return; }
    addEngine(raw, "previousEngine");
  }

  function orderedRecords() {
    return [records.currentEngine, records.currentWeb, records.previousEngine, records.previousWeb,
      records.contextCreation, records.contextLoss].concat(records.memoryGrows).filter(Boolean);
  }

  function fallback(record) {
    if (record.fallback) return;
    record.fallback = true;
    try { root.console.error(record.error); } catch (_) {}
  }

  function sink() {
    try {
      var backend = root.__platformSdkInternalBackend;
      return backend && typeof backend.errorSink === "function" ? backend.errorSink() : null;
    } catch (_) {
      return null;
    }
  }

  function flush() {
    timerActive = false;
    var pending = orderedRecords().filter(function (record) { return !record.attempted; });
    if (!pending.length) return;
    var reportSink = sink();
    if (reportSink && typeof reportSink.captureError === "function") {
      pending.forEach(function (record) {
        record.attempted = true;
        try { reportSink.captureError(record.error); } catch (_) { fallback(record); }
      });
      return;
    }
    if (readinessPolls < 10) {
      readinessPolls += 1;
      schedule(1000);
      return;
    }
    pending.forEach(function (record) {
      fallback(record);
    });
  }

  function schedule(delay) {
    if (timerActive) return;
    timerActive = true;
    root.setTimeout(flush, delay);
  }

  function deferEngineFallback(kind, cause) {
    root.setTimeout(function () {
      if (!liveEngine) addWeb(kind, cause, "currentWeb", true);
    }, 0);
  }

  function runtimeUrl(value) {
    if (typeof value !== "string" || !value) return null;
    try {
      var source = value.replace(/:\d+(?::\d+)?$/, "");
      return new URL(source, root.location && root.location.href || undefined);
    } catch (_) {
      return null;
    }
  }

  function frameUrls(value) {
    if (typeof value !== "string") return [];
    return value.match(/(?:https?|wss?):\/\/[^\s)]+/g) || [];
  }

  function matchesOwnedScript(value) {
    var url = runtimeUrl(value);
    return Boolean(url && expectedRuntime && url.origin === expectedRuntime.origin && ownedScripts.some(function (script) {
      return script && url.origin === script.origin && url.pathname === script.pathname;
    }));
  }

  function isOwnedScript(value) {
    if (!expectedRuntime || typeof value !== "string") return false;
    if (!/[\r\n]/.test(value) && matchesOwnedScript(value)) return true;
    return frameUrls(value).some(matchesOwnedScript);
  }

  function hasForeignRuntimeOrigin(value) {
    if (typeof value !== "string") return false;
    if (/(?:chrome|moz)-extension:\/\//.test(value)) return true;
    return frameUrls(value).some(function (frame) {
      var url = runtimeUrl(frame);
      return Boolean(url && expectedRuntime && url.origin !== expectedRuntime.origin);
    });
  }

  function engineEligible(cause) {
    var stack = cause && cause.stack || "";
    return cause && cause.name === "RuntimeError" || /wasm-function\[|wasm:\/\/|\[wasm code\]/.test(stack);
  }

  function observeError(cause, filename, kind) {
    var stack = cause && cause.stack || "";
    var attributable = isOwnedScript(filename) || isOwnedScript(stack);
    if (engineEligible(cause)) {
      if (hasForeignRuntimeOrigin(String(filename || "") + "\n" + stack)) return;
      if (!filename || attributable) deferEngineFallback(kind, cause);
      return;
    }
    if (attributable) addWeb(kind, cause, "currentWeb", true);
  }

  function report(kind, cause) {
    var label = String(kind || "");
    if (!validKind(label)) return;
    if (/^gfx\.context_(?:lost|creation)$/.test(label)) {
      var stack = webStack(label, cause);
      add(label === "gfx.context_lost" ? "contextLoss" : "contextCreation", {
        raw: label + "\n" + stack, error: createError(label, stack), fatal: false,
      });
      return;
    }
    if (/^(?:wasm\.|runtime\.abort)/.test(label)) deferEngineFallback(label, cause);
    else addWeb(label, cause, "currentWeb", true);
  }

  function setStage(name) {
    if (validKind(name)) stage = name;
  }

  if (!prefix) return { report: report, intentionalLoss: function () {}, stage: setStage };
  watchWasmMemory();
  watchWasmGrowth();
  previousEngine();
  previousWeb();
  if (typeof root.addEventListener === "function") {
    root.addEventListener("platform-diagnostics-ready", function () { watchStage(); schedule(0); });
    root.addEventListener("nt-crash", function (event) {
      var text = event && event.detail && event.detail.text;
      if (!engineEnvelope(text)) return;
      liveEngine = true;
      addEngine(text, "currentEngine", memoryLine());
    });
    root.addEventListener("error", function (event) {
      observeError(event && event.error || { message: event && event.message }, event && event.filename, "runtime.error");
    });
    root.addEventListener("unhandledrejection", function (event) {
      observeError(event && event.reason, "", "runtime.rejection");
    });
  }
  var canvas = root.document && typeof root.document.getElementById === "function" ? root.document.getElementById("canvas") : null;
  if (canvas && typeof canvas.addEventListener === "function") {
    canvas.addEventListener("webglcontextcreationerror", function (event) {
      report("gfx.context_creation", { message: event && event.statusMessage || "WebGL context creation failed" });
    });
    canvas.addEventListener("webglcontextlost", function (event) {
      if (monotonicNow() > intentionalLossUntil) {
        report("gfx.context_lost", { message: event && event.statusMessage || "WebGL context lost" });
      }
    });
  }
  return {
    report: report,
    intentionalLoss: function () { intentionalLossUntil = monotonicNow() + 2000; },
    stage: setStage,
  };
}
