#!/usr/bin/env node
// Prints the funnel of one game build from the studio telemetry endpoint.
// Read only: it calls the report path of the same function the games post to.

import { renderReport } from "./lib/render.mjs";

const USAGE = `usage:
  node ai_studio/telemetry/report.mjs --url <function-url> --key <key> --game <game-id>
                                      [--build <build-id>] [--platform <portal>] [--json]

--build and --platform are optional; leaving them out aggregates every bucket
of the game. The url and the key are the ones the lead set on the function.`;

function parseArgs(argv) {
  const options = {};
  const args = [...argv];
  while (args.length) {
    const arg = args.shift();
    if (arg === "--json") options.json = true;
    else if (arg === "--help" || arg === "-h") options.help = true;
    else if (arg.startsWith("--")) options[arg.slice(2)] = args.shift();
    else throw new Error(`unexpected argument: ${arg}`);
  }
  return options;
}

function reportUrl(options) {
  const url = new URL(options.url);
  url.searchParams.set("report", "1");
  url.searchParams.set("key", options.key);
  url.searchParams.set("game", options.game);
  if (options.build) url.searchParams.set("build", options.build);
  if (options.platform) url.searchParams.set("platform", options.platform);
  return url;
}

async function main() {
  const options = parseArgs(process.argv.slice(2));
  if (options.help) {
    console.log(USAGE);
    return;
  }
  for (const required of ["url", "key", "game"]) {
    if (!options[required]) {
      console.error(`missing --${required}\n\n${USAGE}`);
      process.exit(2);
    }
  }

  const response = await fetch(reportUrl(options));
  const text = await response.text();
  if (!response.ok) {
    console.error(`report failed: ${response.status} ${text.slice(0, 400)}`);
    process.exit(1);
  }

  const report = JSON.parse(text);
  if (options.json) {
    console.log(JSON.stringify(report, null, 2));
    return;
  }
  console.log(renderReport(report, options));
}

main().catch((error) => {
  console.error(error && error.message ? error.message : String(error));
  process.exit(1);
});
