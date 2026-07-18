// Warm Python worker manager (T0202). One long-lived Python process per interpreter
// serves EVERY image-tool + canvas Python entrypoint over the line-delimited JSON
// protocol in worker.py, so the ~165-278ms interpreter-startup + numpy/PIL import
// floor is paid once at boot instead of on every detect/slice/render/export.
//
// This is a pure transport swap under the bridge's runPython: the served scripts run
// the same argv/argparse main they ran on the cold path, so tool parity is untouched.
// The worker is spawned LAZILY on the first request. Requests QUEUE FIFO and are
// serialized to ONE in flight at a time (the page's long-op queue already caps
// concurrency; one worker is enough, and the queue never deadlocks — a second op just
// waits for the first). An idle worker is killed after a timeout; a crashed worker
// FAILS the in-flight request loudly (no silent retry, no cold-spawn fallback — the
// no-fallbacks law) and a fresh worker is spawned to serve the next request.
//
// Lifecycle safety: the child is unref'd while idle so a one-shot process (the agent
// CLI, a test run) still exits when its work is done; a process-exit + signal hook
// kills every worker so nothing is orphaned (Windows has no process-group auto-kill).
import { spawn } from "node:child_process";
import { delimiter } from "node:path";
import { fileURLToPath } from "node:url";

const WORKER_PY = fileURLToPath(new URL("./worker.py", import.meta.url));
const DEFAULT_IDLE_MS = 5 * 60 * 1000;

// Read the idle timeout at arm time so tests can shorten it via env without a restart.
function idleMs() {
  const raw = Number(process.env.AI_STUDIO_IMAGE_WORKER_IDLE_MS);
  return Number.isFinite(raw) && raw > 0 ? raw : DEFAULT_IDLE_MS;
}

// One worker per resolved interpreter path (in practice one).
const workers = new Map();

let exitHookInstalled = false;
function installExitHook() {
  if (exitHookInstalled) return;
  exitHookInstalled = true;
  const killAll = () => {
    for (const worker of workers.values()) worker.killNow();
    workers.clear();
  };
  // Fires on a clean drain and on process.exit(); the sync kill prevents orphans.
  process.once("exit", killAll);
  // The studio server has no signal handlers of its own, so terminating on Ctrl-C /
  // SIGTERM after killing the child keeps the historical "process dies" behavior while
  // guaranteeing the Python child dies with it. process.once => one-shot, no leak.
  for (const signal of ["SIGINT", "SIGTERM", "SIGHUP"]) {
    try {
      process.once(signal, () => {
        killAll();
        process.exit();
      });
    } catch {
      // Some platforms reject certain signals; the exit hook still covers cleanup.
    }
  }
}

class Worker {
  constructor(pythonPath, cwd) {
    this.pythonPath = pythonPath;
    this.cwd = cwd;
    this.child = null;
    this.buffer = "";
    this.inflight = null; // { script, argv, resolve, reject }
    this.queue = []; // pending { script, argv, resolve, reject }
    this.seq = 0;
    this.idleTimer = null;
    this.stderrTail = "";
  }

  // ---- process lifecycle ----------------------------------------------------

  spawnChild() {
    const ambientPythonPath = String(process.env.PYTHONPATH || "");
    const child = spawn(this.pythonPath, ["-u", WORKER_PY], {
      cwd: this.cwd,
      windowsHide: true,
      stdio: ["pipe", "pipe", "pipe"],
      env: {
        ...process.env,
        PYTHONPATH: ambientPythonPath ? `${this.cwd}${delimiter}${ambientPythonPath}` : this.cwd,
        PYTHONUTF8: "1",
        PYTHONIOENCODING: "utf-8",
      },
    });
    this.child = child;
    this.buffer = "";
    this.stderrTail = "";
    child.stdout.setEncoding("utf8");
    child.stdout.on("data", (chunk) => this.onData(chunk));
    child.stderr.setEncoding("utf8");
    child.stderr.on("data", (chunk) => {
      this.stderrTail = (this.stderrTail + chunk).slice(-2000);
    });
    child.on("exit", (code, signal) => this.onExit(child, code, signal, null));
    child.on("error", (error) => this.onExit(child, null, null, error));
    return child;
  }

  ensureChild() {
    if (!this.child) this.spawnChild();
    this.clearIdle();
    return this.child;
  }

  // A clean, intentional shutdown (idle timeout / process exit). Detaches the
  // handlers first so the late 'exit' event never runs the crash path.
  killNow() {
    this.clearIdle();
    const child = this.child;
    this.child = null;
    this.buffer = "";
    if (child) {
      child.removeAllListeners("exit");
      child.removeAllListeners("error");
      try {
        child.kill();
      } catch {
        // already gone
      }
    }
  }

