// Memory census of a running web game, taken from outside the runtime: the page
// script below is installed before any page code (CDP
// Page.addScriptToEvaluateOnNewDocument) and keeps a ledger of every live WebGL
// resource and the module's WebAssembly memory. It needs no DevAPI, so a release
// build can be measured the same way as a DevAPI build.
//
// GPU bytes are an estimate from what the page asked GL to allocate: texture
// levels by internal format, buffer sizes, renderbuffer storage times samples, and
// the default framebuffer (colour, depth-stencil, double-buffered). Drivers pad and
// may keep shadow copies, so the real cost is higher; the ledger is for comparing
// builds on one machine, not for a device budget in absolute bytes.
//
//   import { MEMORY_CENSUS_HOOK, readMemoryCensus } from "ai_studio/game_tools/web_memory_census.mjs";
//   await cdp.call("Page.addScriptToEvaluateOnNewDocument", { source: MEMORY_CENSUS_HOOK });
//   const census = await readMemoryCensus(evaluate); // evaluate(expression) -> value
//
// The GL inventory is the same ledger with the wasm call stack that made each resource,
// what each program's samplers read, and per-frame GL call counts at named places. It
// costs a wrapper on every GL call, so only the CLI below installs it:
//
//   node ai_studio/game_tools/web_memory_census.mjs --zip <package.zip> --out <report.json>
//     [--places <file.json>] [--symbols <file.js.symbols>] [--shaders <dir,dir>]
//     [--mode phone|phone-land|desktop] [--lang <code>] [--save <file> --save-key <slot>] [--storage '{"key":"value"}']
//
// A place is { "name": "...", "plan": [...], "reload": false }: the page loads (unless
// "reload" is false, which continues the place before it), runs the plan steps of
// lib/web_page_plan.mjs and records a window of frames. The report's resources are those
// of the last place, so a final place that walks on counts what the walk created.
import { existsSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { checkPlan, openGamePage, parseFlags, readJsonInput, storageFromFlags, unpackStoreZip, viewportFor } from "./lib/web_page_plan.mjs";

// Bytes per texel by sized internal format; a block format's bytes are per 4x4 block.
const FORMAT_BYTES = {
  0x8229: 1, 0x822b: 2, 0x8051: 4, 0x8058: 4, 0x8c41: 4, 0x8c43: 4, 0x8d62: 2, 0x8056: 2, 0x8057: 2,
  0x8059: 4, 0x8c3a: 4, 0x8c3d: 4, 0x822d: 2, 0x822f: 4, 0x881b: 8, 0x881a: 8, 0x822e: 4, 0x8230: 8,
  0x8815: 16, 0x8814: 16, 0x8232: 1, 0x8231: 1, 0x8236: 4, 0x8235: 4, 0x8d70: 16, 0x8d7c: 4, 0x8d8e: 4,
  0x81a5: 2, 0x81a6: 4, 0x8cac: 4, 0x88f0: 4, 0x8cad: 8, 0x8d48: 1,
  0x1908: 4, 0x1907: 4, 0x1909: 1, 0x190a: 2, 0x1906: 1, 0x1902: 4, 0x84f9: 4,
};
const BLOCK_BYTES = {
  0x83f0: 8, 0x83f1: 8, 0x83f2: 16, 0x83f3: 16, 0x8c4c: 8, 0x8c4d: 8, 0x8c4e: 16, 0x8c4f: 16,
  0x9270: 8, 0x9271: 8, 0x9272: 16, 0x9273: 16, 0x9274: 8, 0x9275: 8, 0x9276: 8, 0x9277: 8, 0x9278: 16, 0x9279: 16,
  0x8d64: 8, 0x8e8c: 16, 0x8e8d: 16, 0x8e8e: 16, 0x8e8f: 16, 0x93b0: 16, 0x93d0: 16,
};

export const MEMORY_CENSUS_HOOK = `(() => {
if (window.__memoryCensus) return;
const FORMAT_BYTES = ${JSON.stringify(FORMAT_BYTES)}, BLOCK_BYTES = ${JSON.stringify(BLOCK_BYTES)};
const census = window.__memoryCensus = { textures: new Map(), buffers: new Map(), renderbuffers: new Map(),
  programs: 0, shaders: 0, framebuffers: 0, peak_gpu: 0, peak_wasm: 0, memory: null, contexts: [], unknown_formats: {},
  wasm_initial: null, wasm_seen: null, grows: [], stack: null };
const madeBy = () => (census.stack ? census.stack() : null);
const now =() => Math.round(performance.now ? performance.now() : 0);
// Each change of the heap size is one grow: the runtime allocated a larger block and copied the heap.
const noteWasm = () => {
  if (!census.memory) return;
  const bytes = census.memory.buffer.byteLength;
  if (census.wasm_initial === null) census.wasm_initial = bytes;
  else if (bytes !== census.wasm_seen) census.grows.push({ from_bytes: census.wasm_seen, to_bytes: bytes, at_ms: now() });
  census.wasm_seen = bytes;
};
const levelBytes = (format, w, h, d) => {
  if (BLOCK_BYTES[format]) return Math.ceil(w / 4) * Math.ceil(h / 4) * BLOCK_BYTES[format] * d;
  if (!FORMAT_BYTES[format]) census.unknown_formats[format] = 1;
  return w * h * d * (FORMAT_BYTES[format] || 4);
};
census.levelBytes = levelBytes;
const keep = (values) => {
  for (const v of Object.values(values || {})) if (!census.memory && v instanceof WebAssembly.Memory) { census.memory = v; noteWasm(); }
};
const grow = WebAssembly.Memory.prototype.grow;
// A memory first seen by its own grow takes its size before that grow as the baseline.
if (grow) WebAssembly.Memory.prototype.grow = function (...args) {
  let before = null;
  try { before = this.buffer.byteLength; } catch (e) {}
  const r = grow.apply(this, args);
  try {
    if (!census.memory) { census.memory = this; census.wasm_initial = before; census.wasm_seen = before; }
    if (this === census.memory) noteWasm();
  } catch (e) {}
  return r;
};
for (const name of ["instantiate", "instantiateStreaming"]) {
  const original = WebAssembly[name];
  if (original) WebAssembly[name] = function (...args) {
    keep(args[1] && args[1].env);
    return original.apply(this, args).then((r) => { keep((r.instance || r).exports); return r; });
  };
}
const gpuTotal = () => {
  let total = 0;
  for (const t of census.textures.values()) for (const b of t.levels.values()) total += b;
  for (const b of census.buffers.values()) total += b.bytes;
  for (const r of census.renderbuffers.values()) total += r.bytes;
  return total;
};
const notePeak = () => {
  census.peak_gpu = Math.max(census.peak_gpu, gpuTotal());
  noteWasm();
  if (census.memory) census.peak_wasm = Math.max(census.peak_wasm, census.memory.buffer.byteLength);
};
function wrap(gl) {
  const C = gl.constructor.prototype, state = { unit: 0, units: [{}], buffers: {}, vao: null, vaoElements: new Map(), rb: null };
  const bound = (target) => {
    const t = target === gl.TEXTURE_CUBE_MAP || (target >= 0x8515 && target <= 0x851a) ? gl.TEXTURE_CUBE_MAP : target;
    return (state.units[state.unit] || {})[t];
  };
  const entry = (tex, info) => {
    if (!tex) return null;
    let t = census.textures.get(tex);
    if (!t) census.textures.set(tex, t = { levels: new Map(), width: 0, height: 0, format: 0, faces: 1, stack: madeBy() });
    Object.assign(t, info);
    return t;
  };
  const face = (target) => (target >= 0x8515 && target <= 0x851a ? target - 0x8515 : 0);
  const hook = (name, after) => {
    const original = C[name];
    if (!original) return;
    gl[name] = function (...a) { const r = original.apply(this, a); try { after(r, ...a); notePeak(); } catch (e) {} return r; };
  };
  hook("activeTexture", (r, unit) => { state.unit = unit - gl.TEXTURE0; state.units[state.unit] ||= {}; });
  hook("bindTexture", (r, target, tex) => { (state.units[state.unit] ||= {})[target] = tex; });
  hook("deleteTexture", (r, tex) => census.textures.delete(tex));
  const image = (target, level, format, w, h, d) => {
    const t = entry(bound(target), level === 0 ? { width: w, height: h, format } : {});
    if (t) t.levels.set(face(target) * 64 + level, levelBytes(format, w, h, d));
  };
  // The six-argument form takes its size from the source (image, canvas, bitmap, video).
  hook("texImage2D", (r, target, level, format, ...rest) => {
    if (rest.length >= 6) image(target, level, format, rest[0], rest[1], 1);
    else if (rest[2]) image(target, level, format, rest[2].width || rest[2].videoWidth || 0, rest[2].height || rest[2].videoHeight || 0, 1);
  });
  hook("texImage3D", (r, target, level, format, w, h, d) => image(target, level, format, w, h, d));
  hook("compressedTexImage2D", (r, target, level, format, w, h) => image(target, level, format, w, h, 1));
  const storage = (target, levels, format, w, h, d) => {
    const faces = target === gl.TEXTURE_CUBE_MAP ? 6 : 1;
    const t = entry(bound(target), { width: w, height: h, format, faces });
    if (!t) return;
    for (let f = 0; f < faces; f++) for (let l = 0; l < levels; l++)
      t.levels.set(f * 64 + l, levelBytes(format, Math.max(1, w >> l), Math.max(1, h >> l), target === gl.TEXTURE_3D ? Math.max(1, d >> l) : d));
  };
  hook("texStorage2D", (r, target, levels, format, w, h) => storage(target, levels, format, w, h, 1));
  hook("texStorage3D", (r, target, levels, format, w, h, d) => storage(target, levels, format, w, h, d));
  hook("generateMipmap", (r, target) => {
    const t = census.textures.get(bound(target));
    if (!t || !t.width) return;
    for (let l = 1; (t.width >> l) || (t.height >> l); l++)
      t.levels.set(l, levelBytes(t.format, Math.max(1, t.width >> l), Math.max(1, t.height >> l), 1));
  });
  const bufferKey = (target) => (target === gl.ELEMENT_ARRAY_BUFFER ? state.vaoElements.get(state.vao) : state.buffers[target]);
  hook("bindBuffer", (r, target, buffer) => {
    if (target === gl.ELEMENT_ARRAY_BUFFER) state.vaoElements.set(state.vao, buffer); else state.buffers[target] = buffer;
  });
  hook("bindVertexArray", (r, vao) => { state.vao = vao; });
  hook("deleteBuffer", (r, buffer) => census.buffers.delete(buffer));
  // WebGL2 callers (Emscripten among them) pass a view of the whole heap plus an offset and length.
  hook("bufferData", (r, target, data, usage, offset, length) => {
    const buffer = bufferKey(target);
    if (!buffer) return;
    const element = (data && data.BYTES_PER_ELEMENT) || 1;
    const bytes = typeof data === "number" ? data
      : length ? length * element : Math.max(0, ((data && data.byteLength) || 0) - (offset || 0) * element);
    const known = census.buffers.get(buffer);
    if (known) Object.assign(known, { bytes, target, usage, reallocs: known.reallocs + 1 });
    else census.buffers.set(buffer, { bytes, target, usage, reallocs: 0, stack: madeBy() });
  });
  hook("bindRenderbuffer", (r, target, rb) => { state.rb = rb; });
  hook("deleteRenderbuffer", (r, rb) => census.renderbuffers.delete(rb));
  hook("renderbufferStorage", (r, target, format, w, h) => {
    if (state.rb) census.renderbuffers.set(state.rb, { bytes: levelBytes(format, w, h, 1), width: w, height: h, format, samples: 1, stack: madeBy() });
  });
  hook("renderbufferStorageMultisample", (r, target, samples, format, w, h) => {
    if (state.rb) census.renderbuffers.set(state.rb, { bytes: levelBytes(format, w, h, 1) * Math.max(1, samples), width: w, height: h, format, samples, stack: madeBy() });
  });
  hook("createProgram", () => { census.programs++; });
  hook("deleteProgram", () => { census.programs--; });
  hook("createShader", () => { census.shaders++; });
  hook("deleteShader", () => { census.shaders--; });
  hook("createFramebuffer", () => { census.framebuffers++; });
  hook("deleteFramebuffer", () => { census.framebuffers--; });
  Object.assign(state, { bound, bufferKey });
  gl.__censusState = state;
  census.contexts.push(gl);
  return gl;
}
const getContext = HTMLCanvasElement.prototype.getContext;
HTMLCanvasElement.prototype.getContext = function (type, attributes) {
  const gl = getContext.call(this, type, attributes);
  if (gl && /webgl/.test(type) && !gl.__census) { gl.__census = true; wrap(gl); census.attributes = gl.getContextAttributes(); }
  return gl;
};
window.__memoryCensusRead = (topN = 12) => {
  notePeak();
  const kinds = {};
  const add = (kind, bytes) => { const k = kinds[kind] ||= { count: 0, bytes: 0 }; k.count++; k.bytes += bytes; };
  const textures = [];
  for (const t of census.textures.values()) {
    let bytes = 0; for (const b of t.levels.values()) bytes += b;
    const kind = [0x81a5, 0x81a6, 0x8cac, 0x88f0, 0x8cad].includes(t.format) ? "texture_depth" : "texture";
    add(kind, bytes);
    textures.push({ width: t.width, height: t.height, format: "0x" + t.format.toString(16), faces: t.faces, levels: t.levels.size, bytes });
  }
  for (const b of census.buffers.values()) add(b.target === 0x8a11 ? "buffer_uniform" : b.target === 0x8893 ? "buffer_index" : "buffer_vertex", b.bytes);
  for (const r of census.renderbuffers.values()) add("renderbuffer", r.bytes);
  const gl = census.contexts[0];
  const canvas = gl ? { width: gl.drawingBufferWidth, height: gl.drawingBufferHeight } : null;
  const attrs = census.attributes || {};
  const framebuffer = canvas ? canvas.width * canvas.height * (4 * 2 + (attrs.depth || attrs.stencil ? 4 : 0)) * Math.max(1, attrs.antialias ? 4 : 1) : 0;
  textures.sort((a, b) => b.bytes - a.bytes);
  const histogram = {};
  for (const t of textures) { const key = t.width + "x" + t.height + "@" + t.format; const h = histogram[key] ||= { count: 0, bytes: 0 }; h.count++; h.bytes += t.bytes; }
  const heap = performance.memory || {};
  return {
    wasm_bytes: census.memory ? census.memory.buffer.byteLength : null, wasm_peak_bytes: census.peak_wasm || null,
    wasm_initial_bytes: census.wasm_initial, wasm_grows: census.memory ? census.grows.length : null, wasm_grow_events: census.grows.slice(0, 16),
    gpu_bytes: gpuTotal() + framebuffer, gpu_peak_bytes: census.peak_gpu + framebuffer, default_framebuffer: { ...canvas, bytes: framebuffer },
    kinds, programs: census.programs, shaders: census.shaders, framebuffers: census.framebuffers,
    top_textures: textures.slice(0, topN), texture_sizes: histogram,
    js_heap_used_bytes: heap.usedJSHeapSize ?? null, unknown_formats: Object.keys(census.unknown_formats),
  };
};
})();`;

// Reads the census from a page that installed MEMORY_CENSUS_HOOK before it loaded.
export async function readMemoryCensus(evaluate, topN = 12) {
  const census = await evaluate(`window.__memoryCensusRead ? window.__memoryCensusRead(${Number(topN)}) : null`);
  if (!census) throw new Error("memory census: the page did not install MEMORY_CENSUS_HOOK before loading");
  return census;
}

// The census ledger holds bytes and call stacks; this layer adds what happens to the
// resources. It wraps the WebGL2 prototype, which the census's per-context hooks call
// through, so both see every call. Reading state back goes through `quiet` to stay out of the counts.
export const GL_INVENTORY_HOOK = `${MEMORY_CENSUS_HOOK}
(() => {
if (window.__glInventory) return;
Error.stackTraceLimit = 48;
const FORMAT_BYTES = ${JSON.stringify(FORMAT_BYTES)};
const census = window.__memoryCensus;
const inv = window.__glInventory = { programs: new Map(), shaders: new Map(), framebuffers: new Map(), vaoBuffers: new Map(),
  frames: [], cur: null, seq: 0, linkMs: 0, untracked: {} };
const newFrame = () => ({ calls: Object.create(null), total: 0, draws: [], tris: 0, uploadBytes: 0, texUploadBytes: 0 });
inv.cur = newFrame();
const raf = window.requestAnimationFrame.bind(window);
window.requestAnimationFrame = (cb) => raf((t) => { inv.frames.push(inv.cur); if (inv.frames.length > 240) inv.frames.shift(); inv.cur = newFrame(); return cb(t); });
// Wasm function indices, and the Emscripten GL import a stack passes through.
census.stack = () => {
  const out = [];
  for (const line of (new Error().stack || "").split("\\n")) {
    const wasm = /wasm-function\\[(\\d+)\\]/.exec(line);
    const named = wasm ? null : /at (?:[^ ]*\\.)?(_gl\\w+|_emscripten_gl\\w+)/.exec(line);
    if (wasm) out.push(Number(wasm[1])); else if (named) out.push(named[1]);
    if (out.length >= 14) break;
  }
  return out;
};
const seqOf = (entry) => (entry.seq ||= ++inv.seq);
const current = new WeakMap();
const cur = (gl) => { let c = current.get(gl); if (!c) current.set(gl, c = { program: null, framebuffer: null }); return c; };
const uploaded = (data, offset, length) => (typeof data === "number" || !data ? 0
  : length ? length * (data.BYTES_PER_ELEMENT || 1) : data.byteLength - (offset || 0) * (data.BYTES_PER_ELEMENT || 1));
const noteVaoBuffer = (gl) => {
  const c = gl.__censusState;
  if (!c.vao) return;
  let set = inv.vaoBuffers.get(c.vao);
  if (!set) inv.vaoBuffers.set(c.vao, set = new Set());
  set.add(c.buffers[0x8892]);
};
const before = {
  useProgram(gl, a) { cur(gl).program = a[0]; },
  bindFramebuffer(gl, a) { if (a[0] !== 0x8ca8) cur(gl).framebuffer = a[1]; },
  vertexAttribPointer: noteVaoBuffer,
  vertexAttribIPointer: noteVaoBuffer,
  shaderSource(gl, a) { inv.shaders.set(a[0], a[1]); },
  attachShader(gl, a) { const p = inv.programs.get(a[0]); if (p) p.shaders.push(a[1]); },
  uniform1i(gl) { const p = inv.programs.get(cur(gl).program); if (p) p.samplers = null; },
  texImage2D(gl, a) { if (a.length >= 8) inv.cur.texUploadBytes += census.levelBytes(a[2], a[3], a[4], 1); },
  compressedTexImage2D(gl, a) { inv.cur.texUploadBytes += census.levelBytes(a[2], a[3], a[4], 1); },
  texSubImage2D(gl, a) {
    const t = census.textures.get(gl.__censusState.bound(a[0]));
    if (!t) return;
    t.uploads = (t.uploads || 0) + 1;
    t.rows ||= {};
    for (let y = a[3]; y < a[3] + a[5]; y++) t.rows[y] = Math.max(t.rows[y] || 0, a[4]);
    inv.cur.texUploadBytes += a[4] * a[5] * (FORMAT_BYTES[t.format] || 4);
  },
  compressedTexSubImage2D(gl, a) { const t = census.textures.get(gl.__censusState.bound(a[0])); if (t) t.uploads = (t.uploads || 0) + 1; },
  bufferData(gl, a) { inv.cur.uploadBytes += uploaded(a[1], a[3], a[4]); },
  bufferSubData(gl, a) {
    const n = uploaded(a[2], a[3], a[4]);
    const b = census.buffers.get(gl.__censusState.bufferKey(a[0]));
    if (b) { b.subBytes = (b.subBytes || 0) + n; b.subCalls = (b.subCalls || 0) + 1; b.maxEnd = Math.max(b.maxEnd || 0, a[1] + n); }
    inv.cur.uploadBytes += n;
  },
  framebufferTexture2D(gl, a) {
    const fb = cur(gl).framebuffer;
    if (!fb) return;
    let e = inv.framebuffers.get(fb);
    if (!e) inv.framebuffers.set(fb, e = { seq: ++inv.seq, att: {}, stack: census.stack(), draws: 0 });
    const t = census.textures.get(a[3]);
    e.att["0x" + a[1].toString(16)] = t ? seqOf(t) : 0;
  },
};
let quiet = false;
const hushed = (fn) => { quiet = true; try { return fn(); } finally { quiet = false; } };
const SAMPLER_TARGET = { 0x8b60: 0x8513, 0x8b5f: 0x806f, 0x8dd2: 0x806f, 0x8dc1: 0x8c1a };
const SAMPLER_TYPES = [0x8b5e, 0x8b62, 0x8dc1, 0x8b60, 0x8b5f, 0x8dcf, 0x8dd2, 0x8dc5];
const samplersOf = (gl, program) => {
  const out = [];
  const count = gl.getProgramParameter(program, 0x8b86);
  for (let i = 0; i < count; i++) {
    const u = gl.getActiveUniform(program, i);
    if (u && SAMPLER_TYPES.includes(u.type)) out.push({ name: u.name, unit: gl.getUniform(program, gl.getUniformLocation(program, u.name)), type: u.type });
  }
  return out;
};
const blocksOf = (gl, program) => {
  const out = [];
  const count = gl.getProgramParameter(program, 0x8a36);
  for (let i = 0; i < count; i++) out.push(gl.getActiveUniformBlockName(program, i));
  return out;
};
const noteDraw = (gl, count, instances) => {
  const c = gl.__censusState, { program, framebuffer } = cur(gl), p = inv.programs.get(program);
  if (!p) return;
  p.draws++;
  if (c.vao) {
    const used = [...(inv.vaoBuffers.get(c.vao) || []), c.vaoElements.get(c.vao)];
    for (const buffer of used) { const b = census.buffers.get(buffer); if (b) b.drawn = (b.drawn || 0) + 1; }
  }
  if (!p.samplers) hushed(() => { p.samplers = samplersOf(gl, program); p.blocks ||= blocksOf(gl, program); });
  for (const u of p.samplers) {
    const tex = (c.units[u.unit] || {})[SAMPLER_TARGET[u.type] || 0x0de1];
    const t = tex && census.textures.get(tex);
    const use = p.seq + ":" + u.name;
    if (t) { t.uses ||= {}; t.uses[use] = (t.uses[use] || 0) + 1; } else if (tex) inv.untracked[use] = 1;
  }
  const fb = framebuffer && inv.framebuffers.get(framebuffer);
  if (fb) fb.draws++;
  inv.cur.draws.push({ p: p.seq, fb: fb ? fb.seq : 0, n: count, i: instances });
  inv.cur.tris += count * instances / 3;
};
const draws = {
  drawArrays: (gl, a) => noteDraw(gl, a[2], 1), drawElements: (gl, a) => noteDraw(gl, a[1], 1),
  drawArraysInstanced: (gl, a) => noteDraw(gl, a[2], a[3]), drawElementsInstanced: (gl, a) => noteDraw(gl, a[1], a[4]),
  drawRangeElements: (gl, a) => noteDraw(gl, a[3], 1),
};
const P = WebGL2RenderingContext.prototype;
// "constructor" stays: the census reaches this prototype through gl.constructor.prototype.
for (const name of Object.getOwnPropertyNames(P)) {
  const d = Object.getOwnPropertyDescriptor(P, name);
  if (name === "constructor" || !d || typeof d.value !== "function") continue;
  const original = d.value, pre = before[name], draw = draws[name];
  P[name] = function (...a) {
    if (quiet) return original.apply(this, a);
    const frame = inv.cur;
    frame.calls[name] = (frame.calls[name] || 0) + 1;
    frame.total++;
    if (pre) pre(this, a);
    if (draw) draw(this, a);
    if (name === "linkProgram") {
      const started = performance.now();
      const r = original.apply(this, a);
      // Linking finishes when its status is read.
      hushed(() => P.getProgramParameter.call(this, a[0], 0x8b82));
      const ms = performance.now() - started;
      inv.linkMs += ms;
      const p = inv.programs.get(a[0]);
      if (p) { p.linked++; p.linkMs = (p.linkMs || 0) + ms; }
      return r;
    }
    const r = original.apply(this, a);
    if (name === "createProgram") inv.programs.set(r, { seq: ++inv.seq, shaders: [], stack: census.stack(), draws: 0, linked: 0, blocks: null, samplers: null });
    return r;
  };
}
const wasmReach = () => {
  if (!census.memory) return null;
  const u = new Uint8Array(census.memory.buffer);
  let top = u.length - 1;
  while (top > 0 && u[top] === 0) top--;
  let touched = 0;
  for (let i = 0; i < u.length; i += 65536) {
    for (let j = i; j < Math.min(i + 65536, u.length); j += 64) if (u[j] !== 0) { touched += 65536; break; }
  }
  return { size: u.length, top, touched };
};
window.__glInventoryRead = () => {
  const c = document.querySelector("canvas");
  const sum = (values) => [...values].reduce((total, x) => total + x, 0);
  return JSON.stringify({
    wasm: wasmReach(), canvas: c ? [c.width, c.height] : null, attrs: census.attributes || null, linkMs: inv.linkMs,
    unknown: { ...census.unknown_formats, ...inv.untracked },
    tex: [...census.textures.values()].map((t) => ({ seq: seqOf(t), w: t.width, h: t.height, f: t.format, lv: t.levels.size,
      bytes: sum(t.levels.values()), stack: t.stack, uses: t.uses || {}, uploads: t.uploads || 0,
      rowsUsed: t.rows ? Object.keys(t.rows).length : 0, texelsUsed: t.rows ? sum(Object.values(t.rows)) : 0,
      maxRow: t.rows ? Math.max(...Object.values(t.rows)) : 0 })),
    buf: [...census.buffers.values()].map((b) => ({ seq: seqOf(b), target: b.target, usage: b.usage, size: b.bytes, subBytes: b.subBytes || 0,
      subCalls: b.subCalls || 0, reallocs: b.reallocs, maxEnd: b.maxEnd || 0, drawn: b.drawn || 0, stack: b.stack })),
    rb: [...census.renderbuffers.values()].map((r) => ({ seq: seqOf(r), f: r.format, w: r.width, h: r.height, samples: r.samples, bytes: r.bytes, stack: r.stack })),
    fbo: [...inv.framebuffers.values()],
    prog: [...inv.programs.values()].map((p) => ({ seq: p.seq, draws: p.draws, linked: p.linked, linkMs: p.linkMs, stack: p.stack,
      blocks: p.blocks, samplers: p.samplers, src: p.shaders.map((s) => inv.shaders.get(s)) })),
  });
};
// Medians over the last frames, then starts a new window.
window.__glInventoryWindow = () => {
  const frames = inv.frames.slice(-60);
  inv.frames.length = 0;
  const med = (f) => { const v = frames.map(f).sort((a, b) => a - b); return v.length ? v[v.length >> 1] : 0; };
  const pick = (names) => med((x) => names.reduce((s, n) => s + (x.calls[n] || 0), 0));
  const typical = frames.slice().sort((a, b) => b.draws.length - a.draws.length)[frames.length >> 1] || newFrame();
  const byProgram = {};
  for (const d of typical.draws) {
    const e = byProgram[d.p + "/" + d.fb] ||= { draws: 0, instances: 0, tris: 0 };
    e.draws++; e.instances += d.i; e.tris += Math.round(d.n * d.i / 3);
  }
  return JSON.stringify({ frames: frames.length, medTotal: med((x) => x.total), draws: med((x) => x.draws.length), tris: med((x) => x.tris),
    programsPerFrame: med((x) => new Set(x.draws.map((d) => d.p)).size), useProgram: pick(["useProgram"]),
    bindTexture: pick(["bindTexture", "activeTexture", "bindSampler"]), bindVao: pick(["bindVertexArray"]),
    bindBuffer: pick(["bindBuffer", "bindBufferBase", "bindBufferRange"]),
    uniforms: med((x) => Object.keys(x.calls).filter((n) => n.startsWith("uniform")).reduce((s, n) => s + x.calls[n], 0)),
    fixed: pick(["enable", "disable", "blendFunc", "blendFuncSeparate", "blendEquation", "blendEquationSeparate", "depthMask", "depthFunc", "colorMask", "cullFace", "frontFace", "viewport", "scissor", "polygonOffset"]),
    fb: pick(["bindFramebuffer", "clear", "clearBufferfv", "clearBufferfi", "invalidateFramebuffer"]),
    uploads: pick(["bufferData", "bufferSubData", "texSubImage2D", "compressedTexSubImage2D", "texImage2D"]),
    uploadBytes: med((x) => x.uploadBytes), texUploadBytes: med((x) => x.texUploadBytes), lastDrawsByProgram: byProgram, lastCalls: typical.calls });
};
})();`;

export function parseSymbols(text) {
  const symbols = new Map();
  for (const line of text.split(/\r?\n/)) {
    const colon = line.indexOf(":");
    if (colon > 0) symbols.set(Number(line.slice(0, colon)), line.slice(colon + 1));
  }
  return symbols;
}

const shaderLines = (text) => text.split(/\r?\n/).map((line) => line.replace(/\/\/.*$/, "").trim()).filter((line) => line && !line.startsWith("#"));

// Names a compiled source by the shader file with the most lines in common; "~0.42" marks a weak match.
export function matchShader(source, candidates, extension) {
  if (!source) return "?";
  const have = new Set(shaderLines(source));
  let best = null;
  let score = 0;
  for (const candidate of candidates) {
    if (!candidate.file.endsWith(extension)) continue;
    let hits = 0;
    for (const line of candidate.lines) if (have.has(line)) hits++;
    const jaccard = hits / (candidate.lines.size + have.size - hits || 1);
    if (jaccard > score) { score = jaccard; best = candidate.file; }
  }
  return best ? `${best}${score < 0.7 ? `~${score.toFixed(2)}` : ""}` : "?";
}

export function readShaderSources(roots) {
  const expand = (file, seen = new Set()) => {
    if (seen.has(file) || !existsSync(file)) return "";
    seen.add(file);
    return readFileSync(file, "utf8").replace(/#include\s+"([^"]+)"/g, (_, included) => {
      const root = roots.find((candidate) => existsSync(join(candidate, included)));
      return root ? expand(join(root, included), seen) : "";
    });
  };
  const sources = [];
  for (const root of roots) {
    for (const file of readdirSync(root)) {
      if (/\.(vert|frag)$/.test(file) && statSync(join(root, file)).isFile()) sources.push({ file, lines: new Set(shaderLines(expand(join(root, file)))) });
    }
  }
  return sources;
}

