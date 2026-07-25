import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import fs from "node:fs";
import test from "node:test";

const schema = JSON.parse(
  fs.readFileSync(
    new URL("../schemas/scene-devapi.v1.schema.json", import.meta.url),
    "utf8",
  ),
);

function resolveRef(root, ref) {
  assert.match(ref, /^#\//);
  return ref
    .slice(2)
    .split("/")
    .reduce((value, segment) => value[segment], root);
}

function hasType(value, type) {
  if (type === "null") return value === null;
  if (type === "array") return Array.isArray(value);
  if (type === "object") {
    return value !== null && typeof value === "object" && !Array.isArray(value);
  }
  if (type === "integer") return Number.isInteger(value);
  return typeof value === type;
}

function validates(node, value, root = schema) {
  if (node.$ref) return validates(resolveRef(root, node.$ref), value, root);
  if (node.type && !hasType(value, node.type)) return false;
  if (Object.hasOwn(node, "const") && value !== node.const) return false;
  if (node.enum && !node.enum.includes(value)) return false;
  if (node.oneOf) {
    if (node.oneOf.filter((entry) => validates(entry, value, root)).length !== 1) {
      return false;
    }
  }
  if (node.allOf && !node.allOf.every((entry) => validates(entry, value, root))) {
    return false;
  }
  if (node.if && validates(node.if, value, root) &&
      !validates(node.then, value, root)) {
    return false;
  }
  if (typeof value === "string") {
    if (node.minLength !== undefined && value.length < node.minLength) return false;
    if (node.maxLength !== undefined && value.length > node.maxLength) return false;
    if (node.pattern && !new RegExp(node.pattern).test(value)) return false;
  }
  if (typeof value === "number") {
    if (node.minimum !== undefined && value < node.minimum) return false;
    if (node.maximum !== undefined && value > node.maximum) return false;
  }
  if (Array.isArray(value) && node.items) {
    if (!value.every((entry) => validates(node.items, entry, root))) return false;
  }
  if (value !== null && typeof value === "object" && !Array.isArray(value)) {
    const keys = Object.keys(value);
    if (node.maxProperties !== undefined && keys.length > node.maxProperties) {
      return false;
    }
    if (node.required &&
        !node.required.every((key) => Object.hasOwn(value, key))) {
      return false;
    }
    if (node.properties) {
      for (const [key, propertySchema] of Object.entries(node.properties)) {
        if (Object.hasOwn(value, key) &&
            !validates(propertySchema, value[key], root)) {
          return false;
        }
      }
    }
    if (node.additionalProperties === false) {
      const allowed = new Set(Object.keys(node.properties ?? {}));
      if (!keys.every((key) => allowed.has(key))) return false;
    }
  }
  return true;
}

test("actual C adapter response envelopes satisfy the published schema", () => {
  const fixture = process.env.SCENE_DEVAPI_SCHEMA_FIXTURE;
  assert.ok(fixture, "SCENE_DEVAPI_SCHEMA_FIXTURE must name the C fixture");
  const run = spawnSync(fixture, ["--dump-schema-fixtures"], {
    encoding: "utf8",
  });
  assert.equal(run.status, 0, run.stderr);
  const envelopes = run.stdout
    .split(/\r?\n/)
    .filter(Boolean)
    .map((line) => JSON.parse(line));
  assert.ok(envelopes.length >= 8);
  for (const envelope of envelopes) {
    assert.equal(
      validates(schema, envelope),
      true,
      `response violates scene-devapi.v1.schema.json:\n${JSON.stringify(envelope)}`,
    );
  }
});
