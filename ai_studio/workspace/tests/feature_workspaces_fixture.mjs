// Shared Git fixtures for the feature workspace tests, split across files so
// node --test runs them in parallel.
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, symlinkSync, utimesSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { hostname, tmpdir } from "node:os";
import { createServer } from "node:net";

import {
  checkFeatureWorkspace,
  createFeatureWorkspace,
  defaultWorkspaceBase,
  forgetMissingFeatureWorkspaces,
  inspectCommittedTask,
  inspectRepository,
  normalizeWorkspaceName,
  parseCommandLine,
  listFeatureWorkspaces,
  prepareFeatureWorkspacePython,
  reallocateWorkspacePorts,
  recoverFeatureWorkspace,
  removeFeatureWorkspace,
  removeMergedFeatureWorkspaces,
} from "../feature_workspaces.mjs";


export function git(root, ...args) {
  return execFileSync("git", args, { cwd: root, encoding: "utf8" }).trim();
}

export function write(root, relativePath, contents) {
  const file = join(root, relativePath);
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, contents, "utf8");
}

export function taskDoc(id, status = "backlog", { project = "P001", epic = "E001" } = {}) {
  return `---\nid: ${id}\ntitle: Fixture task\nstatus: ${status}\nproject: ${project}\nepic: ${epic}\npriority: P2\ncreated: 2026-01-01\nupdated: 2026-01-01\n---\n\n## What\n\nFixture.\n\n## Done when\n\n- [ ] Fixture is complete.\n\n## Log\n\n- 2026-01-01: Fixture created.\n`;
}

export function repoFixture() {
  const root = realpathSync.native(mkdtempSync(join(tmpdir(), "feature-workspace-contract-")));
  git(root, "init", "-b", "main");
  git(root, "config", "user.name", "Fixture");
  git(root, "config", "user.email", "fixture@example.invalid");
  write(root, ".ai_studio/taskboard/items/projects/P001.md", "---\nid: P001\ntitle: Fixture\nstatus: active\nkind: game\ntarget: games/private/fixture-game\n---\n");
  write(root, ".ai_studio/taskboard/items/epics/E001.md", "---\nid: E001\ntitle: Fixture epic\nstatus: active\nproject: P001\n---\n");
  write(root, ".ai_studio/taskboard/items/active/T0001.md", taskDoc("T0001"));
  write(root, ".ai_studio/taskboard/items/.counters.json", '{"P":1,"E":1,"T":1}\n');
  write(root, "tracked.txt", "committed\n");
  git(root, "add", ".");
  git(root, "commit", "-m", "fixture");
  return root;
}

