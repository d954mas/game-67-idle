import assert from "node:assert/strict";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";

import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";

import { doctor, generate, generationRequest, loadKimodoConfig, parseOffsets, resolveInstall, resolveMap, retarget, runGeneratorSession } from "./client.mjs";
import { parseArgs } from "./cli.mjs";

async function fixture(t, { vulkan = false } = {}) {
  const root = await mkdtemp(join(tmpdir(), "kimodo-test-"));
  await mkdir(join(root, "ai_studio"), { recursive: true });
  await writeFile(join(root, "ai_studio", "studio.config.json"), JSON.stringify({ schema: "ai_studio.studio_config.v1", kimodoWorkRoot: "tmp/kimodo" }));
  const home = join(root, "local", "kimodo");
  for (const build of vulkan ? ["release", "vulkan"] : ["release"]) {
    await mkdir(join(home, "src", "build", build, "bin"), { recursive: true });
    await writeFile(join(home, "src", "build", build, "kmd-generate.exe"), "fixture");
    await writeFile(join(home, "src", "build", build, "bin", "ggml.dll"), "fixture");
  }
  await mkdir(join(home, "src", "scripts"), { recursive: true });
  await writeFile(join(home, "src", "scripts", "export_bvh.py"), "fixture");
  await mkdir(join(home, "weights", "models"), { recursive: true });
  for (const file of ["models/kimodo-soma-rp-v1.1-f32.gguf", "Llama-3-Kimodo-Q8_0.gguf", "tokenizer.gguf"]) {
    await writeFile(join(home, "weights", file), "fixture");
  }
  const blender = join(root, "blender.exe");
  await writeFile(blender, "fixture");
  t.after(() => rm(root, { recursive: true, force: true }));
  return { root, home, env: { KIMODO_HOME: home, KIMODO_BLENDER: blender } };
}

function fakeDependencies() {
  const calls = { sessions: [], blender: [] };
  return {
    calls,
    buildIdentity: async () => ({ kimodo_commit: "abc123", generator_sha256: "00", motion_model_bytes: 1, text_model_bytes: 1 }),
    runGeneratorSession: async (install, requests) => {
      calls.sessions.push({ install, requests });
      return requests.map(() => "OK\t90\t30");
    },
    runBlender: async (blender, args) => {
      calls.blender.push(args);
      if (args[0] === "preview") {
        const motionDir = args[args.indexOf("--motion-dir") + 1];
        await writeFile(join(motionDir, "motion.bvh"), "HIERARCHY");
        await writeFile(join(motionDir, "preview.png"), "png");
      } else {
        const outDir = args[args.indexOf("--out-dir") + 1];
        await writeFile(join(outDir, "clip.glb"), "glb");
        await writeFile(join(outDir, "sheet.png"), "png");
        await writeFile(join(outDir, "metrics.json"), JSON.stringify({ legs: { L: { slide_before_m: 0.5, slide_after_m: 0.0 }, R: { slide_before_m: 0.4, slide_after_m: 0.0 } } }));
      }
    },
  };
}

test("config defaults the install under LOCALAPPDATA and honours overrides", async (t) => {
  const { root } = await fixture(t);
  const config = loadKimodoConfig(root, { LOCALAPPDATA: "C:\\Local" });
  assert.equal(config.home, join("C:\\Local", "AIStudio", "tools", "kimodo"));
  assert.equal(config.workRoot, join(root, "tmp", "kimodo"));
  assert.equal(loadKimodoConfig(root, { KIMODO_HOME: "D:\\k", KIMODO_WORK_ROOT: "tmp/other" }).workRoot, join(root, "tmp", "other"));
});

test("install prefers the Vulkan build and cpu forces the CPU backend", async (t) => {
  const { root, env } = await fixture(t, { vulkan: true });
  const config = loadKimodoConfig(root, env);
  assert.equal(resolveInstall(config).backend, "vulkan");
  assert.equal(resolveInstall(config, { backend: "cpu" }).backend, "cpu");
  const cpuOnly = await fixture(t);
  assert.equal(resolveInstall(loadKimodoConfig(cpuOnly.root, cpuOnly.env)).build, "release");
});

