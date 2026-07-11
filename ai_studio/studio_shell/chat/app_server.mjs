import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawn } from "node:child_process";

export const REPO_ROOT = resolve(fileURLToPath(new URL("../../..", import.meta.url)));
const DEFAULT_TIMEOUT_MS = 900_000;
const MAX_EARLY_MESSAGES = 128;
const APPROVAL_METHODS = new Set([
  "item/commandExecution/requestApproval",
  "item/fileChange/requestApproval",
]);

function protocolError(message, sessionId) {
  const error = new Error(`codex app-server: ${message}`);
  if (sessionId) error.sessionId = sessionId;
  return error;
}

function spawnCodexAppServer() {
  const resolved = resolveCodexAppServerCommand();
  return spawn(resolved.command, resolved.args, {
    cwd: REPO_ROOT,
    stdio: ["pipe", "pipe", "pipe"],
    windowsHide: true,
    detached: process.platform !== "win32",
  });
}

export function resolveCodexAppServerCommand({
  env = process.env,
  platform = process.platform,
  pathExists = existsSync,
} = {}) {
  if (env.CODEX_BIN) {
    return { command: env.CODEX_BIN, args: ["app-server", "--listen", "stdio://"] };
  }
  if (env.CODEX_APP_SERVER_JS) {
    if (!pathExists(env.CODEX_APP_SERVER_JS)) {
      throw protocolError(`configured CODEX_APP_SERVER_JS does not exist: ${env.CODEX_APP_SERVER_JS}`);
    }
    return { command: process.execPath, args: [env.CODEX_APP_SERVER_JS, "app-server", "--listen", "stdio://"] };
  }
  const defaultJs = env.APPDATA
    ? join(env.APPDATA, "npm", "node_modules", "@openai", "codex", "bin", "codex.js")
    : "";
  if (defaultJs && pathExists(defaultJs)) {
    return { command: process.execPath, args: [defaultJs, "app-server", "--listen", "stdio://"] };
  }
  return { command: platform === "win32" ? "codex.exe" : "codex", args: ["app-server", "--listen", "stdio://"] };
}

export function killAppServerProcessTree(child) {
  if (!child || child.exitCode !== null || child.signalCode != null || child.killed) return Promise.resolve();
  if (process.platform === "win32" && child.pid) {
    return new Promise((resolveKill, rejectKill) => {
      const killer = spawn("taskkill", ["/PID", String(child.pid), "/T", "/F"], {
        stdio: "ignore",
        windowsHide: true,
      });
      killer.once("error", rejectKill);
      killer.once("close", (code) => {
        if (code === 0) resolveKill();
        else rejectKill(new Error(`taskkill exited ${code}`));
      });
    });
  }
  if (child.pid) {
    try {
      process.kill(-child.pid, "SIGKILL");
      return Promise.resolve();
    } catch {}
  }
  child.kill("SIGKILL");
  return Promise.resolve();
}

function turnKey(threadId, turnId) {
  return `${threadId}\u0000${turnId}`;
}

