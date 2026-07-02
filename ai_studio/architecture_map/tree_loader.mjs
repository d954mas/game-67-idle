// Architecture tree loader.
//
// The workspace tree is stored split: ai_studio/tree.json is a small index that
// lists per-child part files under architecture_map/tree/. This module merges the
// index and its parts back into one tree object for the validator, the Studio
// Shell API route, and the renderer. A legacy single-file tree (root.children with
// no root.parts) is returned unchanged, so older callers and test fixtures work.

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";

// Merge a parsed index object into a full tree.
// `loadPart(partPath)` returns the parsed child subtree for one part path, where
// part paths are relative to the directory that holds the index file.
export function mergeArchitectureTree(index, loadPart) {
  if (!index || !index.root || !Array.isArray(index.root.parts)) return index;
  const { parts, ...rootMeta } = index.root;
  const children = parts.map((partPath) => loadPart(partPath));
  const { index: _indexFlag, ...rest } = index;
  return { ...rest, root: { ...rootMeta, children } };
}

// Load and merge the architecture tree from disk.
// `repoRoot` is absolute; `mapPath` is the repo-relative index path (ai_studio/tree.json).
export function loadArchitectureTree(repoRoot, mapPath) {
  const indexAbs = join(repoRoot, mapPath);
  const index = JSON.parse(readFileSync(indexAbs, "utf8"));
  const baseDir = dirname(indexAbs);
  return mergeArchitectureTree(index, (partPath) =>
    JSON.parse(readFileSync(join(baseDir, partPath), "utf8")),
  );
}
