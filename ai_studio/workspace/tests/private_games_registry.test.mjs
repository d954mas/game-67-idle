import assert from "node:assert/strict";
import { execFileSync, spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

import {
  auditParentGitCommand,
  auditPrivateGamePreflight,
  listGameMounts,
  localGameRegistryRelPath,
  runPrivateGamePreflight,
} from "../games.mjs";

const script = resolve(dirname(fileURLToPath(import.meta.url)), "..", "games.mjs");

function tempRoot(prefix = "ai-studio-workspace-games-") {
  return mkdtempSync(join(tmpdir(), prefix));
}

function writeJson(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function writePublicGames(root, games) {
  writeJson(join(root, "games", "games.json"), {
    schema: "ai_studio.assets.games.v1",
    games,
  });
}

function writeLocalGames(root, games) {
  writeJson(join(root, "ai_studio", "workspace", "games.local.json"), {
    schema: "ai_studio.workspace.games.local.v1",
    games,
  });
}

function privateMount(overrides = {}) {
  return {
    schemaVersion: 1,
    storeId: "game:secret-game",
    kind: "game",
    gameId: "secret-game",
    root: "games/secret-game",
    visibility: "private",
    gitRoot: "games/secret-game",
    commitPolicy: "nested-private",
    enabledStores: ["assets", "taskboard", "canvas", "evidence"],
    aliases: ["Secret Codename"],
    remoteHints: ["git@example.test:studio/secret-game.git"],
    ...overrides,
  };
}

test("listGameMounts reads public games by default and keeps local private games hidden", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  writePublicGames(root, [
    {
      id: "public-game",
      title: "Public Game",
      folder: "games/public-game",
      assets: "games/public-game/assets",
      status: "active",
    },
  ]);
  writeLocalGames(root, [privateMount()]);

  assert.deepEqual(listGameMounts(root), [
    {
      schemaVersion: 1,
      storeId: "game:public-game",
      kind: "game",
      gameId: "public-game",
      title: "Public Game",
      root: "games/public-game",
      visibility: "public",
      gitRoot: "",
      commitPolicy: "parent-public",
      enabledStores: ["assets"],
      assetRoot: "games/public-game/assets",
      status: "active",
      source: "public",
    },
  ]);
});

test("listGameMounts includes private local mounts only when explicitly requested", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  writePublicGames(root, [
    { id: "public-game", title: "Public Game", folder: "games/public-game", assets: "games/public-game/assets" },
  ]);
  writeLocalGames(root, [privateMount()]);

  assert.deepEqual(
    listGameMounts(root, { includePrivate: true, skipPreflight: true }).map((mount) => `${mount.visibility}:${mount.storeId}`),
    ["public:game:public-game", "private:game:secret-game"],
  );
});

test("local private registry rejects duplicate public game ids unless overriding a public fixture", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  writePublicGames(root, [
    { id: "secret-game", title: "Real Public", folder: "games/secret-game", assets: "games/secret-game/assets", status: "active" },
  ]);
  writeLocalGames(root, [privateMount({ overridesPublicFixture: true })]);

  assert.throws(
    () => listGameMounts(root, { includePrivate: true, skipPreflight: true }),
    /duplicate game id 'secret-game'/,
  );

  writePublicGames(root, [
    { id: "secret-game", title: "Fixture", folder: "games/secret-game", assets: "games/secret-game/assets", status: "fixture" },
  ]);
  assert.equal(listGameMounts(root, { includePrivate: true, skipPreflight: true }).length, 1);
  assert.equal(listGameMounts(root, { includePrivate: true, skipPreflight: true })[0].visibility, "private");
});

test("local private registry rejects duplicate local game ids", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  writeLocalGames(root, [
    privateMount(),
    privateMount({ title: "Duplicate" }),
  ]);

  assert.throws(
    () => listGameMounts(root, { includePrivate: true, skipPreflight: true }),
    /duplicate game id 'secret-game'/,
  );
});

