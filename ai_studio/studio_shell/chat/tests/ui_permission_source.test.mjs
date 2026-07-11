import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import {
  activeTurnRequestTarget,
  activeTurnsForProject,
  permissionDecisionTarget,
  renderPermission,
} from "../../../assets/canvas/site/chat_panel.js";

const panel = readFileSync(resolve(import.meta.dirname, "../../../assets/canvas/site/chat_panel.js"), "utf8");
const css = readFileSync(resolve(import.meta.dirname, "../../../assets/canvas/site/canvas.css"), "utf8");

test("chat UI bootstraps a launch token and sends it on protected requests", () => {
  assert.match(panel, /\/api\/chat\/bootstrap/);
  assert.match(panel, /x-ai-studio-chat-token/);
});

test("chat UI renders opaque permission JSON as text and exposes native decisions", () => {
  assert.match(panel, /permission-request/);
  assert.match(panel, /JSON\.stringify\([^)]*exactRequest/);
  assert.match(panel, /textContent/);
  assert.match(panel, /permission-decision/);
  assert.match(css, /chat-permission/);
});

test("permission DOM renders hostile opaque JSON as text and wires native allow/deny buttons", () => {
  class FakeElement {
    constructor(tagName) {
      this.tagName = tagName;
      this.children = [];
      this.listeners = {};
      this.attributes = {};
      this.textContent = "";
    }
    appendChild(child) { this.children.push(child); return child; }
    append(...children) { this.children.push(...children); }
    addEventListener(name, listener) { this.listeners[name] = listener; }
    setAttribute(name, value) { this.attributes[name] = value; }
  }
  const previousDocument = globalThis.document;
  globalThis.document = { createElement: (tagName) => new FakeElement(tagName) };
  try {
    const permission = {
      id: "p/1",
      projectId: "project-a",
      storeId: "game:private-a",
      state: "pending",
      exactRequest: { command: "<img src=x onerror=sentinel()>" },
    };
    const decisions = [];
    const node = renderPermission(permission, (entry, decision) => decisions.push([entry.id, decision]));
    assert.equal(node.tagName, "section");
    assert.equal(node.children[1].tagName, "pre");
    assert.equal(node.children[1].textContent, JSON.stringify(permission.exactRequest, null, 2));
    assert.equal("innerHTML" in node.children[1], false);
    const [approve, deny] = node.children[2].children;
    assert.equal(approve.tagName, "button");
    assert.equal(deny.tagName, "button");
    approve.listeners.click();
    deny.listeners.click();
    assert.deepEqual(decisions, [["p/1", "allow"], ["p/1", "deny"]]);
  } finally {
    globalThis.document = previousDocument;
  }
});

test("permission decision target stays bound to its originating project and store", () => {
  const target = permissionDecisionTarget({
    id: "permission-id",
    projectId: "origin-project",
    storeId: "game:origin",
  }, "allow");
  assert.deepEqual(target, {
    path: "/projects/origin-project/permissions/permission-id/decision",
    body: { decision: "allow" },
    storeId: "game:origin",
  });
});

test("an in-flight permission display is recovered only for its originating project", () => {
  const originTurns = [{ role: "assistant", permissions: [{ state: "pending" }] }];
  const active = { key: "game:origin:project-a", turns: originTurns };
  assert.equal(activeTurnsForProject(active, "game:other:project-a"), null);
  assert.equal(activeTurnsForProject(active, "game:origin:project-a"), originTurns);
});

test("cancel remains bound to the active turn after the visible project switches", () => {
  const active = { projectId: "origin-project", storeId: "game:origin" };
  assert.deepEqual(activeTurnRequestTarget(active, "visible-project", "game:visible"), {
    projectId: "origin-project",
    storeId: "game:origin",
  });
});
