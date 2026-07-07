// Style card ops tests (T0239 increment 3). A style card is a GROUP carrying an additive
// `style` object — the SAME "group + additive blob" shape as a recipe card (recipe.test.mjs
// is the structural precedent this file mirrors throughout): name + prompt + exactly ONE ref
// image (the only member SENT to generation) + any number of example members (eyes-only,
// NEVER sent). Covers: createStyleCard/patchStyle + the symmetric "cards don't nest inside
// cards" guard, the applyStyleAutoRef membership hook (assignToGroup + pasteNodes/
// duplicateNodes), generateFromRecipe's style-mixing section, patchRecipe's tightened
// style_ref validation, the buildNodesSpec copy/paste pin (tree.mjs itself is untouched —
// group.style rides the existing deep clone for free), and CLI/API parity. Codex/agy NEVER
// spawn in this suite — every generation test injects a fake `generators` map (the T0238/
// T0239 contract).
// Run: node --test ai_studio/assets/canvas/tests/style.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import {
  addImage,
  addText,
  assignToGroup,
  createGroup,
  createProject,
  createRecipeCard,
  createStyleCard,
  duplicateNodes,
  exportProject,
  generateFromRecipe,
  getProject,
  historyEntryLabel,
  patchGroup,
  patchRecipe,
  patchStyle,
  pasteNodes,
  redoOp,
  undoOp,
  updateProject,
} from "../ops.mjs";
import { buildNodesSpec } from "../tree.mjs";
import { createCanvasApi } from "../api.mjs";
import { resolveProjectFile } from "../store.mjs";
import { solidPng } from "./png_fixture.mjs";

// Metadata ops resolve store paths only, so any placeholder root works (no Python).
const ROOT = "C:/unused-repo-root";
// addText reads the REAL repo-relative fonts manifest (ops.mjs's readFontsManifest), so the
// two tests that create a text element (to exercise "ref must be an IMAGE") drive a handler
// bound to the REAL repo root — mirrors recipe.test.mjs's exportProject test. Project storage
// still redirects to the temp dir via CANVAS_PROJECTS_ROOT regardless of which root is passed
// (canvasProjectsRoot's env override wins over the root arg).
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-style-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

function run(env, ...args) {
  const stdout = execFileSync(process.execPath, [CLI, ...args], {
    env: { ...process.env, ...env },
    encoding: "utf8",
  });
  const line = stdout.trim().split("\n").filter(Boolean).at(-1);
  return JSON.parse(line);
}

function runFail(env, ...args) {
  try {
    execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    assert.fail(`expected "${args.join(" ")}" to fail`);
  } catch (error) {
    return error;
  }
}

// Minimal req/res doubles (mirrors tests/api.test.mjs's own invokeApi).
function invokeApi(handler, method, path, body) {
  const req = new EventEmitter();
  req.method = method;
  req.setEncoding = () => {};
  req.destroy = () => {};
  const chunks = [];
  const res = {
    statusCode: 0,
    headers: {},
    writeHead(status, headers) {
      this.statusCode = status;
      this.headers = headers || {};
    },
    write(chunk) {
      chunks.push(Buffer.from(chunk));
      return true;
    },
    end(chunk) {
      if (chunk !== undefined && chunk !== null && chunk !== "") chunks.push(Buffer.from(chunk));
      const buffer = Buffer.concat(chunks);
      this._resolve({
        status: this.statusCode,
        headers: this.headers,
        buffer,
        json() {
          return JSON.parse(buffer.toString("utf8"));
        },
      });
    },
  };
  const done = new Promise((r) => {
    res._resolve = r;
  });
  handler(req, res, new URL(path, "http://canvas.local"));
  queueMicrotask(() => {
    if (body !== undefined) req.emit("data", Buffer.from(typeof body === "string" ? body : JSON.stringify(body)));
    req.emit("end");
  });
  return done;
}

// A fake generator that returns fixed bytes (or throws, if given an Error), counting every
// call for exact argument/order assertions (mirrors recipe.test.mjs's own fakeGen).
function fakeGen(bytesOrError) {
  const calls = [];
  const fn = async (args) => {
    calls.push(args);
    if (bytesOrError instanceof Error) throw bytesOrError;
    return bytesOrError;
  };
  fn.calls = calls;
  return fn;
}

const DEFAULT_STYLE = { v: 1, prompt: "", ref: null };

// ================================================================================
// createStyleCard / patchStyle — the card object + blob, symmetric nesting guard.
// ================================================================================

