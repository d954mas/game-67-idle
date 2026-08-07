import assert from "node:assert/strict";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";

import { buildPlan, doctor, loadHunyuanConfig, runGeneration } from "./client.mjs";
import { parseArgs } from "./cli.mjs";

async function fixture(t) {
  const root = await mkdtemp(join(tmpdir(), "hunyuan3d-test-"));
  await mkdir(join(root, "ai_studio"), { recursive: true });
  await writeFile(join(root, "ai_studio", "studio.config.json"), JSON.stringify({
    schema: "ai_studio.studio_config.v1",
    hunyuan3dWorkRoot: "tmp/hunyuan3d",
    hunyuan3dUrl: "http://127.0.0.1:8081",
  }));
  const home = join(root, "local", "Hunyuan3D-2");
  await mkdir(join(home, ".venv", "Scripts"), { recursive: true });
  await writeFile(join(home, ".venv", "Scripts", "python.exe"), "fixture");
  await writeFile(join(home, "api_server.py"), "fixture");
  t.after(() => rm(root, { recursive: true, force: true }));
  return { root, home, env: { HUNYUAN3D_HOME: home } };
}

function glbFixture() {
  const json = Buffer.from(JSON.stringify({ asset: { version: "2.0" } }), "utf8");
  const jsonLength = Math.ceil(json.length / 4) * 4;
  const buffer = Buffer.alloc(20 + jsonLength, 0x20);
  buffer.write("glTF", 0, "ascii");
  buffer.writeUInt32LE(2, 4);
  buffer.writeUInt32LE(buffer.length, 8);
  buffer.writeUInt32LE(jsonLength, 12);
  buffer.writeUInt32LE(0x4e4f534a, 16);
  json.copy(buffer, 20);
  return buffer;
}

function headerOnlyGlbFixture() {
  const buffer = Buffer.alloc(12);
  buffer.write("glTF", 0, "ascii");
  buffer.writeUInt32LE(2, 4);
  buffer.writeUInt32LE(buffer.length, 8);
  return buffer;
}

function texturedGlbFixture() {
  const json = Buffer.from(JSON.stringify({
    asset: { version: "2.0" },
    materials: [{ pbrMetallicRoughness: { baseColorTexture: { index: 0 } } }],
    textures: [{ source: 0 }],
    images: [{ bufferView: 0, mimeType: "image/png" }],
    bufferViews: [{ buffer: 0, byteOffset: 0, byteLength: 4 }],
    buffers: [{ byteLength: 4 }],
  }), "utf8");
  const jsonLength = Math.ceil(json.length / 4) * 4;
  const binLength = 4;
  const buffer = Buffer.alloc(12 + 8 + jsonLength + 8 + binLength, 0);
  buffer.write("glTF", 0, "ascii");
  buffer.writeUInt32LE(2, 4);
  buffer.writeUInt32LE(buffer.length, 8);
  buffer.writeUInt32LE(jsonLength, 12);
  buffer.writeUInt32LE(0x4e4f534a, 16);
  json.copy(buffer, 20);
  buffer.fill(0x20, 20 + json.length, 20 + jsonLength);
  const binHeader = 20 + jsonLength;
  buffer.writeUInt32LE(binLength, binHeader);
  buffer.writeUInt32LE(0x004e4942, binHeader + 4);
  buffer.set([137, 80, 78, 71], binHeader + 8);
  return buffer;
}

function declaredButUnlinkedTextureGlbFixture() {
  const json = Buffer.from(JSON.stringify({
    asset: { version: "2.0" },
    materials: [{}],
    textures: [{ source: 0 }],
    images: [{ bufferView: 0, mimeType: "image/png" }],
  }), "utf8");
  const jsonLength = Math.ceil(json.length / 4) * 4;
  const buffer = Buffer.alloc(20 + jsonLength, 0x20);
  buffer.write("glTF", 0, "ascii");
  buffer.writeUInt32LE(2, 4);
  buffer.writeUInt32LE(buffer.length, 8);
  buffer.writeUInt32LE(jsonLength, 12);
  buffer.writeUInt32LE(0x4e4f534a, 16);
  json.copy(buffer, 20);
  return buffer;
}

test("plan fingerprints source bytes and contains no base64 payload", async (t) => {
  const { root } = await fixture(t);
  const image = join(root, "reference.png");
  await writeFile(image, "png fixture");
  const plan = await buildPlan("image", { image });
  assert.equal(plan.estimated_credits, 0);
  assert.equal(plan.request.num_inference_steps, 5);
  assert.equal(plan.input.bytes, 11);
  assert.doesNotMatch(JSON.stringify(plan), /cG5nIGZpeHR1cmU=/);
});