test("only commercial SOMA checkpoints are accepted", async (t) => {
  const { root, env } = await fixture(t);
  assert.throws(() => resolveInstall(loadKimodoConfig(root, env), { model: "smplx-rp-v1" }), /--model must be one of/);
  assert.throws(() => resolveInstall(loadKimodoConfig(root, env), { backend: "cuda" }), /--backend/);
});

test("generation request validates prompts and ranges", () => {
  assert.throws(() => generationRequest({ prompts: [] }), /--prompt is required/);
  assert.throws(() => generationRequest({ prompts: ["walk"], frames: 301 }), /--frames/);
  assert.throws(() => generationRequest({ prompts: ["walk"], steps: 0 }), /--steps/);
  const request = generationRequest({ prompts: [" walk "] });
  assert.deepEqual(request.prompts, ["walk"]);
  assert.ok(request.frames >= 2 && request.frames <= 300);
});

test("doctor reports each missing piece", async (t) => {
  const { root, env } = await fixture(t);
  assert.equal((await doctor({ root, env })).ok, true);
  const broken = await doctor({ root, env: { ...env, KIMODO_BLENDER: join(root, "missing.exe") } });
  assert.equal(broken.ok, false);
  assert.deepEqual(broken.checks.filter((c) => !c.ok).map((c) => c.name), ["blender"]);
  assert.match(broken.hint, /README/);
});

test("generate batches prompts through one server session, records provenance, and reuses results", async (t) => {
  const { root, env } = await fixture(t);
  const deps = fakeDependencies();
  const first = await generate({ root, env, prompts: ["walk forward", "wave hello"], frames: 90 }, deps);
  assert.equal(deps.calls.sessions.length, 1);
  const [request] = deps.calls.sessions[0].requests;
  assert.equal(request.length, 6);
  assert.equal(request[4], 90);
  assert.equal(await readFile(request[5], "utf8"), "walk forward");
  assert.equal(deps.calls.blender.filter((args) => args[0] === "preview").length, 2);
  const provenance = JSON.parse(await readFile(first[0].provenance, "utf8"));
  assert.equal(provenance.origin, "ai");
  assert.equal(provenance.request.prompt, "walk forward");
  assert.match(provenance.licenses.motion_model, /NVIDIA Open Model License/);
  assert.notEqual(first[0].run_dir, first[1].run_dir);

  const again = await generate({ root, env, prompts: ["walk forward"], frames: 90 }, deps);
  assert.equal(again[0].cached, true);
  assert.equal(deps.calls.sessions.length, 1);
});

test("a failed clip is reported while finished clips keep their provenance", async (t) => {
  const { root, env } = await fixture(t);
  const deps = { ...fakeDependencies(), runGeneratorSession: async () => ["OK\t90\t30", "ERR\tbad prompt"] };
  await assert.rejects(generate({ root, env, prompts: ["walk", "fly"] }, deps), /1 of 2 clip\(s\):\n"fly": ERR\tbad prompt/);
  const again = await generate({ root, env, prompts: ["walk"] }, deps);
  assert.equal(again[0].cached, true);
});

test("blender exiting cleanly without its outputs is a failure, not a cached run", async (t) => {
  const { root, env } = await fixture(t);
  const deps = { ...fakeDependencies(), runBlender: async () => {} };
  await assert.rejects(generate({ root, env, prompts: ["walk"] }, deps), /without motion\.bvh, preview\.png/);
  const deps2 = fakeDependencies();
  const results = await generate({ root, env, prompts: ["walk"] }, deps2);
  assert.equal(results[0].cached, false);
});

test("duplicate prompts become one clip", async (t) => {
  const { root, env } = await fixture(t);
  const deps = fakeDependencies();
  const results = await generate({ root, env, prompts: ["walk", "walk "] }, deps);
  assert.equal(results.length, 1);
  assert.equal(deps.calls.sessions[0].requests.length, 1);
});

