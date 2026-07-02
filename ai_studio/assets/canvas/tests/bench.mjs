// Canvas performance benchmark. This is a plain Node script, NOT a node:test
// file (no *.test.mjs suffix), so `node --test ai_studio/assets/canvas/tests/
// *.test.mjs` never runs it. Run it directly:
//   node ai_studio/assets/canvas/tests/bench.mjs
//
// Baselines every canvas metadata op (store + journal), journal/readHistory
// scaling, the Python-bridged region/slice/render ops (+ raw process-spawn
// overhead in isolation), and one HTTP round-trip — all against a throwaway
// CANVAS_PROJECTS_ROOT under the OS temp dir. Never touches the real projects
// root or the live :8780 server; uses its own :8784 for the HTTP measurement.
//
// Output: an aligned console table plus tmp/canvas_bench_<date>.json.
import { execFile, spawn } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { performance } from "node:perf_hooks";
import { fileURLToPath } from "node:url";

import {
  addImage,
  createGroup,
  createProject,
  detectRegions,
  exportElements,
  getProject,
  listProjects,
  patchElement,
  readHistory,
  redoOp,
  removeElement,
  renderGroup,
  sliceRegions,
  undoOp,
} from "../ops.mjs";
import { magentaSheetPng, solidPng } from "./png_fixture.mjs";

// raster2d runs Python with cwd = repo root and writes its session under
// <repoRoot>/tmp, exactly like tests/ops.test.mjs — so this must be the real
// repo root, not the temp projects root.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const HTTP_PORT = 8784;
const WARMUPS = 3;
const RUNS = 20;
const FIXTURE_SIZES = [5, 25, 100];

const PROJECTS_ROOT = mkdtempSync(join(tmpdir(), "canvas-bench-"));
process.env.CANVAS_PROJECTS_ROOT = PROJECTS_ROOT;

const results = [];
const raster2dSessions = new Set();
let pythonAvailable = true;
let pythonSkipReason = "";

function cleanup() {
  for (const sessionId of raster2dSessions) {
    if (sessionId) {
      rmSync(join(REPO_ROOT, "tmp", "ai_studio", "assets", "raster2d", sessionId), { recursive: true, force: true });
    }
  }
  rmSync(PROJECTS_ROOT, { recursive: true, force: true });
}

// ---- timing helpers -----------------------------------------------------------

function median(samples) {
  const sorted = [...samples].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  return sorted.length % 2 ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2;
}

function p95(samples) {
  const sorted = [...samples].sort((a, b) => a - b);
  const idx = Math.min(sorted.length - 1, Math.ceil(0.95 * sorted.length) - 1);
  return sorted[idx];
}

// Runs `before(i)` (untimed setup) -> `op(i)` (TIMED) -> `after(i)` (untimed
// teardown/restore) for `warmups + runs` iterations; only the last `runs`
// iterations are recorded. Works uniformly for sync and async op/before/after.
async function bench(group, opName, fixture, { before, op, after, warmups = WARMUPS, runs = RUNS }) {
  const total = warmups + runs;
  const samples = [];
  for (let i = 0; i < total; i += 1) {
    if (before) await before(i);
    const t0 = performance.now();
    await op(i);
    const dt = performance.now() - t0;
    if (after) await after(i);
    if (i >= warmups) samples.push(dt);
  }
  const row = { group, op: opName, fixture, medianMs: median(samples), p95Ms: p95(samples), samples, runs: samples.length };
  results.push(row);
  return row;
}

async function withScratchRoot(fn) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-bench-scratch-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  try {
    return await fn(dir);
  } finally {
    process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  }
}

// ---- fixtures -------------------------------------------------------------

function tinyPng(seed) {
  // Distinct-enough bytes per element (store dedups identical content), so each
  // addImage/removeElement exercises a real file write, not a dedup shortcut.
  return solidPng(4, 3, [seed % 256, (seed * 7) % 256, (seed * 13) % 256]);
}

function buildProject(title, count) {
  const project = createProject(REPO_ROOT, { title });
  for (let i = 0; i < count; i += 1) {
    addImage(REPO_ROOT, project.id, { name: `img_${i}.png`, bytes: tinyPng(i), x: i * 10, y: 0 });
  }
  return getProject(REPO_ROOT, project.id);
}

// ---- metadata ops -----------------------------------------------------------

