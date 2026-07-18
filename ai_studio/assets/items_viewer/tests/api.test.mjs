import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import {
  createItemsViewerApi,
  createRequestCoordinator,
  RequestOverloadedError,
} from "../api.mjs";
import { ItemsCliTimeoutError } from "../ops.mjs";

const REPO_ROOT = fileURLToPath(new URL("../../../..", import.meta.url)).replace(/[\\/]$/, "");

function responseCapture() {
  return {
    status: null,
    headers: null,
    body: "",
    writeHead(status, headers) {
      this.status = status;
      this.headers = headers;
    },
    end(body = "") {
      this.body = body;
    },
  };
}

async function invoke(handler, path, { method = "GET", body, headers = {} } = {}) {
  const response = responseCapture();
  const encoded = body === undefined ? null : Buffer.from(JSON.stringify(body));
  const req = {
    method,
    headers: {
      host: "studio.local",
      ...(encoded ? { "content-type": "application/json", origin: "http://studio.local" } : {}),
      ...headers,
    },
    async *[Symbol.asyncIterator]() {
      if (encoded) yield encoded;
    },
  };
  const handled = await handler(
    req,
    response,
    new URL(path, "http://studio.local"),
  );
  const contentType = String(response.headers?.["content-type"] || "");
  const json = contentType.startsWith("application/json")
    ? JSON.parse(Buffer.isBuffer(response.body) ? response.body.toString("utf8") : response.body)
    : null;
  return { handled, response, json };
}

async function request(path, { viewerOptions = {}, ...requestOptions } = {}) {
  const handler = createItemsViewerApi(REPO_ROOT, {
    allowedHosts: ["studio.local"],
    ...viewerOptions,
  });
  return invoke(handler, path, requestOptions);
}

test("catalog request coordinator coalesces duplicates and enforces one active job", async () => {
  const coordinator = createRequestCoordinator(1);
  let active = 0;
  let peak = 0;
  let calls = 0;
  let releaseFirst;
  const blocked = new Promise((resolve) => { releaseFirst = resolve; });
  const work = async (value, gate) => {
    calls += 1;
    active += 1;
    peak = Math.max(peak, active);
    await gate;
    active -= 1;
    return value;
  };

  const first = coordinator.run("catalog:a", () => work("a", blocked));
  const duplicate = coordinator.run("catalog:a", () => work("duplicate", Promise.resolve()));
  const second = coordinator.run("catalog:b", () => work("b", Promise.resolve()));
  await Promise.resolve();
  assert.equal(calls, 1);
  releaseFirst();

  assert.deepEqual(await Promise.all([first, duplicate, second]), ["a", "a", "b"]);
  assert.equal(calls, 2);
  assert.equal(peak, 1);
});

test("request coordinator rejects distinct work once its bounded queue is full", async () => {
  const coordinator = createRequestCoordinator(1, 1);
  let releaseFirst;
  const blocked = new Promise((resolve) => { releaseFirst = resolve; });

  const first = coordinator.run("item:a", async () => {
    await blocked;
    return "a";
  });
  const duplicate = coordinator.run("item:a", async () => "duplicate");
  const queued = coordinator.run("chart:b", async () => "b");
  await assert.rejects(
    coordinator.run("item:c", async () => "c"),
    RequestOverloadedError,
  );

  releaseFirst();
  assert.deepEqual(await Promise.all([first, duplicate, queued]), ["a", "a", "b"]);
});

test("item and chart endpoints share a bounded request queue", async () => {
  let releaseFirst;
  const blocked = new Promise((resolve) => { releaseFirst = resolve; });
  let itemCalls = 0;
  let chartCalls = 0;
  const handler = createItemsViewerApi(REPO_ROOT, {
    allowedHosts: ["studio.local"],
    maxConcurrentRequests: 1,
    maxQueuedRequests: 1,
    reservedEditSlots: 0,
    getItemDetail: async (_root, _catalog, item) => {
      itemCalls += 1;
      if (item === "first") await blocked;
      return { item };
    },
    getItemChart: async () => {
      chartCalls += 1;
      return { points: [] };
    },
  });

  const first = invoke(handler, "/api/items-viewer/item?catalog=test&item=first");
  const queued = invoke(handler, "/api/items-viewer/chart?catalog=test&item=second&field=power");
  const refused = await invoke(handler, "/api/items-viewer/item?catalog=test&item=third");
  assert.equal(refused.response.status, 429);
  assert.equal(refused.response.headers["retry-after"], "1");
  assert.equal(itemCalls, 1);
  assert.equal(chartCalls, 0);

  releaseFirst();
  assert.equal((await first).response.status, 200);
  assert.equal((await queued).response.status, 200);
  assert.equal(itemCalls, 1);
  assert.equal(chartCalls, 1);
});

test("evaluator timeouts are explicit gateway timeouts", async () => {
  const { response, json } = await request(
    "/api/items-viewer/item?catalog=test&item=slow",
    {
      viewerOptions: {
        getItemDetail: async () => {
          throw new ItemsCliTimeoutError("items_cli.py timed out after 5ms");
        },
      },
    },
  );
  assert.equal(response.status, 504);
  assert.match(json.error, /timed out/);
});

