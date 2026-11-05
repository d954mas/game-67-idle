import assert from "node:assert/strict";
import test from "node:test";
import { runInNewContext } from "node:vm";

import { assembleInventory, checkPlaces, GL_INVENTORY_HOOK, matchShader, MEMORY_CENSUS_HOOK, noteStreams, parseSymbols, readMemoryCensus } from "../web_memory_census.mjs";

const GL = {
  TEXTURE0: 0x84c0, TEXTURE_2D: 0x0de1, TEXTURE_3D: 0x806f, TEXTURE_CUBE_MAP: 0x8513,
  ARRAY_BUFFER: 0x8892, ELEMENT_ARRAY_BUFFER: 0x8893, UNIFORM_BUFFER: 0x8a11,
};

function page({ growable = false } = {}) {
  class FakeGl {
    constructor() { Object.assign(this, GL, { drawingBufferWidth: 100, drawingBufferHeight: 50 }); }
    getContextAttributes() { return { depth: true, stencil: false, antialias: false }; }
  }
  for (const name of ["activeTexture", "bindTexture", "deleteTexture", "texImage2D", "texImage3D", "compressedTexImage2D",
    "texStorage2D", "texStorage3D", "generateMipmap", "bindBuffer", "bindVertexArray", "deleteBuffer", "bufferData",
    "bindRenderbuffer", "deleteRenderbuffer", "renderbufferStorage", "renderbufferStorageMultisample", "createProgram",
    "deleteProgram", "createShader", "deleteShader", "createFramebuffer", "deleteFramebuffer"]) {
    FakeGl.prototype[name] = function () { return {}; };
  }
  class HTMLCanvasElement { getContext() { return new FakeGl(); } }
  class Memory { constructor(bytes) { this.buffer = { byteLength: bytes }; } }
  if (growable) Memory.prototype.grow = function (pages) { this.buffer = { byteLength: this.buffer.byteLength + pages * 65536 }; return 0; };
  const window = { HTMLCanvasElement, WebAssembly: { Memory, instantiate: async () => ({ instance: { exports: { memory: new Memory(1 << 20) } } }) },
    performance: {} };
  window.window = window;
  runInNewContext(MEMORY_CENSUS_HOOK, window);
  return window;
}

test("ledgers live GL resources by kind and counts heap-view uploads by their length", async () => {
  const window = page();
  await window.WebAssembly.instantiate(new Uint8Array(0), { env: {} });
  const gl = new window.HTMLCanvasElement().getContext("webgl2");
  const tex = {}, gone = {}, vbo = {}, ibo = {}, vao = {}, rb = {};
  gl.bindTexture(GL.TEXTURE_2D, tex);
  gl.texStorage2D(GL.TEXTURE_2D, 2, 0x8058, 64, 32); // RGBA8: 8192 + 2048
  gl.bindTexture(GL.TEXTURE_2D, gone);
  gl.texImage2D(GL.TEXTURE_2D, 0, 0x8058, 256, 256, 0, 0x1908, 0x1401, null);
  gl.deleteTexture(gone);
  const heap = new Uint8Array(1 << 20);
  gl.bindBuffer(GL.ARRAY_BUFFER, vbo);
  gl.bufferData(GL.ARRAY_BUFFER, heap, 0x88e4, 4096, 300);
  gl.bindVertexArray(vao);
  gl.bindBuffer(GL.ELEMENT_ARRAY_BUFFER, ibo);
  gl.bufferData(GL.ELEMENT_ARRAY_BUFFER, 120, 0x88e4);
  gl.bindRenderbuffer(0x8d41, rb);
  gl.renderbufferStorageMultisample(0x8d41, 4, 0x81a6, 10, 10);
  gl.createProgram(); gl.createProgram(); gl.deleteProgram({});

  const census = await readMemoryCensus(async (expression) => runInNewContext(expression, window));
  assert.equal(census.kinds.texture.count, 1);
  assert.equal(census.kinds.texture.bytes, 8192 + 2048);
  assert.equal(census.kinds.buffer_vertex.bytes, 300);
  assert.equal(census.kinds.buffer_index.bytes, 120);
  assert.equal(census.kinds.renderbuffer.bytes, 1600);
  assert.equal(census.programs, 1);
  assert.equal(census.wasm_bytes, 1 << 20);
  assert.equal(census.default_framebuffer.bytes, 100 * 50 * 12);
  assert.equal(census.gpu_bytes, 8192 + 2048 + 300 + 120 + 1600 + 100 * 50 * 12);
  assert.ok(census.gpu_peak_bytes >= 8192 + 2048 + 262144 + 100 * 50 * 12, "the deleted texture still counts toward the peak");
});

