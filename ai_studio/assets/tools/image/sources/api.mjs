// image/sources bridge: accept an uploaded source image into the session tmp
// tree. Thin wrapper over the shared _bridge plumbing. The tmp namespace stays
// "raster2d" (bridge default) so the frozen viewer's /tmp/.../raster2d/ URLs and
// the public endpoint contract are unchanged.
import { randomUUID } from "node:crypto";
import { mkdirSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";

import {
  decodeDataUrl,
  extensionForUpload,
  safeSlug,
  tmpRoot,
  tmpUrl,
  workspaceRel,
} from "../_bridge/bridge.mjs";

export async function uploadImageSource(root, body) {
  const { mime, buffer } = decodeDataUrl(body.dataUrl);
  if (!buffer.length) throw new Error("empty source image");
  const sessionId = `${new Date().toISOString().replace(/[:.]/g, "-")}-${randomUUID().slice(0, 8)}`;
  const outDir = join(tmpRoot(root), sessionId, "sources");
  mkdirSync(outDir, { recursive: true });
  const ext = extensionForUpload(body.fileName, mime);
  const sourceName = `${safeSlug(basename(String(body.fileName || "source")))}${ext}`;
  const sourcePath = join(outDir, sourceName);
  writeFileSync(sourcePath, buffer);
  return {
    sessionId,
    sourcePath: workspaceRel(root, sourcePath),
    sourceUrl: tmpUrl(root, sourcePath),
    fileName: sourceName,
  };
}
