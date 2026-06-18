#!/usr/bin/env node
import { readFileSync } from "node:fs";
import path from "node:path";

const DEFAULT_GLB =
  "gamedesign/projects/mine-cards/visual/skeletal_spike/minecards_skeletal_miner_probe.glb";

const COMPONENT_SIZE = new Map([
  [5120, 1],
  [5121, 1],
  [5122, 2],
  [5123, 2],
  [5125, 4],
  [5126, 4],
]);

const TYPE_WIDTH = new Map([
  ["SCALAR", 1],
  ["VEC2", 2],
  ["VEC3", 3],
  ["VEC4", 4],
  ["MAT4", 16],
]);

function usage() {
  return [
    "Usage: node tools/assets/inspect_skeletal_glb.mjs [file.glb] [--time seconds] [--animation name] [--json-out]",
    "",
    "Reads a binary glTF/GLB, validates skins/animations, and samples one pose.",
  ].join("\n");
}

function parseArgs(argv) {
  const args = {
    file: DEFAULT_GLB,
    time: 0.5,
    animationName: null,
    jsonOut: false,
  };

  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--help" || arg === "-h") {
      console.log(usage());
      process.exit(0);
    }
    if (arg === "--time") {
      const next = argv[++i];
      args.time = Number.parseFloat(next);
      if (!Number.isFinite(args.time)) {
        throw new Error(`Invalid --time value: ${next}`);
      }
      continue;
    }
    if (arg === "--animation") {
      const next = argv[++i];
      if (!next) {
        throw new Error("--animation requires a clip name");
      }
      args.animationName = next;
      continue;
    }
    if (arg === "--json-out") {
      args.jsonOut = true;
      continue;
    }
    if (arg.startsWith("--")) {
      throw new Error(`Unknown option: ${arg}`);
    }
    args.file = arg;
  }

  return args;
}

function readGlb(file) {
  const bytes = readFileSync(file);
  if (bytes.length < 20 || bytes.toString("utf8", 0, 4) !== "glTF") {
    throw new Error(`${file} is not a GLB file`);
  }

  const version = bytes.readUInt32LE(4);
  if (version !== 2) {
    throw new Error(`Unsupported GLB version ${version}; expected 2`);
  }

  let json = null;
  let bin = null;
  let offset = 12;
  while (offset < bytes.length) {
    const chunkLength = bytes.readUInt32LE(offset);
    const chunkType = bytes.readUInt32LE(offset + 4);
    offset += 8;
    const chunk = bytes.subarray(offset, offset + chunkLength);
    offset += chunkLength;

    if (chunkType === 0x4e4f534a) {
      json = JSON.parse(chunk.toString("utf8").replace(/\0+$/u, "").trimEnd());
    } else if (chunkType === 0x004e4942) {
      bin = chunk;
    }
  }

  if (!json) {
    throw new Error("GLB has no JSON chunk");
  }
  return { json, bin };
}

function componentReader(buffer, componentType, offset) {
  switch (componentType) {
    case 5120:
      return buffer.readInt8(offset);
    case 5121:
      return buffer.readUInt8(offset);
    case 5122:
      return buffer.readInt16LE(offset);
    case 5123:
      return buffer.readUInt16LE(offset);
    case 5125:
      return buffer.readUInt32LE(offset);
    case 5126:
      return buffer.readFloatLE(offset);
    default:
      throw new Error(`Unsupported componentType ${componentType}`);
  }
}