export function pairFixture({ withPython = false } = {}) {
  const fixtureRoot = realpathSync.native(mkdtempSync(join(tmpdir(), "feature-workspace-create-")));
  const engineRoot = join(fixtureRoot, "engine-source");
  mkdirSync(engineRoot);
  git(engineRoot, "init", "-b", "main");
  git(engineRoot, "config", "user.name", "Fixture");
  git(engineRoot, "config", "user.email", "fixture@example.invalid");
  write(engineRoot, "engine.txt", "engine\n");
  write(engineRoot, ".gitattributes", "missing.bin filter=lfs diff=lfs merge=lfs -text\n");
  write(engineRoot, "missing.bin", "version https://git-lfs.github.com/spec/v1\noid sha256:0000000000000000000000000000000000000000000000000000000000000000\nsize 1\n");
  git(engineRoot, "add", ".");
  git(engineRoot, "commit", "-m", "engine base");
  write(engineRoot, "engine-next.txt", "engine gitlink\n");
  git(engineRoot, "add", ".");
  git(engineRoot, "commit", "-m", "engine gitlink");
  const engineCommit = git(engineRoot, "rev-parse", "HEAD");

  const studioRoot = join(fixtureRoot, "studio");
  mkdirSync(studioRoot);
  git(studioRoot, "init", "-b", "main");
  git(studioRoot, "config", "user.name", "Fixture");
  git(studioRoot, "config", "user.email", "fixture@example.invalid");
  write(studioRoot, ".gitignore", "games/private/\n.vite/\n.venv/\n__pycache__/\nbuild/\n**/src/generated/\n**/release/artifacts/\n");
  write(studioRoot, ".gitmodules", '[submodule "external/neotolis-engine"]\n\tpath = external/neotolis-engine\n\turl = https://invalid.example/engine.git\n');
  write(studioRoot, "studio.txt", "studio\n");
  if (withPython) {
    const interpreterParts = process.platform === "win32" ? [".venv", "Scripts", "python.exe"] : [".venv", "bin", "python"];
    write(studioRoot, "ai_studio/dev_environment/python_setup.mjs", `import { mkdirSync, writeFileSync } from "node:fs";\nimport { dirname, join } from "node:path";\nconst root = process.cwd();\nconst python = join(root, ...${JSON.stringify(interpreterParts)});\nmkdirSync(dirname(python), { recursive: true });\nwriteFileSync(join(root, ".venv", "pyvenv.cfg"), "home = fixture\\n", "utf8");\nwriteFileSync(python, "fixture\\n", "utf8");\n`);
    write(studioRoot, "ai_studio/dev_environment/python_check.mjs", `import { existsSync } from "node:fs";\nimport { join } from "node:path";\nconst root = process.cwd();\nconst ready = existsSync(join(root, ".venv", "pyvenv.cfg")) && existsSync(join(root, ...${JSON.stringify(interpreterParts)}));\nconsole.log(JSON.stringify({ python: "3.12.4", prefix_ok: ready }));\nprocess.exitCode = ready ? 0 : 1;\n`);
  }
  git(studioRoot, "add", ".");
  git(studioRoot, "update-index", "--add", "--cacheinfo", `160000,${engineCommit},external/neotolis-engine`);
  git(studioRoot, "commit", "-m", "studio");
  mkdirSync(join(studioRoot, "external"), { recursive: true });
  execFileSync("git", ["clone", "--quiet", engineRoot, join(studioRoot, "external/neotolis-engine")], {
    env: { ...process.env, GIT_LFS_SKIP_SMUDGE: "1" },
  });
  if (withPython) {
    write(studioRoot, ".venv/pyvenv.cfg", `home = fixture\nexecutable = ${process.execPath}\n`);
  }

  const gameRoot = join(studioRoot, "games/private/fixture-game");
  mkdirSync(gameRoot, { recursive: true });
  git(gameRoot, "init", "-b", "main");
  git(gameRoot, "config", "user.name", "Fixture");
  git(gameRoot, "config", "user.email", "fixture@example.invalid");
  write(gameRoot, "game.json", '{"schema":"ai_studio.game.v1","id":"fixture-game","title":"Fixture Game","storageNamespace":"fixture-game"}\n');
  write(gameRoot, "dependencies.json", `${JSON.stringify({
    schema: "ai_studio.game.dependencies.v3",
    engine: { source: "external/neotolis-engine", version: "0.1.0", revision: engineCommit, compatibility: "fixture" },
    features: [],
    compatibility: "fixture",
  }, null, 2)}\n`);
  write(gameRoot, ".ai_studio/taskboard/items/projects/P001.md", "---\nid: P001\ntitle: Fixture\nstatus: active\nkind: game\ntarget: games/private/fixture-game\npriority: P2\ncreated: 2026-01-01\nupdated: 2026-01-01\n---\n\n## Goal\n\nFixture goal.\n\n## In scope\n\n- Fixture work.\n\n## Out of scope\n\n- Production work.\n\n## Log\n\n- 2026-01-01: Fixture created.\n");
  write(gameRoot, ".ai_studio/taskboard/items/epics/E001.md", "---\nid: E001\ntitle: Fixture epic\nstatus: active\nproject: P001\npriority: P2\ncreated: 2026-01-01\nupdated: 2026-01-01\n---\n\n## Goal\n\nFixture epic goal.\n\n## In scope\n\n- Fixture task.\n\n## Out of scope\n\n- Other tasks.\n\n## Log\n\n- 2026-01-01: Fixture created.\n");
  write(gameRoot, ".ai_studio/taskboard/items/active/T0001.md", taskDoc("T0001"));
  write(gameRoot, ".ai_studio/taskboard/items/active/T0002.md", taskDoc("T0002"));
  write(gameRoot, ".ai_studio/taskboard/items/.counters.json", '{"P":1,"E":1,"T":2}\n');
  write(gameRoot, ".gitignore", "build/\n__pycache__/\nsrc/generated/\nrelease/artifacts/\n.ai_studio/evidence/\nassets/restricted/\n");
  write(gameRoot, "tracked.txt", "committed\n");
  git(gameRoot, "add", ".");
  git(gameRoot, "commit", "-m", "game");
  return { fixtureRoot, studioRoot, gameRoot, engineRoot, engineCommit, base: join(fixtureRoot, "workspaces") };
}

export function markRemovalInterrupted(base, name) {
  const activePath = join(base, ".feature-workspaces/active", `${name}.json`);
  const record = JSON.parse(readFileSync(activePath, "utf8"));
  record.state = "recovery-required";
  record.transactionMode = "remove";
  record.removalStartedAt = new Date().toISOString();
  writeFileSync(activePath, `${JSON.stringify(record, null, 2)}\n`, "utf8");
}

export function commitInEngine(engineRoot, message) {
  write(engineRoot, `${message}.txt`, `${message}\n`);
  git(engineRoot, "add", `${message}.txt`);
  git(engineRoot, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-m", message);
  return git(engineRoot, "rev-parse", "HEAD");
}