test("createStyleCard mints a group with a default style blob; one entry; undo removes it", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Style" });
  const before = getProject(ROOT, project.id);

  const { group } = createStyleCard(ROOT, { projectId: project.id, name: "My style" });
  assert.equal(group.name, "My style");
  assert.deepEqual(group.style, DEFAULT_STYLE);
  assert.equal(group.visible, true);
  assert.equal(group.parentId, undefined, "no parentId given -> top level");

  const stored = getProject(ROOT, project.id);
  assert.equal(stored.groups.length, 1);
  assert.deepEqual(stored.groups[0].style, DEFAULT_STYLE);

  // One journal entry; a byte-exact undo removes the card and restores the project.
  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal((undone.groups || []).length, 0);
  assert.equal(JSON.stringify(undone.elements), JSON.stringify(before.elements));
  assert.equal(JSON.stringify(undone.groups || []), JSON.stringify(before.groups || []));

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.deepEqual(redone.groups[0].style, DEFAULT_STYLE);
});

test("createStyleCard: explicit bounds/parentId honored; default size falls back; unknown parent is loud", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Bounds" });

  // No x/y/w/h -> default frame at the origin, same 360x280 default as a recipe card.
  const { group: defaultCard } = createStyleCard(ROOT, { projectId: project.id });
  assert.deepEqual({ x: defaultCard.x, y: defaultCard.y, w: defaultCard.w, h: defaultCard.h }, { x: 0, y: 0, w: 360, h: 280 });

  const { group: placed } = createStyleCard(ROOT, { projectId: project.id, x: 50, y: 60, w: 400, h: 300 });
  assert.deepEqual({ x: placed.x, y: placed.y, w: placed.w, h: placed.h }, { x: 50, y: 60, w: 400, h: 300 });

  const outer = createGroup(ROOT, { projectId: project.id, name: "Outer", x: 0, y: 0, w: 1000, h: 1000 }).group;
  const { group: nested } = createStyleCard(ROOT, { projectId: project.id, parentId: outer.id });
  assert.equal(nested.parentId, outer.id);

  assert.throws(
    () => createStyleCard(ROOT, { projectId: project.id, parentId: "grp_missing" }),
    /group not found/,
  );
});

test("cards do not nest inside cards: createStyleCard refuses a recipe-card parent, createRecipeCard refuses a style-card parent (symmetric guard); a PLAIN parent is fine", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "No nesting" });
  const recipeCard = createRecipeCard(ROOT, { projectId: project.id, name: "A recipe" }).group;
  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "A style" }).group;

  assert.throws(
    () => createStyleCard(ROOT, { projectId: project.id, parentId: recipeCard.id }),
    /cannot create a style card inside a recipe card/,
  );
  assert.throws(
    () => createRecipeCard(ROOT, { projectId: project.id, parentId: styleCard.id }),
    /cannot create a recipe card inside a style card/,
  );

  // Only CROSS-card nesting is refused — a plain group parent is unaffected.
  const plain = createGroup(ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 10, h: 10 }).group;
  const nestedStyle = createStyleCard(ROOT, { projectId: project.id, parentId: plain.id }).group;
  assert.equal(nestedStyle.parentId, plain.id);
  const nestedRecipe = createRecipeCard(ROOT, { projectId: project.id, parentId: plain.id }).group;
  assert.equal(nestedRecipe.parentId, plain.id);
});

test("exportProject never exports a top-level style-card group either (mirrors the recipe-card case) — T0332 B1: createStyleCard never sets screen:true", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Export style" });
  const screen = createGroup(REPO_ROOT, { projectId: project.id, name: "Screen", x: 0, y: 0, w: 20, h: 20 }).group;
  patchGroup(REPO_ROOT, { projectId: project.id, groupId: screen.id, screen: true });
  const { group: card } = createStyleCard(REPO_ROOT, { projectId: project.id, name: "Style card" });
  assert.equal(card.screen, undefined, "createStyleCard never sets screen — unflagged by construction");

  let result;
  try {
    result = await exportProject(REPO_ROOT, { projectId: project.id });
  } catch (error) {
    t.skip(`render_group.py / python pipeline unavailable: ${error.message}`);
    return;
  }
  assert.equal(result.screens.length, 1, "only the plain group is exported as a screen");
  assert.equal(result.screens[0].name, "Screen");
});

