import { createPlatformSdkAdapter } from "./platform-sdk-adapter.js";

const root = globalThis;
const config = root.__PLATFORM_SDK_CONFIG__ || {};
const platformSdkInternalBackend = createPlatformSdkAdapter({
  config,
  host: root,
  platformSdk: config.platformSdk || "",
  target: config.target || "local",
});

root.__platformSdkInternalBackend = platformSdkInternalBackend;
if (typeof root.__platformSdkSetLoadingProgress === "function") {
  root.__platformSdkSetLoadingProgress(0.015);
}
// Portal readiness is a milestone, not a share of the download. Claiming a
// wide band here parks the bar until the real byte counts overtake it.
Promise.resolve(platformSdkInternalBackend.ready())
  .then((ready) => {
    if (ready && typeof root.__platformSdkSetLoadingProgress === "function") {
      root.__platformSdkSetLoadingProgress(0.02);
    }
  })
  .catch(() => {});
