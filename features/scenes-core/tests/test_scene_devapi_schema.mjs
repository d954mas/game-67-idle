import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";

const schema = JSON.parse(
  fs.readFileSync(
    new URL("../schemas/scene-devapi.v1.schema.json", import.meta.url),
    "utf8",
  ),
);
const adapter = fs.readFileSync(
  new URL("../src/scene_manager_devapi.c", import.meta.url),
  "utf8",
);

test("DevAPI schema method and parameter names match the adapter", () => {
  const adapterMethods = [
    ...new Set(
      [...adapter.matchAll(/"game\.scene\.[a-z_]+"/g)].map((match) =>
        match[0].slice(1, -1),
      ),
    ),
  ].sort();
  const schemaMethods =
    schema.$defs.request.properties.method.enum.toSorted();

  assert.deepEqual(schemaMethods, adapterMethods);
  assert.deepEqual(schema.$defs.sceneParams.required, ["scene"]);
  assert.equal(
    Object.hasOwn(schema.$defs.sceneParams.properties, "sceneId"),
    false,
  );
  assert.equal(schema.$defs.backParams.properties.count.maximum, 128);
  assert.equal(
    schema.$defs.operationParams.properties.operationId.maximum,
    Number.MAX_SAFE_INTEGER,
  );
});

test("DevAPI schema exposes every adapter result shape", () => {
  const refs = schema.$defs.successEnvelope.properties.result.oneOf.map(
    (entry) => entry.$ref,
  );
  assert.deepEqual(refs, [
    "#/$defs/listResult",
    "#/$defs/statusResult",
    "#/$defs/navigationResult",
    "#/$defs/preloadResult",
    "#/$defs/operationStatusResult",
  ]);

  for (const field of [
    "scenes",
    "history",
    "inputGated",
    "top",
    "result",
    "operationId",
    "blockingOperationId",
    "kind",
    "state",
    "scene",
  ]) {
    assert.match(adapter, new RegExp(`"${field}"`));
  }

  assert.ok(
    schema.$defs.navigationResult.oneOf.some(
      (variant) => variant.properties.result.const === "not_top",
    ),
  );
});

test("navigation result variants forbid fields from other outcomes", () => {
  const variants = schema.$defs.navigationResult.oneOf;
  assert.equal(variants.length, 5);

  const byResult = new Map(
    variants.map((variant) => [
      variant.properties.result.const,
      variant,
    ]),
  );
  assert.deepEqual(
    [...byResult.keys()].toSorted(),
    ["accepted", "already_top", "busy", "not_top", "root_protected"],
  );
  assert.deepEqual(byResult.get("accepted").required, [
    "result",
    "operationId",
  ]);
  assert.deepEqual(byResult.get("busy").required, [
    "result",
    "blockingOperationId",
  ]);
  for (const variant of variants) {
    assert.equal(variant.additionalProperties, false);
  }
});
