#!/usr/bin/env node
/* Checks a store folder against a portal's declared spec and says what is
 * missing or wrong. A moderation queue that runs for a month is not a place to
 * discover that an icon is 500 px wide.
 *
 *   node ai_studio/store_kit/check.mjs --portal yandex --dir <games/.../release/store/yandex>
 *
 * The spec lives beside this file (yandex_spec.json) and the draft form in the
 * portal's console is what it answers to.
 */

import { readFileSync, readdirSync, statSync, existsSync } from "node:fs";
import { basename, dirname, extname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));

function parseArgs(argv) {
  const args = { portal: "yandex", dir: null };
  for (let i = 2; i < argv.length; ++i) {
    if (argv[i] === "--portal" && argv[i + 1]) args.portal = argv[++i];
    else if (argv[i] === "--dir" && argv[i + 1]) args.dir = argv[++i];
  }
  if (!args.dir) throw new Error("usage: check.mjs --portal <name> --dir <store folder>");
  return args;
}

/* Width and height without a decoder: PNG keeps them in the IHDR, JPEG in the
 * first SOF marker. Anything else is refused rather than guessed at. */
function imageSize(file) {
  const buf = readFileSync(file);
  if (buf.length > 24 && buf.toString("ascii", 1, 4) === "PNG") {
    return { format: "png", width: buf.readUInt32BE(16), height: buf.readUInt32BE(20) };
  }
  if (buf.length > 4 && buf[0] === 0xff && buf[1] === 0xd8) {
    let offset = 2;
    while (offset + 9 < buf.length) {
      if (buf[offset] !== 0xff) { offset += 1; continue; }
      const marker = buf[offset + 1];
      const length = buf.readUInt16BE(offset + 2);
      if (marker >= 0xc0 && marker <= 0xcf && marker !== 0xc4 && marker !== 0xc8 && marker !== 0xcc) {
        return { format: "jpg", height: buf.readUInt16BE(offset + 5), width: buf.readUInt16BE(offset + 7) };
      }
      offset += 2 + length;
    }
  }
  return null;
}

function match(files, entry) {
  if (entry.file) return files.filter((f) => f === entry.file);
  const glob = entry.file_glob.replace(/[.*+?^${}()|[\]\\]/g, "\\$&").replace(/\\\*/g, ".*");
  const re = new RegExp("^" + glob + "$");
  return files.filter((f) => re.test(f));
}

function checkVideo(dir, entry, found, problems, notes) {
  for (const name of found) {
    const bytes = statSync(join(dir, name)).size;
    if (entry.max_bytes && bytes > entry.max_bytes) {
      problems.push(`${name}: ${(bytes / 1048576).toFixed(2)} MB over the ${(entry.max_bytes / 1048576).toFixed(2)} MB cap`);
    }
    const ext = extname(name).slice(1).toLowerCase();
    if (entry.formats && !entry.formats.includes(ext)) {
      problems.push(`${name}: format .${ext}, allowed ${entry.formats.join("/")}`);
    }
  }
  /* Duration and ratio are not read here: the form states them and rejects a
   * file that misses them, and no decoder belongs in this checker. */
  notes.push(`${entry.id}: ${entry.aspect || "ratio"} and the ${entry.max_seconds}s limit are the form's to enforce`);
}

function checkAsset(dir, entry, problems, notes) {
  const files = readdirSync(dir).filter((f) => statSync(join(dir, f)).isFile());
  const found = match(files, entry);
  const min = entry.min_count ?? 1;
  const max = entry.max_count ?? 1;
  if (found.length < min) {
    problems.push(`${entry.id}: expected at least ${min} file(s) (${entry.file || entry.file_glob}), found ${found.length}`);
    return;
  }
  if (found.length > max) problems.push(`${entry.id}: ${found.length} files, the portal takes at most ${max}`);
  if (entry.kind === "video") { checkVideo(dir, entry, found, problems, notes); return; }
  for (const name of found) {
    const path = join(dir, name);
    const size = imageSize(path);
    const bytes = statSync(path).size;
    const ext = extname(name).slice(1).toLowerCase();
    if (!size) { problems.push(`${name}: not a PNG or JPEG`); continue; }
    if (entry.formats && !entry.formats.includes(size.format)) {
      problems.push(`${name}: format ${size.format}, allowed ${entry.formats.join("/")}`);
    }
    if (ext !== size.format && !(ext === "jpeg" && size.format === "jpg")) {
      notes.push(`${name}: extension .${ext} but the bytes are ${size.format}`);
    }
    if (entry.width && (size.width !== entry.width || size.height !== entry.height)) {
      problems.push(`${name}: ${size.width}x${size.height}, the spec says ${entry.width}x${entry.height}`);
    }
    if (entry.square && size.width !== size.height) {
      problems.push(`${name}: an icon must be square, this is ${size.width}x${size.height}`);
    }
    if (entry.max_bytes && bytes > entry.max_bytes) {
      problems.push(`${name}: ${(bytes / 1048576).toFixed(2)} MB over the ${(entry.max_bytes / 1048576).toFixed(2)} MB cap`);
    }
  }
}

function checkTexts(dir, spec, problems, notes) {
  const seen = new Map();
  for (const entry of spec.texts) {
    for (const lang of spec.languages) {
      const name = entry.file.replace("<lang>", lang);
      const path = join(dir, name);
      if (!existsSync(path)) { problems.push(`${name}: missing`); continue; }
      const text = readFileSync(path, "utf8").trim();
      const chars = [...text].length;
      if (entry.min_chars && chars < entry.min_chars) problems.push(`${name}: ${chars} characters, the field wants at least ${entry.min_chars}`);
      if (entry.max_chars && chars > entry.max_chars) problems.push(`${name}: ${chars} characters, over the ${entry.max_chars} limit`);
      if (/(.)\1{4,}/u.test(text)) problems.push(`${name}: a run of repeated characters — the portal reads that as padding`);
      const key = `${lang}:${text}`;
      if (seen.has(key)) problems.push(`${name}: the same text as ${seen.get(key)} — duplicated fields are refused`);
      else seen.set(key, name);
    }
  }
}

function main() {
  const args = parseArgs(process.argv);
  const specPath = resolve(here, `${args.portal}_spec.json`);
  if (!existsSync(specPath)) throw new Error(`no spec for portal ${args.portal}`);
  const spec = JSON.parse(readFileSync(specPath, "utf8"));
  const dir = resolve(args.dir);
  if (!existsSync(dir)) throw new Error(`store folder not found: ${dir}`);

  const problems = [];
  const notes = [];
  for (const entry of spec.assets) checkAsset(dir, entry, problems, notes);
  checkTexts(dir, spec, problems, notes);

  console.log(`store kit: ${args.portal} — ${basename(dir)}`);
  for (const note of notes) console.log(`  note: ${note}`);
  if (problems.length === 0) {
    console.log("  every declared field is present and inside its limits");
    console.log("  still human work: no screenshots as icon or cover, no borders or rounded");
    console.log("  corners, no system or portal interface in the frames, gameplay at 70% of");
    console.log("  each screenshot, one name across game, draft and materials.");
    return;
  }
  for (const problem of problems) console.log(`  PROBLEM ${problem}`);
  process.exitCode = 1;
}

main();