async function benchMetadataOps() {
  console.log("== metadata ops ==");

  let createCounter = 0;
  await bench("metadata", "createProject", "-", {
    op: () => {
      createCounter += 1;
      createProject(REPO_ROOT, { title: `Bench Create ${createCounter}` });
    },
  });

  for (const size of FIXTURE_SIZES) {
    const base = buildProject(`AddImage Base ${size}`, size);
    let n = 0;
    await bench("metadata", "addImage", `${size} elements`, {
      op: () => {
        n += 1;
        addImage(REPO_ROOT, base.id, { name: `extra_${n}.png`, bytes: tinyPng(1000 + n) });
      },
    });
  }

  for (const size of FIXTURE_SIZES) {
    const base = buildProject(`Move Base ${size}`, size);
    const targetId = base.elements[0].id;
    let n = 0;
    await bench("metadata", "patchElement (move)", `${size} elements`, {
      op: () => {
        n += 1;
        patchElement(REPO_ROOT, base.id, targetId, { x: n, y: n * 2 });
      },
    });
  }

  for (const size of FIXTURE_SIZES) {
    const base = buildProject(`Remove Base ${size}`, size);
    const extraIds = [];
    for (let i = 0; i < WARMUPS + RUNS; i += 1) {
      const { element } = addImage(REPO_ROOT, base.id, { name: `doomed_${i}.png`, bytes: tinyPng(2000 + i) });
      extraIds.push(element.id);
    }
    let idx = 0;
    await bench("metadata", "removeElement", `~${size} elements`, {
      op: () => {
        removeElement(REPO_ROOT, base.id, extraIds[idx]);
        idx += 1;
      },
    });
  }

  for (const size of FIXTURE_SIZES) {
    const base = buildProject(`Undo Base ${size}`, size); // history_seq === size (one addImage per entry)
    await bench("metadata", "undoOp", `${size} elements`, {
      op: () => undoOp(REPO_ROOT, { projectId: base.id }),
      after: () => redoOp(REPO_ROOT, { projectId: base.id }), // restore full state before the next undo
    });
  }

  for (const size of FIXTURE_SIZES) {
    const base = buildProject(`Redo Base ${size}`, size);
    await bench("metadata", "redoOp", `${size} elements`, {
      before: () => undoOp(REPO_ROOT, { projectId: base.id }), // create a redo-able state
      op: () => redoOp(REPO_ROOT, { projectId: base.id }),
    });
  }

  for (const size of FIXTURE_SIZES) {
    const base = buildProject(`GetProject ${size}`, size);
    await bench("metadata", "getProject", `${size} elements`, {
      op: () => getProject(REPO_ROOT, base.id),
    });
  }

  await withScratchRoot(async () => {
    for (let i = 0; i < 30; i += 1) createProject(REPO_ROOT, { title: `List Filler ${i}` });
    await bench("metadata", "listProjects", "30 projects", {
      op: () => listProjects(REPO_ROOT),
    });
  });

  const exportBase = buildProject("Export Base", 10);
  const exportIds = exportBase.elements.map((element) => element.id);
  await bench("metadata", "exportElements", "10 elements", {
    op: () => exportElements(REPO_ROOT, { projectId: exportBase.id, elementIds: exportIds }),
  });
}

// ---- journal scaling (REALISTIC snapshots) ----------------------------------
//
// The research showed empty-snapshot fixtures under-report ~40x, so this runs on a
// 100-element project: every journaled mutation now carries a realistic ~100-element
// snapshot (in a sidecar), and the thin journal line stays tiny. Compaction is
// DISABLED here (CANVAS_HISTORY_DEPTH=0) so the journal genuinely reaches 1000
// entries — isolating the sidecar-vs-fat-line win; default operation caps at
// canvasHistoryDepth (200), which bounds this further in real use. Measures append
// (patchElement) + readHistory at 100/500/1000 entries and undo at 100 elements.

async function benchJournalScaling() {
  console.log("== journal scaling (100-element project, realistic snapshots) ==");
  const prevDepth = process.env.CANVAS_HISTORY_DEPTH;
  process.env.CANVAS_HISTORY_DEPTH = "0"; // unlimited: let the journal reach 1000 entries
  try {
    const base = buildProject("Journal Scaling 100", 100); // 100 realistic addImage snapshots
    const targetId = base.elements[0].id;
    let journalLen = 100; // one addImage entry per element
    let x = 0;
    for (const target of [100, 500, 1000]) {
      while (journalLen < target) {
        x += 1;
        patchElement(REPO_ROOT, base.id, targetId, { x, y: 0 });
        journalLen += 1;
      }
      // Append cost: one more realistic-snapshot mutation at this journal length.
      await bench("journal", "append (patch, 100 el)", `${journalLen} entries`, {
        op: () => {
          x += 1;
          patchElement(REPO_ROOT, base.id, targetId, { x, y: 0 });
          journalLen += 1;
        },
      });
      await bench("journal", "readHistory (100 el)", `${journalLen} entries`, {
        op: () => readHistory(REPO_ROOT, { projectId: base.id }),
      });
    }
    // Undo cost on a 100-element project (restore via redo so each run is comparable).
    await bench("journal", "undoOp (100 el)", `${journalLen} entries`, {
      op: () => undoOp(REPO_ROOT, { projectId: base.id }),
      after: () => redoOp(REPO_ROOT, { projectId: base.id }),
    });
  } finally {
    if (prevDepth === undefined) delete process.env.CANVAS_HISTORY_DEPTH;
    else process.env.CANVAS_HISTORY_DEPTH = prevDepth;
  }
}

