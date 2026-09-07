/* A headless Chrome driven over the DevTools pipe, with no dependency on any
 * game's own tooling: a feature pack that needs a browser to prove a portal SDK
 * has to carry its own, or every game that adopts it inherits a copy.
 *
 * The transport is the pipe rather than a port so no listening socket is opened
 * for the length of a probe.
 */

import { spawn } from "node:child_process";
import { existsSync, mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

function browserCandidates(env = process.env, platform = process.platform) {
  const candidates = [env.CHROME_PATH];
  if (platform === "win32") {
    for (const root of [env.PROGRAMFILES, env["PROGRAMFILES(X86)"], env.LOCALAPPDATA]) {
      if (root) {
        candidates.push(join(root, "Google", "Chrome", "Application", "chrome.exe"));
        candidates.push(join(root, "Chromium", "Application", "chrome.exe"));
      }
    }
  } else if (platform === "darwin") {
    candidates.push("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome");
    candidates.push("/Applications/Chromium.app/Contents/MacOS/Chromium");
  } else {
    candidates.push("/usr/bin/google-chrome", "/usr/bin/google-chrome-stable", "/usr/bin/chromium", "/usr/bin/chromium-browser");
  }
  return [...new Set(candidates.filter(Boolean).map((path) => resolve(path)))];
}

export function findSupportedBrowser(options = {}) {
  const candidates = options.chromePath ? [resolve(options.chromePath)] : browserCandidates(options.env, options.platform);
  const found = candidates.find((path) => existsSync(path));
  if (!found) throw new Error("supported Chrome/Chromium browser was not found (set CHROME_PATH)");
  return found;
}

export function browserSandboxArgs(env = process.env) {
  return env.AI_STUDIO_CHROME_NO_SANDBOX === "1"
    ? ["--no-sandbox", "--disable-gpu-sandbox"]
    : [];
}

export class PipeTransport {
  constructor(input, output) {
    this.input = input;
    this.output = output;
    this.buffer = Buffer.alloc(0);
    this.listeners = new Map();
    this.closed = false;
    output.on("data", (bytes) => this.feed(bytes));
    for (const stream of [input, output]) {
      stream.on("error", () => this.fail());
      stream.on("close", () => this.fail());
    }
  }

  addEventListener(type, callback) {
    this.listeners.set(type, [...(this.listeners.get(type) || []), callback]);
  }

  emit(type, event) {
    for (const callback of this.listeners.get(type) || []) callback(event);
  }

  feed(bytes) {
    this.buffer = Buffer.concat([this.buffer, bytes]);
    for (let end = this.buffer.indexOf(0); end >= 0; end = this.buffer.indexOf(0)) {
      const message = this.buffer.subarray(0, end).toString("utf8");
      this.buffer = this.buffer.subarray(end + 1);
      if (message) this.emit("message", { data: message });
    }
  }

  fail() {
    if (this.closed) return;
    this.closed = true;
    this.emit("close", {});
  }

  send(message) {
    if (this.closed) throw new Error("Chrome CDP pipe is closed");
    this.input.write(`${message}\0`, (error) => { if (error) this.fail(); });
  }

  close() {
    if (this.closed) return;
    this.closed = true;
    this.input.end();
    this.output.destroy();
  }
}

export class CdpClient {
  constructor(transport) {
    this.transport = transport;
    this.nextId = 0;
    this.pending = new Map();
    this.listeners = new Map();
    this.sessionId = "";
    transport.addEventListener("message", (event) => this.message(event));
    transport.addEventListener("close", () => {
      for (const pending of this.pending.values()) pending.reject(new Error(`Chrome CDP connection closed during ${pending.method}`));
      this.pending.clear();
    });
  }

  message(event) {
    const message = JSON.parse(event.data);
    if (message.id) {
      const pending = this.pending.get(message.id);
      if (!pending) return;
      this.pending.delete(message.id);
      if (message.error) pending.reject(new Error(message.error.message));
      else pending.resolve(message.result);
      return;
    }
    for (const listener of this.listeners.get(message.method) || []) listener(message.params || {});
  }

  on(method, listener) {
    this.listeners.set(method, [...(this.listeners.get(method) || []), listener]);
  }

  call(method, params, deadline) {
    return this.callInSession(method, params, deadline, this.sessionId);
  }

  callInSession(method, params, deadline, sessionId) {
    const budget = deadline - Date.now();
    if (budget <= 0) return Promise.reject(new Error(`${method} had no time budget left`));
    return new Promise((done, fail) => {
      const id = ++this.nextId;
      const timer = setTimeout(() => {
        this.pending.delete(id);
        fail(new Error(`${method} timed out`));
      }, budget);
      this.pending.set(id, {
        method,
        resolve: (value) => { clearTimeout(timer); done(value); },
        reject: (error) => { clearTimeout(timer); fail(error); },
      });
      try {
        this.transport.send(JSON.stringify({ id, method, params: params || {}, ...(sessionId ? { sessionId } : {}) }));
      } catch (error) {
        this.pending.delete(id);
        clearTimeout(timer);
        fail(error);
      }
    });
  }

  close() { this.transport.close(); }
}

/* Software WebGL is deliberate: a portal probe cares about the SDK calls a run
   makes, and a machine without a GPU must reach the same verdict. */
export async function launchHeadlessBrowser({ deadline, chromePath, extraArgs = [], viewport = { width: 1280, height: 720 } }) {
  const browser = findSupportedBrowser({ chromePath });
  const profileDir = mkdtempSync(join(tmpdir(), "platform-sdk-probe-"));
  const child = spawn(browser, [
    "--headless=new",
    "--remote-debugging-pipe",
    `--user-data-dir=${profileDir}`,
    "--no-first-run",
    "--no-default-browser-check",
    "--disable-extensions",
    "--disable-default-apps",
    "--disable-breakpad",
    "--disable-background-networking",
    "--disable-component-update",
    "--disable-sync",
    ...browserSandboxArgs(),
    "--use-angle=swiftshader",
    "--enable-unsafe-swiftshader",
    "--autoplay-policy=no-user-gesture-required",
    "--mute-audio",
    ...extraArgs,
    "about:blank",
  ], {
    stdio: ["ignore", "ignore", "ignore", "pipe", "pipe"],
    windowsHide: true,
    detached: process.platform !== "win32",
  });
  await new Promise((done, fail) => {
    child.once("spawn", done);
    child.once("error", fail);
  });
  if (!child.stdio[3] || !child.stdio[4]) throw new Error("headless browser CDP pipes are unavailable");

  const client = new CdpClient(new PipeTransport(child.stdio[3], child.stdio[4]));
  await client.call("Target.setDiscoverTargets", { discover: true }, deadline);
  const created = await client.call("Target.createTarget", { url: "about:blank" }, deadline);
  const attached = await client.call("Target.attachToTarget", { targetId: created.targetId, flatten: true }, deadline);
  if (!attached.sessionId) throw new Error("Chrome did not attach the probe target");
  client.sessionId = attached.sessionId;
  await Promise.all([
    client.call("Page.enable", {}, deadline),
    client.call("Runtime.enable", {}, deadline),
    client.call("Emulation.setDeviceMetricsOverride", { ...viewport, deviceScaleFactor: 1, mobile: false }, deadline),
  ]);

  return {
    client,
    close() {
      try { client.close(); } catch { /* the pipe is already gone */ }
      try { child.kill(); } catch { /* the browser already exited */ }
      try { rmSync(profileDir, { recursive: true, force: true }); } catch { /* a locked profile is not a probe failure */ }
    },
  };
}
