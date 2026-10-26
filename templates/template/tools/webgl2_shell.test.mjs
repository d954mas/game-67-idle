import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { test } from "node:test";
import { runInNewContext } from "node:vm";

const html = readFileSync(new URL("../web/index.html.in", import.meta.url), "utf8");
const shell = [...html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/g)]
  .map((match) => match[1]).find((source) => source.includes("function showRuntimeFatal"));

test("safe area reaches UI Kit in CSS pixels and follows live browser changes", () => {
  const bridge = html.match(/<script id="safe-area-bridge">([\s\S]*?)<\/script>/)?.[1];
  assert.ok(bridge, "web shell must supply the UI Kit safe-area bridge");
  assert.match(html, /viewport-fit=cover/);
  for (const edge of ["left", "right", "top", "bottom"]) {
    assert.ok(html.includes(`env(safe-area-inset-${edge}, 0px)`));
  }
  let style = { paddingLeft: "3px", paddingRight: "7px", paddingTop: "47px", paddingBottom: "21px" };
  const probe = {};
  const window = {};
  runInNewContext(bridge, {
    window,
    document: { getElementById(id) { assert.equal(id, "safe-area-probe"); return probe; } },
    getComputedStyle(element) { assert.equal(element, probe); return style; },
  });
  const read = () => Array.from(window.__gameSafeAreaInsets());
  assert.deepEqual(read(), [3, 7, 47, 21]);
  style = { paddingLeft: "47px", paddingRight: "12px", paddingTop: "0px", paddingBottom: "21px" };
  assert.deepEqual(read(), [47, 12, 0, 21]);
  style = { paddingLeft: "", paddingRight: "-5px", paddingTop: "Infinity", paddingBottom: "0px" };
  assert.deepEqual(read(), [0, 0, 0, 0]);
});

test("phone safe-area minimum follows orientation without adding to reported insets", () => {
  const bridge = html.match(/<script id="safe-area-bridge">([\s\S]*?)<\/script>/)?.[1];
  let style = { paddingLeft: '0px', paddingRight: '0px', paddingTop: '0px', paddingBottom: '0px' };
  const window = { innerWidth:390, innerHeight:844, screen:{width:390,height:844,orientation:{type:'portrait-primary'}},
    matchMedia: () => ({matches:true}) };
  runInNewContext(bridge, {window, document:{getElementById:()=>({})}, getComputedStyle:()=>style});
  const read = () => Array.from(window.__gameSafeAreaInsets());
  assert.deepEqual(read(), [0,0,32,0]);
  style.paddingTop = '48px';
  assert.deepEqual(read(), [0,0,48,0]);
  window.innerHeight = 250;
  assert.deepEqual(read(), [0,0,48,0], 'keyboard does not rotate the cutout');
  style = {paddingLeft:'48px',paddingRight:'0px',paddingTop:'0px',paddingBottom:'21px'};
  window.screen.orientation.type = 'landscape-primary';
  assert.deepEqual(read(), [48,32,0,21]);
  window.matchMedia = () => ({matches:false});
  assert.deepEqual(read(), [48,0,0,21], 'desktop receives only browser insets');
  window.matchMedia = () => ({matches:true});
  window.screen.width = 1024;
  window.screen.height = 1366;
  assert.deepEqual(read(), [48,0,0,21], 'large touch screens do not get a phone minimum');
});

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
