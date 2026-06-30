// Single-purpose shared leaf: content hashing. No orchestration, no I/O policy —
// just bytes -> hex, so every tool that records asset provenance uses ONE sha256
// instead of re-implementing createHash. Compose it; do not bolt logic on.
import { createHash } from "node:crypto";
import { readFile } from "node:fs/promises";

export function sha256Hex(buffer) {
  return createHash("sha256").update(buffer).digest("hex");
}

export async function sha256File(path) {
  return sha256Hex(await readFile(path));
}