// The largest extent each stream buffer was written to, over every place.
export function noteStreams(streams, snapshot) {
  for (const buffer of snapshot.buf) {
    const key = `${buffer.target.toString(16)}:${buffer.size}`;
    streams[key] ||= { size: buffer.size, target: buffer.target, maxEnd: 0 };
    streams[key].maxEnd = Math.max(streams[key].maxEnd, buffer.maxEnd);
  }
}

// Page data to the written report: stacks to owners through the symbol map, compiled
// sources to shader names, and totals. Stream buffers under 256 bytes are not worth listing.
export function assembleInventory({ snapshot, windows, streams = {}, symbols = new Map(), shaders = [] }) {
  const report = { ...snapshot, windows };
  const owner = (stack) => (stack || []).map((frame) => (typeof frame === "number" ? symbols.get(frame) || `fn${frame}` : frame));
  for (const list of [report.tex, report.buf, report.rb, report.fbo, report.prog]) {
    for (const entry of list) { entry.owner = owner(entry.stack); delete entry.stack; }
  }
  for (const program of report.prog) {
    const vert = program.src.find((source) => source && /gl_Position/.test(source));
    const frag = program.src.find((source) => source !== vert);
    program.name = `${matchShader(vert, shaders, ".vert")} + ${matchShader(frag, shaders, ".frag")}`;
    program.srcBytes = program.src.reduce((total, source) => total + (source ? source.length : 0), 0);
    delete program.src;
  }
  const programName = new Map(report.prog.map((program) => [program.seq, program.name]));
  for (const texture of report.tex) {
    texture.uses = Object.fromEntries(Object.entries(texture.uses).map(([key, count]) => {
      const [seq, uniform] = key.split(":");
      return [`${programName.get(Number(seq)) || seq}:${uniform}`, count];
    }));
  }
  for (const window of Object.values(windows)) {
    window.lastDrawsByProgram = Object.fromEntries(Object.entries(window.lastDrawsByProgram).map(([key, value]) => {
      const [program, framebuffer] = key.split("/");
      return [`${programName.get(Number(program)) || program}${framebuffer !== "0" ? ` @fbo${framebuffer}` : ""}`, value];
    }));
  }
  const sum = (list, field) => list.reduce((total, entry) => total + field(entry), 0);
  const mib = (bytes) => +(bytes / 1048576).toFixed(2);
  report.totals = {
    texMB: mib(sum(report.tex, (t) => t.bytes)), texCount: report.tex.length, bufMB: mib(sum(report.buf, (b) => b.size)), bufCount: report.buf.length,
    rbMB: mib(sum(report.rb, (r) => r.bytes)), programs: report.prog.length, programsDrawn: report.prog.filter((p) => p.draws > 0).length,
    linkMs: +report.linkMs.toFixed(1), shaderSourceKB: +(sum(report.prog, (p) => p.srcBytes) / 1024).toFixed(1),
    canvas: report.canvas, attrs: report.attrs, wasm: report.wasm,
  };
  report.streams = Object.values(streams).filter((stream) => stream.size >= 256 && stream.maxEnd > 0).sort((a, b) => b.size - a.size);
  return report;
}

