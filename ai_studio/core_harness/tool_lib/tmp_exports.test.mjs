import assert from "node:assert/strict";
import test from "node:test";
import { mkdtempSync, mkdirSync, writeFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import {
  VALIDATE_EXPORT_PREFIX,
  isValidateExportName,
  listValidateExports,
  partitionByKeep,
} from "./tmp_exports.mjs";

test("isValidateExportName matches only the export prefix", () => {
  assert.equal(isValidateExportName(`${VALIDATE_EXPORT_PREFIX}2026-01-01`), true);
  assert.equal(isValidateExportName("rune_marches"), false);
  assert.equal(isValidateExportName("NanoAlpha"), false);
});

test("listValidateExports returns export DIRS oldest-first, ignoring others and files", () => {
  const dir = mkdtempSync(join(tmpdir(), "tmp-exports-"));
  try {
    mkdirSync(join(dir, `${VALIDATE_EXPORT_PREFIX}2026-06-15T03-00-00-000Z`));
    mkdirSync(join(dir, `${VALIDATE_EXPORT_PREFIX}2026-06-15T01-00-00-000Z`));
    mkdirSync(join(dir, `${VALIDATE_EXPORT_PREFIX}2026-06-15T02-00-00-000Z`));
    mkdirSync(join(dir, "NanoAlpha"));
    writeFileSync(join(dir, `${VALIDATE_EXPORT_PREFIX}stray.txt`), "x", "utf8");
    assert.deepEqual(listValidateExports(dir), [
      `${VALIDATE_EXPORT_PREFIX}2026-06-15T01-00-00-000Z`,
      `${VALIDATE_EXPORT_PREFIX}2026-06-15T02-00-00-000Z`,
      `${VALIDATE_EXPORT_PREFIX}2026-06-15T03-00-00-000Z`,
    ]);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("listValidateExports returns [] for a missing dir", () => {
  assert.deepEqual(listValidateExports(join(tmpdir(), "tmp-exports-definitely-absent-xyz")), []);
});

test("partitionByKeep keeps the newest N and stales the rest", () => {
  const names = ["a", "b", "c", "d"];
  assert.deepEqual(partitionByKeep(names, 3), { kept: ["b", "c", "d"], stale: ["a"] });
  assert.deepEqual(partitionByKeep(names, 0), { kept: [], stale: ["a", "b", "c", "d"] });
  assert.deepEqual(partitionByKeep(names, 10), { kept: ["a", "b", "c", "d"], stale: [] });
  assert.deepEqual(partitionByKeep([], 3), { kept: [], stale: [] });
});
