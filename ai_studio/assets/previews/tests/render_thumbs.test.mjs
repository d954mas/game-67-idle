import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { deflateSync, inflateSync } from "node:zlib";
import test from "node:test";
import { scanPackManifestSource } from "../../manifests/ops.mjs";
import { thumbItem } from "../cache.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const RENDER_THUMBS = join(HERE, "..", "render_thumbs.py");
// A mid tone: a dropped texture renders the importer's grey, a dropped sRGB decode shifts it.
const PALETTE_RGB = [200, 48, 40];

function findBlender() {
  const override = process.env.BLENDER || process.env.BLENDER_EXECUTABLE;
  if (override && existsSync(override)) return override;
  const base = "C:/Program Files/Blender Foundation";
  if (existsSync(base)) {
    for (const dir of readdirSync(base).sort().reverse()) {
      const exe = join(base, dir, "blender.exe");
      if (existsSync(exe)) return exe;
    }
  }
  const which = spawnSync(process.platform === "win32" ? "where" : "which", ["blender"], { encoding: "utf8" });
  return which.status === 0 ? which.stdout.split(/\r?\n/)[0].trim() : "";
}

function crc32(bytes) {
  let crc = ~0;
  for (const byte of bytes) {
    crc ^= byte;
    for (let k = 0; k < 8; k += 1) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return ~crc >>> 0;
}

function pngChunk(tag, data) {
  const head = Buffer.alloc(8);
  head.writeUInt32BE(data.length, 0);
  head.write(tag, 4, "ascii");
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(Buffer.concat([head.subarray(4), data])), 0);
  return Buffer.concat([head, data, crc]);
}

// A 4x4 RGBA palette in one colour; alpha 90 like a palette that stores data there.
function palettePng() {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(4, 0);
  ihdr.writeUInt32BE(4, 4);
  ihdr[8] = 8;
  ihdr[9] = 6;
  const row = Buffer.from([0, ...Array.from({ length: 4 }, () => [...PALETTE_RGB, 90]).flat()]);
  const raw = Buffer.concat(Array.from({ length: 4 }, () => row));
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), pngChunk("IHDR", ihdr), pngChunk("IDAT", deflateSync(raw)), pngChunk("IEND", Buffer.alloc(0))]);
}

function decodePng(bytes) {
  let offset = 8;
  let width = 0;
  let height = 0;
  let channels = 0;
  const idat = [];
  while (offset < bytes.length) {
    const length = bytes.readUInt32BE(offset);
    const tag = bytes.toString("ascii", offset + 4, offset + 8);
    const data = bytes.subarray(offset + 8, offset + 8 + length);
    if (tag === "IHDR") {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      assert.equal(data[8], 8, "8-bit PNG expected");
      channels = { 2: 3, 6: 4 }[data[9]];
    } else if (tag === "IDAT") idat.push(data);
    offset += 12 + length;
  }
  const raw = inflateSync(Buffer.concat(idat));
  const stride = width * channels;
  const pixels = Buffer.alloc(stride * height);
  for (let y = 0; y < height; y += 1) {
    const filter = raw[y * (stride + 1)];
    for (let x = 0; x < stride; x += 1) {
      const value = raw[y * (stride + 1) + 1 + x];
      const left = x >= channels ? pixels[y * stride + x - channels] : 0;
      const up = y > 0 ? pixels[(y - 1) * stride + x] : 0;
      const upLeft = x >= channels && y > 0 ? pixels[(y - 1) * stride + x - channels] : 0;
      const paeth = () => {
        const p = left + up - upLeft;
        const [pa, pb, pc] = [Math.abs(p - left), Math.abs(p - up), Math.abs(p - upLeft)];
        return pa <= pb && pa <= pc ? left : pb <= pc ? up : upLeft;
      };
      const predictor = [0, left, up, (left + up) >> 1, filter === 4 ? paeth() : 0][filter];
      pixels[y * stride + x] = (value + predictor) & 0xff;
    }
  }
  return { width, height, channels, pixels };
}

