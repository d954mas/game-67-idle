/* The board manifest: one source of truth for what boards a game has and who
   serves them. Validation, the generated C table, the console checklist and the
   Playgama config block all read this file, so a board id is written once. */

export const PUBLISH_TARGETS = ["local", "itch", "poki", "yandex", "playgama", "crazygames"];

/* What each target's portal can actually serve, from SPEC.md section 2. A
   portal board never resets, so no portal can hold a day scope; the http
   backend is the only source that can. */
export const PORTAL_ABILITY = {
  yandex: { boards: true, read: true, write: true, scopes: ["all_time"] },
  crazygames: { boards: true, read: false, write: true, scopes: ["all_time"] },
  playgama: { boards: true, read: "runtime", write: true, scopes: ["all_time"] },
  poki: { boards: false, read: false, write: false, scopes: [] },
  itch: { boards: false, read: false, write: false, scopes: [] },
  local: { boards: false, read: false, write: false, scopes: [] },
};

const HTTP_SCOPES = ["all_time", "utc_day"];
const SCOPE_ENUM = { all_time: "LEADERBOARD_SCOPE_ALL_TIME", utc_day: "LEADERBOARD_SCOPE_UTC_DAY" };
const SORT_ENUM = { desc: "LEADERBOARD_SORT_DESC", asc: "LEADERBOARD_SORT_ASC" };

export function portalId(board, target) {
  const raw = (board.portal_ids || {})[target];
  if (raw == null) return null;
  return typeof raw === "string" ? { id: raw, isMain: false } : { isMain: false, ...raw };
}

/* Every problem, not the first one: an author fixing a manifest wants the whole
   list, and a half-valid manifest fails at run time as a missing board. */
export function validateManifest(manifest, { file = "leaderboards.json" } = {}) {
  const errors = [];
  const fail = (msg) => errors.push(`${file}: ${msg}`);

  if (!manifest || typeof manifest !== "object") {
    fail("not an object");
    return errors;
  }
  if (manifest.schema !== "ai_studio.leaderboards.v1") {
    fail(`schema must be "ai_studio.leaderboards.v1", got ${JSON.stringify(manifest.schema)}`);
  }
  const boards = manifest.boards;
  if (!Array.isArray(boards) || boards.length === 0) {
    fail("boards must be a non-empty array");
    return errors;
  }
  if (boards.length > 4) fail(`at most 4 boards, got ${boards.length}`);

  const seen = new Set();
  for (const board of boards) {
    const id = board && board.id;
    const where = `board ${JSON.stringify(id ?? "?")}`;
    if (typeof id !== "string" || !/^[a-z][a-z0-9_]{0,30}$/.test(id)) {
      fail(`${where}: id must match [a-z][a-z0-9_]{0,30}`);
      continue;
    }
    if (seen.has(id)) fail(`${where}: duplicate id`);
    seen.add(id);

    if (typeof board.metric !== "string" || board.metric === "") fail(`${where}: metric is required`);
    if (!["desc", "asc"].includes(board.sort)) fail(`${where}: sort must be desc or asc`);

    const scopes = Array.isArray(board.scopes) ? board.scopes : [];
    if (scopes.length === 0) fail(`${where}: scopes must be a non-empty array`);
    for (const scope of scopes) {
      if (!SCOPE_ENUM[scope]) fail(`${where}: unknown scope ${JSON.stringify(scope)}`);
    }

    const backends = board.backends || {};
    for (const target of Object.keys(backends)) {
      if (!PUBLISH_TARGETS.includes(target)) fail(`${where}: unknown publish target ${JSON.stringify(target)}`);
    }
    for (const target of PUBLISH_TARGETS) {
      const family = backends[target];
      if (!family) {
        fail(`${where}: publish target ${target} has no backend family`);
        continue;
      }
      if (!["portal", "http", "none"].includes(family)) {
        fail(`${where}: ${target} family must be portal, http or none`);
        continue;
      }
      if (family !== "portal") continue;
      if (!PORTAL_ABILITY[target].boards) {
        fail(`${where}: ${target} has no leaderboard API, so its family cannot be portal`);
        continue;
      }
      if (!portalId(board, target)) {
        fail(`${where}: family is portal on ${target} but portal_ids.${target} is missing`);
      }
    }

    /* A scope nobody can serve is an authoring mistake; a scope only some
       targets serve is normal and the caps simply drop it there. */
    for (const scope of scopes.filter((s) => SCOPE_ENUM[s])) {
      const served = PUBLISH_TARGETS.some((target) => {
        const family = backends[target];
        if (family === "http") return HTTP_SCOPES.includes(scope);
        if (family === "portal") return PORTAL_ABILITY[target].scopes.includes(scope);
        return false;
      });
      if (!served) fail(`${where}: no backend on any target can serve the ${scope} scope`);
    }

    for (const target of Object.keys(board.portal_ids || {})) {
      if (!PUBLISH_TARGETS.includes(target)) fail(`${where}: portal_ids names unknown target ${JSON.stringify(target)}`);
    }
  }
  return errors;
}

function scopeMask(scopes) {
  return scopes.map((s) => `(1u << ${SCOPE_ENUM[s]})`).join(" | ") || "0u";
}

/* The table the game hands to leaderboard_init, plus the ids it refers to
   boards by. Which backend family runs is a build-target answer, so the header
   is generated per target. */