function readAccessor(gltf, bin, accessorIndex) {
  const accessor = gltf.accessors?.[accessorIndex];
  if (!accessor) {
    throw new Error(`Missing accessor ${accessorIndex}`);
  }
  if (accessor.sparse) {
    throw new Error(`Sparse accessor ${accessorIndex} is not supported in this spike`);
  }

  const width = TYPE_WIDTH.get(accessor.type);
  const componentSize = COMPONENT_SIZE.get(accessor.componentType);
  if (!width || !componentSize) {
    throw new Error(`Unsupported accessor ${accessorIndex} type/component`);
  }

  const bufferView = gltf.bufferViews?.[accessor.bufferView];
  if (!bufferView) {
    throw new Error(`Missing bufferView for accessor ${accessorIndex}`);
  }
  if (bufferView.buffer !== 0) {
    throw new Error(`Only GLB binary buffer 0 is supported; got buffer ${bufferView.buffer}`);
  }

  const stride = bufferView.byteStride ?? componentSize * width;
  const start = (bufferView.byteOffset ?? 0) + (accessor.byteOffset ?? 0);
  const values = [];
  for (let row = 0; row < accessor.count; row += 1) {
    const item = [];
    const rowStart = start + row * stride;
    for (let col = 0; col < width; col += 1) {
      item.push(componentReader(bin, accessor.componentType, rowStart + col * componentSize));
    }
    values.push(width === 1 ? item[0] : item);
  }
  return values;
}

function identity() {
  return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
}

function multiply(a, b) {
  const out = new Array(16).fill(0);
  for (let col = 0; col < 4; col += 1) {
    for (let row = 0; row < 4; row += 1) {
      out[col * 4 + row] =
        a[0 * 4 + row] * b[col * 4 + 0] +
        a[1 * 4 + row] * b[col * 4 + 1] +
        a[2 * 4 + row] * b[col * 4 + 2] +
        a[3 * 4 + row] * b[col * 4 + 3];
    }
  }
  return out;
}

function normalizeQuat(q) {
  const length = Math.hypot(q[0], q[1], q[2], q[3]) || 1;
  return [q[0] / length, q[1] / length, q[2] / length, q[3] / length];
}

function trsMatrix(translation, rotation, scale) {
  const [x, y, z, w] = normalizeQuat(rotation);
  const [sx, sy, sz] = scale;
  const x2 = x + x;
  const y2 = y + y;
  const z2 = z + z;
  const xx = x * x2;
  const xy = x * y2;
  const xz = x * z2;
  const yy = y * y2;
  const yz = y * z2;
  const zz = z * z2;
  const wx = w * x2;
  const wy = w * y2;
  const wz = w * z2;

  return [
    (1 - (yy + zz)) * sx,
    (xy + wz) * sx,
    (xz - wy) * sx,
    0,
    (xy - wz) * sy,
    (1 - (xx + zz)) * sy,
    (yz + wx) * sy,
    0,
    (xz + wy) * sz,
    (yz - wx) * sz,
    (1 - (xx + yy)) * sz,
    0,
    translation[0],
    translation[1],
    translation[2],
    1,
  ];
}

function lerp(a, b, t) {
  return a + (b - a) * t;
}

function lerpArray(a, b, t) {
  return a.map((value, index) => lerp(value, b[index], t));
}

function slerpQuat(a, b, t) {
  let bx = b[0];
  let by = b[1];
  let bz = b[2];
  let bw = b[3];
  let cos = a[0] * bx + a[1] * by + a[2] * bz + a[3] * bw;
  if (cos < 0) {
    cos = -cos;
    bx = -bx;
    by = -by;
    bz = -bz;
    bw = -bw;
  }
  if (cos > 0.9995) {
    return normalizeQuat([lerp(a[0], bx, t), lerp(a[1], by, t), lerp(a[2], bz, t), lerp(a[3], bw, t)]);
  }
  const theta = Math.acos(Math.max(-1, Math.min(1, cos)));
  const sinTheta = Math.sin(theta);
  const wa = Math.sin((1 - t) * theta) / sinTheta;
  const wb = Math.sin(t * theta) / sinTheta;
  return normalizeQuat([
    a[0] * wa + bx * wb,
    a[1] * wa + by * wb,
    a[2] * wa + bz * wb,
    a[3] * wa + bw * wb,
  ]);
}