test("same-origin edits use reserved priority when read admission is saturated", async () => {
  let releaseFirst;
  const blocked = new Promise((resolve) => { releaseFirst = resolve; });
  const order = [];
  const handler = createItemsViewerApi(REPO_ROOT, {
    allowedHosts: ["studio.local"],
    maxConcurrentRequests: 1,
    maxQueuedRequests: 2,
    getItemDetail: async (_root, _catalog, item) => {
      order.push(item);
      if (item === "first") await blocked;
      return { item };
    },
    editCatalogItem: async () => {
      order.push("edit");
      return { ok: true };
    },
  });

  const first = invoke(handler, "/api/items-viewer/item?catalog=test&item=first");
  const queuedRead = invoke(handler, "/api/items-viewer/item?catalog=test&item=queued");
  const refusedRead = invoke(handler, "/api/items-viewer/item?catalog=test&item=refused");
  const edit = invoke(handler, "/api/items-viewer/edit", {
    method: "POST",
    body: { catalog: "test", edit: {}, apply: false },
  });

  releaseFirst();
  assert.equal((await first).response.status, 200);
  assert.equal((await refusedRead).response.status, 429);
  assert.equal((await edit).response.status, 200);
  assert.equal((await queuedRead).response.status, 200);
  assert.deepEqual(order, ["first", "edit", "queued"]);
});

test("focused item endpoint resolves only registered catalogs", async () => {
  const { handled, response, json } = await request(
    "/api/items-viewer/item?catalog=template%3Atemplate&item=tmpl.sword",
  );
  assert.equal(handled, true);
  assert.equal(response.status, 200);
  assert.equal(json.item.item.id, "tmpl.sword");
  assert.equal(json.source.definition.file, "design/items/weapons.lua");
  assert.deepEqual(json.fields, []);

  const missing = await request(
    "/api/items-viewer/item?catalog=template%3Aunknown&item=tmpl.sword",
  );
  assert.equal(missing.response.status, 404);
});

test("catalog JSON references a separately served bounded icon page", async () => {
  const png = readFileSync(new URL("./fixtures/icon_pack/icons_page0.png", import.meta.url));
  const viewerOptions = {
    getCatalogView: async (_root, id) => (
      id === "fixture:icons" ? { icons: { page_available: true } } : null
    ),
    getIconPage: async (_root, id) => (
      id === "fixture:icons" ? { data: png } : null
    ),
  };
  const catalog = await request(
    "/api/items-viewer/catalog?id=fixture%3Aicons",
    { viewerOptions },
  );
  assert.equal(catalog.response.status, 200);
  assert.equal(Object.hasOwn(catalog.json.icons, "page_data_uri"), false);
  assert.equal(catalog.json.icons.page_url, "/api/items-viewer/icon-page?catalog=fixture%3Aicons");

  const page = await request(catalog.json.icons.page_url, { viewerOptions });
  assert.equal(page.response.status, 200);
  assert.equal(page.response.headers["content-type"], "image/png");
  assert.ok(Buffer.isBuffer(page.response.body));
  assert.ok(page.response.body.length > 24);
});

test("focused chart endpoint requires an explicit selected field", async () => {
  const { response, json } = await request(
    "/api/items-viewer/chart?catalog=template%3Atemplate&item=tmpl.sword",
  );
  assert.equal(response.status, 400);
  assert.match(json.error, /field/);
});

test("edit endpoint delegates preview refusal and rejects operations outside the shared semantic contract", async () => {
  const detail = await request(
    "/api/items-viewer/item?catalog=template%3Atemplate&item=tmpl.sword",
  );
  const edit = {
    schema: "items.cli.patch.v1",
    operation: "level-set",
    item: "tmpl.sword",
    field: "attack",
    level: 1,
    value: 16,
    expected_source_hash: detail.json.source.source_hash,
  };
  const refused = await request("/api/items-viewer/edit", {
    method: "POST",
    body: { catalog: "template:template", edit, apply: false },
  });
  assert.equal(refused.response.status, 422);
  assert.equal(refused.json.ok, false);

  const invalid = await request("/api/items-viewer/edit", {
    method: "POST",
    body: { catalog: "template:template", edit: { ...edit, operation: "rewrite-lua" }, apply: false },
  });
  assert.equal(invalid.response.status, 400);
  assert.match(invalid.json.error, /unsupported/);
});

test("edit endpoint rejects hostile Host or non-same Origin before invoking the writer", async () => {
  const edit = {
    schema: "items.cli.patch.v1",
    operation: "level-set",
    item: "tmpl.sword",
    field: "attack",
    level: 1,
    value: 16,
    expected_source_hash: `sha256:${"a".repeat(64)}`,
  };
  const hostileHost = await request("/api/items-viewer/edit", {
    method: "POST",
    body: { catalog: "template:template", edit, apply: false },
    headers: { host: "evil.test", origin: "http://evil.test" },
  });
  assert.equal(hostileHost.response.status, 403);

  const hostileOrigin = await request("/api/items-viewer/edit", {
    method: "POST",
    body: { catalog: "template:template", edit, apply: false },
    headers: { origin: "http://evil.test" },
  });
  assert.equal(hostileOrigin.response.status, 403);

  const missingOrigin = await request("/api/items-viewer/edit", {
    method: "POST",
    body: { catalog: "template:template", edit, apply: false },
    headers: { origin: "" },
  });
  assert.equal(missingOrigin.response.status, 403);
});
