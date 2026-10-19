import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";

import { copyGitSourceTree, copyTrackedGameSourceTree } from "../copy_source_tree.mjs";

function git(root, ...args) {
  return execFileSync("git", args, { cwd: root, encoding: "utf8" }).trim();
}

function write(root, relativePath, contents) {
  const file = join(root, relativePath);
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, contents, "utf8");
}

function repoFixture() {
  const root = mkdtempSync(join(tmpdir(), "copy-source-tree-"));
  git(root, "init", "-b", "main");
  git(root, "config", "user.name", "Fixture");
  git(root, "config", "user.email", "fixture@example.invalid");
  return root;
}

function stageDanglingLink(root, repoRelativePath) {
  const link = join(root, ...repoRelativePath.split("/"));
  const outside = `${root}-outside`;
  mkdirSync(dirname(link), { recursive: true });
  if (process.platform === "win32") {
    mkdirSync(outside);
    const canary = join(outside, "canary.txt");
    writeFileSync(canary, "outside canary\n", "utf8");
    symlinkSync(outside, link, "junction");
    const blob = git(root, "hash-object", "-w", canary);
    git(root, "update-index", "--add", "--cacheinfo", `120000,${blob},${repoRelativePath}`);
  } else {
    writeFileSync(outside, "outside canary\n", "utf8");
    symlinkSync(outside, link, "file");
    git(root, "add", repoRelativePath);
  }
  rmSync(outside, { recursive: true, force: true });
}

test("copyGitSourceTree copies regular tracked template files", (t) => {
  const root = repoFixture();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const source = join(root, "templates", "fixture");
  const destination = join(root, "output");
  write(root, "templates/fixture/regular.txt", "regular source\n");
  git(root, "add", ".");
  git(root, "commit", "-m", "regular template");

  copyGitSourceTree(root, source, destination);

  assert.equal(readFileSync(join(destination, "regular.txt"), "utf8"), "regular source\n");
});

test("copyGitSourceTree copies regular files through a Studio root alias", (t) => {
  const root = repoFixture();
  const alias = `${root}-alias`;
  t.after(() => {
    rmSync(alias, { recursive: true, force: true });
    rmSync(root, { recursive: true, force: true });
  });
  write(root, "templates/fixture/regular.txt", "regular source\n");
  git(root, "add", ".");
  git(root, "commit", "-m", "regular template");
  symlinkSync(root, alias, process.platform === "win32" ? "junction" : "dir");

  copyGitSourceTree(alias, join(alias, "templates", "fixture"), join(root, "output"));

  assert.equal(readFileSync(join(root, "output", "regular.txt"), "utf8"), "regular source\n");
});

test("copyGitSourceTree rejects a tracked template symlink", (t) => {
  const root = repoFixture();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const source = join(root, "templates", "fixture");
  const destination = join(root, "output");
  const linked = join(source, "linked-secret");
  write(root, "templates/fixture/regular.txt", "regular source\n");
  git(root, "add", "templates/fixture/regular.txt");
  if (process.platform === "win32") {
    const outside = join(root, "outside");
    mkdirSync(outside);
    writeFileSync(join(outside, "secret.txt"), "must not copy\n", "utf8");
    symlinkSync(outside, linked, "junction");
    const blob = git(root, "hash-object", "-w", join(outside, "secret.txt"));
    git(root, "update-index", "--add", "--cacheinfo", `120000,${blob},templates/fixture/linked-secret`);
  } else {
    const outside = join(root, "outside-secret.txt");
    writeFileSync(outside, "must not copy\n", "utf8");
    symlinkSync(outside, linked, "file");
    git(root, "add", "templates/fixture/linked-secret");
  }
  git(root, "commit", "-m", "linked template file");

  assert.throws(() => copyGitSourceTree(root, source, destination), /regular files|link|reparse/i);
  assert.equal(existsSync(join(destination, "linked-secret")), false);
});

test("copyGitSourceTree rejects a regular tracked leaf behind a template reparse directory", (t) => {
  const root = repoFixture();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const source = join(root, "templates", "fixture");
  const destination = join(root, "output");
  const linkedDirectory = join(source, "linked-dir");
  const outside = join(root, "outside");
  write(root, "templates/fixture/linked-dir/secret.txt", "original tracked content\n");
  git(root, "add", ".");
  git(root, "commit", "-m", "tracked template leaf");
  rmSync(linkedDirectory, { recursive: true, force: true });
  mkdirSync(outside);
  writeFileSync(join(outside, "secret.txt"), "outside canary\n", "utf8");
  symlinkSync(outside, linkedDirectory, process.platform === "win32" ? "junction" : "dir");

  assert.throws(() => copyGitSourceTree(root, source, destination), /regular files|link|reparse/i);
  assert.equal(existsSync(join(destination, "linked-dir", "secret.txt")), false);
});

test("copyGitSourceTree rejects a dangling tracked template link", (t) => {
  const root = repoFixture();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const source = join(root, "templates", "fixture");
  const destination = join(root, "output");
  stageDanglingLink(root, "templates/fixture/dangling-link");
  git(root, "commit", "-m", "dangling template link");

  assert.throws(() => copyGitSourceTree(root, source, destination), /regular files|link|reparse/i);
});

test("copyTrackedGameSourceTree rejects a dangling tracked game link", (t) => {
  const root = repoFixture();
  const destination = `${root}-output`;
  t.after(() => {
    rmSync(destination, { recursive: true, force: true });
    rmSync(root, { recursive: true, force: true });
  });
  stageDanglingLink(root, "dangling-link");
  git(root, "commit", "-m", "dangling game link");

  assert.throws(() => copyTrackedGameSourceTree(root, destination), /regular files|link|reparse/i);
});

test("copyGitSourceTree skips a missing tracked regular template file", (t) => {
  const root = repoFixture();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const source = join(root, "templates", "fixture");
  const destination = join(root, "output");
  write(root, "templates/fixture/vanished.txt", "tracked\n");
  git(root, "add", ".");
  git(root, "commit", "-m", "tracked template file");
  rmSync(join(source, "vanished.txt"));

  copyGitSourceTree(root, source, destination);

  assert.equal(existsSync(join(destination, "vanished.txt")), false);
});

test("copyTrackedGameSourceTree skips a missing tracked regular game file", (t) => {
  const root = repoFixture();
  const destination = `${root}-output`;
  t.after(() => {
    rmSync(destination, { recursive: true, force: true });
    rmSync(root, { recursive: true, force: true });
  });
  write(root, "vanished.txt", "tracked\n");
  git(root, "add", ".");
  git(root, "commit", "-m", "tracked game file");
  rmSync(join(root, "vanished.txt"));

  copyTrackedGameSourceTree(root, destination);

  assert.equal(existsSync(join(destination, "vanished.txt")), false);
});