test("auditPrivateGamePreflight requires ignored roots and rejects tracked, staged, gitlink, and text leaks", () => {
  const mounts = [privateMount()];
  const ok = auditPrivateGamePreflight(mounts, {
    ignoredPaths: new Set([localGameRegistryRelPath(), "games/secret-game"]),
    trackedPaths: new Set(),
    stagedPaths: new Set(),
    gitlinks: new Set(),
    nestedGitRoots: new Set(["games/secret-game"]),
    trackedTextFiles: [{ path: "games/games.json", text: "public only" }],
  });
  assert.equal(ok.ok, true, JSON.stringify(ok.violations));

  const bad = auditPrivateGamePreflight(mounts, {
    ignoredPaths: new Set(),
    trackedPaths: new Set([localGameRegistryRelPath(), "games/secret-game/assets/hero.png"]),
    stagedPaths: new Set(["games/secret-game/design/gdd.md"]),
    gitlinks: new Set(["games/secret-game"]),
    nestedGitRoots: new Set(),
    trackedTextFiles: [{ path: "ai_studio/taskboard/items/active/T0001.md", text: "Secret Codename appears in games\\secret-game\\.ai_studio\\canvas" }],
    unscannedTextFiles: [{ path: "ai_studio/taskboard/items/active/T0002.md", reason: "git grep output limit" }],
  });

  assert.equal(bad.ok, false);
  assert.match(bad.violations.map((item) => item.reason).join("\n"), /local private registry is not ignored/);
  assert.match(bad.violations.map((item) => item.reason).join("\n"), /private root is tracked by the parent repository/);
  assert.match(bad.violations.map((item) => item.reason).join("\n"), /private root has staged parent-repo paths/);
  assert.match(bad.violations.map((item) => item.reason).join("\n"), /parent repository records a gitlink/);
  assert.match(bad.violations.map((item) => item.reason).join("\n"), /missing nested git metadata/);
  assert.match(bad.violations.map((item) => item.reason).join("\n"), /tracked file leaks private token/);
  assert.match(bad.violations.map((item) => item.reason).join("\n"), /could not inspect this file/);
});

test("auditPrivateGamePreflight allows explicit public aliases in tracked text", () => {
  const result = auditPrivateGamePreflight([privateMount({ publicAlias: "Private Slot" })], {
    ignoredPaths: new Set([localGameRegistryRelPath(), "games/secret-game"]),
    trackedPaths: new Set(),
    stagedPaths: new Set(),
    gitlinks: new Set(),
    nestedGitRoots: new Set(["games/secret-game"]),
    trackedTextFiles: [{ path: "README.md", text: "Private Slot is a safe display alias" }],
  });

  assert.equal(result.ok, true, JSON.stringify(result.violations));
});

test("auditPrivateGamePreflight rejects generated report and evidence path leaks", () => {
  const result = auditPrivateGamePreflight([privateMount()], {
    ignoredPaths: new Set([localGameRegistryRelPath(), "games/secret-game"]),
    trackedPaths: new Set(),
    stagedPaths: new Set(),
    gitlinks: new Set(),
    nestedGitRoots: new Set(["games/secret-game"]),
    trackedTextFiles: [
      {
        path: "ai_studio/architecture_map/validation-report.json",
        text: JSON.stringify({ issues: [{ path: "games/secret-game" }] }),
      },
      {
        path: "ai_studio/taskboard/items/active/T9999.md",
        text: "runtime evidence: games/secret-game/.ai_studio/evidence/smoke.json",
      },
    ],
  });

  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => `${item.path}: ${item.reason}`).join("\n"), /validation-report\.json: tracked file leaks private token/);
  assert.match(result.violations.map((item) => `${item.path}: ${item.reason}`).join("\n"), /T9999\.md: tracked file leaks private token/);
});