test("patchStyle: prompt/ref roundtrip (Make ref override); validation is loud; a plain group is loud", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Patch style" });
  const { group: card } = createStyleCard(REPO_ROOT, { projectId: project.id });

  const afterPrompt = patchStyle(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "cel-shaded, warm palette" } }).group;
  assert.equal(afterPrompt.style.prompt, "cel-shaded, warm palette");
  assert.equal(afterPrompt.style.ref, null, "untouched fields survive a partial patch");

  assert.throws(
    () => patchStyle(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: 5 } }),
    /style prompt must be a string/,
  );
  assert.throws(
    () => patchStyle(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { ref: "el_missing" } }),
    /style ref must be null or an existing element id/,
  );

  // An existing element that never joined this card is loud (not a member).
  const outsider = addImage(REPO_ROOT, project.id, { name: "outsider.png", bytes: solidPng() }).element;
  assert.throws(
    () => patchStyle(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { ref: outsider.id } }),
    /style ref must be a member of this style card/,
  );

  // A member that is not an IMAGE (a text element) is loud (ref must be an image).
  const textMember = addText(REPO_ROOT, project.id, { content: "hi" }).element;
  assignToGroup(REPO_ROOT, { projectId: project.id, elementIds: [textMember.id], groupId: card.id });
  assert.throws(
    () => patchStyle(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { ref: textMember.id } }),
    /style ref must be an IMAGE element/,
  );

  // Two IMAGE members join one at a time; the FIRST auto-claims the ref (applyStyleAutoRef —
  // its own dedicated tests are below), the SECOND does not steal it.
  const first = addImage(REPO_ROOT, project.id, { name: "first.png", bytes: solidPng() }).element;
  assignToGroup(REPO_ROOT, { projectId: project.id, elementIds: [first.id], groupId: card.id });
  const second = addImage(REPO_ROOT, project.id, { name: "second.png", bytes: solidPng() }).element;
  assignToGroup(REPO_ROOT, { projectId: project.id, elementIds: [second.id], groupId: card.id });
  assert.equal(getProject(REPO_ROOT, project.id).groups.find((g) => g.id === card.id).style.ref, first.id);

  // "Make ref": patchStyle explicitly switches the ref to the second image — unlike
  // applyStyleAutoRef, patchStyle IS allowed to overwrite an already-set ref.
  const switched = patchStyle(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { ref: second.id } }).group;
  assert.equal(switched.style.ref, second.id);

  const cleared = patchStyle(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { ref: null } }).group;
  assert.equal(cleared.style.ref, null);

  assert.throws(() => patchStyle(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: {} }), /requires at least one of/);

  // A PLAIN group (no style) is a loud error — it is not a card.
  const plain = createGroup(REPO_ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 10, h: 10 }).group;
  assert.throws(
    () => patchStyle(REPO_ROOT, { projectId: project.id, groupId: plain.id, patch: { prompt: "x" } }),
    /not a style card/,
  );
  assert.throws(
    () => patchStyle(REPO_ROOT, { projectId: project.id, groupId: "grp_missing", patch: { prompt: "x" } }),
    /group not found/,
  );
});

test("patchStyle is one journal entry per call; undo restores the prior style blob byte-exact", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Undo style" });
  const { group: card } = createStyleCard(ROOT, { projectId: project.id });
  const afterCreate = getProject(ROOT, project.id);

  patchStyle(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a bold graphic style" } });
  const afterPatch = getProject(ROOT, project.id);
  assert.equal(afterPatch.groups[0].style.prompt, "a bold graphic style");

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(JSON.stringify(undone.groups), JSON.stringify(afterCreate.groups), "undo restores the style blob byte-exact");

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.groups[0].style.prompt, "a bold graphic style");
});

// ================================================================================
// applyStyleAutoRef — the FIRST image to join an empty style card claims the ref.
// ================================================================================

test("applyStyleAutoRef (assignToGroup): the FIRST image to join an empty style card claims the ref, in the SAME journal entry; later images never steal it", (t) => {
  tempProjects(t);
  // Uses REPO_ROOT (not the placeholder ROOT): the trailing TEXT-element case needs addText,
  // which reads the REAL repo-relative fonts manifest (see the REPO_ROOT const comment above).
  const project = createProject(REPO_ROOT, { title: "Auto-ref assign" });
  const card = createStyleCard(REPO_ROOT, { projectId: project.id }).group;
  const a = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: solidPng() }).element;
  const b = addImage(REPO_ROOT, project.id, { name: "b.png", bytes: solidPng() }).element;
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;

  // Two images assigned in ONE assignToGroup call -> the FIRST in the given id order claims
  // the ref, folded into this SAME commit (one journal entry, not two).
  assignToGroup(REPO_ROOT, { projectId: project.id, elementIds: [a.id, b.id], groupId: card.id });
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "one journal entry for the whole assign gesture");
  assert.equal(after.groups.find((g) => g.id === card.id).style.ref, a.id);

  // A later image joining does NOT overwrite the already-claimed ref.
  const c = addImage(REPO_ROOT, project.id, { name: "c.png", bytes: solidPng() }).element;
  assignToGroup(REPO_ROOT, { projectId: project.id, elementIds: [c.id], groupId: card.id });
  assert.equal(getProject(REPO_ROOT, project.id).groups.find((g) => g.id === card.id).style.ref, a.id, "ref stays on the FIRST image");

  // Moving an element OUT of a group (groupId: null) never touches a ref.
  assignToGroup(REPO_ROOT, { projectId: project.id, elementIds: [c.id], groupId: null });
  assert.equal(getProject(REPO_ROOT, project.id).groups.find((g) => g.id === card.id).style.ref, a.id);

  // A TEXT element joining never claims the ref (images only).
  const text = addText(REPO_ROOT, project.id, { content: "hi" }).element;
  const emptyCard = createStyleCard(REPO_ROOT, { projectId: project.id, name: "Text-only" }).group;
  assignToGroup(REPO_ROOT, { projectId: project.id, elementIds: [text.id], groupId: emptyCard.id });
  assert.equal(getProject(REPO_ROOT, project.id).groups.find((g) => g.id === emptyCard.id).style.ref, null);
});