  // An UNEXPECTED exit (crash, external kill, failed spawn). Fails the in-flight
  // request loudly — never a silent retry — then respawns to serve the queue.
  onExit(child, code, signal, error) {
    if (this.child && this.child !== child) return; // stale event from an old child
    this.child = null;
    this.buffer = "";
    const detail = error
      ? `python worker failed to start: ${error.message}`
      : `python worker exited unexpectedly (code=${code}, signal=${signal})` +
        (this.stderrTail.trim() ? `: ${this.stderrTail.trim()}` : "");
    if (this.inflight) {
      const request = this.inflight;
      this.inflight = null;
      const failure = new Error(detail);
      failure.stderr = this.stderrTail;
      request.reject(failure);
    }
    // Queued requests never reached the dead worker; their first execution on a fresh
    // worker is not a retry. Pump respawns; with nothing queued we go fully idle.
    if (this.queue.length) this.pump();
    else this.unref();
  }

  // ---- protocol -------------------------------------------------------------

  onData(chunk) {
    this.buffer += chunk;
    let newline;
    while ((newline = this.buffer.indexOf("\n")) >= 0) {
      const line = this.buffer.slice(0, newline).trim();
      this.buffer = this.buffer.slice(newline + 1);
      if (!line) continue;
      let message;
      try {
        message = JSON.parse(line);
      } catch {
        continue; // stray non-protocol output (should not happen — scripts are redirected)
      }
      if (!message || typeof message !== "object" || !("ok" in message)) continue;
      this.settle(message);
    }
  }

  settle(message) {
    const request = this.inflight;
    if (!request) return; // unexpected extra frame
    this.inflight = null;
    if (message.ok) {
      request.resolve({ stdout: message.stdout || "", stderr: message.stderr || "" });
    } else {
      const failure = new Error(
        String(message.error || message.stderr || "python worker script failed").trim(),
      );
      failure.stderr = message.stderr || message.error || "";
      failure.code = message.code;
      request.reject(failure);
    }
    this.pump();
  }

  pump() {
    if (this.inflight) return;
    const next = this.queue.shift();
    if (!next) {
      this.armIdle();
      return;
    }
    const child = this.ensureChild();
    this.inflight = next;
    const frame = JSON.stringify({ id: (this.seq += 1), script: next.script, argv: next.argv }) + "\n";
    try {
      child.stdin.write(frame);
    } catch (error) {
      // Write to a dying pipe: treat as a crash of this request, then respawn.
      this.inflight = null;
      next.reject(new Error(`python worker unavailable: ${error.message}`));
      this.killNow();
      this.pump();
    }
  }

  run(script, argv) {
    return new Promise((resolve, reject) => {
      this.queue.push({ script, argv, resolve, reject });
      this.clearIdle();
      this.ref();
      this.pump();
    });
  }

  // ---- idle / event-loop ref accounting -------------------------------------

  armIdle() {
    if (this.inflight || this.queue.length || !this.child) return;
    this.unref(); // let a one-shot host process exit while the worker sits idle
    this.clearIdle();
    this.idleTimer = setTimeout(() => this.killNow(), idleMs());
    this.idleTimer.unref?.();
  }

  clearIdle() {
    if (this.idleTimer) {
      clearTimeout(this.idleTimer);
      this.idleTimer = null;
    }
  }

  ref() {
    const child = this.child;
    if (!child) return;
    child.ref?.();
    child.stdout?.ref?.();
    child.stderr?.ref?.();
    child.stdin?.ref?.();
  }

  unref() {
    const child = this.child;
    if (!child) return;
    child.unref?.();
    child.stdout?.unref?.();
    child.stderr?.unref?.();
    child.stdin?.unref?.();
  }
}

function workerFor(pythonPath, cwd) {
  installExitHook();
  let worker = workers.get(pythonPath);
  if (!worker) {
    worker = new Worker(pythonPath, cwd);
    workers.set(pythonPath, worker);
  }
  return worker;
}

// Run one Python script's main via the warm worker for `pythonPath` (spawned lazily,
// cwd = `root`). Resolves { stdout, stderr } on exit 0, rejects LOUDLY otherwise (the
// rejection Error carries `.stderr` so the bridge can map a missing-dependency error).
export function runPythonScript(root, pythonPath, scriptAbs, argv = []) {
  return workerFor(pythonPath, root).run(scriptAbs, argv);
}

// Kill every warm worker (clean shutdown). Used by tests; the process-exit hook does
// the same automatically so a server/CLI never orphans a Python child.
export function shutdownImageWorkers() {
  for (const worker of workers.values()) worker.killNow();
  workers.clear();
}

// Test-only: inspect the live worker for an interpreter (child handle, queue depth).
export function __workerForTest(pythonPath) {
  return workers.get(pythonPath) || null;
}