export function checkPlaces(places) {
  if (!Array.isArray(places) || places.length === 0) throw new Error("--places is a non-empty JSON array of { name, plan, reload? }");
  places.forEach((place, index) => {
    if (typeof place.name !== "string" || !place.name) throw new Error(`place ${index} has no name`);
    if (index === 0 && place.reload === false) throw new Error(`place ${place.name} continues nothing: it is the first`);
    checkPlan(place.plan);
  });
  return places;
}

const INVENTORY_FLAGS = ["zip", "out", "places", "symbols", "shaders", "mode", "lang", "save", "save-key", "storage"];
const SETTLE_AFTER_PLACE_MS = 3000;
const WINDOW_MS = 2500;
const DEFAULT_PLACES = [{ name: "start", plan: [["wait", 5000]] }];

async function inventoryCli(argv) {
  const flags = parseFlags(argv, INVENTORY_FLAGS);
  if (!flags.zip || !flags.out) throw new Error("--zip <package.zip> and --out <report.json> are required");
  const viewport = viewportFor(flags.mode || "phone");
  const places = checkPlaces(flags.places ? readJsonInput(flags.places) : DEFAULT_PLACES);
  const scripts = [GL_INVENTORY_HOOK, ...storageFromFlags(flags)];
  const symbols = flags.symbols ? parseSymbols(readFileSync(flags.symbols, "utf8")) : new Map();
  const shaders = readShaderSources((flags.shaders || "").split(",").filter(Boolean));
  const site = unpackStoreZip(flags.zip, "gl-inventory-site-");
  const sleep = (ms) => new Promise((done) => setTimeout(done, ms));
  const windows = {};
  const streams = {};
  let snapshot = null;
  let page = null;
  try {
    page = await openGamePage({ site, viewport, scripts, lang: flags.lang, profilePrefix: "ai-studio-gl-inventory" });
    for (const place of places) {
      if (place.reload !== false) await page.load();
      await page.run(place.plan);
      await sleep(SETTLE_AFTER_PLACE_MS);
      await page.evaluate("window.__glInventoryWindow()");
      await sleep(WINDOW_MS);
      windows[place.name] = JSON.parse(await page.evaluate("window.__glInventoryWindow()"));
      snapshot = JSON.parse(await page.evaluate("window.__glInventoryRead()"));
      noteStreams(streams, snapshot);
      console.log(place.name, windows[place.name].frames, "frames", windows[place.name].draws, "draws");
    }
  } finally {
    if (page) await page.close();
    rmSync(site, { recursive: true, force: true, maxRetries: 5, retryDelay: 500 });
  }
  const report = assembleInventory({ snapshot, windows, streams, symbols, shaders });
  writeFileSync(flags.out, JSON.stringify(report, null, 1));
  console.log(JSON.stringify(report.totals));
  for (const [name, w] of Object.entries(windows)) {
    console.log(name, JSON.stringify({ gl: w.medTotal, draws: w.draws, prog: w.programsPerFrame, use: w.useProgram, tex: w.bindTexture, vao: w.bindVao,
      buf: w.bindBuffer, uni: w.uniforms, fixed: w.fixed, fb: w.fb, up: w.uploads, tris: w.tris }));
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  inventoryCli(process.argv.slice(2)).catch((error) => {
    console.error(`web_memory_census: ${error.message}`);
    process.exitCode = 1;
  });
}
