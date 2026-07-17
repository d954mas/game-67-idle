import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

import { validateStyleLock, validateStyleLockFile } from "./validate.mjs";
import { resolveGenerationOrigin } from "./generation_origin.mjs";

const readJson = (relative) => JSON.parse(readFileSync(fileURLToPath(new URL(relative, import.meta.url)), "utf8"));
const example = readJson("./style_lock.example.json");
const schema = readJson("./style_lock.schema.json");
const clone = (value) => structuredClone(value);

test("committed example and schema expose the v1 style-lock contract", () => {
  assert.equal(schema.$id, "https://ai-studio.local/schemas/game-style-lock-v1.json");
  assert.equal(schema.properties.schema.const, "ai_studio.game.style_lock.v1");
  assert.equal(schema.properties.exemplar_refs.allOf.length, 2);
  assert.match(schema.$comment, /validate\.mjs.*same Canvas project/i);
  assert.equal("technical_thresholds" in schema.properties, false);
  assert.ok(schema.properties.technical_gate);
  assert.equal(schema.allOf[0].then.properties.technical_gate.properties.max_spill_edge_ratio.type, "number");
  assert.equal(schema.allOf[0].else.properties.technical_gate.properties.max_spill_edge_ratio.type, "null");
  assert.deepEqual(validateStyleLock(example), example);
});

