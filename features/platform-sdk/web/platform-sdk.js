import { createPlatformSdkWebBackend } from "./platform-sdk-core.js";
import { createPlatformSdkAdapter } from "./platform-sdk-adapter.js";

function readConfig(root) {
  const config = root.__PLATFORM_SDK_CONFIG__ || {};
  return {
    ...config,
    platformSdk: config.platformSdk || "",
    target: config.target || "local",
  };
}

const root = globalThis;
const platformSdkInternalBackend = createPlatformSdkWebBackend({
  adapterFactory: createPlatformSdkAdapter,
  config: readConfig(root),
  host: root,
});

root.__platformSdkInternalBackend = platformSdkInternalBackend;
if (typeof root.__platformSdkSetLoadingProgress === "function") {
  root.__platformSdkSetLoadingProgress(0.02);
}
platformSdkInternalBackend.ready()
  .then((ready) => {
    if (ready && typeof root.__platformSdkSetLoadingProgress === "function") {
      root.__platformSdkSetLoadingProgress(0.10);
    }
  })
  .catch(() => {});
