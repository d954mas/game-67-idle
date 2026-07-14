import { copyFileSync, existsSync, mkdirSync } from "node:fs";
import { dirname, relative, resolve, sep } from "node:path";
import { spawnSync } from "node:child_process";

function toGitPath(value) {
  return value.split(sep).join("/");
}

export function copyGitSourceTree(repoRoot, sourceDir, destinationDir) {
  const root = resolve(repoRoot);
  const source = resolve(sourceDir);
  const sourceRel = relative(root, source);
  if (!sourceRel || sourceRel === ".." || sourceRel.startsWith(`..${sep}`)) {
    throw new Error("source tree must be inside the Studio Git checkout");
  }

  const result = spawnSync("git", [
    "-C", root, "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", toGitPath(sourceRel),
  ], { encoding: "utf8", shell: false, windowsHide: true });
  if (result.status !== 0) {
    throw new Error(`failed to enumerate source files with Git: ${result.stderr.trim()}`);
  }

  mkdirSync(destinationDir, { recursive: true });
  for (const repoRel of result.stdout.split("\0").filter(Boolean)) {
    const sourceFile = resolve(root, repoRel);
    if (!existsSync(sourceFile)) continue;
    const targetRel = relative(source, sourceFile);
    if (!targetRel || targetRel === ".." || targetRel.startsWith(`..${sep}`)) continue;
    const targetFile = resolve(destinationDir, targetRel);
    mkdirSync(dirname(targetFile), { recursive: true });
    copyFileSync(sourceFile, targetFile);
  }
}
