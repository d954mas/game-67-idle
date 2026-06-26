// Single source of truth for "may this asset be committed to an open git repo?"
//
// Publishable  => redistribution of the asset is allowed (CC0/OFL/CC-BY/...). The
//                 binary may live in the committed tree (assets/source, assets/previews).
// Restricted   => paid/commercial/unknown license. The binary must live under the
//                 gitignored assets/restricted/ root; only the catalog/license .md
//                 (metadata, no binary) is committed.
//
// Used by ai_studio/assets/asset_viewer/pull.mjs (routing) and the leak guard
// tools/assets/audit/restricted_assets_guard.mjs so both agree on one rule.

// Free/open licenses whose source asset may be redistributed in a public repo.
export const FREE_LICENSE_RE =
  /\b(cc0|cc[-_ ]?by(?:[-_ ]?sa)?|ofl|open\s*font|public[\s-]?domain|unlicense|mit|apache|zlib|bsd)\b/i;

const TRUE_TOKENS = new Set(["true", "1", "yes", "y"]);
const FALSE_TOKENS = new Set(["false", "0", "no", "n"]);

function asBool(value) {
  if (value === true) return true;
  if (value === false) return false;
  if (typeof value === "string") {
    const s = value.trim().toLowerCase();
    if (TRUE_TOKENS.has(s)) return true;
    if (FALSE_TOKENS.has(s)) return false;
  }
  return undefined;
}

// Decide publishability from a parsed catalog frontmatter object.
// Order of precedence: explicit `publish` -> `redistribution_allowed` -> license string.
// Fail-safe: an empty/unrecognized license is treated as NOT publishable so a paid
// asset can never leak into git just because someone forgot to set a flag.
export function isPublishable(fm = {}) {
  const explicit = asBool(fm.publish);
  if (explicit !== undefined) return explicit;
  const redist = asBool(fm.redistribution_allowed);
  if (redist !== undefined) return redist;
  const license = `${fm.license || ""} ${fm.license_url || ""}`.trim();
  if (!license) return false;
  return FREE_LICENSE_RE.test(license);
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