test("applyStyleAutoRef (pasteNodes/duplicateNodes): pasting an image straight into an EXISTING style card claims its ref; a duplicate of the ref never steals it back", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Auto-ref paste" });
  const card = createStyleCard(ROOT, { projectId: project.id, name: "Target style" }).group;
  const image = addImage(ROOT, project.id, { name: "src.png", bytes: solidPng() }).element;

  // Copy a single (unrelated, top-level) image and paste it straight into the EXISTING style
  // card's scope -> the pasted copy claims the still-null ref.
  const spec = buildNodesSpec(getProject(ROOT, project.id), [image.id]);
  const pasted = pasteNodes(ROOT, { projectId: project.id, spec, dx: 0, dy: 0, scopeId: card.id });
  const pastedId = pasted.elementIds[0];
  assert.equal(getProject(ROOT, project.id).groups.find((g) => g.id === card.id).style.ref, pastedId);

  // duplicateNodes goes through the SAME pasteNodes path: duplicating the ref image right
  // back into the SAME card does NOT steal the already-claimed ref.
  const duped = duplicateNodes(ROOT, { projectId: project.id, nodeIds: [pastedId], scopeId: card.id });
  assert.equal(getProject(ROOT, project.id).groups.find((g) => g.id === card.id).style.ref, pastedId, "ref stays on the original pasted image");
  assert.notEqual(duped.elementIds[0], pastedId);
});

// ================================================================================
// Pointer remap (T0239-3 bug fix): a pasted card's style.ref / recipe.style_ref is an id
// POINTER, not placement — pasteNodes now remaps/nulls it instead of carrying a dangling
// original id (tree.mjs keeps ids in the spec specifically so this remap is possible).
// ================================================================================

test("pasteNodes pointer remap: copying a WHOLE style card (with its ref) — the COPY's ref points at its OWN pasted member, never the original's id", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Remap style ref" });
  const sourceCard = createStyleCard(ROOT, { projectId: project.id, name: "Source style" }).group;
  const sourceMember = addImage(ROOT, project.id, { name: "member.png", bytes: solidPng() }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [sourceMember.id], groupId: sourceCard.id }); // claims sourceCard's ref

  const sourceCardBefore = getProject(ROOT, project.id).groups.find((g) => g.id === sourceCard.id);
  assert.equal(sourceCardBefore.style.ref, sourceMember.id);

  // Copy the WHOLE card (group + its ref member) in ONE gesture.
  const cardSpec = buildNodesSpec(getProject(ROOT, project.id), [sourceCard.id]);
  const cardPasted = pasteNodes(ROOT, { projectId: project.id, spec: cardSpec, dx: 50, dy: 0 });
  const after = getProject(ROOT, project.id);
  const newCard = after.groups.find((g) => g.id === cardPasted.groupIds[0]);
  const newMember = after.elements.find((e) => e.groupId === newCard.id);

  assert.notEqual(newCard.id, sourceCard.id, "pasted card has a fresh id");
  assert.notEqual(newMember.id, sourceMember.id, "pasted member has a fresh id");
  assert.equal(newCard.style.ref, newMember.id, "the COPY's ref points at its OWN pasted member");
  assert.notEqual(newCard.style.ref, sourceMember.id, "never the original member's id");

  // The SOURCE card is completely untouched by the paste.
  assert.equal(
    getProject(ROOT, project.id).groups.find((g) => g.id === sourceCard.id).style.ref,
    sourceMember.id,
    "source card's own ref is unaffected",
  );

  // duplicateNodes (live nodes, same pasteNodes path) gets the same remap.
  const duped = duplicateNodes(ROOT, { projectId: project.id, nodeIds: [sourceCard.id] });
  const dupedCard = duped.project.groups.find((g) => g.id === duped.groupIds[0]);
  const dupedMember = duped.project.elements.find((e) => e.groupId === dupedCard.id);
  assert.equal(dupedCard.style.ref, dupedMember.id);
});

