import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const CONFIG_PATH = join(HERE, "targets.json");
const DESCRIPTOR_KEYS = [
  "adapter", "target_id", "sdk_id", "external_links_allowed", "ads_supported",
  "rewarded_supported", "storage_supported", "portal",
];

function load() {
  const parsed = JSON.parse(readFileSync(CONFIG_PATH, "utf8"));
  if (parsed?.schema !== "ai_studio.platform_target_descriptors.v1"
      || !parsed.targets || typeof parsed.targets !== "object" || Array.isArray(parsed.targets)) {
    throw new Error("platform target descriptors are invalid");
  }
  const entries = Object.entries(parsed.targets);
  if (entries.length === 0) throw new Error("platform target descriptors are empty");
  const targetIds = new Set();
  for (const [target, descriptor] of entries) {
    if (!/^[a-z][a-z0-9-]*$/.test(target)
        || JSON.stringify(Object.keys(descriptor || {}).sort()) !== JSON.stringify([...DESCRIPTOR_KEYS].sort())
        || !/^[a-z][a-z0-9-]*$/.test(descriptor.adapter || "")
        || !Number.isSafeInteger(descriptor.target_id) || descriptor.target_id < 0
        || !Number.isSafeInteger(descriptor.sdk_id) || descriptor.sdk_id < 0
        || typeof descriptor.external_links_allowed !== "boolean"
        || typeof descriptor.ads_supported !== "boolean"
        || typeof descriptor.rewarded_supported !== "boolean"
        || typeof descriptor.storage_supported !== "boolean"
        || typeof descriptor.portal !== "boolean"
        || targetIds.has(descriptor.target_id)) {
      throw new Error(`platform target descriptor is invalid: ${target}`);
    }
    targetIds.add(descriptor.target_id);
  }
  return Object.freeze(parsed.targets);
}

const TARGETS = load();

function exposed(target, descriptor) {
  return Object.freeze({
    target,
    adapter: descriptor.adapter,
    targetId: descriptor.target_id,
    sdkId: descriptor.sdk_id,
    externalLinksAllowed: descriptor.external_links_allowed,
    adsSupported: descriptor.ads_supported,
    rewardedSupported: descriptor.rewarded_supported,
    storageSupported: descriptor.storage_supported,
  });
}

export function targetNames() {
  return Object.keys(TARGETS);
}

export function portalTargetNames() {
  return targetNames().filter((target) => TARGETS[target].portal);
}

export function descriptorForTarget(target) {
  const descriptor = TARGETS[target];
  if (!descriptor) throw new Error(`unknown publish target: ${target}`);
  return exposed(target, descriptor);
}

export function isPortalTarget(target) {
  return Boolean(TARGETS[target]?.portal);
}

export function compileProfileForTarget(target, preset, flags = {}) {
  const descriptor = descriptorForTarget(target);
  const profileFlags = {
    debugUi: false, devapi: false, analytics: false, eventsLogMirror: false, ...flags,
  };
  if (!["wasm-release", "wasm-release-asserts", "wasm-debug", "wasm-devapi-debug", "wasm-devapi-perf"].includes(preset)
      || JSON.stringify(Object.keys(profileFlags).sort()) !== JSON.stringify(["analytics", "debugUi", "devapi", "eventsLogMirror"])
      || Object.values(profileFlags).some((value) => typeof value !== "boolean")) {
    throw new Error("web compile profile is invalid");
  }
  return Object.freeze({ target: descriptor.target, adapter: descriptor.adapter, preset, ...profileFlags });
}

const ZONE_ANCHORS = ["top-left", "top-right"];
const ZONE_KEYS = ["anchor", "id", "sizes", "source", "top_px"];
const isPx = (value) => Number.isSafeInteger(value) && value >= 0;

// Reserved zones are portal overlays drawn over the game frame, in CSS px of that frame.
// Each zone's sizes must cover every frame width exactly once, ordered by width.
export function validateReservedZones(zones, label) {
  if (!Array.isArray(zones)) throw new Error(`${label} reserved_zones must be an array`);
  const ids = new Set();
  for (const zone of zones) {
    const where = `${label} reserved zone ${zone?.id ?? "?"}`;
    if (!zone || typeof zone !== "object"
        || JSON.stringify(Object.keys(zone).sort()) !== JSON.stringify(ZONE_KEYS)
        || !/^[a-z][a-z0-9-]*$/.test(zone.id || "") || ids.has(zone.id)
        || !ZONE_ANCHORS.includes(zone.anchor) || !isPx(zone.top_px)
        || typeof zone.source !== "string" || !zone.source.trim()
        || !Array.isArray(zone.sizes) || zone.sizes.length === 0) {
      throw new Error(`${where} is invalid`);
    }
    ids.add(zone.id);
    zone.sizes.forEach((size, index) => {
      const keys = Object.keys(size || {});
      const first = index === 0;
      const last = index === zone.sizes.length - 1;
      if (keys.some((key) => !["width_px", "height_px", "min_frame_width_px", "max_frame_width_px"].includes(key))
          || !isPx(size.width_px) || size.width_px === 0 || !isPx(size.height_px) || size.height_px === 0
          || ("min_frame_width_px" in size) === first || ("max_frame_width_px" in size) === last
          || (!first && !isPx(size.min_frame_width_px)) || (!last && !isPx(size.max_frame_width_px))
          || (!first && size.min_frame_width_px !== zone.sizes[index - 1].max_frame_width_px + 1)
          || (!first && !last && size.max_frame_width_px < size.min_frame_width_px)) {
        throw new Error(`${where} sizes must cover every frame width once, in order`);
      }
    });
  }
  return Object.freeze(zones.map((zone) => Object.freeze({ ...zone, sizes: Object.freeze(zone.sizes.map((size) => Object.freeze({ ...size }))) })));
}

export function reservedZonesForTarget(target) {
  descriptorForTarget(target);
  const path = join(HERE, `${target}.json`);
  let manifest;
  try {
    manifest = JSON.parse(readFileSync(path, "utf8"));
  } catch (error) {
    if (error?.code === "ENOENT") return Object.freeze([]);
    throw error;
  }
  return validateReservedZones(manifest?.metadata?.reserved_zones ?? [], `${target} publish manifest`);
}
