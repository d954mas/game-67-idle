import { readFileSync } from "node:fs";

import { __promoteAssetToGameForTest, __withPromotionLockForTest } from "../ops/asset_promotion.mjs";

const [mode, root, ...args] = process.argv.slice(2);

if (mode === "lock") {
  const [gameId, holdMs, staleMs] = args;
  await __withPromotionLockForTest(root, gameId, async () => {
    process.stdout.write(`acquired ${Date.now()}\n`);
    await new Promise((resolveWait) => setTimeout(resolveWait, Number(holdMs)));
    process.stdout.write(`released ${Date.now()}\n`);
  }, { staleMs: Number(staleMs), retryTotalMs: 3000, retryIntervalMs: 20 });
} else if (mode === "crash-after-destination" || mode === "crash-after-prepared-cleanup") {
  const [projectId, elementId, metadataPath, lockPath] = args;
  const metadata = JSON.parse(readFileSync(metadataPath, "utf8"));
  const lock = JSON.parse(readFileSync(lockPath, "utf8"));
  await __promoteAssetToGameForTest(root, { projectId, elementId, metadata }, {
    resolveStyleLock() { return { gameId: "demo-game", lock }; },
    ...(mode === "crash-after-destination"
      ? { afterDestinationCommit() { process.exit(91); } }
      : { afterPreparedMarkerCleanup() { process.exit(92); } }),
  });
} else {
  throw new Error(`unknown promotion fixture mode: ${mode}`);
}