test("pasteNodes pointer remap: copying a style card WITHOUT its ref member — the copy's ref is nulled, then auto-claimed by an EXAMPLE member that WAS included", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Remap style ref partial" });
  const sourceCard = createStyleCard(ROOT, { projectId: project.id, name: "Source style" }).group;
  const refMember = addImage(ROOT, project.id, { name: "ref.png", bytes: solidPng() }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [refMember.id], groupId: sourceCard.id }); // claims the ref
  const exampleMember = addImage(ROOT, project.id, { name: "example.png", bytes: solidPng() }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [exampleMember.id], groupId: sourceCard.id }); // does not steal it

  // Hand-build a spec that carries the card + the EXAMPLE member only (the ref member is
  // deliberately left out — e.g. a selective copy of just the group and one child).
  const fullSpec = buildNodesSpec(getProject(ROOT, project.id), [sourceCard.id]);
  const partialSpec = {
    schema: fullSpec.schema,
    nodes: [
      {
        ...fullSpec.nodes[0],
        children: fullSpec.nodes[0].children.filter((child) => child.element.id === exampleMember.id),
      },
    ],
  };
  assert.equal(partialSpec.nodes[0].children.length, 1, "sanity: only the example member is in the spec");

  const pasted = pasteNodes(ROOT, { projectId: project.id, spec: partialSpec, dx: 60, dy: 0 });
  const after = getProject(ROOT, project.id);
  const newCard = after.groups.find((g) => g.id === pasted.groupIds[0]);
  const newExample = after.elements.find((e) => e.groupId === newCard.id);

  // The ref element was NOT part of this paste -> nulled, then immediately auto-claimed by
  // the pasted example member (applyStyleAutoRef runs right after the remap, same commit).
  assert.equal(newCard.style.ref, newExample.id, "the only pasted member auto-claims the nulled ref");
});

test("pasteNodes pointer remap: recipe card + its linked style card pasted TOGETHER — style_ref remapped to the NEW style card's id", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Remap recipe style_ref together" });
  const outer = createGroup(ROOT, { projectId: project.id, name: "Outer", x: 0, y: 0, w: 1000, h: 1000 }).group;
  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "Cel", parentId: outer.id }).group;
  const recipeCard = createRecipeCard(ROOT, { projectId: project.id, name: "Hero", parentId: outer.id }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: recipeCard.id, patch: { style_ref: styleCard.id } });

  // Copy BOTH cards together (a multi-root paste, same as a marquee-select-and-copy).
  const spec = buildNodesSpec(getProject(ROOT, project.id), [styleCard.id, recipeCard.id]);
  const pasted = pasteNodes(ROOT, { projectId: project.id, spec, dx: 40, dy: 0, scopeId: outer.id });
  const pastedGroups = pasted.groupIds.map((id) => pasted.project.groups.find((g) => g.id === id));
  const newStyleCard = pastedGroups.find((g) => g.style);
  const newRecipeCard = pastedGroups.find((g) => g.recipe);

  assert.notEqual(newStyleCard.id, styleCard.id);
  assert.notEqual(newRecipeCard.id, recipeCard.id);
  assert.equal(newRecipeCard.recipe.style_ref, newStyleCard.id, "remapped to the NEW pasted style card, not the original");

  // The originals are untouched.
  assert.equal(getProject(ROOT, project.id).groups.find((g) => g.id === recipeCard.id).recipe.style_ref, styleCard.id);
});

test("pasteNodes pointer remap: a recipe card pasted ALONE, same project — style_ref is KEPT VERBATIM (a legitimate shared-style-card alias)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Remap recipe style_ref alone" });
  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "Shared style" }).group;
  const recipeCard = createRecipeCard(ROOT, { projectId: project.id, name: "Hero" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: recipeCard.id, patch: { style_ref: styleCard.id } });

  // Copy ONLY the recipe card — the linked style card is NOT part of this paste, but it still
  // exists in THIS project, so the pointer is a legitimate "share the same style card" alias.
  const spec = buildNodesSpec(getProject(ROOT, project.id), [recipeCard.id]);
  const pasted = pasteNodes(ROOT, { projectId: project.id, spec, dx: 40, dy: 0 });
  const newRecipeCard = getProject(ROOT, project.id).groups.find((g) => g.id === pasted.groupIds[0]);
  assert.equal(newRecipeCard.recipe.style_ref, styleCard.id, "kept as-is — both recipe cards now share the SAME style card");
});