// ---- python-bridged ops (3 runs each, no warmup — the spawn IS the thing we
// are measuring) --------------------------------------------------------------

async function benchPythonOps() {
  console.log("== python-bridged ops ==");
  const pyProject = createProject(REPO_ROOT, { title: "Python Bench" });
  const { element: sheetEl } = addImage(REPO_ROOT, pyProject.id, { name: "sheet.png", bytes: magentaSheetPng() });

  try {
    await bench("python", "detectRegions", "magenta sheet (64x48)", {
      warmups: 0,
      runs: 3,
      op: async () => {
        const res = await detectRegions(REPO_ROOT, { projectId: pyProject.id, elementId: sheetEl.id });
        if (res.run.result_summary.session_id) raster2dSessions.add(res.run.result_summary.session_id);
      },
    });
  } catch (error) {
    pythonAvailable = false;
    pythonSkipReason = error.message;
    console.log(`  skip: raster2d/python pipeline unavailable — ${error.message}`);
  }

  if (pythonAvailable) {
    try {
      await bench("python", "sliceRegions", "magenta sheet regions", {
        warmups: 0,
        runs: 3,
        op: async () => {
          const res = await sliceRegions(REPO_ROOT, { projectId: pyProject.id, elementId: sheetEl.id });
          if (res.run.result_summary.session_id) raster2dSessions.add(res.run.result_summary.session_id);
        },
      });
    } catch (error) {
      console.log(`  skip: sliceRegions unavailable — ${error.message}`);
    }

    try {
      const renderProject = createProject(REPO_ROOT, { title: "Render Bench" });
      const { element: chipEl } = addImage(REPO_ROOT, renderProject.id, {
        name: "chip.png",
        bytes: solidPng(16, 16, [220, 40, 40]),
      });
      const { group } = createGroup(REPO_ROOT, { projectId: renderProject.id, name: "Screen", fromElements: [chipEl.id] });
      await bench("python", "renderGroup", "1-element group", {
        warmups: 0,
        runs: 3,
        op: () => renderGroup(REPO_ROOT, { projectId: renderProject.id, groupId: group.id, scale: 1 }),
      });
    } catch (error) {
      console.log(`  skip: renderGroup unavailable — ${error.message}`);
    }
  }

  // Isolate raw process-spawn + interpreter/import overhead: run the
  // region-detect entry point with --help, which argparse serves before any
  // real work — but the module-level `import numpy` / `from PIL import ...`
  // still pay their full cost. Comparing this to the detectRegions number above
  // splits "process startup" from "actual work".
  try {
    await bench("python", "python spawn (detect_regions.py --help)", "n/a", {
      warmups: 0,
      runs: 3,
      op: () => runPythonRaw(["ai_studio/assets/tools/raster2d/regions/detect_regions.py", "--help"]),
    });
  } catch (error) {
    console.log(`  skip: python spawn overhead unavailable — ${error.message}`);
  }
}

// Minimal duplicate of ops.mjs's Python discovery, kept local on purpose: this
// bench file must not edit ops.mjs or any tools/** file, only read/invoke them.
function pythonCandidates() {
  const candidates = [];
  const add = (command, args = []) => {
    if (command && !candidates.some((candidate) => candidate.command === command)) candidates.push({ command, args });
  };
  for (const command of [process.env.AI_STUDIO_PYTHON, process.env.PYTHON]) add(command);
  const bundled = process.env.USERPROFILE
    ? join(process.env.USERPROFILE, ".cache", "codex-runtimes", "codex-primary-runtime", "dependencies", "python", "python.exe")
    : "";
  if (bundled && existsSync(bundled)) add(bundled);
  add("py", ["-3.12"]);
  for (const command of ["C:\\Python312\\python.exe", "C:\\Python314\\python.exe"]) {
    if (existsSync(command)) add(command);
  }
  add("python");
  return candidates;
}

