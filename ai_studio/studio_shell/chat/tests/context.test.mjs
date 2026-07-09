// Chat context digest + transcript/state store tests (T0242 increment 1). Pure/read-only
// over ops.mjs (digest) + node:fs (chat/ store) — no codex, no spawn.
// Run: node --test ai_studio/studio_shell/chat/tests/context.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, readdirSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { addImage, addText, createGroup, createProject, patchElement } from "../../../assets/canvas/ops.mjs";
import { solidPng } from "../../../assets/canvas/tests/png_fixture.mjs";
import {
  appendTurn,
  buildChatContext,
  clearConversation,
  formatSelectionRef,
  readChatState,
  readTranscript,
  writeChatState,
} from "../context.mjs";

// addText reads the bundled fonts.json off the repo root (text.test.mjs's own note), so
// this suite uses the REAL repo root as the `root` ops argument and only redirects the
// PROJECTS dir via CANVAS_PROJECTS_ROOT (same split as groups.test.mjs/text.test.mjs: the
// outer ROOT constant is passed to every ops/context call; tempProjects()'s own return
// value is only used for raw on-disk assertions).
const ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "chat-context-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// One project: an image element, a text element inside a group, and the group itself.
function seedProject() {
  const project = createProject(ROOT, { title: "Fixture Screen" });
  const group = createGroup(ROOT, { projectId: project.id, name: "Screen A", x: 0, y: 0, w: 200, h: 200 }).group;
  const image = addImage(ROOT, project.id, { name: "hero.png", bytes: solidPng(10, 10) }).element;
  const textResult = addText(ROOT, project.id, { content: "Title", groupId: group.id });
  return { projectId: project.id, groupId: group.id, elementId: image.id, textId: textResult.element.id };
}

// ---- buildChatContext / formatSelectionRef -----------------------------------------------

test("buildChatContext: digest shape, counts, head over an empty selection", (t) => {
  tempProjects(t);
  const { projectId } = seedProject();
  const digest = buildChatContext(ROOT, { projectId, selection: [] });
  assert.equal(digest.projectId, projectId);
  assert.equal(digest.title, "Fixture Screen");
  assert.deepEqual(digest.selection, []);
  assert.equal(digest.counts.elements, 2);
  assert.equal(digest.counts.groups, 1);
  assert.equal(typeof digest.head, "number");
  assert.ok(digest.head >= 0);
});

test("buildChatContext: element selection resolves ref/name/type/w/h/groupId matching context_menu.js's format", (t) => {
  tempProjects(t);
  const { projectId, elementId } = seedProject();
  patchElement(ROOT, projectId, elementId, { name: "Hero art" });
  const digest = buildChatContext(ROOT, { projectId, selection: [{ kind: "element", id: elementId }] });
  assert.equal(digest.selection.length, 1);
  const entry = digest.selection[0];
  assert.equal(entry.kind, "element");
  assert.equal(entry.id, elementId);
  assert.equal(entry.type, "image");
  assert.equal(entry.name, "Hero art");
  assert.equal(entry.w, 10);
  assert.equal(entry.h, 10);
  assert.equal(entry.groupId, null); // the image was added at root, not into the group
  assert.equal(entry.ref, `canvas://${projectId}/element/${elementId} — project "Fixture Screen", element "Hero art"`);
});

test("buildChatContext: private store refs use canvas://game and omit private names", (t) => {
  tempProjects(t);
  const { projectId, elementId } = seedProject();
  patchElement(ROOT, projectId, elementId, { name: "Secret Hero" });
  const digest = buildChatContext(ROOT, {
    projectId,
    selection: [{ kind: "element", id: elementId }],
    store: {
      storeId: "game:secret-game",
      visibility: "private",
      kind: "game",
      gameId: "secret-game",
    },
  });

  assert.equal(digest.storeId, "game:secret-game");
  assert.equal(digest.visibility, "private");
  assert.equal(digest.qualifiedId, `game:secret-game:${projectId}`);
  assert.equal(digest.selection[0].ref, `canvas://game/secret-game/${projectId}/element/${elementId}`);
  assert.doesNotMatch(digest.selection[0].ref, /Fixture Screen|Secret Hero/);
});

test("buildChatContext: group selection resolves the group ref format", (t) => {
  tempProjects(t);
  const { projectId, groupId } = seedProject();
  const digest = buildChatContext(ROOT, { projectId, selection: [{ kind: "group", id: groupId }] });
  const entry = digest.selection[0];
  assert.equal(entry.kind, "group");
  assert.equal(entry.name, "Screen A");
  assert.equal(entry.ref, `canvas://${projectId}/group/${groupId} — project "Fixture Screen", group "Screen A"`);
});