test("pasteNodes pointer remap: a recipe card whose style_ref matches nothing in the project (hand-authored / cross-project spec) — style_ref is null after paste, never a throw", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Remap recipe style_ref dangling" });
  const recipeCard = createRecipeCard(ROOT, { projectId: project.id, name: "Hero" }).group;
  const spec = buildNodesSpec(getProject(ROOT, project.id), [recipeCard.id]);
  // Simulate a spec captured in a DIFFERENT project (or hand-authored): style_ref names a
  // group id that resolves to nothing here at all.
  spec.nodes[0].group.recipe = { ...spec.nodes[0].group.recipe, style_ref: "grp_from_another_project" };

  const pasted = pasteNodes(ROOT, { projectId: project.id, spec, dx: 40, dy: 0 });
  const newRecipeCard = getProject(ROOT, project.id).groups.find((g) => g.id === pasted.groupIds[0]);
  assert.equal(newRecipeCard.recipe.style_ref, null, "a dangling cross-project pointer is nulled, not carried");
});

// ================================================================================
// generateFromRecipe — style mixing (R1 increment 3).
// ================================================================================

test("generateFromRecipe: style mixing — effective prompt appends '\\n\\nStyle: <prompt>', refPaths append the style ref LAST, style_snapshot frozen, refs_snapshot includes it, EXAMPLE members never travel", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Style mix" });

  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "Cel shade" }).group;
  patchStyle(ROOT, { projectId: project.id, groupId: styleCard.id, patch: { prompt: "cel-shaded, thick black outlines" } });
  const styleRefImg = addImage(ROOT, project.id, { name: "style-ref.png", bytes: solidPng(4, 3, [9, 9, 9]) }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [styleRefImg.id], groupId: styleCard.id }); // auto-claims the ref
  const exampleImg = addImage(ROOT, project.id, { name: "example.png", bytes: solidPng(4, 3, [8, 8, 8]) }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [exampleImg.id], groupId: styleCard.id }); // NOT the ref — an example

  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Hero" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a red fox", style_ref: styleCard.id } });
  const cardRef = addImage(ROOT, project.id, { name: "card-ref.png", bytes: solidPng(4, 3, [1, 1, 1]) }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [cardRef.id], groupId: card.id });

  const codex = fakeGen(solidPng());
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } });

  assert.equal(codex.calls.length, 1);
  assert.equal(codex.calls[0].prompt, "a red fox\n\nStyle: cel-shaded, thick black outlines", "exact effective-prompt form");
  assert.deepEqual(
    codex.calls[0].refPaths,
    [resolveProjectFile(ROOT, project.id, cardRef.src), resolveProjectFile(ROOT, project.id, styleRefImg.src)],
    "recipe card's own member refs THEN the style ref, last",
  );
  assert.ok(
    !codex.calls[0].refPaths.includes(resolveProjectFile(ROOT, project.id, exampleImg.src)),
    "the style card's non-ref EXAMPLE member never travels",
  );

  const el = result.elements[0];
  assert.equal(el.meta.recipe.prompt_snapshot, "a red fox\n\nStyle: cel-shaded, thick black outlines");
  assert.deepEqual(el.meta.recipe.refs_snapshot, [cardRef.src, styleRefImg.src]);
  assert.deepEqual(el.meta.recipe.style_snapshot, {
    cardId: styleCard.id,
    name: "Cel shade",
    prompt: "cel-shaded, thick black outlines",
  });
});

test("generateFromRecipe: style mixing — a prompt-only style card (ref=null) still appends its prompt and contributes no extra ref", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Style prompt only" });
  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "Moody" }).group;
  patchStyle(ROOT, { projectId: project.id, groupId: styleCard.id, patch: { prompt: "dark, moody lighting" } });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Knight" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a knight", style_ref: styleCard.id } });

  const codex = fakeGen(solidPng());
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } });

  assert.equal(codex.calls[0].prompt, "a knight\n\nStyle: dark, moody lighting");
  assert.deepEqual(codex.calls[0].refPaths, [], "no card refs, no style ref -> empty");
  assert.deepEqual(result.elements[0].meta.recipe.style_snapshot, {
    cardId: styleCard.id,
    name: "Moody",
    prompt: "dark, moody lighting",
  });
});

