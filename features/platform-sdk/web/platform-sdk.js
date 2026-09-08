import { createPlatformSdkAdapter } from "./platform-sdk-adapter.js";

const root = globalThis;
const config = root.__PLATFORM_SDK_CONFIG__ || {};

/* Portal lifecycle state can arrive before WASM. Keep it until the C bridge
   installs its hooks, then replay the effective pause and audio state once. */
const lifecycleState = root.__platformSdkLifecycleState || { paused: false, audioEnabled: true };
root.__platformSdkLifecycleState = lifecycleState;

const lifecycle = {
  pause() {
    lifecycleState.paused = true;
    if (typeof root.__platformSdkPortalPause === "function") root.__platformSdkPortalPause();
  },
  resume() {
    lifecycleState.paused = false;
    if (typeof root.__platformSdkPortalResume === "function") root.__platformSdkPortalResume();
  },
  /* A portal mute switch, not a pause: the game keeps running without sound. */
  audio(enabled) {
    lifecycleState.audioEnabled = Boolean(enabled);
    if (typeof root.__platformSdkPortalAudio === "function") root.__platformSdkPortalAudio(lifecycleState.audioEnabled);
  },
  adVisible(requestId, visible) {
    if (typeof root.__platformSdkAdVisible === "function") root.__platformSdkAdVisible(requestId, visible);
  },
};

const platformSdkInternalBackend = createPlatformSdkAdapter({
  config,
  host: root,
  lifecycle,
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
