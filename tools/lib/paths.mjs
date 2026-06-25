// Tiny path leaf for one-shot tools. Pure (node builtins only).
//
// SCOPE NOTE: the bare `x.replaceAll("\\","/")` idiom appears in many tools but
// in varied contexts (command strings, trailing-slash strips, one-off refs) —
// that is a one-liner, not a shared contract, so it is deliberately LEFT INLINE
// rather than force-importing toPosix everywhere (coupling without benefit).
// `findRepoRoot` is likewise NOT here: the only walk-up implementations live in
// the leak guard and its dependency chain (find_assets), which must stay
// self-contained. What IS shared is relCwdPosix (repeated verbatim in 3 product
// gate tools), with toPosix as its primitive + the plan's named helper.
import { resolve } from "node:path";

// Forward slashes for display/serialization (Windows "\" -> "/").
export function toPosix(value) {
  return String(value ?? "").replaceAll("\\", "/");
}

// A path expressed relative to the current working directory with posix slashes,
// or the original string unchanged if it resolves outside cwd. Gives stable,
// portable path strings in gate reports.
export function relCwdPosix(path) {
  const absolute = resolve(path);
  return absolute.startsWith(process.cwd()) ? toPosix(absolute.slice(process.cwd().length + 1)) : path;
}
