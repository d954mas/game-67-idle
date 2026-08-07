import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";

import { buildPlan, runGeneration } from "./client.mjs";
import { parseArgs } from "./cli.mjs";

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

async function fixture(t) {
  const root = await mkdtemp(join(tmpdir(), "meshy-test-"));
  await mkdir(join(root, "ai_studio"), { recursive: true });
  await writeFile(join(root, "ai_studio", "studio.config.json"), JSON.stringify({
    schema: "ai_studio.studio_config.v1",
    meshyWorkRoot: "tmp/meshy",
    meshyCreditReserve: 20,
  }));
  t.after(() => rm(root, { recursive: true, force: true }));
  return root;
}

function jsonResponse(body, status = 200) {
  const text = JSON.stringify(body);
  return {
    ok: status >= 200 && status < 300,
    status,
    statusText: "",
    text: async () => text,
    arrayBuffer: async () => Buffer.from(text),
  };
}

test("plans default to cheap previews and redact image bytes", async (t) => {
  const root = await fixture(t);
  const image = join(root, "ref.png");
  await writeFile(image, Buffer.from("png fixture"));
  const textPlan = await buildPlan("text-preview", { prompt: "market stall" });
  assert.equal(textPlan.profile, "draft");
  assert.equal(textPlan.estimated_credits, 5);
  assert.equal(textPlan.request.ai_model, "meshy-5");
  const refinePlan = await buildPlan("text-refine", { previewTaskId: "preview-12345678" });
  assert.equal(refinePlan.profile, "draft");
  assert.equal(refinePlan.request.ai_model, "meshy-5");
  const imagePlan = await buildPlan("image", { image, profile: "game" });
  assert.equal(imagePlan.estimated_credits, 15);
  assert.match(imagePlan.request.image_url, /<redacted>$/);
  assert.doesNotMatch(JSON.stringify(imagePlan.request), /cG5nIGZpeHR1cmU=/);
});

test("paid run refuses before any network call without exact approval", async (t) => {
  const root = await fixture(t);
  let calls = 0;
  const fetchImpl = async () => { calls += 1; throw new Error("unexpected fetch"); };
  await assert.rejects(
    runGeneration("text-preview", { prompt: "market stall" }, { root, env: { MESHY_API_KEY: "msy_test" }, fetchImpl }),
    /paid generation blocked/,
  );
  assert.equal(calls, 0);
  await assert.rejects(
    runGeneration("text-preview", {
      prompt: "market stall", execute: true, confirmCredits: 5, maxCredits: 20,
    }, { root, env: { MESHY_API_KEY: "msy_test" }, fetchImpl }),
    /--max-credits must exactly equal/,
  );
  assert.equal(calls, 0);
});

test("reserve gate blocks before paid POST", async (t) => {
  const root = await fixture(t);
  const calls = [];
  const fetchImpl = async (url, options = {}) => {
    calls.push({ url, method: options.method || "GET" });
    return jsonResponse({ balance: 24 });
  };
  await assert.rejects(
    runGeneration("text-preview", {
      prompt: "market stall", execute: true, confirmCredits: 5, maxCredits: 5,
    }, { root, env: { MESHY_API_KEY: "msy_test" }, fetchImpl }),
    /balance 24, estimate 5, reserve 20/,
  );
  assert.deepEqual(calls.map((call) => call.method), ["GET"]);
});

test("lowering the configured reserve requires an exact second confirmation", async (t) => {
  const root = await fixture(t);
  let calls = 0;
  const fetchImpl = async () => { calls += 1; throw new Error("unexpected fetch"); };
  await assert.rejects(
    runGeneration("text-preview", {
      prompt: "market stall", execute: true, confirmCredits: 5, maxCredits: 5,
      reserveCredits: 0,
    }, { root, env: { MESHY_API_KEY: "msy_test" }, fetchImpl }),
    /--confirm-reserve-credits must equal 0/,
  );
  assert.equal(calls, 0);
  assert.deepEqual(
    parseArgs(["run", "text-preview", "--prompt", "market stall", "--reserve-credits", "0", "--confirm-reserve-credits", "0"]),
    {
      command: "run",
      kind: "text-preview",
      options: { prompt: "market stall", reserveCredits: "0", confirmReserveCredits: "0" },
    },
  );
});

