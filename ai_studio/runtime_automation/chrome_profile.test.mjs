import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, rmSync, utimesSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { createChromeProfile, sweepStaleProfiles } from "./chrome_profile.mjs";

const CHROME = [process.env.CHROME_PATH,
  join(process.env.PROGRAMFILES || "", "Google", "Chrome", "Application", "chrome.exe"),
  join(process.env["PROGRAMFILES(X86)"] || "", "Google", "Chrome", "Application", "chrome.exe"),
  "/usr/bin/google-chrome", "/usr/bin/chromium",
  "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"].find((path) => path && existsSync(path));

function scratchRoot(t) {
  const root = mkdtempSync(join(tmpdir(), "chrome-profile-test-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return root;
}

test("sweep removes profiles older than the limit and keeps fresh ones", (t) => {
  const root = scratchRoot(t);
  const old = join(root, "old-1");
  const fresh = join(root, "fresh-1");
  mkdirSync(old);
  mkdirSync(fresh);
  const twoDaysAgo = new Date(Date.now() - 2 * 24 * 60 * 60 * 1000);
  utimesSync(old, twoDaysAgo, twoDaysAgo);

  assert.equal(sweepStaleProfiles({ root }), 1);
  assert.deepEqual(readdirSync(root), ["fresh-1"]);
});

test("a missing root sweeps nothing", () => {
  assert.equal(sweepStaleProfiles({ root: join(tmpdir(), "chrome-profile-test-missing") }), 0);
});

test("dispose removes the profile a running Chrome has filled", { skip: !CHROME && "Chrome is not installed" }, async (t) => {
  const root = scratchRoot(t);
  const profile = createChromeProfile("probe", { root });
  const child = spawn(CHROME, ["--headless=new", `--user-data-dir=${profile.dir}`, "--no-first-run",
    "--no-default-browser-check", "--remote-debugging-port=0", "about:blank"], { stdio: "ignore", windowsHide: true });
  // Let Chrome finish starting: from then on Windows refuses to delete the files it holds.
  const deadline = Date.now() + 15000;
  while (!existsSync(join(profile.dir, "Default")) && Date.now() < deadline) {
    await new Promise((done) => setTimeout(done, 100));
  }
  assert.ok(existsSync(join(profile.dir, "Default")), "Chrome did not create its profile");
  await new Promise((done) => setTimeout(done, 1000));

  await profile.dispose(child);
  assert.equal(existsSync(profile.dir), false);
});
