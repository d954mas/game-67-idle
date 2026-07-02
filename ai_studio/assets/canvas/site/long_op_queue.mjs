// Pure FIFO concurrency limiter for the page's long (python-backed) ops — detect,
// slice, render, export. DOM-free and framework-free on purpose, so it unit-tests in
// node (slot accounting, FIFO order, a failed op freeing its slot, and cancel of a
// still-queued op). The toast UI layer (`toasts.js`) wraps this to show the queued /
// running / result state; it is the ONLY concurrency control on the page.
//
// Why a page-side limiter at all: every detect/slice/render/export spawns a Python
// process on the server. One browser firing many at once would spawn a swarm; N=2
// keeps at most two in flight and QUEUES the rest, run FIFO as slots free. This is a
// page concern only — the CLI / direct ops path is unlimited (an agent orchestrates
// its own concurrency). The queue lives in memory: a page reload clears it (documented).

export class LongOpQueue {
  constructor({ max = 2 } = {}) {
    if (!(max >= 1)) throw new Error("LongOpQueue max must be >= 1");
    this.max = max;
    this._running = new Set(); // ids currently executing
    this._queue = []; // waiting tasks, FIFO (index 0 = next to run)
    this._seq = 0;
  }

  get runningCount() {
    return this._running.size;
  }

  get queuedCount() {
    return this._queue.length;
  }

  isRunning(id) {
    return this._running.has(id);
  }

  isQueued(id) {
    return this._queue.some((task) => task.id === id);
  }

  // Submit a unit of work. `run` is an async `() => resultSpec`. Optional lifecycle
  // hooks (all synchronous, best-effort):
  //   onStart()             — the task left the queue and began running
  //   onSettled(err, res)   — the task finished (err set on failure, else res = run()'s value)
  //   onQueue(position)     — the task is (still) queued at 1-based `position`
  // A task that starts immediately (a slot was free) fires onStart and never onQueue.
  // Returns the task id (pass it to `cancel`).
  submit({ label, run, onStart, onSettled, onQueue }) {
    const id = `op-${(this._seq += 1)}`;
    this._queue.push({ id, label, run, onStart, onSettled, onQueue });
    this._pump();
    this._notifyQueue();
    return id;
  }

  // Cancel a task that has NOT started yet. There is no server-side cancellation, so a
  // RUNNING task cannot be cancelled here — cancel() returns false for it (and for an
  // unknown id). Returns true only when a still-queued task was removed (it never runs).
  cancel(id) {
    const index = this._queue.findIndex((task) => task.id === id);
    if (index === -1) return false;
    this._queue.splice(index, 1);
    this._notifyQueue();
    return true;
  }

  // Start queued tasks in FIFO order while a slot is free.
  _pump() {
    while (this._running.size < this.max && this._queue.length) {
      const task = this._queue.shift();
      this._running.add(task.id);
      if (task.onStart) task.onStart();
      // Defer the actual work to a microtask so submit()/settle() stay synchronous for
      // slot accounting; a synchronous throw in run() becomes a rejection here.
      Promise.resolve()
        .then(() => task.run())
        .then(
          (res) => this._settle(task, null, res),
          (err) => this._settle(task, err || new Error("long op failed")),
        );
    }
  }

  _settle(task, err, res) {
    this._running.delete(task.id);
    if (task.onSettled) task.onSettled(err, res);
    this._pump(); // a freed slot starts the next queued op (FIFO)
    this._notifyQueue(); // remaining queued positions shifted up
  }

  // Tell every still-queued task its current 1-based position (drives "Queued … (#2)").
  _notifyQueue() {
    this._queue.forEach((task, index) => {
      if (task.onQueue) task.onQueue(index + 1);
    });
  }
}
