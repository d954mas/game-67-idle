import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { scaffoldScene } from "../scripts/scaffold_scene.mjs";

const repositoryRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "../../..",
);

function createFixture() {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "scene-scaffold-"));
  fs.mkdirSync(path.join(root, "src"), { recursive: true });
  fs.writeFileSync(
    path.join(root, "src", "game_scenes.c"),
    '#include "game_scenes.h"\n/* scene-scaffold:includes */\nstatic const scene_descriptor_t k[] = {\n    /* scene-scaffold:catalog */\n};\n',
  );
  fs.writeFileSync(
    path.join(root, "CMakeLists.txt"),
    'add_executable(${GAME_TARGET}\n    # scene-scaffold:sources\n)\n# scene-scaffold:debug-sources\n',
  );
  return root;
}

function createRealTemplateCopy() {
  const source = path.join(repositoryRoot, "templates", "template");
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "scene-template-"));
  fs.mkdirSync(path.join(root, "src"), { recursive: true });
  for (const relative of ["CMakeLists.txt", path.join("src", "game_scenes.c")]) {
    fs.copyFileSync(path.join(source, relative), path.join(root, relative));
  }
  return root;
}

test("scaffold preflights, applies, and never creates a resource pack", () => {
  const root = createFixture();

  const preview = scaffoldScene({
    projectRoot: root,
    sceneId: "debug.recording",
    kind: "screen",
    debug: true,
  });
  assert.equal(preview.status, "would_create");
  assert.equal(fs.existsSync(path.join(root, "src", "scenes")), false);

  const applied = scaffoldScene({
    projectRoot: root,
    sceneId: "debug.recording",
    kind: "screen",
    debug: true,
    mode: "apply",
  });
  assert.equal(applied.status, "created");
  assert.equal(
    fs.existsSync(path.join(root, "src", "scenes", "debug_recording_scene.c")),
    true,
  );
  assert.equal(
    fs
      .readFileSync(path.join(root, "CMakeLists.txt"), "utf8")
      .includes("if(GAME_DEBUG_SCENES_ENABLED)"),
    true,
  );
  assert.match(
    fs.readFileSync(
      path.join(root, "src", "scenes", "debug_recording_scene.c"),
      "utf8",
    ),
    /game_debug_recording_scene_t/,
  );
  assert.match(
    fs.readFileSync(path.join(root, "src", "game_scenes.c"), "utf8"),
    /#if GAME_DEBUG_SCENES_ENABLED[\s\S]*\.id = "debug\.recording"[\s\S]*#endif/,
  );
  assert.equal(
    fs.readdirSync(root, { recursive: true }).some((entry) => String(entry).endsWith(".ntpack")),
    false,
  );

  if (process.env.SCENE_TEST_CC) {
    const compiler = process.env.SCENE_TEST_CC;
    const compileArgs = [
      "-std=c17",
      "-fsyntax-only",
      "-I",
      path.join(repositoryRoot, "features", "scenes-core", "include"),
      "-I",
      path.join(root, "src"),
      path.join(root, "src", "scenes", "debug_recording_scene.c"),
    ];
    const windowsBatch =
      process.platform === "win32" &&
      /\.(?:bat|cmd)$/i.test(compiler);
    const command = windowsBatch
      ? (process.env.ComSpec ?? "cmd.exe")
      : compiler;
    const commandArgs = windowsBatch
      ? [
          "/d",
          "/s",
          "/c",
          `call "${compiler.replaceAll('"', '""')}" ${compileArgs
            .map((arg) => `"${arg.replaceAll('"', '""')}"`)
            .join(" ")}`,
        ]
      : compileArgs;
    const compile = spawnSync(command, commandArgs, {
      encoding: "utf8",
      windowsVerbatimArguments: windowsBatch,
    });
    assert.equal(
      compile.status,
      0,
      `generated scene did not compile:\n${compile.stdout}${compile.stderr}`,
    );
  }

  const repeated = scaffoldScene({
    projectRoot: root,
    sceneId: "debug.recording",
    kind: "screen",
    debug: true,
    mode: "apply",
  });
  assert.equal(repeated.status, "already_exists");
});

test("scaffold composes every kind and debug policy with the real template", () => {
  const root = createRealTemplateCopy();
  try {
    const cases = [
      { sceneId: "chapter.select", kind: "screen", debug: false },
      { sceneId: "inventory", kind: "modal", debug: false },
      { sceneId: "debug.navigation", kind: "screen", debug: true },
      { sceneId: "debug.inspector", kind: "modal", debug: true },
    ];
    for (const scene of cases) {
      assert.equal(
        scaffoldScene({ projectRoot: root, ...scene, mode: "apply" }).status,
        "created",
      );
    }

    const gameScenes = fs.readFileSync(
      path.join(root, "src", "game_scenes.c"),
      "utf8",
    );
    const cmake = fs.readFileSync(path.join(root, "CMakeLists.txt"), "utf8");
    for (const scene of cases) {
      const name = scene.sceneId.replaceAll(".", "_");
      assert.match(
        gameScenes,
        new RegExp(`#include "scenes/${name}_scene\\.h"`),
      );
      assert.match(
        gameScenes,
        new RegExp(
          `\\.id = "${scene.sceneId.replaceAll(".", "\\.")}"[\\s\\S]*?` +
            `\\.kind = ${scene.kind === "modal" ? "SCENE_KIND_MODAL" : "SCENE_KIND_SCREEN"}`,
        ),
      );
      const source = `src/scenes/${name}_scene.c`;
      if (scene.debug) {
        assert.match(
          cmake,
          new RegExp(
            `if\\(GAME_DEBUG_SCENES_ENABLED\\)[\\s\\S]*?${source.replaceAll(".", "\\.")}`,
          ),
        );
        assert.match(
          gameScenes,
          new RegExp(
            `#if GAME_DEBUG_SCENES_ENABLED[\\s\\S]*?\\.id = "${scene.sceneId.replaceAll(".", "\\.")}"`,
          ),
        );
      } else {
        assert.match(cmake, new RegExp(`\\s+${source.replaceAll(".", "\\.")}`));
      }
    }
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test("scaffold rejects ids that cannot be embedded safely", () => {
  const root = createFixture();
  const before = fs.readFileSync(path.join(root, "src", "game_scenes.c"), "utf8");

  assert.throws(
    () =>
      scaffoldScene({
        projectRoot: root,
        sceneId: 'debug"\n.injected',
        mode: "apply",
      }),
    /scene id/,
  );
  assert.equal(fs.readFileSync(path.join(root, "src", "game_scenes.c"), "utf8"), before);
  assert.equal(fs.existsSync(path.join(root, "src", "scenes")), false);
});

test("scaffold rejects a digit-leading id before generating invalid C", () => {
  const root = createFixture();

  assert.throws(
    () =>
      scaffoldScene({
        projectRoot: root,
        sceneId: "1record",
        mode: "apply",
      }),
    /scene id/,
  );
  assert.equal(fs.existsSync(path.join(root, "src", "scenes")), false);
});

test("scaffold reports incomplete prior state instead of claiming idempotence", () => {
  const root = createFixture();
  const scenes = path.join(root, "src", "scenes");
  fs.mkdirSync(scenes, { recursive: true });
  fs.writeFileSync(path.join(scenes, "partial_scene.h"), "user content\n");

  assert.throws(
    () =>
      scaffoldScene({
        projectRoot: root,
        sceneId: "partial",
        mode: "apply",
      }),
    /incomplete scaffold state/,
  );
  assert.equal(fs.readFileSync(path.join(scenes, "partial_scene.h"), "utf8"), "user content\n");
  assert.equal(fs.existsSync(path.join(scenes, "partial_scene.c")), false);
});

test("scaffold rejects distinct ids that normalize to one C identifier", () => {
  const root = createFixture();
  scaffoldScene({
    projectRoot: root,
    sceneId: "foo-bar",
    mode: "apply",
  });

  assert.throws(
    () =>
      scaffoldScene({
        projectRoot: root,
        sceneId: "foo.bar",
        mode: "apply",
      }),
    /normalizes to .* already used by 'foo-bar'/,
  );
});

test("scaffold rolls back every destination when a commit rename fails", () => {
  const root = createFixture();
  const gameScenesPath = path.join(root, "src", "game_scenes.c");
  const cmakePath = path.join(root, "CMakeLists.txt");
  const beforeGameScenes = fs.readFileSync(gameScenesPath, "utf8");
  const beforeCmake = fs.readFileSync(cmakePath, "utf8");
  let renameCount = 0;

  assert.throws(
    () =>
      scaffoldScene({
        projectRoot: root,
        sceneId: "transaction",
        mode: "apply",
        renameSync(from, to) {
          renameCount += 1;
          if (renameCount === 3) {
            throw new Error("injected rename failure");
          }
          fs.renameSync(from, to);
        },
      }),
    /injected rename failure/,
  );

  assert.equal(fs.readFileSync(gameScenesPath, "utf8"), beforeGameScenes);
  assert.equal(fs.readFileSync(cmakePath, "utf8"), beforeCmake);
  assert.equal(
    fs.existsSync(path.join(root, "src", "scenes", "transaction_scene.h")),
    false,
  );
  assert.equal(
    fs.existsSync(path.join(root, "src", "scenes", "transaction_scene.c")),
    false,
  );
  assert.equal(fs.existsSync(path.join(root, "src", "scenes")), false);
});

test("scaffold removes a partial temporary file when staging write fails", () => {
  const root = createFixture();
  const gameScenesPath = path.join(root, "src", "game_scenes.c");
  const cmakePath = path.join(root, "CMakeLists.txt");
  const beforeGameScenes = fs.readFileSync(gameScenesPath, "utf8");
  const beforeCmake = fs.readFileSync(cmakePath, "utf8");

  assert.throws(
    () =>
      scaffoldScene({
        projectRoot: root,
        sceneId: "staging-failure",
        mode: "apply",
        writeFileSync(file) {
          fs.writeFileSync(file, "partial");
          throw new Error("injected staging write failure");
        },
      }),
    /injected staging write failure/,
  );

  assert.equal(fs.readFileSync(gameScenesPath, "utf8"), beforeGameScenes);
  assert.equal(fs.readFileSync(cmakePath, "utf8"), beforeCmake);
  assert.equal(
    fs.readdirSync(root, { recursive: true }).some(
      (entry) => String(entry).includes(".scene-scaffold-"),
    ),
    false,
  );
  assert.equal(fs.existsSync(path.join(root, "src", "scenes")), false);
});

test("scaffold preflight rejects missing anchors without writing files", () => {
  const root = createFixture();
  const gameScenesPath = path.join(root, "src", "game_scenes.c");
  fs.writeFileSync(gameScenesPath, "static const int no_anchor = 1;\n");

  assert.throws(
    () =>
      scaffoldScene({
        projectRoot: root,
        sceneId: "missing-anchor",
        mode: "apply",
      }),
    /expected exactly one/,
  );
  assert.equal(fs.existsSync(path.join(root, "src", "scenes")), false);
});
