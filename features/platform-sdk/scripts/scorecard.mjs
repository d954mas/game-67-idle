#!/usr/bin/env node
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

function toNumber(value) {
  return typeof value === "number" && Number.isFinite(value) ? value : null;
}

function eventName(event) {
  if (typeof event.name === "string") return event.name;
  if (typeof event.type === "string") return event.type;
  return "";
}

function eventTimestamp(event, fallback) {
  return toNumber(event.ts) ?? toNumber(event.t) ?? toNumber(event.time_ms) ?? fallback;
}

function parseNdjson(input) {
  const events = [];
  for (const line of String(input || "").split(/\r?\n/)) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    events.push(JSON.parse(trimmed));
  }
  return events;
}

function classifyDropOff(events) {
  const names = events.map(eventName).filter(Boolean);
  if (!names.length) return "no-events";
  if (!names.includes("game.loading_finished") && !names.includes("game.ready")) {
    return "before-game-loading-finished";
  }
  if (!names.includes("gameplay.start")) return "before-gameplay-start";
  if (!names.includes("first_60s.complete")) return names[names.length - 1];
  return "none";
}

function recommendation({ first60sCompletion, sessionLengthSec, rewardOrUpgradeInteraction, adBreakOpportunity }) {
  if (first60sCompletion && sessionLengthSec >= 60 && (rewardOrUpgradeInteraction || adBreakOpportunity)) {
    return "continue";
  }
  if (!first60sCompletion && sessionLengthSec < 30) return "kill";
  return "review";
}

export function scorecardFromEvents(events) {
  let startedAt = null;
  let lastTs = null;
  let first60sCompletion = false;
  let rewardOrUpgradeInteraction = false;
  let adBreakOpportunity = false;

  const ordered = [];
  for (const event of events) {
    const fallback = lastTs ?? startedAt ?? 0;
    const ts = eventTimestamp(event, fallback);
    if (event.kind === "header" && toNumber(event.started_at) != null) {
      startedAt = toNumber(event.started_at);
      lastTs = startedAt;
    }
    if (ts != null) lastTs = ts;
    ordered.push({ ...event, __ts: ts });

    const name = eventName(event);
    if (name === "first_60s.complete") first60sCompletion = true;
    if (name === "ad.interstitial.request" || name === "ad.rewarded.request") adBreakOpportunity = true;
    if (
      name === "ad.rewarded.result" && (event.rewarded === true || (event.data && event.data.rewarded === true))
    ) {
      rewardOrUpgradeInteraction = true;
    }
    if (name.startsWith("upgrade.") || name === "items.txn" || name === "progression.levelup") {
      rewardOrUpgradeInteraction = true;
    }
  }

  const start = startedAt ?? (ordered.length ? ordered[0].__ts ?? 0 : 0);
  const end = lastTs ?? start;
  const sessionLengthSec = Math.max(0, Math.round((end - start) / 1000));
  const keyDropOff = classifyDropOff(ordered);

  return {
    first60sCompletion,
    sessionLengthSec,
    keyDropOff,
    rewardOrUpgradeInteraction,
    adBreakOpportunity,
    continueKillRecommendation: recommendation({
      adBreakOpportunity,
      first60sCompletion,
      rewardOrUpgradeInteraction,
      sessionLengthSec,
    }),
  };
}

export function scorecardFromNdjson(input) {
  return scorecardFromEvents(parseNdjson(input));
}

function parseCli(argv) {
  const args = { input: "", pretty: false };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--input") args.input = argv[++i];
    else if (arg === "--pretty") args.pretty = true;
    else throw new Error(`unknown option: ${arg}`);
  }
  if (!args.input) throw new Error("--input is required");
  return args;
}

if (process.argv[1] && resolve(fileURLToPath(import.meta.url)) === resolve(process.argv[1])) {
  try {
    const args = parseCli(process.argv.slice(2));
    const result = scorecardFromNdjson(readFileSync(args.input, "utf8"));
    console.log(JSON.stringify(result, null, args.pretty ? 2 : 0));
  } catch (error) {
    console.error(`platform-sdk scorecard: ${error.message}`);
    process.exitCode = 2;
  }
}