test("refuses a page that loaded without the hook", async () => {
  await assert.rejects(readMemoryCensus(async () => null), /did not install/);
});

test("counts each WebAssembly memory grow with its old and new size", async () => {
  const window = page({ growable: true });
  const { instance } = await window.WebAssembly.instantiate(new Uint8Array(0), { env: {} });
  instance.exports.memory.grow(16);
  instance.exports.memory.grow(16);
  const census = await readMemoryCensus(async (expression) => runInNewContext(expression, window));
  assert.equal(census.wasm_initial_bytes, 1 << 20);
  assert.equal(census.wasm_grows, 2);
  assert.deepEqual(JSON.parse(JSON.stringify(census.wasm_grow_events.map((g) => [g.from_bytes, g.to_bytes]))), [[1 << 20, (1 << 20) + (1 << 20)], [2 << 20, 3 << 20]]);
});

test("a memory first seen by its own grow keeps the size before it as the baseline", async () => {
  const window = page({ growable: true });
  const memory = new window.WebAssembly.Memory(1 << 20);
  memory.grow(16);
  const census = await readMemoryCensus(async (expression) => runInNewContext(expression, window));
  assert.equal(census.wasm_initial_bytes, 1 << 20);
  assert.equal(census.wasm_grows, 1);
});

function inventoryPage() {
  class WebGL2RenderingContext {
    constructor() { Object.assign(this, GL, { drawingBufferWidth: 100, drawingBufferHeight: 50 }); }
    getContextAttributes() { return { depth: true, stencil: false, antialias: false }; }
    getProgramParameter(program, name) { return name === 0x8b86 ? 1 : name === 0x8a36 ? 0 : true; }
    getActiveUniform() { return { name: "u_tex", type: 0x8b5e }; }
    getUniform() { return 2; }
    createProgram() { return { program: true }; }
    createShader() { return { shader: true }; }
  }
  for (const name of ["activeTexture", "bindTexture", "deleteTexture", "texStorage2D", "texSubImage2D", "generateMipmap", "bindBuffer", "bindVertexArray",
    "deleteBuffer", "bufferData", "bufferSubData", "vertexAttribPointer", "bindRenderbuffer", "renderbufferStorage", "useProgram", "shaderSource",
    "attachShader", "linkProgram", "uniform1i", "getUniformLocation", "drawElementsInstanced", "drawArrays", "bindFramebuffer", "framebufferTexture2D"]) {
    WebGL2RenderingContext.prototype[name] = function () { return {}; };
  }
  class HTMLCanvasElement { getContext() { return new WebGL2RenderingContext(); } }
  const pending = [];
  const window = { HTMLCanvasElement, WebGL2RenderingContext, WebAssembly: { Memory: class {}, instantiate: async () => ({}) },
    performance: { now: () => 0 }, document: { querySelector: () => ({ width: 100, height: 50 }) },
    requestAnimationFrame: (callback) => pending.push(callback) };
  window.window = window;
  runInNewContext(GL_INVENTORY_HOOK, window);
  window.__memoryCensus.stack = () => [7, "_glTexStorage2D", 9];
  const endFrame = () => { window.requestAnimationFrame(() => {}); for (const callback of pending.splice(0)) callback(16); };
  return { window, endFrame };
}

function drawOneFrame() {
  const { window, endFrame } = inventoryPage();
  const gl = new window.HTMLCanvasElement().getContext("webgl2");
  const tex = {}, vbo = {}, vao = {}, shader = gl.createShader();
  const program = gl.createProgram();
  gl.shaderSource(shader, "void main() { gl_Position = vec4(0.0); }");
  gl.attachShader(program, shader);
  gl.linkProgram(program);
  gl.activeTexture(GL.TEXTURE0 + 2);
  gl.bindTexture(GL.TEXTURE_2D, tex);
  gl.texStorage2D(GL.TEXTURE_2D, 1, 0x8058, 64, 32);
  gl.texSubImage2D(GL.TEXTURE_2D, 0, 0, 0, 10, 2, 0x1908, 0x1401, new Uint8Array(80));
  gl.bindVertexArray(vao);
  gl.bindBuffer(GL.ARRAY_BUFFER, vbo);
  gl.bufferData(GL.ARRAY_BUFFER, 1024, 0x88e8);
  gl.vertexAttribPointer(0, 3, 0x1406, false, 0, 0);
  gl.bufferSubData(GL.ARRAY_BUFFER, 16, new Uint8Array(1 << 20), 0, 100);
  gl.bufferData(GL.ARRAY_BUFFER, 2048, 0x88e8);
  gl.useProgram(program);
  gl.drawElementsInstanced(4, 30, 0x1403, 0, 2);
  endFrame();
  return window;
}

