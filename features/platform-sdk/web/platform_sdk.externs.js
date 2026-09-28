/**
 * @fileoverview Closure externs for a client that links the platform-sdk web
 * backend: every name its EM_JS code shares with the platform bundle, which
 * Closure never sees. A name missing here is renamed on one side only and
 * fails silently.
 * @externs
 */

// Globals the platform bundle or the page shell defines and the client calls.
var __platformSdkInternalBackend;
var __platformSdkLifecycleState;
var __platformSdkSetLoadingProgress;
var __platformSdkHideLoadingOverlay;

// Globals the client defines and the platform bundle calls.
var __platformSdkPortalPause;
var __platformSdkPortalResume;
var __platformSdkPortalAudio;
var __platformSdkPortalSound;
var __platformSdkAdVisible;

/** The backend every adapter returns. @record */
function PlatformSdkBackend() {}
PlatformSdkBackend.prototype.ready;
PlatformSdkBackend.prototype.destroy;
PlatformSdkBackend.prototype.getLocale;
PlatformSdkBackend.prototype.getPlayer;
PlatformSdkBackend.prototype.login;
PlatformSdkBackend.prototype.gameLoadingProgress;
PlatformSdkBackend.prototype.gameLoadingFinished;
PlatformSdkBackend.prototype.gameReady;
PlatformSdkBackend.prototype.gameplayStart;
PlatformSdkBackend.prototype.gameplayStop;
PlatformSdkBackend.prototype.measure;
PlatformSdkBackend.prototype.showBanner;
PlatformSdkBackend.prototype.hideBanner;
PlatformSdkBackend.prototype.soundSwitches;
PlatformSdkBackend.prototype.setSoundMuted;
PlatformSdkBackend.prototype.showInterstitial;
PlatformSdkBackend.prototype.showRewarded;
PlatformSdkBackend.prototype.leaderboardCaps;
PlatformSdkBackend.prototype.submitScore;
PlatformSdkBackend.prototype.fetchEntries;
PlatformSdkBackend.prototype.showLeaderboard;
PlatformSdkBackend.prototype.loadData;
PlatformSdkBackend.prototype.saveData;

/** Fields of the objects the backend returns and of the lifecycle state. @record */
function PlatformSdkResult() {}
PlatformSdkResult.prototype.status;
PlatformSdkResult.prototype.value;
PlatformSdkResult.prototype.reason;
PlatformSdkResult.prototype.supported;
PlatformSdkResult.prototype.shown;
PlatformSdkResult.prototype.rewarded;
PlatformSdkResult.prototype.authorized;
PlatformSdkResult.prototype.name;
PlatformSdkResult.prototype.avatarUrl;
PlatformSdkResult.prototype.canRead;
PlatformSdkResult.prototype.canWrite;
PlatformSdkResult.prototype.needsLogin;
PlatformSdkResult.prototype.nativePopup;
PlatformSdkResult.prototype.player;
PlatformSdkResult.prototype.top;
PlatformSdkResult.prototype.around;
PlatformSdkResult.prototype.rank;
PlatformSdkResult.prototype.you;
PlatformSdkResult.prototype.extra;
PlatformSdkResult.prototype.paused;
PlatformSdkResult.prototype.audioEnabled;
PlatformSdkResult.prototype.soundMuted;
