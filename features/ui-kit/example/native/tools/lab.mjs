#!/usr/bin/env node
// The lab's command owner: configure, build, run and shoot the native UI Lab
// without letting compiler output into a chat transcript. Build logs go to a
// file; only failures print their tail.
//
//   node features/ui-kit/example/native/tools/lab.mjs build
//   node features/ui-kit/example/native/tools/lab.mjs run --scene upgrade --theme night
//   node features/ui-kit/example/native/tools/lab.mjs shot upgrade-phone --scene upgrade --size 390x844
//   node features/ui-kit/example/native/tools/lab.mjs shots            # every scene, desktop and phone
//   node features/ui-kit/example/native/tools/lab.mjs art              # regenerate the kit art for every theme
//   node features/ui-kit/example/native/tools/lab.mjs test             # the lab's own node tests
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const LAB = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const STUDIO = resolve(LAB, "..", "..", "..", "..");
const BUILD = join(LAB, "build", "native-debug");
const EXE = join(BUILD, "bin", process.platform === "win32" ? "ui_lab.exe" : "ui_lab");
const SHOTS = join(STUDIO, "tmp", "ui-lab");
const PYTHON = join(STUDIO, "ai_studio", "dev_environment", "python_run.mjs");
const THEMES = ["b", "forest", "ember", "night"];
const SCENES = ["hud", "upgrade", "result", "settings", "components", "themes"];

function run(cmd, args, { cwd = LAB, log } = {}) {
  const r = spawnSync(cmd, args, { cwd, encoding: "utf8", shell: process.platform === "win32" && cmd === "cmake" ? false : false });
  if (log) writeFileSync(log, (r.stdout || "") + (r.stderr || ""));
  if (r.status !== 0) {
    const text = (r.stdout || "") + (r.stderr || "");
    const lines = text.trim().split(/\r?\n/);
    console.error(`${cmd} ${args.join(" ")} exited ${r.status}`);
    console.error(lines.slice(-40).join("\n"));
    if (log) console.error(`full log: ${log}`);
    process.exit(r.status || 1);
  }
  return r;
}

function configure() {
  if (existsSync(join(BUILD, "CMakeCache.txt"))) return;
  mkdirSync(BUILD, { recursive: true });
  run("cmake", ["-S", LAB, "-B", BUILD, "-G", "Ninja", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_BUILD_TYPE=Debug"],
    { log: join(BUILD, "configure.log") });
}

function build() {
  if (THEMES.some((theme) => !existsSync(join(LAB, "assets", "ui", theme, "panel.png")))) art();
  configure();
  run("cmake", ["--build", BUILD], { log: join(BUILD, "build.log") });
  console.log(`built ${EXE}`);
}

function launch(args) {
  if (!existsSync(EXE)) build();
  const r = spawnSync(EXE, args, { cwd: dirname(EXE), encoding: "utf8" });
  if (r.status !== 0) {
    console.error((r.stdout || "") + (r.stderr || ""));
    process.exit(r.status || 1);
  }
  return r;
}

// PPM -> PNG through the studio's Python, so the C side stays dependency-free.
function toPng(ppm, png) {
  run("node", [PYTHON, "-c", `from PIL import Image; Image.open(r'${ppm}').save(r'${png}')`], { cwd: STUDIO });
}

function shot(name, args) {
  mkdirSync(SHOTS, { recursive: true });
  const ppm = join(SHOTS, `${name}.ppm`);
  const png = join(SHOTS, `${name}.png`);
  const r = launch(["--shot", ppm, ...args]);
  toPng(ppm, png);
  const wrote = (r.stdout || "").split(/\r?\n/).find((l) => l.includes("wrote")) || "";
  console.log(`${png} ${wrote.replace(/.*\(/, "(")}`);
  return png;
}

function shots() {
  const sizes = { desktop: "1280x800", phone: "390x844" };
  for (const scene of SCENES) {
    for (const [tag, size] of Object.entries(sizes)) {
      shot(`${scene}-${tag}`, ["--scene", scene, "--size", size]);
    }
  }
}

function art() {
  const gen = join(STUDIO, "features", "ui-kit", "tools", "gen_ui_kit.py");
  for (const theme of THEMES) {
    const sheet = theme === "b" ? join(STUDIO, "features", "ui-kit", "tokens", "studio_b.json") : join(LAB, "themes", `${theme}.json`);
    run("node", [PYTHON, gen, "--tokens", sheet, "--out", join(LAB, "assets", "ui", theme)], { cwd: STUDIO });
    console.log(`art: ${theme}`);
  }
}

function test() {
  const tests = readdirSync(join(LAB, "tests")).filter((f) => f.endsWith(".test.mjs")).map((f) => join(LAB, "tests", f));
  run("node", ["--test", ...tests], { cwd: STUDIO });
  console.log(`tests: ${tests.length} file(s) passed`);
}

const [command, ...rest] = process.argv.slice(2);
switch (command) {
  case "configure": configure(); break;
  case "build": build(); break;
  case "run": launch(rest); break;
  case "shot": shot(rest[0], rest.slice(1)); break;
  case "shots": shots(); break;
  case "art": art(); break;
  case "test": test(); break;
  default:
    console.error(readFileSync(fileURLToPath(import.meta.url), "utf8").split("\n").slice(1, 11).map((l) => l.replace(/^\/\/ ?/, "")).join("\n"));
    process.exit(2);
}