function sampleSampler(times, values, interpolation, sampleTime, pathName) {
  if (interpolation === "CUBICSPLINE") {
    throw new Error(`CUBICSPLINE animation is not supported yet for ${pathName}`);
  }
  if (times.length === 0) {
    throw new Error(`Animation sampler for ${pathName} has no keyframes`);
  }
  if (sampleTime <= times[0]) {
    return values[0];
  }
  if (sampleTime >= times[times.length - 1]) {
    return values[values.length - 1];
  }

  let key = 0;
  while (key < times.length - 2 && times[key + 1] < sampleTime) {
    key += 1;
  }

  if (interpolation === "STEP") {
    return values[key];
  }

  const span = times[key + 1] - times[key];
  const alpha = span === 0 ? 0 : (sampleTime - times[key]) / span;
  if (pathName === "rotation") {
    return slerpQuat(values[key], values[key + 1], alpha);
  }
  return lerpArray(values[key], values[key + 1], alpha);
}

function baseTransforms(gltf) {
  return (gltf.nodes ?? []).map((node) => ({
    translation: node.translation ? [...node.translation] : [0, 0, 0],
    rotation: node.rotation ? [...node.rotation] : [0, 0, 0, 1],
    scale: node.scale ? [...node.scale] : [1, 1, 1],
    matrix: node.matrix ? [...node.matrix] : null,
  }));
}

function findRoots(gltf) {
  const childSet = new Set();
  for (const node of gltf.nodes ?? []) {
    for (const child of node.children ?? []) {
      childSet.add(child);
    }
  }
  const scene = gltf.scenes?.[gltf.scene ?? 0];
  if (scene?.nodes?.length) {
    return scene.nodes;
  }
  return (gltf.nodes ?? []).map((_, index) => index).filter((index) => !childSet.has(index));
}

function computeWorlds(gltf, transforms) {
  const worlds = new Array(gltf.nodes?.length ?? 0);
  const visit = (nodeIndex, parentWorld) => {
    const transform = transforms[nodeIndex];
    const local = transform.matrix ?? trsMatrix(transform.translation, transform.rotation, transform.scale);
    const world = parentWorld ? multiply(parentWorld, local) : local;
    worlds[nodeIndex] = world;
    for (const child of gltf.nodes[nodeIndex].children ?? []) {
      visit(child, world);
    }
  };
  for (const root of findRoots(gltf)) {
    visit(root, null);
  }
  return worlds;
}

function samplePose(gltf, bin, animation, sampleTime) {
  const transforms = baseTransforms(gltf);
  const clipTimes = [];

  for (const channel of animation.channels ?? []) {
    const sampler = animation.samplers?.[channel.sampler];
    if (!sampler) {
      throw new Error(`Missing sampler ${channel.sampler}`);
    }
    const times = readAccessor(gltf, bin, sampler.input);
    const values = readAccessor(gltf, bin, sampler.output);
    clipTimes.push(...times);
    const pathName = channel.target.path;
    const nodeIndex = channel.target.node;
    transforms[nodeIndex][pathName] = sampleSampler(
      times,
      values,
      sampler.interpolation ?? "LINEAR",
      sampleTime,
      pathName,
    );
  }

  const worlds = computeWorlds(gltf, transforms);
  const minTime = Math.min(...clipTimes);
  const maxTime = Math.max(...clipTimes);
  return { transforms, worlds, minTime, maxTime };
}

function positionFromMatrix(matrix) {
  return [matrix[12], matrix[13], matrix[14]];
}

function roundedArray(values) {
  return values.map((value) => Number(value.toFixed(5)));
}

function selectAnimation(animations, animationName) {
  if (!animationName) {
    return { animation: animations[0], index: 0 };
  }
  const index = animations.findIndex((animation) => animation.name === animationName);
  if (index < 0) {
    const names = animations.map((animation) => animation.name ?? "unnamed").join(", ");
    throw new Error(`Animation not found: ${animationName}. Available: ${names}`);
  }
  return { animation: animations[index], index };
}