// One horizontal quad with UVs and no material, as palette-indexed exports ship.
function quadGlb() {
  const positions = new Float32Array([-0.5, 0, -0.5, 0.5, 0, -0.5, 0.5, 0, 0.5, -0.5, 0, 0.5]);
  const normals = new Float32Array([0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0]);
  const uvs = new Float32Array([0.4, 0.4, 0.6, 0.4, 0.6, 0.6, 0.4, 0.6]);
  const indices = new Uint16Array([0, 2, 1, 0, 3, 2]);
  const bin = Buffer.concat([positions, normals, uvs, indices].map((array) => Buffer.from(array.buffer)));
  const views = [[0, 48], [48, 48], [96, 32], [128, 12]].map(([byteOffset, byteLength]) => ({ buffer: 0, byteOffset, byteLength }));
  const json = {
    asset: { version: "2.0" },
    scene: 0,
    scenes: [{ nodes: [0] }],
    nodes: [{ mesh: 0 }],
    meshes: [{ primitives: [{ attributes: { POSITION: 0, NORMAL: 1, TEXCOORD_0: 2 }, indices: 3 }] }],
    buffers: [{ byteLength: bin.length }],
    bufferViews: views,
    accessors: [
      { bufferView: 0, componentType: 5126, count: 4, type: "VEC3", min: [-0.5, 0, -0.5], max: [0.5, 0, 0.5] },
      { bufferView: 1, componentType: 5126, count: 4, type: "VEC3" },
      { bufferView: 2, componentType: 5126, count: 4, type: "VEC2" },
      { bufferView: 3, componentType: 5123, count: 6, type: "SCALAR" },
    ],
  };
  let text = Buffer.from(JSON.stringify(json));
  text = Buffer.concat([text, Buffer.alloc((4 - (text.length % 4)) % 4, 0x20)]);
  const chunk = (type, data) => {
    const head = Buffer.alloc(8);
    head.writeUInt32LE(data.length, 0);
    head.writeUInt32LE(type, 4);
    return Buffer.concat([head, data]);
  };
  const body = Buffer.concat([chunk(0x4e4f534a, text), chunk(0x004e4942, bin)]);
  const header = Buffer.alloc(12);
  header.writeUInt32LE(0x46546c67, 0);
  header.writeUInt32LE(2, 4);
  header.writeUInt32LE(12 + body.length, 8);
  return Buffer.concat([header, body]);
}

test("a pack row's texture reaches the render item", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "thumb-item-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const pack = join(root, "packs", "kit");
  mkdirSync(join(pack, "files"), { recursive: true });
  writeFileSync(join(pack, "files", "prop.glb"), quadGlb());
  writeFileSync(join(pack, "files", "palette.png"), palettePng());
  writeFileSync(join(pack, "pack.json"), JSON.stringify({ pack: "kit", kind: "model" }));
  writeFileSync(join(pack, "assets.jsonl"), `${JSON.stringify({ asset_id: "kit__prop", kind: "model", resource: "files/prop.glb", texture: "files/palette.png" })}\n`);

  const { records } = await scanPackManifestSource(root);
  assert.equal(records[0].texturePath, join(pack, "files", "palette.png"));
  assert.equal(thumbItem("C:\\a\\prop.glb", "kit__prop", "C:\\a\\palette.png"), "C:/a/prop.glb::kit__prop::C:/a/palette.png");
  assert.equal(thumbItem("C:\\a\\prop.glb", "kit__prop", ""), "C:/a/prop.glb::kit__prop");
});

test("render_thumbs colours a material-less model from its declared palette", { skip: !findBlender() && "Blender not installed" }, (t) => {
  const dir = mkdtempSync(join(tmpdir(), "render-thumbs-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const glb = join(dir, "quad.glb");
  const palette = join(dir, "palette.png");
  writeFileSync(glb, quadGlb());
  writeFileSync(palette, palettePng());
  const out = join(dir, "out");
  const result = spawnSync(findBlender(), [
    "--background", "--factory-startup", "--python", RENDER_THUMBS, "--", out, "64",
    thumbItem(glb, "plain"), thumbItem(glb, "tinted", palette),
  ], { encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);

  const centre = (name) => {
    const { width, height, channels, pixels } = decodePng(readFileSync(join(out, `${name}.png`)));
    const at = (Math.floor(height / 2) * width + Math.floor(width / 2)) * channels;
    return [pixels[at], pixels[at + 1], pixels[at + 2]];
  };
  const [pr, pg, pb] = centre("plain");
  assert.ok(Math.abs(pr - pg) < 12 && Math.abs(pg - pb) < 12, `plain render should stay grey, got ${[pr, pg, pb]}`);
  const [r, g, b] = centre("tinted");
  assert.ok(r > g * 1.4 && r > b * 1.4, `declared palette should tint the model red, got ${[r, g, b]}`);
});
