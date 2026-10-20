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
  var records = {
    currentEngine: null,
    currentWeb: null,
    previousEngine: null,
    previousWeb: null,
    contextCreation: null,
    contextLoss: null,
  };
  var seenRaw = Object.create(null);
  var liveEngine = false;
  var timerActive = false;
  var readinessPolls = 0;
  var intentionalLossUntil = -Infinity;
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

  function engineStack(text) {
    return capBytes(clean(text), MAX_OUTGOING, true);
  }

  function webStack(kind, cause) {
    var stack = cause && cause.stack ? cause.stack : cause && cause.message ? cause.message : "platform failure";
    var packet = JSON.stringify({
      version: 1,
      at: Date.now(),
      kind: String(kind),
      build: String(config.runtimeBuildFingerprint || ""),
    });
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

  function addEngine(text, slot) {
    if (!engineEnvelope(text)) return false;
    return add(slot, { raw: text, error: createError("engine.crash", engineStack(text)), fatal: true });
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
      records.contextCreation, records.contextLoss].filter(Boolean);
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
      add(label === "gfx.context_lost" ? "contextLoss" : "contextCreation", {
        raw: label + "\n" + webStack(label, cause), error: createError(label, webStack(label, cause)), fatal: false,
      });
      return;
    }
    if (/^(?:wasm\.|runtime\.abort)/.test(label)) deferEngineFallback(label, cause);
    else addWeb(label, cause, "currentWeb", true);
  }

  if (!prefix) return { report: report, intentionalLoss: function () {} };
  previousEngine();
  previousWeb();
  if (typeof root.addEventListener === "function") {
    root.addEventListener("platform-diagnostics-ready", function () { schedule(0); });
    root.addEventListener("nt-crash", function (event) {
      var text = event && event.detail && event.detail.text;
      if (!engineEnvelope(text)) return;
      liveEngine = true;
      addEngine(text, "currentEngine");
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
  };
}
