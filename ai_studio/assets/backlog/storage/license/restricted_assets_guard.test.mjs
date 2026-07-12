import test from "node:test";
import assert from "node:assert/strict";

import { argumentError, formatTextReport, parseArgs } from "./restricted_assets_guard.mjs";
import { validateLicenseRecord } from "./registry.mjs";

test("CLI accepts only stable --json and --scope arguments", () => {
  assert.deepEqual(parseArgs([]), { ok: true, json: false, scope: "" });
  assert.deepEqual(parseArgs(["--json", "--scope", "templates/template/assets"]), { ok: true, json: true, scope: "templates/template/assets" });
  for (const args of [["--wat"], ["--scope"], ["--scope", "--json"], ["--json", "--json"], ["--scope", "a", "--scope", "b"]]) {
    assert.equal(parseArgs(args).ok, false, args.join(" "));
  }
});

test("argument failures use the stable report schema and setup exit 2", () => {
  const result = argumentError("bad args");
  assert.equal(result.schema, "ai_studio.asset_integrity_report.v1");
  assert.equal(result.exitCode, 2);
  assert.equal(result.setup, true);
});

test("compact text groups field issues by path and caps paths", () => {
  const result = {
    ok: false, setup: false, exitCode: 1,
    summary: { scope: "all" },
    issues: [
      { path: "a.png", code: "missing-license" }, { path: "a.png", code: "missing-origin" },
      { path: "b.png", code: "missing-license" }, { path: "c.png", code: "missing-license" },
    ],
  };
  const text = formatTextReport(result, { cap: 2 });
  assert.match(text, /blocked 3 file\(s\) with 4 field issue\(s\)/);
  assert.match(text, /a\.png: missing-license, missing-origin/);
  assert.match(text, /1 more path/);
  assert.doesNotMatch(text, /c\.png/);
});

test("repo-relative license evidence uses license_file, never license_url", () => {
  const base = {
    license: "Studio-Owned-Public-1.0", license_kind: "custom", publish: "true",
    redistribution_allowed: "true", commercial_use: "true", modification_allowed: "true",
  };
  const wrong = validateLicenseRecord({ ...base, license_url: "games/example/LICENSE.md" }, { forPublicBinary: true });
  assert.equal(wrong.ok, false);
  assert.match(wrong.issues.join("; "), /absolute http\(s\) URL/);
  assert.equal(validateLicenseRecord({ ...base, license_file: "games/example/LICENSE.md" }, { forPublicBinary: true }).ok, true);
});
