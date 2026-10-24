import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { test } from "node:test";
import { runInNewContext } from "node:vm";

const html = readFileSync(new URL("../web/index.html.in", import.meta.url), "utf8");
const shell = [...html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/g)]
  .map((match) => match[1]).find((source) => source.includes("function showRuntimeFatal"));

test("WebGL2 creation failure keeps its explanation after the runtime aborts", () => {
  assert.ok(shell);
  const listeners = new Map();
  const elements = new Map();
  function element(id) {
    if (!elements.has(id)) elements.set(id, {
      hidden: true,
      textContent: "",
      style: {},
      classList: { add() {} },
      setAttribute() {},
      removeAttribute() {},
      addEventListener(type, callback) { listeners.set(`${id}:${type}`, callback); },
      focus() {},
    });
    return elements.get(id);
  }
  const window = {
    requestAnimationFrame() {},
    addEventListener() {},
    location: { reload() {} },
  };
  runInNewContext(shell.replace("@GAME_PLATFORM_SDK_RELEASE_JS@", "false"), {
    window,
    document: { getElementById: element },
    setTimeout() {},
    Object,
    Number,
  });

  const onCreationError = listeners.get("canvas:webglcontextcreationerror");
  assert.equal(typeof onCreationError, "function");
  onCreationError({ statusMessage: "disabled by enterprise policy or commandline switch" });
  onCreationError({ statusMessage: "Web page caused context loss and was blocked" });
  window.__showRuntimeFatal();
  assert.equal(elements.get("runtime-overlay").hidden, false);
  assert.match(elements.get("runtime-title").textContent, /WebGL 2/);
  assert.match(elements.get("runtime-message").textContent, /disabled by enterprise policy or commandline switch/);
  assert.match(elements.get("runtime-message").textContent, /Web page caused context loss and was blocked/);
  assert.doesNotMatch(elements.get("runtime-message").textContent, /Something went wrong/);
  assert.doesNotMatch(elements.get("runtime-message").textContent, /Close other game tabs/);
});
