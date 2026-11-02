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
  wasm_initial: null, wasm_seen: null, grows: [] };
const now = () => Math.round(performance.now ? performance.now() : 0);
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
const keep = (values) => {
  for (const v of Object.values(values || {})) if (!census.memory && v instanceof WebAssembly.Memory) { census.memory = v; noteWasm(); }
};
const grow = WebAssembly.Memory.prototype.grow;
if (grow) WebAssembly.Memory.prototype.grow = function (...args) {
  const r = grow.apply(this, args);
  try { if (!census.memory) census.memory = this; if (this === census.memory) noteWasm(); } catch (e) {}
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
    if (!t) census.textures.set(tex, t = { levels: new Map(), width: 0, height: 0, format: 0, faces: 1 });
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
    census.buffers.set(buffer, { bytes, target, usage });
  });
  hook("bindRenderbuffer", (r, target, rb) => { state.rb = rb; });
  hook("deleteRenderbuffer", (r, rb) => census.renderbuffers.delete(rb));
  hook("renderbufferStorage", (r, target, format, w, h) => {
    if (state.rb) census.renderbuffers.set(state.rb, { bytes: levelBytes(format, w, h, 1), width: w, height: h, format, samples: 1 });
  });
  hook("renderbufferStorageMultisample", (r, target, samples, format, w, h) => {
    if (state.rb) census.renderbuffers.set(state.rb, { bytes: levelBytes(format, w, h, 1) * Math.max(1, samples), width: w, height: h, format, samples });
  });
  hook("createProgram", () => { census.programs++; });
  hook("deleteProgram", () => { census.programs--; });
  hook("createShader", () => { census.shaders++; });
  hook("deleteShader", () => { census.shaders--; });
  hook("createFramebuffer", () => { census.framebuffers++; });
  hook("deleteFramebuffer", () => { census.framebuffers--; });
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