test("local adapter rejects non-loopback URLs", async (t) => {
  const { root, env } = await fixture(t);
  assert.throws(
    () => loadHunyuanConfig(root, { ...env, HUNYUAN3D_URL: "https://example.com" }),
    /must stay on loopback/,
  );
});

test("doctor reports the isolated install and ready API", async (t) => {
  const { root, env } = await fixture(t);
  const result = await doctor({
    root,
    env,
    fetchImpl: async () => ({ ok: true, status: 200 }),
  });
  assert.equal(result.ok, true);
  assert.equal(result.install.python_exists, true);
  assert.equal(result.api.reachable, true);
});

test("run is explicit, validates GLB, writes provenance, and caches", async (t) => {
  const { root, env } = await fixture(t);
  const image = join(root, "reference.png");
  await writeFile(image, "png fixture");
  let calls = 0;
  const glb = glbFixture();
  const fetchImpl = async (_url, options) => {
    calls += 1;
    const request = JSON.parse(options.body);
    assert.equal(Buffer.from(request.image, "base64").toString(), "png fixture");
    assert.equal(request.num_inference_steps, 5);
    return { ok: true, status: 200, arrayBuffer: async () => glb };
  };
  await assert.rejects(
    runGeneration("image", { image }, { root, env, fetchImpl }),
    /pass --execute/,
  );
  assert.equal(calls, 0);
  const first = await runGeneration("image", { image, execute: true }, {
    root, env, fetchImpl, now: () => new Date("2026-08-05T00:00:00.000Z"),
  });
  assert.deepEqual(await readFile(first.model), glb);
  const provenance = JSON.parse(await readFile(first.provenance, "utf8"));
  assert.equal(provenance.origin, "ai");
  assert.equal(provenance.output.textured, false);
  assert.equal(provenance.input.sha256.length, 64);
  const cached = await runGeneration("image", { image, execute: true }, { root, env, fetchImpl });
  assert.equal(cached.cached, true);
  assert.equal(calls, 1);
});

test("run rejects a successful HTTP response that is not GLB", async (t) => {
  const { root, env } = await fixture(t);
  const image = join(root, "reference.png");
  await writeFile(image, "png fixture");
  await assert.rejects(
    runGeneration("image", { image, execute: true }, {
      root,
      env,
      fetchImpl: async () => ({ ok: true, status: 200, arrayBuffer: async () => Buffer.from("not glb") }),
    }),
    /not a valid GLB/,
  );
});

test("run rejects a GLB envelope without its mandatory JSON chunk", async (t) => {
  const { root, env } = await fixture(t);
  const image = join(root, "reference.png");
  await writeFile(image, "png fixture");
  await assert.rejects(
    runGeneration("image", { image, execute: true }, {
      root,
      env,
      fetchImpl: async () => ({ ok: true, status: 200, arrayBuffer: async () => headerOnlyGlbFixture() }),
    }),
    /not a valid GLB/,
  );
});

test("multiview plan fingerprints named views with the verified quality profile", async (t) => {
  const { root } = await fixture(t);
  const views = join(root, "views");
  await mkdir(views);
  await writeFile(join(views, "front.png"), "front fixture");
  await writeFile(join(views, "back.png"), "back fixture");
  await writeFile(join(views, "left.png"), "left fixture");

  const plan = await buildPlan("multiview", { views });

  assert.equal(plan.model, "tencent/Hunyuan3D-2mv/hunyuan3d-dit-v2-mv");
  assert.equal(plan.request.num_inference_steps, 30);
  assert.equal(plan.request.octree_resolution, 256);
  assert.equal(plan.request.num_chunks, 20_000);
  assert.deepEqual(plan.input.views.map((view) => view.name), ["front", "back", "left"]);
  assert.ok(plan.input.views.every((view) => view.sha256.length === 64));
});