test("successful run downloads GLB, records provenance, and caches identical work", async (t) => {
  const root = await fixture(t);
  const calls = [];
  const glb = glbFixture();
  const fetchImpl = async (url, options = {}) => {
    const method = options.method || "GET";
    calls.push({ url, method });
    if (url.endsWith("/balance")) return jsonResponse({ balance: calls.length === 1 ? 100 : 95 });
    if (method === "POST") return jsonResponse({ result: "task-12345678" });
    if (url.endsWith("/task-12345678")) return jsonResponse({ status: "SUCCEEDED", progress: 100, model_urls: { glb: "https://cdn.meshy.ai/model.glb" } });
    if (url === "https://cdn.meshy.ai/model.glb") {
      return { ok: true, status: 200, arrayBuffer: async () => glb };
    }
    throw new Error(`unexpected ${method} ${url}`);
  };
  const options = { prompt: "market stall", execute: true, confirmCredits: 5, maxCredits: 5 };
  const result = await runGeneration("text-preview", options, {
    root, env: { MESHY_API_KEY: "msy_test" }, fetchImpl, pollIntervalMs: 0,
    now: () => new Date("2026-08-05T00:00:00.000Z"),
  });
  assert.equal(result.cached, false);
  assert.equal(result.resumed, false);
  assert.deepEqual(await readFile(result.model), glb);
  const provenance = JSON.parse(await readFile(result.provenance, "utf8"));
  assert.equal(provenance.origin, "ai");
  assert.equal(provenance.task_id, "task-12345678");
  assert.equal(provenance.cost.estimated_credits, 5);
  assert.equal(provenance.output.bytes, glb.length);
  const callCount = calls.length;
  const cached = await runGeneration("text-preview", { prompt: "market stall" }, {
    root, env: {}, fetchImpl,
  });
  assert.equal(cached.cached, true);
  assert.equal(calls.length, callCount);
});

test("successful task rejects a download that is not a structurally valid GLB", async (t) => {
  const root = await fixture(t);
  const fetchImpl = async (url, options = {}) => {
    if (url.endsWith("/balance")) return jsonResponse({ balance: 100 });
    if ((options.method || "GET") === "POST") return jsonResponse({ result: "task-invalid-glb" });
    if (url.endsWith("/task-invalid-glb")) {
      return jsonResponse({ status: "SUCCEEDED", model_urls: { glb: "https://assets.meshy.ai/not-a-model.glb" } });
    }
    if (url === "https://assets.meshy.ai/not-a-model.glb") {
      return { ok: true, status: 200, arrayBuffer: async () => headerOnlyGlbFixture() };
    }
    throw new Error(`unexpected ${url}`);
  };
  await assert.rejects(
    runGeneration("text-preview", {
      prompt: "invalid response", execute: true, confirmCredits: 5, maxCredits: 5,
    }, { root, env: { MESHY_API_KEY: "msy_test" }, fetchImpl, pollIntervalMs: 0 }),
    /not a valid GLB/,
  );
});

test("a task id survives interruption and resumes without another paid POST", async (t) => {
  const root = await fixture(t);
  let postCalls = 0;
  const firstFetch = async (url, options = {}) => {
    if (url.endsWith("/balance")) return jsonResponse({ balance: 100 });
    if ((options.method || "GET") === "POST") {
      postCalls += 1;
      return jsonResponse({ result: "task-resume-1234" });
    }
    if (url.endsWith("/task-resume-1234")) throw new Error("poll interrupted");
    throw new Error(`unexpected ${url}`);
  };
  const approved = { prompt: "resume market stall", execute: true, confirmCredits: 5, maxCredits: 5 };
  await assert.rejects(
    runGeneration("text-preview", approved, { root, env: { MESHY_API_KEY: "msy_test" }, fetchImpl: firstFetch }),
    /poll interrupted/,
  );
  assert.equal(postCalls, 1);

  const glb = glbFixture();
  const resumeFetch = async (url, options = {}) => {
    if ((options.method || "GET") === "POST") { postCalls += 1; throw new Error("must not POST"); }
    if (url.endsWith("/task-resume-1234")) return jsonResponse({ status: "SUCCEEDED", model_urls: { glb: "https://cdn.meshy.ai/resumed.glb" } });
    if (url === "https://cdn.meshy.ai/resumed.glb") return { ok: true, status: 200, arrayBuffer: async () => glb };
    if (url.endsWith("/balance")) return jsonResponse({ balance: 95 });
    throw new Error(`unexpected ${url}`);
  };
  const resumed = await runGeneration("text-preview", { prompt: "resume market stall" }, {
    root, env: { MESHY_API_KEY: "msy_test" }, fetchImpl: resumeFetch, pollIntervalMs: 0,
  });
  assert.equal(resumed.resumed, true);
  assert.equal(postCalls, 1);
  assert.deepEqual(await readFile(resumed.model), glb);
});

test("ambiguous submission is persisted and never auto-retried", async (t) => {
  const root = await fixture(t);
  let postCalls = 0;
  const fetchImpl = async (url, options = {}) => {
    if (url.endsWith("/balance")) return jsonResponse({ balance: 100 });
    if ((options.method || "GET") === "POST") { postCalls += 1; throw new Error("network vanished"); }
    throw new Error(`unexpected ${url}`);
  };
  const options = { prompt: "market stall", execute: true, confirmCredits: 5, maxCredits: 5 };
  await assert.rejects(
    runGeneration("text-preview", options, { root, env: { MESHY_API_KEY: "msy_test" }, fetchImpl }),
    /was not retried to avoid duplicate credit spend/,
  );
  assert.equal(postCalls, 1);
  await assert.rejects(
    runGeneration("text-preview", options, { root, env: { MESHY_API_KEY: "msy_test" }, fetchImpl }),
    /submission outcome is unknown/,
  );
  assert.equal(postCalls, 1);
  assert.equal(existsSync(join(root, "tmp", "meshy")), true);
});
