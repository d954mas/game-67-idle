#!/usr/bin/env node
import { existsSync, readFileSync } from "node:fs";
import { isAbsolute, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const DEFAULT_CONTRACT = "ai_studio/core_harness/workflow/enforcement_contract.json";
const ALLOWED = new Set(["host-enforced", "repository-validator-enforced", "process convention"]);
const HOST_PROOF_KINDS = new Set(["runtime-observed", "configuration-only"]);

function valueArg(argv, name, fallback) {
  const index = argv.indexOf(name);
  if (index < 0) return fallback;
  const value = argv[index + 1];
  if (!value || value.startsWith("--")) throw new Error(`${name} requires a value`);
  return value;
}

export function validateEnforcementContract(root, contractPath = DEFAULT_CONTRACT) {
  const absolute = resolve(root, contractPath);
  const errors = [];
  let contract;
  try {
    contract = JSON.parse(readFileSync(absolute, "utf8"));
  } catch (error) {
    return [`cannot read enforcement contract ${contractPath}: ${error.message}`];
  }
  if (contract.schema !== "ai_studio.enforcement.v1") errors.push("schema must be ai_studio.enforcement.v1");
  if (JSON.stringify(contract.classifications) !== JSON.stringify([...ALLOWED])) {
    errors.push("classifications must list the three canonical labels in order");
  }
  if (!Array.isArray(contract.rules) || contract.rules.length === 0) return [...errors, "rules must be a non-empty array"];
  const ids = new Set();
  for (const rule of contract.rules) {
    if (!rule || typeof rule !== "object" || Array.isArray(rule)) {
      errors.push("each rule must be an object");
      continue;
    }
    const label = rule && typeof rule.id === "string" ? rule.id : "(missing id)";
    if (!rule || typeof rule.id !== "string" || !rule.id) errors.push("each rule requires id");
    else if (ids.has(rule.id)) errors.push(`duplicate rule id: ${rule.id}`);
    else ids.add(rule.id);
    if (!ALLOWED.has(rule.classification)) errors.push(`${label}: invalid classification ${String(rule.classification)}`);
    if (typeof rule.claim !== "string" || !rule.claim.trim()) errors.push(`${label}: claim is required`);
    if (!Array.isArray(rule.sources) || rule.sources.length === 0) errors.push(`${label}: sources must be non-empty`);
    for (const path of Array.isArray(rule.sources) ? rule.sources : []) {
      if (typeof path !== "string" || !path.trim() || isAbsolute(path) || relative(root, resolve(root, path)).startsWith("..")) {
        errors.push(`${label}: source must stay inside the repository: ${String(path)}`);
      } else if (!existsSync(resolve(root, path))) errors.push(`${label}: missing source ${path}`);
    }
    const proof = Array.isArray(rule.proof) ? rule.proof : [];
    if (rule.classification === "process convention" && proof.length > 0) {
      errors.push(`${label}: process convention must not claim technical proof`);
    }
    if (rule.classification !== "process convention" && proof.length === 0) {
      errors.push(`${label}: enforced claim requires proof links`);
    }
    if (rule.classification === "host-enforced") {
      if (!HOST_PROOF_KINDS.has(rule.proof_kind)) errors.push(`${label}: host-enforced claim requires a valid proof_kind`);
      if (rule.proof_kind === "configuration-only" && !rule.claim.includes("Runtime application is not repository-verified.")) {
        errors.push(`${label}: configuration-only claim must state that runtime application is not repository-verified`);
      }
    } else if (rule.classification === "repository-validator-enforced" && rule.proof_kind !== "repository-validator") {
      errors.push(`${label}: repository-validator-enforced claim requires repository-validator proof_kind`);
    } else if (rule.classification === "process convention" && rule.proof_kind !== undefined) {
      errors.push(`${label}: process convention must not declare proof_kind`);
    }
    for (const path of proof) {
      if (typeof path !== "string" || !path.trim() || isAbsolute(path) || relative(root, resolve(root, path)).startsWith("..")) {
        errors.push(`${label}: proof must stay inside the repository: ${String(path)}`);
      } else if (!existsSync(resolve(root, path))) errors.push(`${label}: missing proof ${path}`);
    }
  }
  return errors;
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(fileURLToPath(import.meta.url))) {
  try {
    const root = resolve(valueArg(process.argv.slice(2), "--root", "."));
    const contract = valueArg(process.argv.slice(2), "--contract", DEFAULT_CONTRACT);
    const errors = validateEnforcementContract(root, contract);
    if (errors.length > 0) {
      process.stderr.write(`${errors.join("\n")}\n`);
      process.exitCode = 1;
    } else {
      process.stdout.write("enforcement contract valid\n");
    }
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 2;
  }
}