test("multiview run uses the local runner, validates GLB, and records provenance", async (t) => {
  const { root, env } = await fixture(t);
  const views = join(root, "views");
  await mkdir(views);
  await writeFile(join(views, "front.png"), "front fixture");
  await writeFile(join(views, "right.png"), "right fixture");
  const glb = glbFixture();
  let calls = 0;
  const execFileImpl = (_file, args, options, callback) => {
    calls += 1;
    assert.equal(options.cwd, env.HUNYUAN3D_HOME);
    assert.equal(options.env.HF_HUB_OFFLINE, "1");
    assert.equal(args[1], "multiview");
    const output = args[args.indexOf("--output") + 1];
    writeFile(output, glb).then(() => callback(null, "ok", ""), callback);
  };

  const result = await runGeneration("multiview", { views, execute: true }, {
    root, env, execFileImpl, now: () => new Date("2026-08-05T00:00:00.000Z"),
  });

  assert.equal(calls, 1);
  assert.deepEqual(await readFile(result.model), glb);
  const provenance = JSON.parse(await readFile(result.provenance, "utf8"));
  assert.equal(provenance.output.textured, false);
  assert.deepEqual(provenance.input.views.map((view) => view.name), ["front", "right"]);
});

test("CLI parses multiview folders", () => {
  assert.deepEqual(
    parseArgs(["run", "multiview", "--views", "tmp/views", "--execute"]),
    { command: "run", kind: "multiview", options: { views: "tmp/views", execute: true } },
  );
});

test("texture plan fingerprints the source GLB and reference image", async (t) => {
  const { root } = await fixture(t);
  const mesh = join(root, "source.glb");
  const image = join(root, "reference.png");
  await writeFile(mesh, glbFixture());
  await writeFile(image, "texture fixture");

  const plan = await buildPlan("texture", { mesh, image });

  assert.equal(plan.model, "tencent/Hunyuan3D-2/hunyuan3d-paint-v2-0-turbo");
  assert.deepEqual(plan.request, { low_vram: true, vae_slicing: true, texture_size: 2048 });
  assert.equal(plan.input.mesh.sha256.length, 64);
  assert.equal(plan.input.image.sha256.length, 64);
});

test("texture run uses CPU offload runner and marks the output textured", async (t) => {
  const { root, env } = await fixture(t);
  const mesh = join(root, "source.glb");
  const image = join(root, "reference.png");
  await writeFile(mesh, glbFixture());
  await writeFile(image, "texture fixture");
  const glb = texturedGlbFixture();
  const execFileImpl = (_file, args, options, callback) => {
    assert.equal(args[1], "texture");
    assert.equal(args[args.indexOf("--mesh") + 1], mesh);
    assert.equal(args[args.indexOf("--image") + 1], image);
    assert.equal(options.env.PYTORCH_CUDA_ALLOC_CONF, "expandable_segments:True");
    const output = args[args.indexOf("--output") + 1];
    writeFile(output, glb).then(() => callback(null, "ok", ""), callback);
  };

  const result = await runGeneration("texture", { mesh, image, execute: true }, {
    root, env, execFileImpl, now: () => new Date("2026-08-05T00:00:00.000Z"),
  });

  const provenance = JSON.parse(await readFile(result.provenance, "utf8"));
  assert.equal(provenance.output.textured, true);
  assert.equal(provenance.input.mesh.path, mesh);
  assert.equal(provenance.input.image.path, image);
});

test("texture run rejects a GLB without embedded material, texture, and image", async (t) => {
  const { root, env } = await fixture(t);
  const mesh = join(root, "source.glb");
  const image = join(root, "reference.png");
  await writeFile(mesh, glbFixture());
  await writeFile(image, "texture fixture");
  const execFileImpl = (_file, args, _options, callback) => {
    const output = args[args.indexOf("--output") + 1];
    writeFile(output, glbFixture()).then(() => callback(null, "ok", ""), callback);
  };

  await assert.rejects(
    runGeneration("texture", { mesh, image, execute: true }, { root, env, execFileImpl }),
    /does not contain an embedded material, texture, and image/,
  );
});

test("texture run rejects declared texture arrays without material links and embedded bytes", async (t) => {
  const { root, env } = await fixture(t);
  const mesh = join(root, "source.glb");
  const image = join(root, "reference.png");
  await writeFile(mesh, glbFixture());
  await writeFile(image, "texture fixture");
  const execFileImpl = (_file, args, _options, callback) => {
    const output = args[args.indexOf("--output") + 1];
    writeFile(output, declaredButUnlinkedTextureGlbFixture()).then(() => callback(null, "ok", ""), callback);
  };

  await assert.rejects(
    runGeneration("texture", { mesh, image, execute: true }, { root, env, execFileImpl }),
    /does not contain an embedded material, texture, and image/,
  );
});

test("CLI parses texture inputs", () => {
  assert.deepEqual(
    parseArgs(["run", "texture", "--mesh", "tmp/model.glb", "--image", "tmp/ref.png", "--execute"]),
    {
      command: "run",
      kind: "texture",
      options: { mesh: "tmp/model.glb", image: "tmp/ref.png", execute: true },
    },
  );
});