export function generateHeader(manifest, target) {
  if (!PUBLISH_TARGETS.includes(target)) throw new Error(`unknown publish target ${target}`);
  const guard = "GAME_LEADERBOARDS_GENERATED_H";
  const lines = [
    "/* Generated from leaderboards.json. Do not edit; edit the manifest and rebuild. */",
    `#ifndef ${guard}`,
    `#define ${guard}`,
    "",
    '#include "features/leaderboard/leaderboard.h"',
    "",
    `#define GAME_LEADERBOARD_TARGET "${target}"`,
    "",
  ];
  for (const board of manifest.boards) {
    lines.push(`#define GAME_LEADERBOARD_${board.id.toUpperCase()} "${board.id}"`);
  }
  lines.push("");
  lines.push("static const leaderboard_board_def_t GAME_LEADERBOARD_BOARDS[] = {");
  for (const board of manifest.boards) {
    const family = board.backends[target];
    const pid = family === "portal" ? portalId(board, target) : null;
    lines.push("    {");
    lines.push(`        .id = "${board.id}",`);
    lines.push(`        .sort = ${SORT_ENUM[board.sort]},`);
    lines.push(`        .scopes = ${scopeMask(board.scopes)},`);
    lines.push(`        .portal_id = ${pid ? `"${pid.id}"` : "NULL"},`);
    lines.push("    },");
  }
  lines.push("};");
  lines.push("");
  lines.push(`#define GAME_LEADERBOARD_BOARD_COUNT ${manifest.boards.length}`);
  const families = new Set(manifest.boards.map((b) => b.backends[target]));
  lines.push("");
  lines.push("/* Which backend this build links; a game with mixed families asks per board. */");
  for (const family of ["portal", "http", "none"]) {
    lines.push(`#define GAME_LEADERBOARD_HAS_${family.toUpperCase()} ${families.has(family) ? 1 : 0}`);
  }
  lines.push("");
  lines.push(`#endif /* ${guard} */`);
  return lines.join("\n") + "\n";
}

/* Neither Yandex nor CrazyGames can create a board over an API, so the boards a
   human must type into a console are printed rather than silently assumed. */
export function consoleChecklist(manifest) {
  const out = [];
  for (const board of manifest.boards) {
    for (const target of PUBLISH_TARGETS) {
      if (board.backends[target] !== "portal") continue;
      const pid = portalId(board, target);
      if (target === "yandex") {
        out.push(
          `yandex: create a leaderboard with technical name "${pid.id}", sort ${board.sort}, integer score, ` +
            `and a localized display name for every language of the card.`
        );
      } else if (target === "crazygames") {
        out.push(
          `crazygames: request the leaderboard for this game (invite-only) and set its board to "${pid.id}"; ` +
            `the portal renders it, the game only submits.`
        );
      } else if (target === "playgama") {
        out.push(
          `playgama: declare "${pid.id}"${pid.isMain ? " as isMain" : ""} in playgama-bridge-config.json; ` +
            `whether it is readable is decided at run time by the host platform.`
        );
      }
    }
  }
  return out;
}

/* Rewrites only the leaderboards block, as text: a Playgama config is
   hand-authored, carries a readme and its own spacing, and a parse-and-restringify
   would silently reformat somebody's file. */
export function mergeLeaderboardsBlock(text, entries) {
  JSON.parse(text); /* refuse to edit a file that is not valid JSON */
  const key = text.indexOf('"leaderboards"');
  const block = entries.length === 0 ? null : renderBlock(entries, detectIndent(text));

  if (key === -1) {
    if (block === null) return text;
    const close = text.lastIndexOf("}");
    const before = text.slice(0, close).replace(/\s*$/, "");
    const comma = before.endsWith("{") ? "" : ",";
    return `${before}${comma}\n${block}\n${text.slice(close)}`;
  }

  const valueStart = text.indexOf("[", key);
  const valueEnd = matchBracket(text, valueStart);
  if (valueStart === -1 || valueEnd === -1) throw new Error("leaderboards block is not an array");
  if (block === null) {
    /* Drop the whole entry, its indentation and one neighbouring comma. */
    let start = text.lastIndexOf("\n", key);
    let end = valueEnd + 1;
    if (text[end] === ",") end += 1;
    else if (text.slice(0, start).trimEnd().endsWith(",")) start = text.lastIndexOf(",", start);
    return text.slice(0, start) + text.slice(end);
  }
  const lineStart = text.lastIndexOf("\n", key) + 1;
  return text.slice(0, lineStart) + block.replace(/^\s+/, text.slice(lineStart, key)) + text.slice(valueEnd + 1);
}

function detectIndent(text) {
  const match = text.match(/\n(\s+)"/);
  return match ? match[1] : "  ";
}

function renderBlock(entries, indent) {
  /* One entry per line in the spacing a Playgama config is written in. */
  const lines = entries.map((entry) => {
    const fields = Object.entries(entry).map(([k, v]) => `"${k}": ${JSON.stringify(v)}`);
    return `${indent}${indent}{ ${fields.join(", ")} }`;
  });
  return `${indent}"leaderboards": [\n${lines.join(",\n")}\n${indent}]`;
}

function matchBracket(text, open) {
  let depth = 0;
  for (let i = open; i < text.length; i += 1) {
    if (text[i] === "[") depth += 1;
    else if (text[i] === "]") {
      depth -= 1;
      if (depth === 0) return i;
    }
  }
  return -1;
}

export function playgamaLeaderboards(manifest) {
  const entries = [];
  for (const board of manifest.boards) {
    if (board.backends.playgama !== "portal") continue;
    const pid = portalId(board, "playgama");
    if (!pid) continue;
    entries.push(pid.isMain ? { id: pid.id, isMain: true } : { id: pid.id });
  }
  return entries;
}