class AppServerClient {
  constructor(child, { timeoutMs, cancelGraceMs, progressIntervalMs, killProcessTree, onDead }) {
    this.child = child;
    this.timeoutMs = timeoutMs;
    this.cancelGraceMs = cancelGraceMs;
    this.progressIntervalMs = progressIntervalMs;
    this.killProcessTree = killProcessTree;
    this.onDead = onDead;
    this.nextId = 1;
    this.pending = new Map();
    this.activeTurns = new Map();
    this.startingThreads = new Set();
    this.earlyByTurn = new Map();
    this.earlyCount = 0;
    this.stderr = "";
    this.buffer = "";
    this.dead = false;
    this.failure = null;

    child.stdout.setEncoding?.("utf8");
    child.stderr.setEncoding?.("utf8");
    child.stdout.on("data", (chunk) => this.#consume(String(chunk)));
    child.stdout.on("end", () => this.#die(protocolError("JSONL stdout reached EOF")));
    child.stderr.on("data", (chunk) => { this.stderr = `${this.stderr}${chunk}`.slice(-4000); });
    child.stdin.on("error", (error) => {
      this.#die(protocolError(`stdin stream failed: ${error?.message || error}`));
    });
    child.on("error", (error) => this.#die(protocolError(`failed to start: ${error.message}`)));
    child.on("close", (code, signal) => {
      const detail = signal ? `signal ${signal}` : `exit ${code}`;
      this.#die(protocolError(`process exited (${detail})${this.stderr ? `: ${this.stderr}` : ""}`));
    });
  }

  #write(message) {
    if (this.dead || !this.child.stdin.writable) throw this.failure || protocolError("process is unavailable");
    this.child.stdin.write(`${JSON.stringify(message)}\n`);
  }

  notify(method, params = {}) {
    this.#write({ method, params });
  }

  request(method, params = {}, timeoutMs = this.timeoutMs) {
    const id = this.nextId++;
    return new Promise((resolveRequest, rejectRequest) => {
      const timer = setTimeout(() => {
        const error = protocolError(`${method} timed out after ${timeoutMs}ms`);
        this.#die(error);
      }, timeoutMs);
      this.pending.set(id, {
        resolve: (value) => { clearTimeout(timer); resolveRequest(value); },
        reject: (error) => { clearTimeout(timer); rejectRequest(error); },
      });
      try {
        this.#write({ id, method, params });
      } catch (error) {
        this.pending.delete(id);
        clearTimeout(timer);
        rejectRequest(error);
      }
    });
  }

  beginThreadTurn(threadId) {
    this.startingThreads.add(threadId);
  }

  abandonThreadTurn(threadId) {
    this.startingThreads.delete(threadId);
    for (const [key, messages] of this.earlyByTurn) {
      if (key.startsWith(`${threadId}\u0000`)) {
        this.earlyByTurn.delete(key);
        this.earlyCount -= messages.length;
      }
    }
  }

  registerTurn({ threadId, turnId, requestPermission, onProgress }) {
    const key = turnKey(threadId, turnId);
    if (this.activeTurns.has(key)) throw protocolError(`duplicate active turn ${turnId}`, threadId);
    this.startingThreads.delete(threadId);
    const promise = new Promise((resolveTurn, rejectTurn) => {
      const timer = setTimeout(() => {
        this.#die(protocolError(`turn timed out after ${this.timeoutMs}ms`, threadId));
      }, this.timeoutMs);
      this.activeTurns.set(key, {
        key,
        threadId,
        turnId,
        requestPermission,
        onProgress,
        text: "",
        pendingProgress: "",
        progressTimer: null,
        permissionError: null,
        pendingApprovals: 0,
        completedTurn: null,
        resolve: (text) => { clearTimeout(timer); resolveTurn(text); },
        reject: (error) => { clearTimeout(timer); rejectTurn(error); },
      });
    });
    const early = this.earlyByTurn.get(key) || [];
    this.earlyByTurn.delete(key);
    this.earlyCount -= early.length;
    let staleApproval = null;
    for (const [queuedKey, messages] of this.earlyByTurn) {
      if (!queuedKey.startsWith(`${threadId}\u0000`)) continue;
      this.earlyByTurn.delete(queuedKey);
      this.earlyCount -= messages.length;
      for (const message of messages) {
        if (Object.hasOwn(message, "id") && APPROVAL_METHODS.has(message.method)) {
          try { this.#write({ id: message.id, result: { decision: "cancel" } }); }
          catch {}
          staleApproval ||= message;
        }
      }
    }
    if (staleApproval) {
      this.#die(protocolError(
        `foreign or stale early approval request for ${threadId}/${staleApproval.params?.turnId}`,
        threadId,
      ));
      return promise;
    }
    for (const message of early) this.#dispatch(message);
    return promise;
  }

  #queueEarly(key, message) {
    if (this.earlyCount >= MAX_EARLY_MESSAGES) {
      this.#die(protocolError("too many early turn messages"));
      return;
    }
    const queued = this.earlyByTurn.get(key) || [];
    queued.push(message);
    this.earlyByTurn.set(key, queued);
    this.earlyCount += 1;
  }

