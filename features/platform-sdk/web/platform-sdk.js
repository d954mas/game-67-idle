import { createPlatformSdkAdapter } from "./platform-sdk-adapter.js";

const root = globalThis;
const config = root.__PLATFORM_SDK_CONFIG__ || {};

/* The portal can pause the game without the player touching it. The C facade
   publishes these hooks when the web backend is installed, so a portal event
   that arrives before wasm is up is dropped rather than queued: nothing is
   running yet to pause. */
const lifecycle = {
  pause() {
    if (typeof root.__platformSdkPortalPause === "function") root.__platformSdkPortalPause();
  },
  resume() {
    if (typeof root.__platformSdkPortalResume === "function") root.__platformSdkPortalResume();
  },
  /* A portal mute switch, not a pause: the game keeps running without sound. */
  audio(enabled) {
    if (typeof root.__platformSdkPortalAudio === "function") root.__platformSdkPortalAudio(enabled);
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
