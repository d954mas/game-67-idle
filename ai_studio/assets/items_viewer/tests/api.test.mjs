import test from "node:test";
import assert from "node:assert/strict";
import { fileURLToPath } from "node:url";
import { createItemsViewerApi } from "../api.mjs";

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

async function request(path) {
  const response = responseCapture();
  const handled = await createItemsViewerApi(REPO_ROOT)(
    { method: "GET" },
    response,
    new URL(path, "http://studio.local"),
  );
  return { handled, response, json: JSON.parse(response.body) };
}

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

test("focused chart endpoint requires an explicit selected field", async () => {
  const { response, json } = await request(
    "/api/items-viewer/chart?catalog=template%3Atemplate&item=tmpl.sword",
  );
  assert.equal(response.status, 400);
  assert.match(json.error, /field/);
});
