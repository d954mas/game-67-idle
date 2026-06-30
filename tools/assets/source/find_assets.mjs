#!/usr/bin/env node
// Compatibility entrypoint. Canonical OKF catalog logic lives in:
// ai_studio/assets/assets_storage/okf_catalog/find_assets.mjs
import { pathToFileURL } from "node:url";
import { resolve } from "node:path";
import { main } from "../../../ai_studio/assets/assets_storage/okf_catalog/find_assets.mjs";

export * from "../../../ai_studio/assets/assets_storage/okf_catalog/find_assets.mjs";

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  main().catch((error) => {
    console.error(error.message);
    process.exit(1);
  });
}
