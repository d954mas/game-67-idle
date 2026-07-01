import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../../../../", import.meta.url)));
const tool = resolve(root, "ai_studio/assets/workflow/art_jobs/plan_source_sheet_prompt.mjs");

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "source-prompt-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function run(args, cwd) {
  return spawnSync(process.execPath, [tool, ...args], {
    cwd,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function writeJob(dir) {
  mkdirSync(join(dir, "games/test/design/art_requests"), { recursive: true });
  const jobPath = "games/test/design/art_requests/ui-job.json";
  writeFileSync(join(dir, jobPath), `${JSON.stringify({
    schema: "game.art_job",
    id: "ui-job",
    asset_family: "prepared-ui-kit",
    reusable_kinds: ["slice9", "icon", "sprite"],
    required_asset_groups: [
      {
        id: "panel_slice9",
        kind: "slice9",
        need: "blank resizable panel and button bases",
      },
      {
        id: "resource_icons",
        kind: "icon",
        need: "isolated resource icons",
      },
    ],
    must_not_bake: ["button labels", "game state values"],
    generation_contract: {
      source_families: ["blank UI kit sheet", "isolated icon sheet"],
      source_family_roles: {
        "blank UI kit sheet": "stretchable bases without unique stretch-zone ornaments",
        "isolated icon sheet": "semantic icons with gutters",
      },
      final_asset_policy: {
        procedural_art_allowed: "debug_only",
        final_must_use_generated_or_artist_source: true,
      },
      prompt_constraints: ["no readable text", "clear gutters between assets", "no unique decoration inside slice9 stretch zones"],
    },
    qa_rejects: ["watermarks", "weak silhouette at gameplay size"],
  }, null, 2)}\n`, "utf8");
  return jobPath;
}

test("writes prompt packet from art job and intake suggested key color", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  const audit = "intake.json";
  writeFileSync(join(dir, audit), `${JSON.stringify({ suggested_key_color: "#00ff00" })}\n`, "utf8");
  const markdown = "prompt.md";
  const json = "prompt.json";

  const result = run([
    "--job", job,
    "--source-family", "blank UI kit sheet",
    "--intake-audit", audit,
    "--output", markdown,
    "--json-output", json,
  ], dir);

  assert.equal(result.status, 0, result.stdout + result.stderr);
  const text = readFileSync(join(dir, markdown), "utf8");
  assert.match(text, /flat chroma background #00ff00/);
  assert.match(text, /not a gameplay screenshot/);
  assert.match(text, /no unique ornaments that will stretch/);
  assert.match(text, /row_major_grid/);
  assert.match(text, /48px gutters/);
  assert.match(text, /Row 1 large_slice9_bases/);
  assert.match(text, /## Source Sheet Layout/);
  const packet = JSON.parse(readFileSync(join(dir, json), "utf8"));
  assert.equal(packet.suggested_key_color, "#00ff00");
  assert.equal(packet.key_color_source, "intake_audit");
  assert.equal(packet.relevant_asset_groups.length, 1);
  assert.equal(packet.relevant_asset_groups[0].id, "panel_slice9");
  assert.equal(packet.source_sheet_layout.sheet_role, "cuttable_source_sheet");
  assert.equal(packet.source_sheet_layout.placement.mode, "row_major_grid");
  assert.equal(packet.source_sheet_layout.placement.gutter_px_min, 48);
  assert.equal(packet.source_sheet_layout.placement.allow_composed_ui_screen, false);
  assert.equal(packet.source_sheet_layout.rows[0].id, "large_slice9_bases");
  assert.equal(packet.source_sheet_layout.rows[1].id, "button_and_chip_bases");
});

test("uses intake next prompt key color before fallback suggested key color", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  writeFileSync(join(dir, "intake.json"), `${JSON.stringify({
    key_color: "#ff00ff",
    suggested_key_color: "#00ff00",
    key_color_action: "regenerate_with_next_prompt_key_color",
    next_prompt_key_color: "#00ffff",
    recommended_next_step: {
      action: "regenerate_source_sheet_with_safer_key_color",
      reason: "current key color conflicts with visible component art or halo colors",
      key_color: "#00ffff",
    },
    blocking_reasons: [
      {
        code: "key_color_conflict",
        count: 1,
        action: "regenerate_source_sheet_with_safer_key_color",
      },
    ],
  })}\n`, "utf8");

  const result = run([
    "--job", job,
    "--source-family", "blank UI kit sheet",
    "--intake-audit", "intake.json",
    "--output", "prompt.md",
    "--json-output", "prompt.json",
  ], dir);

  assert.equal(result.status, 0, result.stdout + result.stderr);
  const text = readFileSync(join(dir, "prompt.md"), "utf8");
  assert.match(text, /flat chroma background #00ffff/);
  const packet = JSON.parse(readFileSync(join(dir, "prompt.json"), "utf8"));
  assert.equal(packet.suggested_key_color, "#00ffff");
  assert.equal(packet.key_color_source, "intake_audit");
  assert.equal(packet.intake_key_color_action, "regenerate_with_next_prompt_key_color");
  assert.equal(packet.intake_recommended_next_step.action, "regenerate_source_sheet_with_safer_key_color");
  assert.equal(packet.intake_blocking_reasons[0].code, "key_color_conflict");
  assert.match(text, /recommended_next_step: regenerate_source_sheet_with_safer_key_color/);
  assert.match(text, /blocking_reasons: key_color_conflict/);
});

test("refuses chroma prompt when intake says split preserve or dual plate", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  writeFileSync(join(dir, "intake.json"), `${JSON.stringify({
    key_color: "#ff00ff",
    suggested_key_color: "#00ffff",
    key_color_action: "split_preserve_or_dual_plate_alpha",
    next_prompt_key_color: null,
  })}\n`, "utf8");

  const result = run([
    "--job", job,
    "--source-family", "blank UI kit sheet",
    "--intake-audit", "intake.json",
    "--output", "prompt.md",
  ], dir);

  assert.equal(result.status, 1);
  assert.match(result.stderr, /refusing to create another chroma prompt/);
});

test("derives preserve refusal from intake recommended next step", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  writeFileSync(join(dir, "intake.json"), `${JSON.stringify({
    key_color: "#ff00ff",
    next_prompt_key_color: null,
    recommended_next_step: {
      action: "split_preserve_or_dual_plate_alpha",
      reason: "key color conflicts exist but no safer candidate key color was found",
      key_color: null,
    },
    blocking_reasons: [
      {
        code: "key_color_conflict",
        count: 1,
        action: "split_preserve_or_dual_plate_alpha",
      },
    ],
  })}\n`, "utf8");

  const result = run([
    "--job", job,
    "--source-family", "blank UI kit sheet",
    "--intake-audit", "intake.json",
    "--output", "prompt.md",
  ], dir);

  assert.equal(result.status, 1);
  assert.match(result.stderr, /refusing to create another chroma prompt/);
});

test("allows explicit diagnostic chroma override after preserve risk", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  writeFileSync(join(dir, "intake.json"), `${JSON.stringify({
    key_color: "#ff00ff",
    suggested_key_color: "#00ffff",
    key_color_action: "split_preserve_or_dual_plate_alpha",
    next_prompt_key_color: null,
  })}\n`, "utf8");

  const result = run([
    "--job", job,
    "--source-family", "blank UI kit sheet",
    "--intake-audit", "intake.json",
    "--allow-chroma-after-preserve-risk",
    "--output", "prompt.md",
    "--json-output", "prompt.json",
  ], dir);

  assert.equal(result.status, 0, result.stdout + result.stderr);
  const packet = JSON.parse(readFileSync(join(dir, "prompt.json"), "utf8"));
  assert.equal(packet.suggested_key_color, "#00ffff");
  assert.equal(packet.key_color_source, "intake_audit");
  assert.equal(packet.intake_key_color_action, "split_preserve_or_dual_plate_alpha");
  assert.equal(packet.diagnostic_chroma_override, true);
});

