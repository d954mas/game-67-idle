import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import { runPython } from "../_bridge/bridge.mjs";

const SCRIPT = "ai_studio/assets/tools/image/quality_gate/asset_quality_gate.py";

export function thresholdsFromStyleLock(lock) {
  const gate = lock?.technical_gate;
  const size = lock?.asset_size;
  if (!gate || !size) throw new Error("asset technical gate requires style-lock technical_gate and asset_size");
  return {
    max_spill_edge_ratio: gate.max_spill_edge_ratio,
    max_halo_edge_ratio: gate.max_halo_edge_ratio,
    max_alpha_noise_ratio: gate.max_alpha_noise_ratio,
    max_empty_margin_ratio: gate.max_empty_margin_ratio,
    aspect_ratio: {
      width: size.width,
      height: size.height,
      max_relative_error: gate.max_aspect_relative_error,
    },
  };
}

export async function runAssetQualityGate(root, { sourcePath, sourceBytes, keyColor, thresholds } = {}) {
  const hasSourcePath = typeof sourcePath === "string" && sourcePath.length > 0;
  const hasSourceBytes = Buffer.isBuffer(sourceBytes);
  if (hasSourcePath === hasSourceBytes) {
    throw new Error("runAssetQualityGate requires exactly one of sourcePath or sourceBytes");
  }
  if (!thresholds || typeof thresholds !== "object") throw new Error("runAssetQualityGate requires thresholds");
  const dir = mkdtempSync(join(tmpdir(), "asset-quality-gate-"));
  const sourceInput = hasSourcePath ? sourcePath : join(dir, "source.png");
  const thresholdsPath = join(dir, "thresholds.json");
  const reportPath = join(dir, "report.json");
  const thumbnailPath = join(dir, "problem.png");
  if (hasSourceBytes) writeFileSync(sourceInput, sourceBytes);
  writeFileSync(thresholdsPath, `${JSON.stringify(thresholds, null, 2)}\n`, "utf8");
  const args = [
    SCRIPT,
    "--source", sourceInput,
    "--thresholds", thresholdsPath,
    "--json-output", reportPath,
    "--problem-thumbnail", thumbnailPath,
  ];
  if (keyColor) args.push("--key-color", keyColor);
  try {
    let executionError = null;
    try {
      await runPython(root, args);
    } catch (error) {
      executionError = error;
    }
    if (!existsSync(reportPath)) throw executionError || new Error("asset technical gate produced no report");
    let report;
    try {
      report = JSON.parse(readFileSync(reportPath, "utf8"));
    } catch (error) {
      throw executionError || new Error(`asset technical gate report is not valid JSON: ${error.message}`);
    }
    return {
      report,
      thumbnailBytes: existsSync(thumbnailPath) ? readFileSync(thumbnailPath) : null,
    };
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}
