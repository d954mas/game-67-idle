import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath, pathToFileURL } from "node:url";

// A public game sits at games/<id>, a private one at games/private/<id>, so the
// hop count up to the studio cannot be written into the path. Walk until the
// feature tree appears instead.
function studioRoot() {
  let dir = dirname(fileURLToPath(import.meta.url));
  for (let i = 0; i < 8; i += 1) {
    if (existsSync(join(dir, "features", "platform-sdk"))) return dir;
    dir = dirname(dir);
  }
  throw new Error("studio root not found above the game tools directory");
}

const featureUrl = (relative) =>
  pathToFileURL(join(studioRoot(), "features", "platform-sdk", relative)).href;

const { descriptorForTarget, portalTargetNames, targetNames } =
  await import(featureUrl("publish-targets/target_config.mjs"));
const { inspectPlatformSdkArtifact } = await import(featureUrl("scripts/artifact_tools.mjs"));

test("canonical target descriptors expose every target and its SDK mapping", () => {
  assert.deepEqual(targetNames(), ["local", "itch", "poki", "yandex", "playgama", "crazygames"]);
  assert.deepEqual(portalTargetNames(), ["itch", "poki", "yandex", "playgama", "crazygames"]);
  assert.deepEqual(descriptorForTarget("poki"), {
    target: "poki", adapter: "poki", targetId: 2, sdkId: 1,
    externalLinksAllowed: false, adsSupported: true, rewardedSupported: true, storageSupported: false,
  });
});

test("artifact inspection rejects a missing local artifact when required files are requested", () => {
  const result = inspectPlatformSdkArtifact({
    target: "local",
    artifactDir: "C:/does-not-exist/platform-sdk-artifact",
    production: true,
    requireFiles: true,
  });
  assert.equal(result.ok, false);
  assert.ok(result.violations.some((item) => item.reason === "missing-artifact-directory"));
});

test("canonical target descriptor IDs match the C platform enums", async () => {
  const { readFileSync } = await import("node:fs");
  const { join } = await import("node:path");
  const root = studioRoot();
  const header = readFileSync(join(root, "features", "platform-sdk", "include", "features", "platform_sdk", "platform_sdk.h"), "utf8");
  const targets = JSON.parse(readFileSync(join(root, "features", "platform-sdk", "publish-targets", "targets.json"), "utf8")).targets;
  const enumValues = (name) => Object.fromEntries(header.matchAll(new RegExp("typedef enum " + name + " \\{([\\s\\S]*?)\\} " + name + ";", "g"))[Symbol.iterator]().next().value[1]
    .matchAll(/\b(PLATFORM_(?:TARGET|SDK)_[A-Z]+)\s*=\s*(\d+)/g)
    .map((match) => [match[1], Number(match[2])]));
  const targetIds = enumValues("platform_target_t");
  const sdkIds = enumValues("platform_sdk_t");
  for (const [target, descriptor] of Object.entries(targets)) {
    assert.equal(descriptor.target_id, targetIds["PLATFORM_TARGET_" + target.toUpperCase()], target);
    assert.equal(descriptor.sdk_id, sdkIds["PLATFORM_SDK_" + descriptor.adapter.toUpperCase()], target);
  }
});
