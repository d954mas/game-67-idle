import { copyFileSync, existsSync, mkdirSync, lstatSync, realpathSync } from "node:fs";
import { dirname, relative, resolve, sep } from "node:path";
import { spawnSync } from "node:child_process";

function toGitPath(value) {
  return value.split(sep).join("/");
}

export function copyTrackedGameSourceTree(sourceDir, destinationDir) {
  const source = realpathSync(sourceDir);
  const top = spawnSync("git", ["-C", source, "rev-parse", "--show-toplevel"], {
    encoding: "utf8", shell: false, windowsHide: true,
  });
  if (top.status !== 0 || realpathSync(top.stdout.trim()) !== source) {
    throw new Error("game source must own a nested Git repository");
  }
  const result = spawnSync("git", ["-C", source, "ls-files", "-z", "--cached"], {
    encoding: "utf8", shell: false, windowsHide: true,
  });
  if (result.status !== 0) throw new Error(`failed to enumerate game source: ${result.stderr.trim()}`);
  mkdirSync(destinationDir, { recursive: true });
  for (const name of result.stdout.split("\0").filter(Boolean)) {
    const parts = name.split("/");
    if (parts.some((part) => [".git", ".."].includes(part))
        || parts.slice(0, -1).some((part) => [".ai_studio", "secrets", "build", "builds", "tmp", ".cache", "node_modules", "__pycache__"].includes(part))
        || [".toolchains", "profiles", "runtime"].includes(parts[0])
        || parts.at(-1) === ".env" || parts.at(-1).startsWith(".env.")
        || name.startsWith("assets/restricted/") || name.startsWith("release/artifacts/")
        || ["assets/PURCHASED_EXPORTS.md", "design/canvas.md"].includes(name)) continue;
    const file = resolve(source, name);
    if (!existsSync(file)) continue;
    if (!lstatSync(file).isFile() || realpathSync(file) !== file) {
      throw new Error(`game source must contain regular files: ${name}`);
    }
    const target = resolve(destinationDir, name);
    mkdirSync(dirname(target), { recursive: true });
    copyFileSync(file, target);
  }
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
