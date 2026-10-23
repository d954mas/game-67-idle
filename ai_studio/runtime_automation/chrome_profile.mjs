/* Throwaway Chrome profiles for headless runs.
 *
 * Every profile lives under one root in the system temp folder, so a run that
 * crashed before cleanup is swept by the next run instead of leaking forever.
 * A fresh profile grows to 50-200 MB on first launch.
 */

import { mkdirSync, mkdtempSync, readdirSync, rmSync, statSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

export const PROFILE_ROOT = join(tmpdir(), "ai-studio-chrome");
const STALE_MS = 24 * 60 * 60 * 1000;
const EXIT_WAIT_MS = 5000;

/** Removes profiles older than maxAgeMs; a profile a live Chrome still holds fails to delete and is kept. */
export function sweepStaleProfiles({ root = PROFILE_ROOT, maxAgeMs = STALE_MS, now = Date.now() } = {}) {
  let entries;
  try { entries = readdirSync(root); } catch { return 0; }
  let removed = 0;
  for (const name of entries) {
    const path = join(root, name);
    try {
      if (now - statSync(path).mtimeMs < maxAgeMs) continue;
      rmSync(path, { recursive: true, force: true });
      removed += 1;
    } catch { /* still in use */ }
  }
  return removed;
}

/**
 * Creates a profile directory for `--user-data-dir`.
 * Call `await profile.dispose(child)` in `finally`: it stops Chrome, waits for the
 * exit (Windows will not delete files a live process holds) and removes the folder.
 */
export function createChromeProfile(prefix = "chrome", { root = PROFILE_ROOT } = {}) {
  mkdirSync(root, { recursive: true });
  sweepStaleProfiles({ root });
  const dir = mkdtempSync(join(root, `${prefix}-`));
  return {
    dir,
    async dispose(child) {
      if (child && child.exitCode === null && child.signalCode === null) {
        const exited = new Promise((done) => child.once("exit", done));
        try { child.kill(); } catch { /* already gone */ }
        await Promise.race([exited, new Promise((done) => setTimeout(done, EXIT_WAIT_MS).unref())]);
      }
      try {
        rmSync(dir, { recursive: true, force: true, maxRetries: 10, retryDelay: 100 });
      } catch { /* the next sweep removes it */ }
    },
  };
}
