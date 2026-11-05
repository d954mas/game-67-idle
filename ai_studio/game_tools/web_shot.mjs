#!/usr/bin/env node
// Screenshots of a packaged web build for fast visual checks; needs no DevAPI.
//
//   node ai_studio/game_tools/web_shot.mjs --zip <package.zip> --out <dir> [--mode desktop|phone|phone-land]
//     [--lang <code>] [--save <save file> --save-key <slot>] [--storage '{"key":"value"}']
//     [--plan '<json>'|<file.json>] [--tag name]
//
// Default mode is desktop, default plan one shot after load; plan steps are listed in
// lib/web_page_plan.mjs. The site is unpacked to the temp folder, since serving from
// build/*/site locks that folder on Windows. Writes <tag>-<mode>[-<lang>]-<name>.png
// and <tag>-<mode>[-<lang>]-console.txt; prints each picture's path.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { checkPlan, openGamePage, parseFlags, readJsonInput, storageFromFlags, unpackStoreZip, viewportFor } from "./lib/web_page_plan.mjs";

const FLAGS = ["zip", "out", "mode", "lang", "save", "save-key", "storage", "plan", "tag"];

export async function webShot(argv) {
  const flags = parseFlags(argv, FLAGS);
  if (!flags.zip || !flags.out) throw new Error("--zip <package.zip> and --out <dir> are required");
  const mode = flags.mode || "desktop";
  const viewport = viewportFor(mode);
  const plan = checkPlan(flags.plan ? readJsonInput(flags.plan) : [["shot", "view"]]);
  const scripts = storageFromFlags(flags);
  const out = resolve(flags.out);
  const tag = [flags.tag || "shot", mode, ...(flags.lang ? [flags.lang] : [])].join("-");
  mkdirSync(out, { recursive: true });
  const site = unpackStoreZip(flags.zip, "web-shot-site-");
  let page = null;
  try {
    page = await openGamePage({ site, viewport, scripts, lang: flags.lang, profilePrefix: "ai-studio-web-shot",
      shot: async (name, call) => {
        const { data } = await call("Page.captureScreenshot", { format: "png" });
        const path = join(out, `${tag}-${name}.png`);
        writeFileSync(path, Buffer.from(data, "base64"));
        console.log(path);
      } });
    await page.load();
    const perf = await page.run(plan);
    if (Object.keys(perf).length) console.log(tag, JSON.stringify(perf));
  } finally {
    if (page) {
      writeFileSync(join(out, `${tag}-console.txt`), page.logs.join("\n"));
      await page.close();
    }
    rmSync(site, { recursive: true, force: true, maxRetries: 5, retryDelay: 500 });
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  webShot(process.argv.slice(2)).catch((error) => {
    console.error(`web_shot: ${error.message}`);
    process.exitCode = 1;
  });
}
