import assert from "node:assert/strict";
import test from "node:test";
import { runInNewContext } from "node:vm";

import { MEMORY_CENSUS_HOOK, readMemoryCensus } from "../web_memory_census.mjs";

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
