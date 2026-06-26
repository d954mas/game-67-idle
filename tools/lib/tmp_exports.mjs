// Single source for the tmp/ export-dir housekeeping contract.
// Legacy pipeline validation exports lived in tmp/pipeline-validate-<ISO>/.
// Two consumers prune around those dirs (pipeline_validate's own --keep-exports
// auto-prune and tmp_sweep's broad scratch sweep) and must agree on the prefix
// plus the "keep newest N" rule, or they drift: tmp_sweep would protect or
// delete the wrong dirs if the export naming changed. Names embed ISO
// timestamps, so a lexical sort is chronological.
import { existsSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";

export const VALIDATE_EXPORT_PREFIX = "pipeline-validate-";

export function isValidateExportName(name) {
  return name.startsWith(VALIDATE_EXPORT_PREFIX);
}

// Names of pipeline-validate-* export DIRS under tmpDir, oldest-first.
// Returns [] when tmpDir is missing or unreadable. Non-directory entries that
// happen to share the prefix are ignored (we only ever prune export dirs).
export function listValidateExports(tmpDir) {
  if (!existsSync(tmpDir)) return [];
  let names;
  try {
    names = readdirSync(tmpDir);
  } catch {
    return [];
  }
  return names
    .filter(isValidateExportName)
    .filter((name) => {
      try {
        return statSync(join(tmpDir, name)).isDirectory();
      } catch {
        return false;
      }
    })
    .sort();
}

// Split oldest-first names into the newest `keep` to retain and the older
// `stale` ones safe to delete.
export function partitionByKeep(names, keep) {
  const cut = Math.max(0, names.length - Math.max(0, keep));
  return { kept: names.slice(cut), stale: names.slice(0, cut) };
}
