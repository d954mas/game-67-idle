// Single source for the pipeline_validate.mjs CLI flag vocabulary. The ai.mjs
// facade pre-validates these same flags before spawning pipeline_validate, so
// the two must agree — a flag accepted by one but rejected by the other is a
// silent drift bug. Boolean flags take no value; VALUE flags consume the next
// arg. (--help/-h are handled by each entrypoint separately, not listed here.)
export const VALIDATE_BOOLEAN_FLAGS = [
  "--quick",
  "--full",
  "--review",
  "--dry-run",
  "--reexport-tests",
  "--no-prune",
  "--with-assets",
];

export const VALIDATE_VALUE_FLAGS = ["--keep-exports"];