function inspect(file, sampleTime, animationName) {
  const { json: gltf, bin } = readGlb(file);
  if (!bin) {
    throw new Error("GLB has no binary chunk");
  }
  const animations = gltf.animations ?? [];
  const skins = gltf.skins ?? [];
  if (!animations.length) {
    throw new Error("GLB has no animations");
  }
  if (!skins.length) {
    throw new Error("GLB has no skins");
  }

  const { animation, index: animationIndex } = selectAnimation(animations, animationName);
  const pose = samplePose(gltf, bin, animation, sampleTime);
  const nodeNames = new Map((gltf.nodes ?? []).map((node, index) => [node.name ?? `node_${index}`, index]));
  const skin = skins[0];
  const inverseBindMatrices = skin.inverseBindMatrices != null ? readAccessor(gltf, bin, skin.inverseBindMatrices) : [];
  const jointNames = (skin.joints ?? []).map((nodeIndex) => gltf.nodes[nodeIndex]?.name ?? `node_${nodeIndex}`);
  const attachmentNodes = ["head", "right_arm", "pickaxe", "handslot.l", "handslot.r"].filter((name) =>
    nodeNames.has(name),
  );
  const attachments = Object.fromEntries(
    attachmentNodes.map((name) => {
      const world = pose.worlds[nodeNames.get(name)];
      return [name, { node: nodeNames.get(name), worldPosition: roundedArray(positionFromMatrix(world)) }];
    }),
  );

  const jointMatrices = (skin.joints ?? []).map((nodeIndex, jointIndex) => {
    const inverseBind = inverseBindMatrices[jointIndex] ?? identity();
    return multiply(pose.worlds[nodeIndex], inverseBind);
  });

  return {
    file: path.normalize(file),
    animation: {
      name: animation.name ?? "animation_0",
      index: animationIndex,
      channels: animation.channels?.length ?? 0,
      samplers: animation.samplers?.length ?? 0,
      minTime: Number(pose.minTime.toFixed(5)),
      maxTime: Number(pose.maxTime.toFixed(5)),
      sampleTime: Number(sampleTime.toFixed(5)),
    },
    skeleton: {
      skinName: skin.name ?? "skin_0",
      jointCount: skin.joints?.length ?? 0,
      joints: jointNames,
      inverseBindMatrixCount: inverseBindMatrices.length,
      sampledJointMatrixCount: jointMatrices.length,
    },
    scene: {
      nodeCount: gltf.nodes?.length ?? 0,
      meshCount: gltf.meshes?.length ?? 0,
      skinCount: skins.length,
      animationCount: animations.length,
    },
    attachments,
  };
}

try {
  const args = parseArgs(process.argv);
  const report = inspect(args.file, args.time, args.animationName);
  if (args.jsonOut) {
    console.log(JSON.stringify(report, null, 2));
  } else {
    console.log(`file: ${report.file}`);
    console.log(
      `scene: ${report.scene.nodeCount} nodes, ${report.scene.meshCount} meshes, ${report.scene.skinCount} skin(s), ${report.scene.animationCount} animation(s)`,
    );
    console.log(
      `animation: ${report.animation.name} (#${report.animation.index}), ${report.animation.channels} channels, ${report.animation.minTime}s..${report.animation.maxTime}s, sampled at ${report.animation.sampleTime}s`,
    );
    console.log(
      `skeleton: ${report.skeleton.skinName}, ${report.skeleton.jointCount} joints, ${report.skeleton.inverseBindMatrixCount} inverse bind matrices`,
    );
    console.log(`joints: ${report.skeleton.joints.join(", ")}`);
    console.log(`attachments: ${JSON.stringify(report.attachments)}`);
  }
} catch (error) {
  console.error(error instanceof Error ? error.message : error);
  process.exit(1);
}
