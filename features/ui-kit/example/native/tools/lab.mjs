#!/usr/bin/env node
// The lab's command owner: configure, build, run and shoot the native UI Lab
// without letting compiler output into a chat transcript. Build logs go to a
// file; only failures print their tail.
//
//   node features/ui-kit/example/native/tools/lab.mjs configure --fresh   # drop CMakeCache.txt first
//   node features/ui-kit/example/native/tools/lab.mjs build
//   node features/ui-kit/example/native/tools/lab.mjs run --scene upgrade --theme night
//   node features/ui-kit/example/native/tools/lab.mjs shot upgrade-phone --scene upgrade --size 390x844
//   node features/ui-kit/example/native/tools/lab.mjs shots            # every scene, desktop and phone
//   node features/ui-kit/example/native/tools/lab.mjs art              # regenerate the kit art for every theme
//   node features/ui-kit/example/native/tools/lab.mjs test             # the lab's own node tests
import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const LAB = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const STUDIO = resolve(LAB, "..", "..", "..", "..");
const BUILD = join(LAB, "build", "native-debug");
const EXE = join(BUILD, "bin", process.platform === "win32" ? "ui_lab.exe" : "ui_lab");
const SHOTS = join(STUDIO, "tmp", "ui-lab");
const PYTHON = join(STUDIO, "ai_studio", "dev_environment", "python_run.mjs");
const GEN = join(STUDIO, "features", "ui-kit", "tools", "gen_ui_kit.py");
const THEMES = ["b", "forest", "ember", "night"];
const SCENES = ["hud", "upgrade", "result", "settings", "components", "themes"];

function run(cmd, args, { cwd = LAB, log } = {}) {
  const r = spawnSync(cmd, args, { cwd, encoding: "utf8" });
  if (log) writeFileSync(log, (r.stdout || "") + (r.stderr || ""));
  if (r.error) {
    console.error(r.error.message);
    process.exit(1);
  }
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

function configure(fresh) {
  const cache = join(BUILD, "CMakeCache.txt");
  if (fresh && existsSync(cache)) unlinkSync(cache);
  if (existsSync(cache)) return;
  mkdirSync(BUILD, { recursive: true });
  run("cmake", ["-S", LAB, "-B", BUILD, "-G", "Ninja", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_BUILD_TYPE=Debug"],
    { log: join(BUILD, "configure.log") });
}

// A theme is stale when its token sheet or the art generator changed since
// the last draw, not merely when the PNGs are missing.
function themeSheet(theme) {
  return theme === "b" ? join(STUDIO, "features", "ui-kit", "tokens", "studio_b.json") : join(LAB, "themes", `${theme}.json`);
}

function themeStamp(theme) {
  const hash = createHash("sha256");
  hash.update(readFileSync(themeSheet(theme)));
  hash.update(readFileSync(GEN));
  return hash.digest("hex");
}

function drawTheme(theme) {
  run("node", [PYTHON, GEN, "--tokens", themeSheet(theme), "--out", join(LAB, "assets", "ui", theme)], { cwd: STUDIO });
  writeFileSync(join(LAB, "assets", "ui", theme, ".stamp"), themeStamp(theme));
  console.log(`art: ${theme}`);
}

function staleThemes() {
  return THEMES.filter((theme) => {
    const stamp = join(LAB, "assets", "ui", theme, ".stamp");
    return !existsSync(stamp) || readFileSync(stamp, "utf8") !== themeStamp(theme);
  });
}

function build() {
  for (const theme of staleThemes()) drawTheme(theme);
  configure();
  run("cmake", ["--build", BUILD], { log: join(BUILD, "build.log") });
  console.log(`built ${EXE}`);
}

function launch(args) {
  if (!existsSync(EXE)) build();
  const r = spawnSync(EXE, args, { cwd: dirname(EXE), encoding: "utf8", timeout: 120000 });
  if (r.error || r.signal) {
    console.error(`ui_lab timed out or failed to start: ${r.error ? r.error.message : r.signal}`);
    process.exit(1);
  }
  if (r.status !== 0) {
    console.error((r.stdout || "") + (r.stderr || ""));
    process.exit(r.status || 1);
  }
  return r;
}

// PPM -> PNG through the studio's Python, so the C side stays dependency-free.
// Paths go through sys.argv, not string interpolation, so a path with a quote
// or backslash cannot break the -c script.
function toPng(ppm, png) {
  run("node", [PYTHON, "-c", "import sys\nfrom PIL import Image\nImage.open(sys.argv[1]).save(sys.argv[2])", ppm, png], { cwd: STUDIO });
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
  for (const theme of THEMES) drawTheme(theme);
}

function test() {
  const tests = readdirSync(join(LAB, "tests")).filter((f) => f.endsWith(".test.mjs")).map((f) => join(LAB, "tests", f));
  run("node", ["--test", ...tests], { cwd: STUDIO });
  console.log(`tests: ${tests.length} file(s) passed`);
}

const [command, ...rest] = process.argv.slice(2);
switch (command) {
  case "configure": configure(rest.includes("--fresh")); break;
  case "build": build(); break;
  case "run": launch(rest); break;
  case "shot": shot(rest[0], rest.slice(1)); break;
  case "shots": shots(); break;
  case "art": art(); break;
  case "test": test(); break;
  default:
    console.error(readFileSync(fileURLToPath(import.meta.url), "utf8").split("\n").slice(1, 12).map((l) => l.replace(/^\/\/ ?/, "")).join("\n"));
    process.exit(2);
}
