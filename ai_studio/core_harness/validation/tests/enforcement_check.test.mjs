import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { validateEnforcementContract } from "../enforcement_check.mjs";

const root = resolve(fileURLToPath(new URL("../../../../", import.meta.url)));

function tempContract(t, rule) {
  const dir = mkdtempSync(join(tmpdir(), "enforcement-check-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  mkdirSync(join(dir, "docs"), { recursive: true });
  writeFileSync(join(dir, "docs", "owner.md"), "owner\n", "utf8");
  writeFileSync(join(dir, "docs", "proof.mjs"), "// proof\n", "utf8");
  writeFileSync(join(dir, "contract.json"), JSON.stringify({
    schema: "ai_studio.enforcement.v1",
    classifications: ["host-enforced", "repository-validator-enforced", "process convention"],
    rules: [rule],
  }), "utf8");
  return dir;
}

test("repository enforcement contract has valid classifications and proof links", () => {
  assert.deepEqual(validateEnforcementContract(root), []);
});

test("enforcement check rejects unknown classification", (t) => {
  const dir = tempContract(t, { id: "bad", classification: "advisory", claim: "bad", sources: ["docs/owner.md"], proof: [] });
  assert.match(validateEnforcementContract(dir, "contract.json").join("\n"), /invalid classification advisory/);
});

test("enforcement check rejects missing proof for enforced claim", (t) => {
  const dir = tempContract(t, { id: "missing", classification: "host-enforced", proof_kind: "runtime-observed", claim: "claim", sources: ["docs/owner.md"], proof: ["docs/missing.mjs"] });
  assert.match(validateEnforcementContract(dir, "contract.json").join("\n"), /missing proof docs\/missing\.mjs/);
});

test("enforcement check separates host configuration from runtime proof", (t) => {
  const dir = tempContract(t, { id: "config", classification: "host-enforced", proof_kind: "configuration-only", claim: "Host owns this setting.", sources: ["docs/owner.md"], proof: ["docs/proof.mjs"] });
  assert.match(validateEnforcementContract(dir, "contract.json").join("\n"), /runtime application is not repository-verified/);
});

test("enforcement check prevents process convention from claiming technical proof", (t) => {
  const dir = tempContract(t, { id: "process", classification: "process convention", claim: "claim", sources: ["docs/owner.md"], proof: ["docs/proof.mjs"] });
  assert.match(validateEnforcementContract(dir, "contract.json").join("\n"), /must not claim technical proof/);
});

test("enforcement check rejects malformed rules and evidence paths outside the repository", (t) => {
  const dir = tempContract(t, null);
  assert.match(validateEnforcementContract(dir, "contract.json").join("\n"), /rule must be an object/);

  writeFileSync(join(dir, "contract.json"), JSON.stringify({
    schema: "ai_studio.enforcement.v1",
    classifications: ["host-enforced", "repository-validator-enforced", "process convention"],
    rules: [{ id: "escape", classification: "host-enforced", proof_kind: "runtime-observed", claim: "claim", sources: ["../outside.md"], proof: ["../outside.mjs"] }],
  }), "utf8");
  const errors = validateEnforcementContract(dir, "contract.json").join("\n");
  assert.match(errors, /source must stay inside the repository/);
  assert.match(errors, /proof must stay inside the repository/);
});