  #consume(chunk) {
    this.buffer += chunk;
    for (;;) {
      const newline = this.buffer.indexOf("\n");
      if (newline < 0) return;
      const line = this.buffer.slice(0, newline).trim();
      this.buffer = this.buffer.slice(newline + 1);
      if (!line) continue;
      let message;
      try { message = JSON.parse(line); }
      catch {
        this.#die(protocolError(`malformed JSONL from process: ${line.slice(0, 200)}`));
        return;
      }
      this.#dispatch(message);
    }
  }

  #messageTurnKey(message) {
    const params = message.params || {};
    const turnId = params.turnId || params.turn?.id;
    return params.threadId && turnId ? turnKey(params.threadId, turnId) : null;
  }

  #dispatch(message) {
    if (Object.hasOwn(message, "id") && !message.method) {
      const pending = this.pending.get(message.id);
      if (!pending) return;
      this.pending.delete(message.id);
      if (message.error) pending.reject(protocolError(message.error.message || JSON.stringify(message.error)));
      else pending.resolve(message.result);
      return;
    }

    if (Object.hasOwn(message, "id") && message.method) {
      if (!APPROVAL_METHODS.has(message.method)) {
        this.#write({ id: message.id, error: { code: -32601, message: `unsupported server request: ${message.method}` } });
        this.#die(protocolError(`unsupported server request: ${message.method}`));
        return;
      }
      const key = this.#messageTurnKey(message);
      const threadId = message.params?.threadId;
      if (!key) {
        this.#write({ id: message.id, result: { decision: "cancel" } });
        this.#die(protocolError(`${message.method} omitted threadId or turnId`));
        return;
      }
      const active = this.activeTurns.get(key);
      if (active) {
        void this.#approve(active, message);
      } else if (this.startingThreads.has(threadId)) {
        this.#queueEarly(key, message);
      } else {
        this.#write({ id: message.id, result: { decision: "cancel" } });
        this.#die(protocolError(`foreign or stale approval request for ${threadId}/${message.params.turnId}`));
      }
      return;
    }

    if (!message.method) return;
    const key = this.#messageTurnKey(message);
    if (!key) return;
    const active = this.activeTurns.get(key);
    if (!active) {
      if (this.startingThreads.has(message.params.threadId)) this.#queueEarly(key, message);
      return;
    }
    if (message.method === "item/agentMessage/delta") {
      const delta = String(message.params?.delta || "");
      active.text += delta;
      active.pendingProgress += delta;
      if (active.pendingProgress.length >= 4096) this.#flushProgress(active);
      this.#scheduleProgress(active);
    } else if (message.method === "item/completed") {
      const item = message.params?.item || {};
      if (!active.text && (item.type === "agentMessage" || item.type === "agent_message") && item.text) {
        active.text = String(item.text);
        active.pendingProgress += active.text;
        this.#scheduleProgress(active);
      }
    } else if (message.method === "turn/completed") {
      const turn = message.params?.turn || {};
      if (active.pendingApprovals > 0) active.completedTurn = turn;
      else this.#finishTurn(active, turn);
    }
  }

  #scheduleProgress(active) {
    if (typeof active.onProgress !== "function" || active.progressTimer) return;
    active.progressTimer = setTimeout(() => {
      active.progressTimer = null;
      this.#flushProgress(active);
    }, this.progressIntervalMs);
  }

  #flushProgress(active) {
    if (active.progressTimer) {
      clearTimeout(active.progressTimer);
      active.progressTimer = null;
    }
    const batch = active.pendingProgress;
    active.pendingProgress = "";
    if (typeof active.onProgress === "function" && batch) {
      try {
        active.onProgress(batch);
      } catch (error) {
        this.#die(protocolError(`progress callback failed: ${error?.message || error}`, active.threadId));
        return false;
      }
    }
    return true;
  }

  #finishTurn(active, turn) {
    if (this.activeTurns.get(active.key) !== active) return;
    if (!this.#flushProgress(active)) return;
    this.activeTurns.delete(active.key);
    const status = turn.status || "completed";
    if (active.permissionError) active.reject(active.permissionError);
    else if (status !== "completed") active.reject(protocolError(`turn ${status}${turn.error?.message ? `: ${turn.error.message}` : ""}`, active.threadId));
    else active.resolve(active.text.trim());
  }

  async #approve(active, message) {
    active.pendingApprovals += 1;
    let decision = "decline";
    let rejection = null;
    try {
      if (typeof active.requestPermission !== "function") throw new Error("permission broker is unavailable");
      await active.requestPermission({ method: message.method, params: message.params });
      decision = "accept";
    } catch (error) {
      rejection = error;
      if (/cancel|expired/i.test(error?.message || "")) decision = "cancel";
    }
    try { this.#write({ id: message.id, result: { decision } }); }
    catch (error) { rejection ||= error; }
    if (rejection && this.activeTurns.get(active.key) === active) active.permissionError ||= rejection;
    active.pendingApprovals -= 1;
    if (active.completedTurn && active.pendingApprovals === 0) this.#finishTurn(active, active.completedTurn);
  }

  async cancelTurn(threadId, turnId) {
    const key = turnKey(threadId, turnId);
    if (!this.activeTurns.has(key) || this.dead) return;
    try {
      await this.request("turn/interrupt", { threadId, turnId }, this.cancelGraceMs);
    } catch (error) {
      if (!this.dead) this.#die(error);
      throw error;
    }
    if (!this.activeTurns.has(key) || this.dead) return;
    await new Promise((resolveWait) => setTimeout(resolveWait, this.cancelGraceMs));
    if (this.activeTurns.has(key) && !this.dead) {
      const error = protocolError(`turn ${turnId} remained active after interrupt acknowledgement`, threadId);
      this.#die(error);
      throw error;
    }
  }

  #die(error) {
    if (this.dead) return;
    this.dead = true;
    this.failure = error;
    for (const pending of this.pending.values()) pending.reject(error);
    this.pending.clear();
    for (const active of this.activeTurns.values()) {
      if (active.progressTimer) clearTimeout(active.progressTimer);
      active.reject(error);
    }
    this.activeTurns.clear();
    this.startingThreads.clear();
    this.earlyByTurn.clear();
    this.earlyCount = 0;
    const cleanup = Promise.resolve().then(() => this.killProcessTree(this.child));
    this.onDead(this, cleanup);
  }

  shutdown() {
    if (this.dead) return;
    for (const active of this.activeTurns.values()) {
      try { this.#write({ id: this.nextId++, method: "turn/interrupt", params: { threadId: active.threadId, turnId: active.turnId } }); }
      catch {}
    }
    this.#die(protocolError("process shut down"));
  }
}