function runPythonRaw(args) {
  return new Promise((resolveRun, rejectRun) => {
    const candidates = pythonCandidates();
    const failures = [];
    const tryCandidate = (index) => {
      if (index >= candidates.length) {
        rejectRun(new Error(failures.filter(Boolean).at(-1) || "no python candidate available"));
        return;
      }
      const candidate = candidates[index];
      execFile(candidate.command, [...candidate.args, ...args], { cwd: REPO_ROOT, windowsHide: true }, (error, stdout, stderr) => {
        if (!error) {
          resolveRun(stdout);
          return;
        }
        failures.push((stderr || stdout || error.message).trim());
        tryCandidate(index + 1);
      });
    };
    tryCandidate(0);
  });
}

// ---- HTTP overhead ------------------------------------------------------------

async function waitForServer(url, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      await fetch(url);
      return;
    } catch {
      // not up yet
    }
    await new Promise((r) => setTimeout(r, 150));
  }
  throw new Error(`server at ${url} did not become ready within ${timeoutMs}ms`);
}

async function benchHttpOverhead() {
  console.log("== HTTP overhead ==");
  const child = spawn(process.execPath, ["ai_studio/studio_shell/server.mjs", String(HTTP_PORT)], {
    cwd: REPO_ROOT,
    env: { ...process.env, CANVAS_PROJECTS_ROOT: PROJECTS_ROOT },
    stdio: ["ignore", "pipe", "pipe"],
  });
  let stderrText = "";
  child.stderr.on("data", (chunk) => {
    stderrText += chunk.toString();
  });

  try {
    await waitForServer(`http://127.0.0.1:${HTTP_PORT}/`, 15000);

    const httpProject = createProject(REPO_ROOT, { title: "HTTP Bench" });
    const { element: httpEl } = addImage(REPO_ROOT, httpProject.id, { name: "http.png", bytes: tinyPng(1) });
    let n = 0;
    await bench("http", "PATCH element (end-to-end)", "1 element", {
      op: async () => {
        n += 1;
        const res = await fetch(
          `http://127.0.0.1:${HTTP_PORT}/api/canvas/projects/${httpProject.id}/elements/${httpEl.id}`,
          {
            method: "PATCH",
            headers: { "content-type": "application/json" },
            body: JSON.stringify({ x: n, y: n }),
          },
        );
        if (!res.ok) throw new Error(`PATCH failed: ${res.status}`);
        await res.json();
      },
    });
  } catch (error) {
    console.log(`  skip: HTTP bench unavailable — ${error.message}${stderrText ? ` (stderr: ${stderrText.trim()})` : ""}`);
  } finally {
    child.kill();
    await new Promise((r) => {
      child.once("exit", r);
      setTimeout(r, 2000);
    });
  }
}

// ---- report -------------------------------------------------------------------

function formatMs(value) {
  return value === null || value === undefined || Number.isNaN(value) ? "-" : value.toFixed(2);
}

function printTable() {
  const headers = ["group", "op", "fixture", "median ms", "p95 ms"];
  const rows = results.map((row) => [row.group, row.op, row.fixture, formatMs(row.medianMs), formatMs(row.p95Ms)]);
  const widths = headers.map((header, col) => Math.max(header.length, ...rows.map((row) => row[col].length)));
  const line = (cells) => cells.map((cell, col) => cell.padEnd(widths[col])).join("  |  ");
  console.log("\n" + line(headers));
  console.log(widths.map((w) => "-".repeat(w)).join("--|--"));
  for (const row of rows) console.log(line(row));

  const pythonRows = results.filter((row) => row.group === "python");
  if (pythonRows.length) {
    console.log("\npython-path raw samples (ms, 3 runs each):");
    for (const row of pythonRows) {
      console.log(`  ${row.op} (${row.fixture}): ${row.samples.map((s) => s.toFixed(2)).join(", ")}`);
    }
  }
}

async function main() {
  await benchMetadataOps();
  await benchJournalScaling();
  await benchPythonOps();
  await benchHttpOverhead();

  printTable();

  const outPath = join(REPO_ROOT, "tmp", "canvas_bench_2026-07-02.json");
  mkdirSync(join(REPO_ROOT, "tmp"), { recursive: true });
  const payload = {
    generatedAt: new Date().toISOString(),
    node: process.version,
    platform: process.platform,
    warmups: WARMUPS,
    runs: RUNS,
    pythonAvailable,
    pythonSkipReason: pythonAvailable ? null : pythonSkipReason,
    httpPort: HTTP_PORT,
    results,
  };
  writeFileSync(outPath, `${JSON.stringify(payload, null, 2)}\n`);
  console.log(`\nwrote ${outPath}`);
}

try {
  await main();
} catch (error) {
  console.error("bench failed:", error);
  process.exitCode = 1;
} finally {
  cleanup();
}