test("generateFromRecipe: style mixing — an empty/whitespace style prompt skips the '\\n\\nStyle:' suffix entirely", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Style empty prompt" });
  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "Blank" }).group; // style.prompt stays ""
  const card = createRecipeCard(ROOT, { projectId: project.id }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a dragon", style_ref: styleCard.id } });

  const codex = fakeGen(solidPng());
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } });

  assert.equal(codex.calls[0].prompt, "a dragon", "no Style suffix when the style prompt is empty");
  assert.deepEqual(result.elements[0].meta.recipe.style_snapshot, { cardId: styleCard.id, name: "Blank", prompt: "" });
});

test("generateFromRecipe: style mixing — a style_ref that does not resolve to a style-card group is loud (defensive; patchRecipe itself refuses this on write)", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Style dangling" });
  const card = createRecipeCard(ROOT, { projectId: project.id }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a griffin" } });
  // Hand-edit style_ref PAST patchRecipe's own write-time guard — the only way to reach
  // generateFromRecipe's OWN defensive re-check is a hand-edited project.json.
  const before = getProject(ROOT, project.id);
  updateProject(ROOT, project.id, {
    groups: before.groups.map((g) => (g.id === card.id ? { ...g, recipe: { ...g.recipe, style_ref: "grp_missing" } } : g)),
  });

  const codex = fakeGen(solidPng());
  await assert.rejects(
    () => generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } }),
    /style_ref that is not a style-card group/,
  );
  assert.equal(codex.calls.length, 0);
  assert.equal(getProject(ROOT, project.id).elements.length, 0);
});

test("generateFromRecipe: style mixing — a style card whose ref points at a non-member element is loud, nothing generated", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Style stale ref (moved out)" });
  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "Stale" }).group;
  const styleRefImg = addImage(ROOT, project.id, { name: "ref.png", bytes: solidPng() }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [styleRefImg.id], groupId: styleCard.id }); // auto-claims ref
  // Move the ref image back OUT of the card — style.ref still names it, but it is no longer a
  // member (patchStyle/assignToGroup never auto-clear a stale ref; this loud check is what
  // catches it at generate time).
  assignToGroup(ROOT, { projectId: project.id, elementIds: [styleRefImg.id], groupId: null });

  const card = createRecipeCard(ROOT, { projectId: project.id }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a phoenix", style_ref: styleCard.id } });

  const codex = fakeGen(solidPng());
  await assert.rejects(
    () => generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } }),
    /ref points at a missing\/non-member element/,
  );
  assert.equal(codex.calls.length, 0);
});

test("historyEntryLabel maps generateFromRecipe's style-mixed run the same as any run (no extra label case needed)", () => {
  // No dedicated label branch for style mixing — generateFromRecipe's existing label
  // (historyEntryLabel("generateFromRecipe", ...)) covers it unchanged; this just pins that
  // adding style mixing did not require (or accidentally shadow) a new switch case.
  assert.deepEqual(historyEntryLabel("generateFromRecipe", { engine: "codex" }), { label: "Generate from recipe", summary: "codex" });
});

// ================================================================================
// patchRecipe's tightened style_ref validation (T0239 increment 3).
// ================================================================================

test("patchRecipe: style_ref must be null or an existing style-card group id (T0239 increment 3 tightening)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Recipe style_ref validation" });
  const card = createRecipeCard(ROOT, { projectId: project.id }).group;
  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "Valid style" }).group;
  const plain = createGroup(ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 10, h: 10 }).group;

  const linked = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: styleCard.id } }).group;
  assert.equal(linked.recipe.style_ref, styleCard.id);

  const cleared = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: null } }).group;
  assert.equal(cleared.recipe.style_ref, null);

  // An unknown id, a PLAIN group, and even ANOTHER RECIPE card are all refused loudly — only
  // a group actually carrying `style` qualifies.
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: "grp_missing" } }),
    /must be null or the id of an existing style-card group/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: plain.id } }),
    /must be null or the id of an existing style-card group/,
  );
  const otherRecipeCard = createRecipeCard(ROOT, { projectId: project.id }).group;
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: otherRecipeCard.id } }),
    /must be null or the id of an existing style-card group/,
  );
});

// ================================================================================
// buildNodesSpec copy/paste pin — tree.mjs itself is untouched (design instruction).
// ================================================================================