test("the server session skips stray output, keeps order, and survives a crash", async (t) => {
  const { root } = await fixture(t);
  const script = fileURLToPath(new URL("./fixtures/fake_generator.mjs", import.meta.url));
  const install = { generator: "fake", dllDir: root, motionModel: "m", textModel: "t", backend: "cpu" };
  const spawnImpl = (file, args, options) => spawn(process.execPath, [script, ...args], options);
  const requests = [1, 2, 3, 4].map((n) => [5, 10, 1, join(root, `run${n}`), 60 + n, "p.txt"]);
  const replies = await runGeneratorSession(install, requests, join(root, "gen.log"), spawnImpl);
  assert.equal(replies[0], "OK\t61\t30");
  assert.equal(replies[1], "ERR\tbad prompt");
  assert.match(replies[2], /^ERR\tkmd-generate (exited|closed)/);
  assert.match(replies[3], /^ERR\t/);
  assert.match(await readFile(join(root, "gen.log"), "utf8"), /\[stdout\] loading models/);
});

test("a generator that cannot start fails every request instead of throwing", async (t) => {
  const { root } = await fixture(t);
  const install = { generator: join(root, "missing.exe"), dllDir: root, motionModel: "m", textModel: "t", backend: "cpu" };
  const replies = await runGeneratorSession(install, [[5, 10, 1, root, 60, "p.txt"]], join(root, "gen.log"));
  assert.match(replies[0], /^ERR\t/);
});

test("retarget resolves bundled maps, records metrics, and caches by input", async (t) => {
  const { root, env } = await fixture(t);
  const runDir = join(root, "run");
  await mkdir(runDir, { recursive: true });
  await writeFile(join(runDir, "motion.bvh"), "HIERARCHY");
  const character = join(root, "hero.glb");
  await writeFile(character, "glb");
  const deps = fakeDependencies();

  const result = await retarget({ root, env, motion: runDir, character, map: "rgpoly", offsets: { "Arm.R": [0, 0, 10] } }, deps);
  assert.equal(result.cached, false);
  assert.deepEqual(result.foot_slide.L, { before_m: 0.5, after_m: 0.0 });
  const args = deps.calls.blender[0];
  assert.equal(JSON.parse(args[args.indexOf("--offsets") + 1])["Arm.R"][2], 10);
  assert.ok(!args.includes("--no-foot-lock"));
  const provenance = JSON.parse(await readFile(result.provenance, "utf8"));
  assert.equal(provenance.input.map.id, "rgpoly");
  assert.equal(provenance.origin, "ai");

  assert.equal((await retarget({ root, env, motion: runDir, character, map: "rgpoly", offsets: { "Arm.R": [0, 0, 10] } }, deps)).cached, true);
  const unlocked = await retarget({ root, env, motion: runDir, character, map: "rgpoly", footLock: false }, deps);
  assert.equal(unlocked.cached, false);
  assert.ok(deps.calls.blender.at(-1).includes("--no-foot-lock"));
});

test("retarget rejects missing inputs and unknown maps", async (t) => {
  const { root, env } = await fixture(t);
  await assert.rejects(retarget({ root, env, motion: join(root, "nope"), character: "x.glb", map: "rgpoly" }, fakeDependencies()), /motion not found/);
  assert.throws(() => resolveMap("no-such-rig"), /rig map not found/);
});

test("offsets and CLI arguments parse strictly", () => {
  assert.deepEqual(parseOffsets(["Arm.L=0,-12.5,4"]), { "Arm.L": [0, -12.5, 4] });
  assert.throws(() => parseOffsets(["Arm.L=1,2"]), /Bone=x,y,z/);
  const gen = parseArgs(["generate", "--prompt", "a", "--prompt", "b", "--frames", "60"]);
  assert.deepEqual(gen.options.prompts, ["a", "b"]);
  const rt = parseArgs(["retarget", "--motion", "m", "--character", "c.glb", "--map", "rgpoly", "--offset", "Arm.R=0,0,5", "--no-foot-lock"]);
  assert.equal(rt.options.footLock, false);
  assert.deepEqual(rt.options.offsets, { "Arm.R": [0, 0, 5] });
  assert.throws(() => parseArgs(["generate", "--map", "x"]), /unknown option/);
  assert.throws(() => parseArgs(["render"]), /usage/);
});