function privateGitFixture(t, prefix = "ai-studio-workspace-games-git-") {
  const root = tempRoot(prefix);
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  writeFileSync(join(root, ".gitignore"), `${localGameRegistryRelPath()}\n`, "utf8");
  writeFileSync(join(root, ".git", "info", "exclude"), "games/secret-game/\n", "utf8");
  writeLocalGames(root, [privateMount({ title: "Secret Title" })]);
  mkdirSync(join(root, "games", "secret-game"), { recursive: true });
  execFileSync("git", ["init"], { cwd: join(root, "games", "secret-game"), stdio: "ignore" });
  writeFileSync(join(root, "games", "secret-game", "README.md"), "private game\n", "utf8");
  return root;
}

test("runPrivateGamePreflight passes a real git fixture with local ignored private root", (t) => {
  const root = privateGitFixture(t);
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root, stdio: "ignore" });

  assert.equal(runPrivateGamePreflight(root).ok, true);

  writeFileSync(join(root, "README.md"), "mentions secret-game\n", "utf8");
  const leaked = runPrivateGamePreflight(root);
  assert.equal(leaked.ok, false);
  assert.match(leaked.violations.map((item) => item.reason).join("\n"), /tracked file leaks private token/);
  assert.match(readFileSync(join(root, ".gitignore"), "utf8"), /games\.local\.json/);
});

test("runPrivateGamePreflight scans staged blobs even when the worktree no longer leaks", (t) => {
  const root = privateGitFixture(t, "ai-studio-workspace-games-staged-");
  writeFileSync(join(root, "README.md"), "mentions Secret Title only in the staged blob\n", "utf8");
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root, stdio: "ignore" });
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");

  const result = runPrivateGamePreflight(root);

  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /tracked file leaks private token/);
});

test("runPrivateGamePreflight blocks an index-only staged local registry", (t) => {
  const root = tempRoot("ai-studio-workspace-games-index-registry-");
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  writeFileSync(join(root, ".gitignore"), `${localGameRegistryRelPath()}\n`, "utf8");
  writeLocalGames(root, [privateMount()]);
  execFileSync("git", ["add", "-f", localGameRegistryRelPath()], { cwd: root, stdio: "ignore" });
  rmSync(join(root, "ai_studio", "workspace", "games.local.json"), { force: true });

  const result = runPrivateGamePreflight(root);

  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /local private registry is tracked or staged/);
});

test("runPrivateGamePreflight rejects invalid nested git metadata", (t) => {
  const root = privateGitFixture(t, "ai-studio-workspace-games-invalid-nested-");
  rmSync(join(root, "games", "secret-game", ".git"), { recursive: true, force: true });
  mkdirSync(join(root, "games", "secret-game", ".git"), { recursive: true });

  const result = runPrivateGamePreflight(root);

  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /missing nested git metadata/);
});