test("buildNodesSpec/pasteNodes/duplicateNodes carry the style blob through with a fresh group id (copy/paste pin — tree.mjs untouched)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Style copy" });
  const { group: card } = createStyleCard(ROOT, { projectId: project.id, name: "Hero style", x: 10, y: 20 });
  patchStyle(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "watercolor, soft edges" } });
  const seeded = getProject(ROOT, project.id).groups.find((g) => g.id === card.id);

  // buildNodesSpec (tree.mjs) is a deep clone of the group record minus parentId/order (id is
  // KEPT — see tree.test.mjs's own pin) — `style` survives with no schema-aware carve-out
  // needed, same as `recipe`. This card's ref stays null throughout, so pasteNodes' pointer
  // remap (style.test.mjs's dedicated tests below) never perturbs it — a plain verbatim carry.
  const spec = buildNodesSpec(getProject(ROOT, project.id), [card.id]);
  assert.equal(spec.nodes.length, 1);
  assert.equal(spec.nodes[0].group.id, card.id, "the spec keeps the ORIGINAL id (pasteNodes still mints a fresh one)");
  assert.deepEqual(spec.nodes[0].group.style, seeded.style);

  const pasted = pasteNodes(ROOT, { projectId: project.id, spec, dx: 100, dy: 0 });
  const pastedGroup = pasted.project.groups.find((g) => g.id === pasted.groupIds[0]);
  assert.notEqual(pastedGroup.id, card.id, "paste mints a fresh id");
  assert.deepEqual(pastedGroup.style, seeded.style, "style blob carried verbatim");

  const duped = duplicateNodes(ROOT, { projectId: project.id, nodeIds: [card.id] });
  const dupedGroup = duped.project.groups.find((g) => g.id === duped.groupIds[0]);
  assert.notEqual(dupedGroup.id, card.id);
  assert.deepEqual(dupedGroup.style, seeded.style);
});

// ================================================================================
// historyEntryLabel
// ================================================================================

test("historyEntryLabel maps createStyleCard/patchStyle to readable labels", () => {
  assert.deepEqual(historyEntryLabel("createStyleCard", { name: "Cel shade" }), { label: "Style card", summary: "Cel shade" });
  assert.deepEqual(historyEntryLabel("patchStyle", {}), { label: "Edit style", summary: "" });
});

// ================================================================================
// CLI / API parity.
// ================================================================================

test("CLI style-create / style-set parity with the ops layer", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-style-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const projectId = run(env, "create", "--title", "CLI Style").project.id;
  const created = run(env, "style-create", projectId, "--name", "CLI style", "--x", "5", "--y", "6", "--w", "300", "--h", "200");
  assert.equal(created.group.name, "CLI style");
  assert.deepEqual(created.group.style, DEFAULT_STYLE);
  assert.deepEqual({ x: created.group.x, y: created.group.y, w: created.group.w, h: created.group.h }, { x: 5, y: 6, w: 300, h: 200 });

  const groupId = created.group.id;
  const patched = run(env, "style-set", projectId, "--group", groupId, "--prompt", "cel-shaded");
  assert.equal(patched.group.style.prompt, "cel-shaded");
  assert.equal(patched.group.style.ref, null);

  const failure = runFail(env, "style-set", projectId, "--group", groupId, "--ref", "el_missing");
  assert.match(String(failure.stderr || failure.message), /style ref must be null or an existing element id/);

  const clearedRef = run(env, "style-set", projectId, "--group", groupId, "--ref", "none");
  assert.equal(clearedRef.group.style.ref, null);

  const shown = run(env, "show", projectId).project;
  assert.equal(shown.groups.length, 1);
  assert.equal(shown.groups[0].style.prompt, "cel-shaded");
});

test("API POST style-cards / PATCH style-cards/<gid> parity with the ops layer", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);

  const createdProject = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "API Style" });
  const projectId = createdProject.json().project.id;

  const created = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/style-cards`, {
    name: "API style",
    x: 1,
    y: 2,
  });
  assert.equal(created.status, 201);
  const group = created.json().group;
  assert.equal(group.name, "API style");
  assert.deepEqual(group.style, DEFAULT_STYLE);

  const patched = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/style-cards/${group.id}`, {
    prompt: "a moody palette",
  });
  assert.equal(patched.status, 200);
  assert.equal(patched.json().group.style.prompt, "a moody palette");
  // sendMutation folds duration_ms + history flags onto every mutating response.
  assert.ok(Number.isFinite(patched.json().duration_ms));
  assert.ok(patched.json().history);

  // A plain group (no style) 400s through the API too — the op's loud error surfaces
  // as a 400, not a silent 200.
  const plainGroup = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
    name: "Plain",
    x: 0,
    y: 0,
    w: 10,
    h: 10,
  });
  const plainId = plainGroup.json().group.id;
  const rejected = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/style-cards/${plainId}`, { prompt: "x" });
  assert.equal(rejected.status, 400);
  assert.match(rejected.json().error, /not a style card/);
});