test("uses explicit key color over intake audit", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  writeFileSync(join(dir, "intake.json"), `${JSON.stringify({ suggested_key_color: "#00ff00" })}\n`, "utf8");

  const result = run([
    "--job", job,
    "--source-family", "isolated icon sheet",
    "--intake-audit", "intake.json",
    "--key-color", "#00ffff",
    "--output", "prompt.md",
    "--json-output", "prompt.json",
  ], dir);

  assert.equal(result.status, 0, result.stdout + result.stderr);
  const packet = JSON.parse(readFileSync(join(dir, "prompt.json"), "utf8"));
  assert.equal(packet.suggested_key_color, "#00ffff");
  assert.equal(packet.key_color_source, "explicit_override");
  assert.equal(packet.relevant_asset_groups[0].id, "resource_icons");
  assert.equal(packet.source_sheet_layout.family_kind, "icon");
  assert.equal(packet.source_sheet_layout.placement.gutter_px_min, 64);
  assert.equal(packet.source_sheet_layout.rows[0].id, "core_gameplay_icons");
  assert.match(packet.prompt, /one centered silhouette per slot/);
});

test("uses custom source sheet layout rows from art job", (t) => {
  const dir = tempDir(t);
  mkdirSync(join(dir, "games/test/design/art_requests"), { recursive: true });
  const jobPath = "games/test/design/art_requests/equipment-job.json";
  writeFileSync(join(dir, jobPath), `${JSON.stringify({
    schema: "game.art_job",
    id: "equipment-job",
    asset_family: "equipment sprites",
    reusable_kinds: ["sprite", "icon", "slice9"],
    required_asset_groups: [
      {
        id: "equipment_item_sprites",
        kind: "sprite",
        role: "isolated equipment sprites",
      },
    ],
    must_not_bake: ["button labels", "game state values"],
    generation_contract: {
      source_families: ["isolated equipment sprite sheet"],
      source_family_roles: {
        "isolated equipment sprite sheet": "standalone equipment sprites",
      },
      source_sheet_layout: {
        canvas: [1792, 1344],
        outer_margin_px: 64,
        min_gutter_px: 48,
        rows: [
          {
            id: "mining_first",
            slots: ["item_pickaxe_worn", "item_pickaxe_copper"],
          },
          {
            id: "progression_trinkets",
            slots: ["item_plain_ring", "item_ruby_ring"],
          },
        ],
      },
      final_asset_policy: {
        procedural_art_allowed: "debug_only",
        final_must_use_generated_or_artist_source: true,
      },
      prompt_constraints: ["clear gutters between assets"],
    },
    qa_rejects: ["watermarks"],
  }, null, 2)}\n`, "utf8");

  const result = run([
    "--job", jobPath,
    "--source-family", "isolated equipment sprite sheet",
    "--key-color", "#0000ff",
    "--output", "prompt.md",
    "--json-output", "prompt.json",
  ], dir);

  assert.equal(result.status, 0, result.stdout + result.stderr);
  const text = readFileSync(join(dir, "prompt.md"), "utf8");
  assert.match(text, /row 1 `mining_first`/);
  assert.match(text, /item_pickaxe_worn/);
  assert.doesNotMatch(text, /isolated_assets/);
  const packet = JSON.parse(readFileSync(join(dir, "prompt.json"), "utf8"));
  assert.deepEqual(packet.source_sheet_layout.recommended_canvas.size_px, [1792, 1344]);
  assert.equal(packet.source_sheet_layout.rows[0].id, "mining_first");
  assert.deepEqual(packet.source_sheet_layout.rows[1].slots, ["item_plain_ring", "item_ruby_ring"]);
});

test("rejects source family outside generation contract", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  const result = run([
    "--job", job,
    "--source-family", "whole game screenshot",
    "--output", "prompt.md",
  ], dir);

  assert.equal(result.status, 1);
  assert.match(result.stderr, /not listed in generation_contract/);
});
