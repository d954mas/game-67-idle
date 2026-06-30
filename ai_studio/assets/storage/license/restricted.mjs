// Public-git routing helpers for asset binaries. The deeper license decision
// lives in registry.mjs; this file keeps the old public API used by pull/guard.
import {
  decideLicense,
  hasAttributionInfo,
  hasNoticeInfo,
} from "./registry.mjs";

export { decideLicense, hasAttributionInfo, hasNoticeInfo, validateLicenseRecord } from "./registry.mjs";

export function isPublishable(fm = {}) {
  return decideLicense(fm).publishable;
}

export function requiresAttribution(fm = {}) {
  return decideLicense(fm).attributionRequired;
}

export function requiresNotice(fm = {}) {
  return decideLicense(fm).noticeRequired;
}

// Binary asset extensions the leak guard requires a recorded license for.
export const ASSET_BINARY_EXT = new Set([
  ".glb", ".gltf", ".bin", ".fbx", ".obj", ".mtl",
  ".png", ".jpg", ".jpeg", ".webp", ".gif", ".tga", ".ktx2", ".dds", ".hdr", ".exr",
  ".wav", ".ogg", ".mp3", ".flac",
  ".ttf", ".otf",
]);

export function isAssetBinary(path) {
  const dot = path.lastIndexOf(".");
  if (dot < 0) return false;
  return ASSET_BINARY_EXT.has(path.slice(dot).toLowerCase());
}

// Gitignored root that holds non-publishable (paid/licensed) binaries.
export const RESTRICTED_ROOT = "restricted";