test("generation origin distinguishes explore, locked production, and explicit no-lock taint", () => {
  const root = mkdtempSync(join(tmpdir(), "style-lock-origin-"));
  try {
    const unowned = resolveGenerationOrigin(root, {});
    assert.deepEqual(unowned, {
      schema: "ai_studio.asset.generation_origin.v1",
      source: "ai",
      mode: "explore",
      game_id: null,
      style_lock_id: null,
      tainted: false,
      taint_reason: null,
    });

    const ownedProject = { ownership: { kind: "game", gameId: example.game_id } };
    assert.throws(
      () => resolveGenerationOrigin(root, { ownership: { kind: "other", gameId: example.game_id } }),
      /ownership must be.*kind:game/i,
    );
    assert.throws(
      () => resolveGenerationOrigin(root, { ownership: { kind: "game", gameId: "private/example-game" } }),
      /ownership\.gameId must be a lowercase slug/i,
    );
    assert.throws(
      () => resolveGenerationOrigin(root, { ownership: { kind: "game", gameId: "" } }, { noLock: true }),
      /ownership\.gameId must be a lowercase slug/i,
    );
    assert.throws(
      () => resolveGenerationOrigin(root, ownedProject),
      /production generation requires.*style_lock\.json.*--no-lock/i,
    );
    assert.deepEqual(resolveGenerationOrigin(root, ownedProject, { noLock: true }), {
      schema: "ai_studio.asset.generation_origin.v1",
      source: "ai",
      mode: "explore",
      game_id: example.game_id,
      style_lock_id: null,
      tainted: true,
      taint_reason: "no-lock",
    });

    const designDir = join(root, "games", example.game_id, "design");
    mkdirSync(join(designDir, "art"), { recursive: true });
    writeFileSync(join(designDir, "art", "art_contract.json"), "{}\n");
    const lockPath = join(designDir, "style_lock.json");
    writeFileSync(lockPath, `${JSON.stringify(example, null, 2)}\n`);
    assert.deepEqual(resolveGenerationOrigin(root, ownedProject), {
      schema: "ai_studio.asset.generation_origin.v1",
      source: "ai",
      mode: "production",
      game_id: example.game_id,
      style_lock_id: example.id,
      tainted: false,
      taint_reason: null,
    });

    const draft = clone(example);
    draft.status = "draft";
    writeFileSync(lockPath, `${JSON.stringify(draft, null, 2)}\n`);
    assert.throws(() => resolveGenerationOrigin(root, ownedProject), /requires an accepted style lock/i);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("style lock requires 2-3 owned Canvas exemplars covering world and GUI", () => {
  const tooFew = clone(example);
  tooFew.exemplar_refs = tooFew.exemplar_refs.slice(0, 1);
  assert.throws(() => validateStyleLock(tooFew), /2-3 owned exemplar/i);

  const sourced = clone(example);
  sourced.exemplar_refs[0].origin = "sourced";
  assert.throws(() => validateStyleLock(sourced), /origin must be owned/i);

  const crossProject = clone(example);
  crossProject.exemplar_refs[0].ref = "canvas://other-board/element/el_world";
  assert.throws(() => validateStyleLock(crossProject), /same Canvas store\/project/i);

  const oneDomain = clone(example);
  oneDomain.exemplar_refs = oneDomain.exemplar_refs.map((entry) => ({ ...entry, domain: "world" }));
  assert.throws(() => validateStyleLock(oneDomain), /world and gui/i);

  const duplicateRef = clone(example);
  duplicateRef.exemplar_refs[1].ref = duplicateRef.exemplar_refs[0].ref;
  assert.throws(() => validateStyleLock(duplicateRef), /unique Canvas elements/i);
});

test("style lock confines references and rejects ambiguous palette/background rules", () => {
  const escapedContract = clone(example);
  escapedContract.art_contract_ref = "../private/art_contract.json";
  assert.throws(() => validateStyleLock(escapedContract), /art_contract_ref/i);

  const duplicatePalette = clone(example);
  duplicatePalette.palette.push(duplicatePalette.palette[0]);
  assert.throws(() => validateStyleLock(duplicatePalette), /palette.*unique/i);

  const lowercasePalette = clone(example);
  lowercasePalette.palette[0] = lowercasePalette.palette[0].toLowerCase();
  assert.throws(() => validateStyleLock(lowercasePalette), /#RRGGBB/);

  const unsafeChroma = clone(example);
  unsafeChroma.bg_rule.key_color = "#123456";
  assert.throws(() => validateStyleLock(unsafeChroma), /magenta or green/i);
});

test("parked model checkpoints stay unused", () => {
  const checkpoint = clone(example);
  checkpoint.model_checkpoint = "style-v1.safetensors";
  assert.throws(() => validateStyleLock(checkpoint), /model_checkpoint.*null/i);
});

test("technical gate thresholds are complete and conditional on background mode", () => {
  const invalidRatio = clone(example);
  invalidRatio.technical_gate.max_alpha_noise_ratio = 1.1;
  assert.throws(() => validateStyleLock(invalidRatio), /max_alpha_noise_ratio.*0\.\.1/i);

  const transparent = clone(example);
  transparent.bg_rule.mode = "transparent";
  transparent.bg_rule.key_color = null;
  transparent.technical_gate.max_spill_edge_ratio = null;
  transparent.technical_gate.max_halo_edge_ratio = null;
  assert.doesNotThrow(() => validateStyleLock(transparent));

  transparent.technical_gate.max_spill_edge_ratio = 0.05;
  assert.throws(() => validateStyleLock(transparent), /transparent.*spill.*halo.*null/i);

  const keyedWithoutThreshold = clone(example);
  keyedWithoutThreshold.technical_gate.max_spill_edge_ratio = null;
  assert.throws(() => validateStyleLock(keyedWithoutThreshold), /chroma.*spill.*halo.*ratios/i);
});

test("file validation binds a lock to its game path and existing art contract", () => {
  const root = mkdtempSync(join(tmpdir(), "style-lock-"));
  try {
    const designDir = join(root, "games", example.game_id, "design");
    const contractPath = join(designDir, "art", "art_contract.json");
    const lockPath = join(designDir, "style_lock.json");
    mkdirSync(join(designDir, "art"), { recursive: true });
    writeFileSync(contractPath, "{}\n");
    writeFileSync(lockPath, `${JSON.stringify(example, null, 2)}\n`);
    assert.deepEqual(validateStyleLockFile(lockPath, { workspaceRoot: root }), example);

    const privateCanvas = clone(example);
    privateCanvas.canvas_ref = "canvas://game/example-game/style-board/group/grp_style";
    privateCanvas.exemplar_refs[0].ref = "canvas://game/example-game/style-board/element/el_world";
    privateCanvas.exemplar_refs[1].ref = "canvas://game/example-game/style-board/element/el_gui";
    writeFileSync(lockPath, `${JSON.stringify(privateCanvas, null, 2)}\n`);
    assert.throws(() => validateStyleLockFile(lockPath, { workspaceRoot: root }), /public game lock.*public Canvas refs/i);

    const mismatched = clone(example);
    mismatched.game_id = "other-game";
    writeFileSync(lockPath, `${JSON.stringify(mismatched, null, 2)}\n`);
    assert.throws(() => validateStyleLockFile(lockPath, { workspaceRoot: root }), /path game id.*game_id/i);

    const dangling = clone(example);
    dangling.art_contract_ref = "design/missing/art_contract.json";
    writeFileSync(lockPath, `${JSON.stringify(dangling, null, 2)}\n`);
    assert.throws(() => validateStyleLockFile(lockPath, { workspaceRoot: root }), /art_contract_ref.*does not exist/i);

    const outsidePath = join(root, "style_lock.json");
    writeFileSync(outsidePath, `${JSON.stringify(example, null, 2)}\n`);
    assert.throws(() => validateStyleLockFile(outsidePath, { workspaceRoot: root }), /workspace games path/i);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("file validation supports private games and rejects physical art-contract escapes", () => {
  const root = mkdtempSync(join(tmpdir(), "style-lock-private-"));
  try {
    const privateLock = clone(example);
    privateLock.game_id = "private-example";
    privateLock.canvas_ref = "canvas://game/private-example/style-board/group/grp_style";
    privateLock.exemplar_refs[0].ref = "canvas://game/private-example/style-board/element/el_world";
    privateLock.exemplar_refs[1].ref = "canvas://game/private-example/style-board/element/el_gui";
    const designDir = join(root, "games", "private", privateLock.game_id, "design");
    const contractPath = join(designDir, "art", "art_contract.json");
    const lockPath = join(designDir, "style_lock.json");
    mkdirSync(join(designDir, "art"), { recursive: true });
    writeFileSync(contractPath, "{}\n");
    writeFileSync(lockPath, `${JSON.stringify(privateLock, null, 2)}\n`);
    assert.deepEqual(validateStyleLockFile(lockPath, { workspaceRoot: root }), privateLock);

    const publicCanvas = clone(privateLock);
    publicCanvas.canvas_ref = "canvas://style-board/group/grp_style";
    publicCanvas.exemplar_refs[0].ref = "canvas://style-board/element/el_world";
    publicCanvas.exemplar_refs[1].ref = "canvas://style-board/element/el_gui";
    writeFileSync(lockPath, `${JSON.stringify(publicCanvas, null, 2)}\n`);
    assert.throws(() => validateStyleLockFile(lockPath, { workspaceRoot: root }), /private game lock.*private Canvas refs/i);

    const directoryContract = join(designDir, "directory", "art_contract.json");
    mkdirSync(directoryContract, { recursive: true });
    privateLock.art_contract_ref = "design/directory/art_contract.json";
    writeFileSync(lockPath, `${JSON.stringify(privateLock, null, 2)}\n`);
    assert.throws(() => validateStyleLockFile(lockPath, { workspaceRoot: root }), /art_contract_ref.*regular file/i);

    const outside = join(root, "outside");
    mkdirSync(outside);
    writeFileSync(join(outside, "art_contract.json"), "{}\n");
    symlinkSync(outside, join(designDir, "linked"), process.platform === "win32" ? "junction" : "dir");
    privateLock.art_contract_ref = "design/linked/art_contract.json";
    writeFileSync(lockPath, `${JSON.stringify(privateLock, null, 2)}\n`);
    assert.throws(() => validateStyleLockFile(lockPath, { workspaceRoot: root }), /art_contract_ref.*physical game design directory/i);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("unknown fields and malformed public/private Canvas refs fail closed", () => {
  const unknown = clone(example);
  unknown.palette_distance_gate = true;
  assert.throws(() => validateStyleLock(unknown), /unexpected fields/i);

  const malformed = clone(example);
  malformed.canvas_ref = "canvas://bare-project";
  assert.throws(() => validateStyleLock(malformed), /canvas_ref/i);

  const privateLock = clone(example);
  privateLock.canvas_ref = "canvas://game/example-game/style-board/group/grp_style";
  privateLock.exemplar_refs[0].ref = "canvas://game/example-game/style-board/element/el_world";
  privateLock.exemplar_refs[1].ref = "canvas://game/example-game/style-board/element/el_gui";
  assert.doesNotThrow(() => validateStyleLock(privateLock));
});
