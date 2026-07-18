import { seedStyleLock } from "./seed.mjs";

const [mode, root, gameId, from] = process.argv.slice(2);

try {
  const dependencies = mode === "crash-after-project"
    ? { afterProjectCreated() { process.exit(81); } }
    : mode === "crash-after-temp"
      ? { afterLockTempWritten() { process.exit(82); } }
      : mode === "crash-after-commit"
        ? { afterLockCommitted() { process.exit(83); } }
      : mode === "hold-after-reclaim-unlink"
        ? { lockHooks: { afterReclaimUnlink() {
          process.stdout.write("reclaimed\n");
          Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 10000);
        } } }
      : mode === "hold-after-reclaim-link"
        ? { lockHooks: { afterReclaimLinked() {
          process.stdout.write("claimed\n");
          Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 10000);
        } } }
      : mode === "hold-after-project"
        ? { afterProjectCreated() {
          process.stdout.write("holding\n");
          Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 500);
        } }
        : {};
  const result = seedStyleLock(root, { gameId, from }, dependencies);
  process.stdout.write(`${JSON.stringify(result)}\n`);
} catch (error) {
  process.stderr.write(`${error?.message || String(error)}\n`);
  process.exitCode = 1;
}