test("the inventory keeps the ledger running and adds who made each resource and what read it", async () => {
  const window = drawOneFrame();
  const census = await readMemoryCensus(async (expression) => runInNewContext(expression, window));
  assert.equal(census.kinds.texture.bytes, 64 * 32 * 4);
  assert.equal(census.kinds.buffer_vertex.bytes, 2048);
  assert.equal(census.programs, 1);
  const read = JSON.parse(window.__glInventoryRead());
  assert.deepEqual(read.tex[0].stack, [7, "_glTexStorage2D", 9]);
  assert.equal(read.tex[0].uploads, 1);
  assert.equal(read.tex[0].rowsUsed, 2);
  assert.equal(read.tex[0].texelsUsed, 20);
  assert.deepEqual(Object.values(read.tex[0].uses), [1]);
  assert.deepEqual(read.buf[0], { seq: read.buf[0].seq, target: GL.ARRAY_BUFFER, usage: 0x88e8, size: 2048, subBytes: 100, subCalls: 1, reallocs: 1,
    maxEnd: 116, drawn: 1, stack: [7, "_glTexStorage2D", 9] });
  assert.equal(read.prog[0].draws, 1);
  assert.equal(read.prog[0].linked, 1);
  assert.deepEqual(read.prog[0].samplers, [{ name: "u_tex", unit: 2, type: 0x8b5e }]);
});

test("a window counts per-frame GL calls and draws, without the inventory's own reads, and starts over", () => {
  const window = drawOneFrame();
  const frame = JSON.parse(window.__glInventoryWindow());
  assert.equal(frame.frames, 1);
  assert.equal(frame.draws, 1);
  assert.equal(frame.tris, 20);
  assert.equal(frame.useProgram, 1);
  assert.equal(frame.bindVao, 1);
  assert.equal(frame.lastCalls.getActiveUniform, undefined);
  assert.equal(frame.lastCalls.getProgramParameter, undefined);
  assert.equal(frame.uploadBytes, 100);
  assert.equal(JSON.parse(window.__glInventoryWindow()).frames, 0);
});

test("the plain census hook records no call stacks", () => {
  const window = page();
  const gl = new window.HTMLCanvasElement().getContext("webgl2");
  gl.bindTexture(GL.TEXTURE_2D, {});
  gl.texStorage2D(GL.TEXTURE_2D, 1, 0x8058, 4, 4);
  assert.equal([...window.__memoryCensus.textures.values()][0].stack, null);
  assert.equal(window.__glInventoryRead, undefined);
});

test("the written report names owners through the symbol map and programs by their shader files", () => {
  const window = drawOneFrame();
  const windows = { spawn: JSON.parse(window.__glInventoryWindow()) };
  const snapshot = JSON.parse(window.__glInventoryRead());
  const streams = {};
  noteStreams(streams, snapshot);
  const shaders = [{ file: "world.vert", lines: new Set(["void main() { gl_Position = vec4(0.0); }"]) }];
  const report = assembleInventory({ snapshot, windows, streams, symbols: parseSymbols("7:make_atlas\r\n"), shaders });
  assert.deepEqual(report.tex[0].owner, ["make_atlas", "_glTexStorage2D", "fn9"]);
  assert.equal(report.tex[0].stack, undefined);
  assert.equal(report.prog[0].name, "world.vert + ?");
  assert.deepEqual(Object.keys(report.tex[0].uses), ["world.vert + ?:u_tex"]);
  assert.deepEqual(Object.keys(report.windows.spawn.lastDrawsByProgram), ["world.vert + ?"]);
  assert.equal(report.totals.programs, 1);
  assert.equal(report.totals.programsDrawn, 1);
  assert.deepEqual(report.streams, [{ size: 2048, target: GL.ARRAY_BUFFER, maxEnd: 116 }]);
});

test("a weak shader match says so, and places are checked before Chrome starts", () => {
  const candidates = [{ file: "a.frag", lines: new Set(["one;", "two;", "three;"]) }];
  assert.equal(matchShader("one;\nfour;", candidates, ".frag"), "a.frag~0.25");
  assert.equal(matchShader("one;\ntwo;\nthree;", candidates, ".frag"), "a.frag");
  assert.equal(matchShader(null, candidates, ".frag"), "?");
  assert.ok(checkPlaces([{ name: "spawn", plan: [] }, { name: "roam", reload: false, plan: [["hold", "w", 1]] }]));
  assert.throws(() => checkPlaces([]), /non-empty/);
  assert.throws(() => checkPlaces([{ plan: [] }]), /place 0 has no name/);
  assert.throws(() => checkPlaces([{ name: "roam", reload: false, plan: [] }]), /continues nothing/);
  assert.throws(() => checkPlaces([{ name: "a", plan: [["fly"]] }]), /unknown plan step/);
});