test("parent git guard blocks broad add and hook-clean commands with private mounts", (t) => {
  const root = privateGitFixture(t, "ai-studio-workspace-games-guard-");

  const add = auditParentGitCommand(root, "git add .");
  assert.equal(add.ok, false);
  assert.match(add.violations[0].reason, /parent git add command can affect private game roots/);

  const hook = spawnSync(process.execPath, [script, "hook-guard", "--root", root], {
    encoding: "utf8",
    input: JSON.stringify({ tool_input: { command: "git clean -fdx" } }),
  });
  assert.equal(hook.status, 1, hook.stdout || hook.stderr);
  assert.match(hook.stderr, /private game git guard blocked this command/);

  const wrappedBash = auditParentGitCommand(root, "bash -lc \"echo ok && git add .\"");
  assert.equal(wrappedBash.ok, false);
  assert.match(wrappedBash.violations[0].reason, /parent git add command can affect private game roots/);

  const wrappedPowerShell = auditParentGitCommand(root, "powershell -Command \"Write-Host ok; git clean -fdx\"");
  assert.equal(wrappedPowerShell.ok, false);
  assert.match(wrappedPowerShell.violations[0].reason, /parent git clean command can affect private game roots/);

  for (const command of [
    "git add -f .\\games\\secret-game",
    "git clean -fdx .\\games\\secret-game",
    "git add -f GAMES/SECRET-GAME",
    "git add -f .\\GAMES\\SECRET-GAME",
    "git add -f :/GAMES/SECRET-GAME",
    "git add :/",
    "git add :/games/secret-game",
    "git clean -fdx :/games/secret-game",
    "git add :(top)games/secret-game",
    "git add -f games/*",
    "git clean -fdx games/*",
    "git add :(glob)games/**",
    "git add games/secret-*",
    "git add games/{public-game,secret-game}",
    "git add --pathspec-from-file=paths.txt",
    "git add --pathspec-from-file paths.txt",
    "git stage .",
    "git stage --pathspec-from-file=paths.txt",
    "git --config-env feature.flag=ENV_NAME add .",
    "git add -f games/public-game/../secret-game",
    "git add -f .\\games\\public-game\\..\\secret-game",
    "git add -f :/games/public-game/../secret-game",
    "git -C . add .",
    "git -C . clean -fdx",
    "git -C games/public-game add ../secret-game",
    "git -C games/public-game add .\\..\\secret-game",
    "git -C games/public-game add -u",
    "git add -u",
    "git add --update",
    "git add -Af",
    "git add -fA",
    "git add -uf",
    "git add -fu",
  ]) {
    const result = auditParentGitCommand(root, command);
    assert.equal(result.ok, false, command);
  }

  const localRegistry = auditParentGitCommand(root, "git add .\\ai_studio\\workspace\\games.local.json");
  assert.equal(localRegistry.ok, false);
  assert.match(localRegistry.violations[0].reason, /local private registry/);

  for (const command of [
    `git add -f "${join(root, "games", "secret-game")}"`,
    `git add "${join(root, "ai_studio", "workspace", "games.local.json")}"`,
    "git add AI_STUDIO/WORKSPACE/GAMES.LOCAL.JSON",
    "git add :/ai_studio/workspace/games.local.json",
    "git add :(top)ai_studio/workspace/games.local.json",
    "git add ai_studio/workspace/*.json",
    "git add ai_studio/public/../workspace/games.local.json",
  ]) {
    const result = auditParentGitCommand(root, command);
    assert.equal(result.ok, false, command);
  }

  for (const command of [
    "git add -u games/public-game",
    "git add --update -- games/public-game",
    "git add -A games/public-game",
    "git add --all -- games/public-game",
    "git add -Af games/public-game",
    "git -C games/public-game add .",
  ]) {
    const result = auditParentGitCommand(root, command);
    assert.equal(result.ok, true, command);
  }
});

test("auditPrivateGamePreflight treats local registry path casing like the platform", () => {
  const result = auditPrivateGamePreflight([], {
    ignoredPaths: new Set(),
    trackedPaths: new Set(["AI_STUDIO/WORKSPACE/GAMES.LOCAL.JSON"]),
    stagedPaths: new Set(),
    gitlinks: new Set(),
    nestedGitRoots: new Set(),
  });

  assert.equal(result.ok, process.platform !== "win32");
  if (process.platform === "win32") {
    assert.match(result.violations.map((item) => item.reason).join("\n"), /local private registry is tracked or staged/);
  }
});

test("preflight CLI fails clearly when a private root is not ignored", (t) => {
  const root = tempRoot("ai-studio-workspace-games-cli-");
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  writeFileSync(join(root, ".gitignore"), `${localGameRegistryRelPath()}\n`, "utf8");
  writeLocalGames(root, [privateMount()]);
  mkdirSync(join(root, "games", "secret-game", ".git"), { recursive: true });
  writeFileSync(join(root, "games", "secret-game", "README.md"), "private game\n", "utf8");

  const result = spawnSync(process.execPath, [script, "preflight", "--root", root, "--json"], { encoding: "utf8" });

  assert.equal(result.status, 1, result.stdout || result.stderr);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.match(parsed.violations.map((item) => item.reason).join("\n"), /private root is not ignored/);
});
