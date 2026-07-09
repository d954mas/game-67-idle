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