export function createAppServerTransport({
  spawnProcess = spawnCodexAppServer,
  killProcessTree = killAppServerProcessTree,
  timeoutMs = DEFAULT_TIMEOUT_MS,
  cancelGraceMs = 250,
  progressIntervalMs = 50,
} = {}) {
  let client = null;
  let starting = null;
  let startingClient = null;
  let stopped = false;
  let cleanupBarrier = Promise.resolve();
  let cleanupFailure = null;

  async function ensureClient() {
    if (stopped) throw protocolError("transport is shut down");
    if (client && !client.dead) return client;
    if (starting) return starting;
    starting = (async () => {
      await cleanupBarrier;
      if (cleanupFailure) throw cleanupFailure;
      const next = new AppServerClient(spawnProcess(), {
        timeoutMs,
        cancelGraceMs,
        progressIntervalMs,
        killProcessTree,
        onDead: (dead, cleanup) => {
          if (client === dead) client = null;
          if (startingClient === dead) startingClient = null;
          cleanupBarrier = cleanupBarrier.catch(() => {}).then(() => cleanup).catch((error) => {
            cleanupFailure = protocolError(`process-tree cleanup failed: ${error?.message || error}`);
            throw cleanupFailure;
          });
          // The barrier is also returned by shutdown/awaited before restart. This observer
          // prevents an unhandled rejection in the interval before either consumer awaits it.
          void cleanupBarrier.catch(() => {});
        },
      });
      startingClient = next;
      try {
        await next.request("initialize", {
          clientInfo: { name: "ai-studio-canvas-chat", title: "AI Studio Canvas Chat", version: "1" },
        });
        next.notify("initialized", {});
        const accountResult = await next.request("account/read", { refreshToken: false });
        if (accountResult?.account?.type !== "chatgpt") {
          throw protocolError("a managed ChatGPT subscription login is required (API-key auth is not accepted)");
        }
        if (stopped) throw protocolError("transport is shut down");
        client = next;
        startingClient = null;
        return next;
      } catch (error) {
        next.shutdown();
        throw error;
      }
    })();
    try { return await starting; }
    finally { starting = null; }
  }

  const transport = async ({ prompt, message, sessionId, onChild, onProgress, requestPermission } = {}) => {
    const current = await ensureClient();
    let threadId = sessionId || null;
    let registered = false;
    try {
      if (threadId) {
        const resumed = await current.request("thread/resume", { threadId });
        const resumedId = resumed?.thread?.id;
        if (resumedId !== threadId) throw protocolError(`thread/resume returned mismatched thread.id (${resumedId || "missing"})`, threadId);
      } else {
        const started = await current.request("thread/start", {
          cwd: REPO_ROOT,
          approvalPolicy: "on-request",
          sandbox: "read-only",
        });
        threadId = started?.thread?.id;
        if (!threadId) throw protocolError("thread/start returned no thread.id");
      }
      const text = sessionId ? message : prompt;
      if (!text) throw protocolError("turn input is empty", threadId);
      current.beginThreadTurn(threadId);
      const turnResult = await current.request("turn/start", {
        threadId,
        input: [{ type: "text", text: String(text) }],
      });
      const turnId = turnResult?.turn?.id;
      if (!turnId) throw protocolError("turn/start returned no turn.id", threadId);
      if (current.dead) throw current.failure || protocolError("process became unavailable during turn/start", threadId);
      const turnPromise = current.registerTurn({ threadId, turnId, requestPermission, onProgress });
      registered = true;
      if (typeof onChild === "function") {
        onChild({ kill: () => { void current.cancelTurn(threadId, turnId).catch(() => {}); } });
      }
      const textResult = await turnPromise;
      if (!textResult) throw protocolError("turn completed with an empty reply", threadId);
      return { text: textResult, sessionId: threadId };
    } catch (error) {
      if (threadId && !registered) current.abandonThreadTurn(threadId);
      if (threadId && !error.sessionId) error.sessionId = threadId;
      throw error;
    }
  };
  transport.approvalAware = true;
  transport.shutdown = () => {
    stopped = true;
    if (startingClient) startingClient.shutdown();
    if (client && client !== startingClient) client.shutdown();
    startingClient = null;
    client = null;
    return cleanupBarrier;
  };
  return transport;
}

export const runAppServerTransport = createAppServerTransport();
