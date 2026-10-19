#!/usr/bin/env node
import { lstatSync, readdirSync, readFileSync } from "node:fs";
import { extname, join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const VERTEX = new Set([".vert", ".vs", ".vsh"]);
const SKIP = new Set([".git", "node_modules", "build", "release", "tmp"]);
const RULE = "QTECH_002";

function tokens(source) {
  // GLSL splices continued lines before comments; retain original diagnostic offsets.
  const offsets = [];
  const joined = source.replace(/\\\r?\n|[\s\S]/g, (text, offset) => {
    if (text.length > 1) return "";
    offsets.push(offset);
    return text;
  });
  const clean = joined.replace(/\/\*[\s\S]*?\*\/|\/\/[^\r\n]*|"(?:\\.|[^"\\])*"/g,
    (text) => text.replace(/[^\r\n]/g, " "));
  return [...clean.matchAll(/[A-Za-z_]\w*|(?:\+\+|--|==|!=|<=|>=|\+=|-=|\*=|\/=|%=|&=|\|=|\^=|<<=|>>=)|[^\s]/g)]
    .map((match) => ({ text: match[0], offset: offsets[match.index],
      directive: match[0] === "gl_Position" && inDirective(clean, match.index) }));
}

function mainTopLevel(words) {
  for (let i = 0; i < words.length; i++) {
    if (words[i] !== "void" || words[i + 1] !== "main" || words[i + 2] !== "(") continue;
    let open = i + 3;
    if (words[open] === "void") open++;
    if (words[open] !== ")" || words[++open] !== "{") continue;
    const indices = new Set();
    let depth = 1;
    let parentheses = 0;
    for (let j = open + 1; j < words.length && depth; j++) {
      if (depth === 1 && parentheses === 0) indices.add(j);
      if (words[j] === "{") depth++;
      if (words[j] === "}") depth--;
      if (words[j] === "(") parentheses++;
      if (words[j] === ")") parentheses--;
    }
    return indices;
  }
  return new Set();
}

function inDirective(source, offset) {
  const start = source.lastIndexOf("\n", offset - 1) + 1;
  return /^\s*#/.test(source.slice(start, offset));
}

export function checkShaderSource(source, file = "shader.vert") {
  const list = tokens(source);
  const words = list.map((token) => token.text);
  const body = mainTopLevel(words);
  const refs = list.flatMap((token, index) => token.text === "gl_Position" ? [index] : []);
  return refs.flatMap((index, occurrence) => {
    let message;
    if (!VERTEX.has(extname(file).toLowerCase())) message = "Keep gl_Position in the vertex entry file; helpers return local values.";
    else if (list[index].directive) message = "Do not hide gl_Position in preprocessor directives or macros.";
    else if (words[index + 1] !== "=" || ![";", "{", "}"].includes(words[index - 1])) message = "Do not read or partially update gl_Position; calculate a local vec4 first.";
    else if (!body.has(index)) message = "Assign gl_Position once at the top level of main, outside helpers, branches and loops.";
    else if (occurrence > 0) message = "Repeated gl_Position write; assign the final local vec4 only once.";
    if (!message) return [];
    return [{ rule: RULE, file, line: source.slice(0, list[index].offset).split("\n").length, message }];
  });
}

export function checkShaderPaths(paths) {
  const files = new Set();
  function visit(path, explicit = false) {
    const stat = lstatSync(path);
    if (stat.isSymbolicLink()) {
      if (explicit) throw new Error(`Use a real shader path, not a symlink: ${path}`);
      return;
    }
    if (stat.isDirectory()) {
      for (const entry of readdirSync(path, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
        if (!SKIP.has(entry.name)) visit(join(path, entry.name));
      }
    } else if (VERTEX.has(extname(path).toLowerCase()) || extname(path).toLowerCase() === ".glsl") files.add(path);
    else if (explicit) throw new Error(`Expected a vertex shader or GLSL include: ${path}`);
  }
  for (const path of paths) visit(resolve(path), true);
  if (!files.size) throw new Error("No vertex shaders or GLSL includes found in the requested paths.");
  return { files: files.size, diagnostics: [...files].flatMap((file) => checkShaderSource(readFileSync(file, "utf8"), file)) };
}

export function main(args = process.argv.slice(2)) {
  const json = args.includes("--json");
  const paths = args.filter((arg) => arg !== "--json");
  try {
    if (!paths.length || paths.some((path) => path.startsWith("--"))) throw new Error("Usage: node ai_studio/quality/check_shader_positions.mjs [--json] <file-or-directory> [...]");
    const result = checkShaderPaths(paths);
    if (json) console.log(JSON.stringify(result));
    else {
      for (const finding of result.diagnostics) console.error(`${finding.file}:${finding.line}: ${finding.rule}: ${finding.message}`);
      console.log(`${RULE}: ${result.files} files, ${result.diagnostics.length} violations`);
    }
    return result.diagnostics.length ? 1 : 0;
  } catch (error) {
    console.error(`${RULE}: ${error.message}`);
    return 2;
  }
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) process.exitCode = main();