test("buildChatContext: text element inside a group carries its groupId", (t) => {
  tempProjects(t);
  const { projectId, textId, groupId } = seedProject();
  const digest = buildChatContext(ROOT, { projectId, selection: [{ kind: "element", id: textId }] });
  const entry = digest.selection[0];
  assert.equal(entry.type, "text");
  assert.equal(entry.groupId, groupId);
});

test("buildChatContext: multiple selection entries preserve input order", (t) => {
  tempProjects(t);
  const { projectId, elementId, groupId } = seedProject();
  const digest = buildChatContext(ROOT, {
    projectId,
    selection: [
      { kind: "group", id: groupId },
      { kind: "element", id: elementId },
    ],
  });
  assert.deepEqual(
    digest.selection.map((entry) => entry.kind),
    ["group", "element"],
  );
});

test("buildChatContext: unresolvable selection id throws loudly (no silent drop)", (t) => {
  tempProjects(t);
  const { projectId } = seedProject();
  assert.throws(
    () => buildChatContext(ROOT, { projectId, selection: [{ kind: "element", id: "el_missing" }] }),
    /selection element not found: el_missing/,
  );
});

test("buildChatContext requires projectId", (t) => {
  tempProjects(t);
  assert.throws(() => buildChatContext(ROOT, {}), /requires projectId/);
});

test("formatSelectionRef: unknown kind throws", () => {
  assert.throws(
    () => formatSelectionRef({ id: "p1", title: "x", elements: [], groups: [] }, { kind: "bogus" }),
    /unknown selection kind/,
  );
});

// ---- transcript.jsonl / state.json store --------------------------------------------------

test("appendTurn + readTranscript: append-only roundtrip, oldest first", (t) => {
  tempProjects(t);
  const { projectId } = seedProject();
  assert.deepEqual(readTranscript(ROOT, { projectId }), []); // nothing yet — never 404s, just empty
  appendTurn(ROOT, { projectId, role: "user", text: "сделай чёрную версию" });
  appendTurn(ROOT, { projectId, role: "assistant", text: "done", seqRange: [3, 5] });
  const rows = readTranscript(ROOT, { projectId });
  assert.equal(rows.length, 2);
  assert.equal(rows[0].role, "user");
  assert.equal(rows[0].text, "сделай чёрную версию");
  assert.equal(rows[0].seqRange, undefined);
  assert.equal(rows[1].role, "assistant");
  assert.deepEqual(rows[1].seqRange, [3, 5]);
  assert.ok(rows[0].at && rows[1].at);
});

test("appendTurn requires a known role and string text", (t) => {
  tempProjects(t);
  const { projectId } = seedProject();
  assert.throws(() => appendTurn(ROOT, { projectId, role: "system", text: "x" }), /requires role/);
  assert.throws(() => appendTurn(ROOT, { projectId, role: "user", text: 5 }), /requires text/);
});

test("appendTurn on an unknown project throws loudly and creates no chat/ dir", (t) => {
  tempProjects(t);
  assert.throws(() => appendTurn(ROOT, { projectId: "nope", role: "user", text: "hi" }), /canvas project not found/);
});

test("readChatState defaults to sessionId null before any write; writeChatState roundtrips", (t) => {
  tempProjects(t);
  const { projectId } = seedProject();
  assert.deepEqual(readChatState(ROOT, { projectId }), { sessionId: null });
  writeChatState(ROOT, { projectId, sessionId: "sess-abc" });
  assert.deepEqual(readChatState(ROOT, { projectId }), { sessionId: "sess-abc" });
});

test("clearConversation archives transcript.jsonl (rename, kept on disk) and resets session_id to null", (t) => {
  const dir = tempProjects(t);
  const { projectId } = seedProject();
  appendTurn(ROOT, { projectId, role: "user", text: "turn one" });
  writeChatState(ROOT, { projectId, sessionId: "sess-1" });

  const result = clearConversation(ROOT, { projectId });
  assert.equal(result.sessionId, null);
  assert.ok(result.archivedAs, "archivedAs path is returned");
  assert.ok(readFileSync(result.archivedAs, "utf8").includes("turn one"), "the archived file keeps the old content");

  assert.deepEqual(readTranscript(ROOT, { projectId }), [], "the live transcript is empty after clear");
  assert.deepEqual(readChatState(ROOT, { projectId }), { sessionId: null });

  const chatFiles = readdirSync(join(dir, projectId, "chat"));
  assert.ok(chatFiles.some((name) => /^transcript-.*\.jsonl$/.test(name)), "archive file is never deleted");
});

test("clearConversation on a project with no prior transcript is a harmless no-op archive", (t) => {
  tempProjects(t);
  const { projectId } = seedProject();
  const result = clearConversation(ROOT, { projectId });
  assert.equal(result.archivedAs, null);
  assert.deepEqual(readChatState(ROOT, { projectId }), { sessionId: null });
});
