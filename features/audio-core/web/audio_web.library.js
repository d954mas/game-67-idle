mergeInto(LibraryManager.library, {
  $AudioWebRuntime: {
    CLIP_CAPACITY: 64,
    VOICE_CAPACITY: 32,
    MAX_CLIP_PCM_BYTES: 134217728,
    MAX_TOTAL_PCM_BYTES: 268435456,
    HANDLE_INDEX_BITS: 8,
    HANDLE_INDEX_MASK: 255,
    HANDLE_GENERATION_MASK: 0x00ffffff,

    context: null,
    masterNode: null,
    musicNode: null,
    sfxNode: null,
    clips: [],
    voices: [],
    masterGain: 1,
    musicGain: 1,
    sfxGain: 1,
    enabled: true,
    paused: false,
    hidden: false,
    gestureAccepted: false,
    gestureSerial: 0,
    everUnlocked: false,
    resumePending: false,
    resumeAskedAt: 0,
    gestureListener: null,
    visibilityListener: null,
    wakeListener: null,
    retryTimer: null,
    pumpTimer: null,
    lastUpdateMs: 0,
    // How often a context that should run is asked to resume again, and how
    // often streams are topped up when the game loop has stopped calling.
    RETRY_MS: 2000,
    PUMP_MS: 150,
    PUMP_STALE_MS: 200,
    // A context that keeps failing is rebuilt after 0.5, 1, 2, 4 s; past five
    // rebuilds a minute it waits for a gesture or the page coming back.
    REBUILD_BACKOFF_MS: 500,
    REBUILD_LIMIT: 5,
    REBUILD_WINDOW_MS: 60000,
    // A resumed stream fades in over this long, as a restart after a gap does.
    RESUME_FADE_SECONDS: 0.008,
    rebuildTimer: null,
    rebuildTimes: [],
    rebuildWanted: false,
    rebuilds: 0,
    decodedPcmBytes: 0,

    _makeSlots: function(capacity) {
      var slots = new Array(capacity);
      for (var i = 0; i < capacity; ++i) {
        slots[i] = { generation: 1, occupied: false };
      }
      return slots;
    },

    _nextGeneration: function(generation) {
      generation = (generation + 1) & AudioWebRuntime.HANDLE_GENERATION_MASK;
      return generation === 0 ? 1 : generation;
    },

    _packHandle: function(index, generation) {
      return (((generation << AudioWebRuntime.HANDLE_INDEX_BITS) | (index + 1)) >>> 0);
    },

    _lookup: function(slots, handle) {
      handle = handle >>> 0;
      var encodedIndex = handle & AudioWebRuntime.HANDLE_INDEX_MASK;
      var generation = handle >>> AudioWebRuntime.HANDLE_INDEX_BITS;
      if (encodedIndex === 0 || encodedIndex > slots.length || generation === 0) return null;
      var slot = slots[encodedIndex - 1];
      if (!slot.occupied || slot.generation !== generation) return null;
      return { index: encodedIndex - 1, slot: slot };
    },

    _clip: function(handle) {
      return AudioWebRuntime._lookup(AudioWebRuntime.clips, handle);
    },

    _voice: function(handle) {
      return AudioWebRuntime._lookup(AudioWebRuntime.voices, handle);
    },

    _finiteGain: function(value) {
      value = Number(value);
      if (!isFinite(value) || value < 0) return 0;
      return value > 1 ? 1 : value;
    },

    _applyMix: function() {
      if (!AudioWebRuntime.masterNode) return;
      var audible = AudioWebRuntime.enabled && !AudioWebRuntime.paused && !AudioWebRuntime.hidden;
      AudioWebRuntime.masterNode.gain.value = audible ? AudioWebRuntime.masterGain : 0;
      AudioWebRuntime.musicNode.gain.value = AudioWebRuntime.musicGain;
      AudioWebRuntime.sfxNode.gain.value = AudioWebRuntime.sfxGain;
    },

    _suspendForPolicy: function() {
      var context = AudioWebRuntime.context;
      if (!context || context.state === "closed") return;
      if ((!AudioWebRuntime.enabled || AudioWebRuntime.paused || AudioWebRuntime.hidden) &&
          context.state !== "suspended") {
        var result = context.suspend();
        if (result && typeof result.catch === "function") result.catch(function() {});
      }
    },

    _releaseVoicesForResume: function(resumeSerial) {
      for (var i = 0; i < AudioWebRuntime.voices.length; ++i) {
        var slot = AudioWebRuntime.voices[i];
        if (slot.occupied && slot.resumeSerial === resumeSerial) {
          AudioWebRuntime._releaseVoice(i, true);
        }
      }
    },

    _cancelPendingResume: function() {
      if (!AudioWebRuntime.resumePending) return;
      var resumeSerial = AudioWebRuntime.gestureSerial;
      AudioWebRuntime.resumePending = false;
      AudioWebRuntime.gestureSerial += 1;
      AudioWebRuntime._releaseVoicesForResume(resumeSerial);
    },

    _requestResume: function(allowFirstUnlock) {
      var context = AudioWebRuntime.context;
      if (!context || !AudioWebRuntime.enabled || AudioWebRuntime.paused ||
          AudioWebRuntime.hidden || context.state === "closed") return false;
      if (context.state === "running") {
        AudioWebRuntime.gestureAccepted = true;
        AudioWebRuntime.everUnlocked = true;
        AudioWebRuntime.resumePending = false;
        return true;
      }
      if (AudioWebRuntime.resumePending) {
        // A resume asked for outside a user activation (a touch's pointerdown
        // is not one) stays pending for good; a later gesture that carries
        // activation must ask again, or the phone stays silent behind that
        // promise. A resume that is merely in flight resolves within a frame,
        // so only a long-pending one is asked again.
        var now = typeof performance !== "undefined" && performance.now ? performance.now() : Date.now();
        if (allowFirstUnlock && now - AudioWebRuntime.resumeAskedAt > 250) {
          AudioWebRuntime.resumeAskedAt = now;
          try {
            var again = context.resume();
            if (again && typeof again.then === "function") again.then(null, function() {});
          } catch (error) { /* the first promise reports the failure */ }
        }
        return true;
      }
      if (!allowFirstUnlock && !AudioWebRuntime.everUnlocked) return false;

      AudioWebRuntime.gestureAccepted = true;
      AudioWebRuntime.resumePending = true;
      AudioWebRuntime.resumeAskedAt = typeof performance !== "undefined" && performance.now ? performance.now() : Date.now();
      var resumeSerial = ++AudioWebRuntime.gestureSerial;
      try {
        var result = context.resume();
        if (result && typeof result.then === "function") {
          result.then(function() {
            if (AudioWebRuntime.context !== context ||
                AudioWebRuntime.gestureSerial !== resumeSerial) return;
            AudioWebRuntime.resumePending = false;
            if (context.state === "running") {
              AudioWebRuntime.gestureAccepted = true;
              AudioWebRuntime.everUnlocked = true;
            } else {
              AudioWebRuntime.gestureAccepted = false;
              AudioWebRuntime._releaseVoicesForResume(resumeSerial);
            }
          }, function() {
            if (AudioWebRuntime.context !== context ||
                AudioWebRuntime.gestureSerial !== resumeSerial) return;
            AudioWebRuntime.resumePending = false;
            AudioWebRuntime.gestureAccepted = false;
            AudioWebRuntime._releaseVoicesForResume(resumeSerial);
          });
        } else {
          AudioWebRuntime.resumePending = false;
          AudioWebRuntime.gestureAccepted = context.state === "running";
          if (AudioWebRuntime.gestureAccepted) AudioWebRuntime.everUnlocked = true;
        }
        return true;
      } catch (error) {
        AudioWebRuntime.resumePending = false;
        AudioWebRuntime.gestureAccepted = false;
        AudioWebRuntime._releaseVoicesForResume(resumeSerial);
        return false;
      }
    },

    _decodedPcmSize: function(buffer) {
      if (!buffer) return -1;
      var frames = Number(buffer.length);
      var channels = Number(buffer.numberOfChannels);
      if (!Number.isSafeInteger(frames) || frames <= 0 ||
          !Number.isSafeInteger(channels) || channels <= 0) return -1;
      if (channels > Math.floor(AudioWebRuntime.MAX_CLIP_PCM_BYTES / 4)) return -1;
      var bytesPerFrame = channels * 4;
      if (frames > Math.floor(AudioWebRuntime.MAX_CLIP_PCM_BYTES / bytesPerFrame)) return -1;
      return frames * bytesPerFrame;
    },

    _releaseClip: function(index) {
      var slot = AudioWebRuntime.clips[index];
      if (!slot || !slot.occupied) return;
      var pcmBytes = slot.pcmBytes || 0;
      AudioWebRuntime.decodedPcmBytes = pcmBytes <= AudioWebRuntime.decodedPcmBytes ?
        AudioWebRuntime.decodedPcmBytes - pcmBytes : 0;
      slot.occupied = false;
      slot.state = 2;
      slot.buffer = null;
      slot.stream = false;
      slot.pcmBytes = 0;
      slot.generation = AudioWebRuntime._nextGeneration(slot.generation);
    },

    _releaseVoice: function(index, stopSource) {
      var slot = AudioWebRuntime.voices[index];
      if (!slot || !slot.occupied) return;
      var source = slot.source;
      var gainNode = slot.gainNode;
      var streamSources = slot.streamSources || [];
      slot.occupied = false;
      slot.active = false;
      slot.source = null;
      slot.gainNode = null;
      slot.streamSources = null;
      slot.generation = AudioWebRuntime._nextGeneration(slot.generation);
      slot.streamParked = null;
      for (var i = 0; i < streamSources.length; ++i) {
        streamSources[i].source.onended = null;
        try { streamSources[i].source.stop(); } catch (ignored) {}
        try { streamSources[i].source.disconnect(); } catch (ignored) {}
      }
      if (source) {
        source.onended = null;
        if (stopSource) {
          try { source.stop(); } catch (ignored) {}
        }
        try { source.disconnect(); } catch (ignored) {}
      }
      if (gainNode) {
        try { gainNode.disconnect(); } catch (ignored) {}
      }
    },

    _createContext: function() {
      var AudioContextClass = null;
      if (typeof window !== "undefined") {
        AudioContextClass = window.AudioContext || window.webkitAudioContext || null;
      }
      if (!AudioContextClass && typeof AudioContext !== "undefined") AudioContextClass = AudioContext;
      if (!AudioContextClass) return false;

      var context = null;
      try {
        context = new AudioContextClass();
        var master = context.createGain();
        var music = context.createGain();
        var sfx = context.createGain();
        music.connect(master);
        sfx.connect(master);
        master.connect(context.destination);
        AudioWebRuntime.context = context;
        AudioWebRuntime.masterNode = master;
        AudioWebRuntime.musicNode = music;
        AudioWebRuntime.sfxNode = sfx;
      } catch (error) {
        if (context && typeof context.close === "function") {
          try { context.close(); } catch (ignored) {}
        }
        return false;
      }
      context.onstatechange = function() { AudioWebRuntime._onStateChange(context); };
      context.onerror = function() {
        if (AudioWebRuntime.context === context) AudioWebRuntime._scheduleRebuild();
      };
      return true;
    },

    // Whether the context should be running now: the page wants sound and a
    // gesture has unlocked it once.
    _wantsRunning: function() {
      return AudioWebRuntime.enabled && !AudioWebRuntime.paused && !AudioWebRuntime.hidden &&
        AudioWebRuntime.everUnlocked;
    },

    // The OS or the browser can suspend or interrupt a running context (a
    // call, a device switch) and a context can close or fail; none of that
    // waits for a gesture here, it recovers on its own.
    _onStateChange: function(context) {
      if (context !== AudioWebRuntime.context) return;
      if (context.state === "closed") {
        AudioWebRuntime._scheduleRebuild();
        return;
      }
      if (context.state === "running") {
        AudioWebRuntime._stopRetry();
        if (!AudioWebRuntime._wantsRunning()) {
          AudioWebRuntime._suspendForPolicy();
          return;
        }
        AudioWebRuntime.resumePending = false;
        AudioWebRuntime.gestureAccepted = true;
        return;
      }
      AudioWebRuntime._wake();
    },

    _nowMs: function() {
      return typeof performance !== "undefined" && performance.now ? performance.now() : Date.now();
    },

    _scheduleRebuild: function() {
      if (AudioWebRuntime.rebuildTimer !== null) return;
      var now = AudioWebRuntime._nowMs();
      AudioWebRuntime.rebuildTimes = AudioWebRuntime.rebuildTimes.filter(function(at) {
        return now - at < AudioWebRuntime.REBUILD_WINDOW_MS;
      });
      var recent = AudioWebRuntime.rebuildTimes.length;
      if (recent === 0) {
        AudioWebRuntime._rebuild();
        return;
      }
      if (recent >= AudioWebRuntime.REBUILD_LIMIT || typeof setTimeout !== "function") {
        AudioWebRuntime.rebuildWanted = true;
        return;
      }
      AudioWebRuntime.rebuildTimer = setTimeout(function() {
        AudioWebRuntime.rebuildTimer = null;
        AudioWebRuntime._rebuild();
      }, AudioWebRuntime.REBUILD_BACKOFF_MS * Math.pow(2, recent - 1));
    },

    // A rebuild that failed or hit its limit is tried again when the player
    // acts or the page comes back.
    _retryRebuild: function() {
      if (AudioWebRuntime.rebuildWanted && AudioWebRuntime.rebuildTimer === null) AudioWebRuntime._rebuild();
    },

    _wake: function() {
      AudioWebRuntime._retryRebuild();
      var context = AudioWebRuntime.context;
      if (!context || context.state === "running" || context.state === "closed" ||
          !AudioWebRuntime._wantsRunning()) return;
      try {
        var result = context.resume();
        if (result && typeof result.catch === "function") result.catch(function() {});
      } catch (ignored) {}
      if (AudioWebRuntime.retryTimer === null && typeof setInterval === "function") {
        AudioWebRuntime.retryTimer = setInterval(AudioWebRuntime._wake, AudioWebRuntime.RETRY_MS);
      }
    },

    _stopRetry: function() {
      if (AudioWebRuntime.retryTimer !== null) clearInterval(AudioWebRuntime.retryTimer);
      AudioWebRuntime.retryTimer = null;
    },

    // A closed or failed context is replaced. Effect voices and streams that
    // already ended go with it; a stream voice keeps its unplayed chunks and
    // its place in the track and is scheduled again on the new context. If no
    // context can be made, the stream voices wait parked for the next try.
    _rebuild: function() {
      var old = AudioWebRuntime.context;
      AudioWebRuntime.rebuildTimes.push(AudioWebRuntime._nowMs());
      for (var i = 0; i < AudioWebRuntime.voices.length; ++i) {
        var slot = AudioWebRuntime.voices[i];
        if (!slot.occupied) continue;
        if (!slot.streamSources || slot.streamEnded) {
          AudioWebRuntime._releaseVoice(i, true);
          continue;
        }
        if (!slot.gainNode) continue;
        if (!slot.streamParked) {
          AudioWebRuntime.streamPark(AudioWebRuntime._packHandle(i, slot.generation));
          slot.rebuildResume = true;
        }
        try { slot.gainNode.disconnect(); } catch (ignored) {}
        slot.gainNode = null;
      }
      var nodes = [AudioWebRuntime.musicNode, AudioWebRuntime.sfxNode, AudioWebRuntime.masterNode];
      for (var n = 0; n < nodes.length; ++n) {
        if (nodes[n]) try { nodes[n].disconnect(); } catch (ignored) {}
      }
      if (old) {
        old.onstatechange = null;
        old.onerror = null;
        if (old.state !== "closed") try { old.close(); } catch (ignored) {}
      }
      AudioWebRuntime.context = null;
      AudioWebRuntime.masterNode = null;
      AudioWebRuntime.musicNode = null;
      AudioWebRuntime.sfxNode = null;
      AudioWebRuntime.gestureAccepted = false;
      AudioWebRuntime.resumePending = false;
      if (!AudioWebRuntime._createContext()) {
        AudioWebRuntime.rebuildWanted = true;
        return;
      }
      AudioWebRuntime.rebuildWanted = false;
      AudioWebRuntime.rebuilds += 1;
      for (var v = 0; v < AudioWebRuntime.voices.length; ++v) {
        var voice = AudioWebRuntime.voices[v];
        if (!voice.occupied || !voice.streamSources || voice.gainNode) continue;
        var voiceGain = AudioWebRuntime.context.createGain();
        voiceGain.gain.value = voice.gainValue;
        voiceGain.connect(voice.streamBus === 0 ? AudioWebRuntime.musicNode : AudioWebRuntime.sfxNode);
        voice.gainNode = voiceGain;
        if (voice.rebuildResume) AudioWebRuntime.streamResume(AudioWebRuntime._packHandle(v, voice.generation));
        voice.rebuildResume = false;
      }
      AudioWebRuntime._applyMix();
      AudioWebRuntime._suspendForPolicy();
      AudioWebRuntime._wake();
    },

    // Streams are fed from the game loop; when frames stop coming while the
    // page still plays (an offscreen or throttled frame), a timer feeds them.
    _pump: function() {
      var context = AudioWebRuntime.context;
      if (!context || context.state !== "running") return;
      var now = typeof performance !== "undefined" && performance.now ? performance.now() : Date.now();
      if (now - AudioWebRuntime.lastUpdateMs < AudioWebRuntime.PUMP_STALE_MS) return;
      AudioWebRuntime.pumps = (AudioWebRuntime.pumps || 0) + 1;
      if (typeof _audio_core_web_pump === "function") _audio_core_web_pump();
    },

    update: function() {
      AudioWebRuntime.lastUpdateMs = typeof performance !== "undefined" && performance.now ?
        performance.now() : Date.now();
    },

    init: function() {
      if (AudioWebRuntime.context) AudioWebRuntime.shutdown();
      if (!AudioWebRuntime._createContext()) return false;

      if (AudioWebRuntime.clips.length !== AudioWebRuntime.CLIP_CAPACITY) {
        AudioWebRuntime.clips = AudioWebRuntime._makeSlots(AudioWebRuntime.CLIP_CAPACITY);
      }
      if (AudioWebRuntime.voices.length !== AudioWebRuntime.VOICE_CAPACITY) {
        AudioWebRuntime.voices = AudioWebRuntime._makeSlots(AudioWebRuntime.VOICE_CAPACITY);
      }
      AudioWebRuntime.masterGain = 1;
      AudioWebRuntime.musicGain = 1;
      AudioWebRuntime.sfxGain = 1;
      AudioWebRuntime.enabled = true;
      AudioWebRuntime.paused = false;
      AudioWebRuntime.gestureAccepted = false;
      AudioWebRuntime.everUnlocked = false;
      AudioWebRuntime.resumePending = false;
      AudioWebRuntime.decodedPcmBytes = 0;
      AudioWebRuntime.hidden = typeof document !== "undefined" && !!document.hidden;
      AudioWebRuntime.gestureListener = function() {
        AudioWebRuntime._retryRebuild();
        AudioWebRuntime._applyMix();
        AudioWebRuntime._requestResume(true);
      };
      AudioWebRuntime.visibilityListener = function() {
        AudioWebRuntime.hidden = !!document.hidden;
        if (AudioWebRuntime.hidden) {
          AudioWebRuntime._cancelPendingResume();
          AudioWebRuntime.gestureAccepted = false;
        }
        AudioWebRuntime._applyMix();
        AudioWebRuntime._suspendForPolicy();
        if (!AudioWebRuntime.hidden) AudioWebRuntime._requestResume(false);
      };
      AudioWebRuntime.wakeListener = function() { AudioWebRuntime._wake(); };
      if (typeof window !== "undefined" && window.addEventListener) {
        window.addEventListener("focus", AudioWebRuntime.wakeListener);
        window.addEventListener("pageshow", AudioWebRuntime.wakeListener);
      }
      if (typeof setInterval === "function") {
        AudioWebRuntime.pumpTimer = setInterval(AudioWebRuntime._pump, AudioWebRuntime.PUMP_MS);
      }
      if (typeof document !== "undefined" && document.addEventListener) {
        document.addEventListener("visibilitychange", AudioWebRuntime.visibilityListener);
        // pointerup, touchend, click and keydown carry user activation on
        // every browser; a touch's pointerdown does not, so it is not enough.
        document.addEventListener("pointerdown", AudioWebRuntime.gestureListener, true);
        document.addEventListener("pointerup", AudioWebRuntime.gestureListener, true);
        document.addEventListener("touchend", AudioWebRuntime.gestureListener, true);
        document.addEventListener("click", AudioWebRuntime.gestureListener, true);
        document.addEventListener("keydown", AudioWebRuntime.gestureListener, true);
      }
      AudioWebRuntime._applyMix();
      AudioWebRuntime._suspendForPolicy();
      return true;
    },

    shutdown: function() {
      if (typeof document !== "undefined" && document.removeEventListener &&
          AudioWebRuntime.visibilityListener) {
        document.removeEventListener("visibilitychange", AudioWebRuntime.visibilityListener);
      }
      if (typeof document !== "undefined" && document.removeEventListener &&
          AudioWebRuntime.gestureListener) {
        document.removeEventListener("pointerdown", AudioWebRuntime.gestureListener, true);
        document.removeEventListener("pointerup", AudioWebRuntime.gestureListener, true);
        document.removeEventListener("touchend", AudioWebRuntime.gestureListener, true);
        document.removeEventListener("click", AudioWebRuntime.gestureListener, true);
        document.removeEventListener("keydown", AudioWebRuntime.gestureListener, true);
      }
      if (typeof window !== "undefined" && window.removeEventListener && AudioWebRuntime.wakeListener) {
        window.removeEventListener("focus", AudioWebRuntime.wakeListener);
        window.removeEventListener("pageshow", AudioWebRuntime.wakeListener);
      }
      if (AudioWebRuntime.pumpTimer !== null) clearInterval(AudioWebRuntime.pumpTimer);
      AudioWebRuntime.pumpTimer = null;
      AudioWebRuntime._stopRetry();
      if (AudioWebRuntime.rebuildTimer !== null) clearTimeout(AudioWebRuntime.rebuildTimer);
      AudioWebRuntime.rebuildTimer = null;
      AudioWebRuntime.rebuildWanted = false;
      AudioWebRuntime.rebuildTimes = [];
      AudioWebRuntime.visibilityListener = null;
      AudioWebRuntime.gestureListener = null;
      AudioWebRuntime.wakeListener = null;
      AudioWebRuntime._cancelPendingResume();
      for (var voiceIndex = 0; voiceIndex < AudioWebRuntime.voices.length; ++voiceIndex) {
        AudioWebRuntime._releaseVoice(voiceIndex, true);
      }
      for (var clipIndex = 0; clipIndex < AudioWebRuntime.clips.length; ++clipIndex) {
        AudioWebRuntime._releaseClip(clipIndex);
      }
      AudioWebRuntime.decodedPcmBytes = 0;
      var nodes = [AudioWebRuntime.musicNode, AudioWebRuntime.sfxNode, AudioWebRuntime.masterNode];
      for (var i = 0; i < nodes.length; ++i) {
        if (nodes[i]) {
          try { nodes[i].disconnect(); } catch (ignored) {}
        }
      }
      var context = AudioWebRuntime.context;
      if (context) {
        context.onstatechange = null;
        context.onerror = null;
      }
      AudioWebRuntime.context = null;
      AudioWebRuntime.masterNode = null;
      AudioWebRuntime.musicNode = null;
      AudioWebRuntime.sfxNode = null;
      if (context && context.state !== "closed" && typeof context.close === "function") {
        try {
          var result = context.close();
          if (result && typeof result.catch === "function") result.catch(function() {});
        } catch (ignored) {}
      }
    },

    decodeBegin: function(pointer, size) {
      if (!AudioWebRuntime.context || !pointer && pointer !== 0 || size <= 0) return 0;
      var index = -1;
      for (var i = 0; i < AudioWebRuntime.clips.length; ++i) {
        if (!AudioWebRuntime.clips[i].occupied) { index = i; break; }
      }
      if (index < 0) return 0;

      var ownedBytes;
      try {
        ownedBytes = HEAPU8.slice(pointer, pointer + size);
      } catch (error) {
        return 0;
      }
      var slot = AudioWebRuntime.clips[index];
      slot.occupied = true;
      slot.state = 0;
      slot.buffer = null;
      slot.stream = false;
      slot.pcmBytes = 0;
      var handle = AudioWebRuntime._packHandle(index, slot.generation);
      try {
        AudioWebRuntime.context.decodeAudioData(ownedBytes.buffer).then(function(buffer) {
          var current = AudioWebRuntime._clip(handle);
          if (!current) return;
          var pcmBytes = AudioWebRuntime._decodedPcmSize(buffer);
          if (pcmBytes < 0 || pcmBytes > AudioWebRuntime.MAX_TOTAL_PCM_BYTES -
              AudioWebRuntime.decodedPcmBytes) {
            current.slot.buffer = null;
            current.slot.pcmBytes = 0;
            current.slot.state = 2;
            return;
          }
          current.slot.buffer = buffer;
          current.slot.pcmBytes = pcmBytes;
          AudioWebRuntime.decodedPcmBytes += pcmBytes;
          current.slot.state = 1;
        }, function() {
          var current = AudioWebRuntime._clip(handle);
          if (!current) return;
          current.slot.buffer = null;
          current.slot.pcmBytes = 0;
          current.slot.state = 2;
        });
      } catch (error) {
        slot.state = 2;
      }
      return handle;
    },

    // A streamed clip holds no PCM here: the C side owns its encoded bytes and
    // pushes each voice's decoded chunks through streamPush.
    streamOpen: function(ready) {
      if (!AudioWebRuntime.context) return 0;
      for (var i = 0; i < AudioWebRuntime.clips.length; ++i) {
        var slot = AudioWebRuntime.clips[i];
        if (slot.occupied) continue;
        slot.occupied = true;
        slot.state = ready ? 1 : 2;
        slot.buffer = null;
        slot.stream = true;
        slot.pcmBytes = 0;
        return AudioWebRuntime._packHandle(i, slot.generation);
      }
      return 0;
    },

    now: function() {
      return AudioWebRuntime.context ? AudioWebRuntime.context.currentTime : 0;
    },

    _streamSlot: function(handle) {
      var entry = AudioWebRuntime._voice(handle);
      return entry && entry.slot.streamSources ? entry.slot : null;
    },

    // A context rebuild restarts the clock; the C side rebases its times on it.
    contextEpoch: function() { return AudioWebRuntime.rebuilds; },

    // Track frames scheduled past the clock; -1 for a voice that is gone or
    // cannot be fed now (no context).
    streamBufferedFrames: function(handle) {
      var slot = AudioWebRuntime._streamSlot(handle);
      if (!slot || !AudioWebRuntime.context || !slot.gainNode) return -1;
      if (slot.streamOrigin === null) return 0;
      var ahead = slot.streamOrigin + slot.streamFrames / slot.streamRate - AudioWebRuntime.context.currentTime;
      return ahead > 0 ? Math.floor(ahead * slot.streamRate) : 0;
    },

    // How far past the clock a stream's first chunk, or one after a stall,
    // starts: past what the output path already holds, or its head is cut.
    _streamLeadSeconds: function(context) {
      var latency = (Number(context.baseLatency) || 0) + (Number(context.outputLatency) || 0);
      return Math.max(0.05, 2 * latency);
    },

    _streamSchedule: function(handle, slot, buffer, start, skip, frames, when) {
      var source = AudioWebRuntime.context.createBufferSource();
      source.buffer = buffer;
      source.connect(slot.gainNode);
      // The next chunk renders from the first context frame at or after this
      // chunk's end, so this one ends half a frame before that frame: no
      // frame twice and none dropped at any rate pair, and no float noise on
      // a join that falls exactly on a frame.
      var rate = AudioWebRuntime.context.sampleRate;
      var end = (slot.streamOrigin + (start + frames) / slot.streamRate) * rate;
      var duration = (Math.ceil(end - 1e-6) - 0.5) / rate - when;
      source.start(when, skip / slot.streamRate, duration);
      var chunk = { source: source, buffer: buffer, start: start, frames: frames };
      slot.streamSources.push(chunk);
      source.onended = function() {
        var current = AudioWebRuntime._voice(handle);
        if (!current || !current.slot.streamSources) return;
        var chunks = current.slot.streamSources;
        var at = chunks.indexOf(chunk);
        if (at >= 0) chunks.splice(at, 1);
        try { source.disconnect(); } catch (ignored) {}
        if (current.slot.streamEnded && chunks.length === 0) AudioWebRuntime._releaseVoice(current.index, false);
      };
    },

    // A chunk is `frames` frames at the track's own rate, planar, each channel
    // followed by one guard frame (the next chunk's first) that only feeds the
    // browser's interpolation at the join. Chunks play back to back on the
    // track's own timeline, origin + frames / rate, so rounding never adds up;
    // a chunk that finds the clock past the queue (a stall longer than the
    // lookahead) moves the timeline a little ahead of the clock instead.
    // Returns 0 when the chunk could not be scheduled.
    streamPush: function(handle, pointer, frames, channels, rate) {
      var slot = AudioWebRuntime._streamSlot(handle);
      var context = AudioWebRuntime.context;
      if (!slot || !context || !slot.gainNode || slot.streamParked || frames <= 0 || channels <= 0 ||
          rate <= 0) return 0;
      if (slot.streamRate !== rate) {
        if (slot.streamFrames !== 0) return 0;
        slot.streamRate = rate;
      }
      var when = slot.streamOrigin === null ? -1 : slot.streamOrigin + slot.streamFrames / rate;
      if (when < context.currentTime) {
        when = context.currentTime + AudioWebRuntime._streamLeadSeconds(context);
        slot.streamOrigin = when - slot.streamFrames / rate;
      }
      try {
        var buffer = context.createBuffer(channels, frames + 1, rate);
        var base = pointer >> 2;
        for (var c = 0; c < channels; ++c) {
          buffer.getChannelData(c).set(HEAPF32.subarray(base + c * (frames + 1), base + (c + 1) * (frames + 1)));
        }
        AudioWebRuntime._streamSchedule(handle, slot, buffer, slot.streamFrames, 0, frames, when);
      } catch (error) {
        return 0;
      }
      slot.streamFrames += frames;
      return 1;
    },

    streamEnd: function(handle) {
      var entry = AudioWebRuntime._voice(handle);
      if (!entry || !entry.slot.streamSources) return;
      entry.slot.streamEnded = true;
      if (entry.slot.streamSources.length === 0) AudioWebRuntime._releaseVoice(entry.index, false);
    },

    // A parked voice keeps the chunks it had not played yet and the track
    // frame it stopped on, so a resume picks up on that sample.
    streamPark: function(handle) {
      var slot = AudioWebRuntime._streamSlot(handle);
      if (!slot || slot.streamParked) return;
      var played = 0;
      if (slot.streamOrigin !== null && AudioWebRuntime.context) {
        played = Math.round((AudioWebRuntime.context.currentTime - slot.streamOrigin) * slot.streamRate);
        played = Math.max(0, Math.min(slot.streamFrames, played));
      }
      var kept = [];
      for (var i = 0; i < slot.streamSources.length; ++i) {
        var chunk = slot.streamSources[i];
        chunk.source.onended = null;
        try { chunk.source.stop(); } catch (ignored) {}
        try { chunk.source.disconnect(); } catch (ignored) {}
        if (chunk.start + chunk.frames > played) kept.push(chunk);
      }
      slot.streamSources = [];
      slot.streamParked = { played: played, chunks: kept };
    },

    _fadeIn: function(buffer, from, seconds) {
      var frames = Math.min(buffer.length - from, Math.round(seconds * buffer.sampleRate));
      for (var c = 0; c < buffer.numberOfChannels; ++c) {
        var data = buffer.getChannelData(c);
        for (var i = 0; i < frames; ++i) data[from + i] *= i / frames;
      }
    },

    streamResume: function(handle) {
      var slot = AudioWebRuntime._streamSlot(handle);
      var context = AudioWebRuntime.context;
      if (!slot || !slot.streamParked || !context || !slot.gainNode) {
        if (slot && slot.streamParked) slot.rebuildResume = true;
        return;
      }
      var parked = slot.streamParked;
      slot.streamParked = null;
      if (slot.streamOrigin === null) return;
      if (parked.chunks.length > 0) {
        var first = parked.chunks[0];
        AudioWebRuntime._fadeIn(first.buffer, Math.max(0, parked.played - first.start),
          AudioWebRuntime.RESUME_FADE_SECONDS);
      }
      slot.streamOrigin = context.currentTime + AudioWebRuntime._streamLeadSeconds(context) -
        parked.played / slot.streamRate;
      for (var i = 0; i < parked.chunks.length; ++i) {
        var chunk = parked.chunks[i];
        var skip = Math.max(0, parked.played - chunk.start);
        try {
          AudioWebRuntime._streamSchedule(handle, slot, chunk.buffer, chunk.start, skip, chunk.frames,
            slot.streamOrigin + (chunk.start + skip) / slot.streamRate);
        } catch (ignored) {}
      }
    },

    decodeState: function(handle) {
      var entry = AudioWebRuntime._clip(handle);
      return entry ? entry.slot.state : 2;
    },

    clipDestroy: function(handle) {
      var entry = AudioWebRuntime._clip(handle);
      if (entry) AudioWebRuntime._releaseClip(entry.index);
    },

    voicePlay: function(clipHandle, bus, gain, loop) {
      if (!AudioWebRuntime.context || !AudioWebRuntime.enabled || AudioWebRuntime.paused ||
          AudioWebRuntime.hidden || !AudioWebRuntime.gestureAccepted) return 0;
      var clip = AudioWebRuntime._clip(clipHandle);
      if (!clip || clip.slot.state !== 1 || (!clip.slot.buffer && !clip.slot.stream) ||
          (bus !== 0 && bus !== 1)) return 0;
      var index = -1;
      for (var i = 0; i < AudioWebRuntime.voices.length; ++i) {
        if (!AudioWebRuntime.voices[i].occupied) { index = i; break; }
      }
      if (index < 0) return 0;
      if (clip.slot.stream) return AudioWebRuntime._streamVoicePlay(index, bus, gain);

      var source;
      var voiceGain;
      try {
        source = AudioWebRuntime.context.createBufferSource();
        voiceGain = AudioWebRuntime.context.createGain();
        source.buffer = clip.slot.buffer;
        source.loop = !!loop;
        voiceGain.gain.value = AudioWebRuntime._finiteGain(gain);
        source.connect(voiceGain);
        voiceGain.connect(bus === 0 ? AudioWebRuntime.musicNode : AudioWebRuntime.sfxNode);
      } catch (error) {
        if (source) try { source.disconnect(); } catch (ignored) {}
        if (voiceGain) try { voiceGain.disconnect(); } catch (ignored) {}
        return 0;
      }

      var slot = AudioWebRuntime.voices[index];
      slot.occupied = true;
      slot.active = true;
      slot.source = source;
      slot.gainNode = voiceGain;
      slot.resumeSerial = AudioWebRuntime.resumePending ? AudioWebRuntime.gestureSerial : 0;
      var handle = AudioWebRuntime._packHandle(index, slot.generation);
      source.onended = function() {
        var current = AudioWebRuntime._voice(handle);
        if (current) AudioWebRuntime._releaseVoice(current.index, false);
      };
      try {
        source.start(0);
      } catch (error) {
        AudioWebRuntime._releaseVoice(index, false);
        return 0;
      }
      return handle;
    },

    _streamVoicePlay: function(index, bus, gain) {
      var voiceGain;
      try {
        voiceGain = AudioWebRuntime.context.createGain();
        voiceGain.gain.value = AudioWebRuntime._finiteGain(gain);
        voiceGain.connect(bus === 0 ? AudioWebRuntime.musicNode : AudioWebRuntime.sfxNode);
      } catch (error) {
        if (voiceGain) try { voiceGain.disconnect(); } catch (ignored) {}
        return 0;
      }
      var slot = AudioWebRuntime.voices[index];
      slot.occupied = true;
      slot.active = true;
      slot.source = null;
      slot.gainNode = voiceGain;
      slot.gainValue = voiceGain.gain.value;
      slot.rebuildResume = false;
      slot.streamBus = bus;
      slot.streamSources = [];
      slot.streamOrigin = null;
      slot.streamRate = 0;
      slot.streamFrames = 0;
      slot.streamParked = null;
      slot.streamEnded = false;
      slot.resumeSerial = AudioWebRuntime.resumePending ? AudioWebRuntime.gestureSerial : 0;
      return AudioWebRuntime._packHandle(index, slot.generation);
    },

    voiceActive: function(handle) {
      var entry = AudioWebRuntime._voice(handle);
      return !!(entry && entry.slot.active);
    },

    voiceStop: function(handle) {
      var entry = AudioWebRuntime._voice(handle);
      if (entry) AudioWebRuntime._releaseVoice(entry.index, true);
    },

    voiceSetGain: function(handle, gain) {
      var entry = AudioWebRuntime._voice(handle);
      if (entry) entry.slot.gainValue = AudioWebRuntime._finiteGain(gain);
      if (!entry || !entry.slot.gainNode) return;
      try { entry.slot.gainNode.gain.value = AudioWebRuntime._finiteGain(gain); } catch (ignored) {}
    },

    voiceSetPitch: function(handle, pitch) {
      var entry = AudioWebRuntime._voice(handle);
      if (!entry || !entry.slot.source || !(pitch > 0) || !isFinite(pitch)) return;
      try { entry.slot.source.playbackRate.value = pitch; } catch (ignored) {}
    },

    setMix: function(master, music, sfx) {
      AudioWebRuntime.masterGain = AudioWebRuntime._finiteGain(master);
      AudioWebRuntime.musicGain = AudioWebRuntime._finiteGain(music);
      AudioWebRuntime.sfxGain = AudioWebRuntime._finiteGain(sfx);
      AudioWebRuntime._applyMix();
    },

    setEnabled: function(enabled) {
      AudioWebRuntime.enabled = !!enabled;
      if (!AudioWebRuntime.enabled) {
        AudioWebRuntime._cancelPendingResume();
        AudioWebRuntime.gestureAccepted = false;
      }
      AudioWebRuntime._applyMix();
      AudioWebRuntime._suspendForPolicy();
      if (AudioWebRuntime.enabled) AudioWebRuntime._requestResume(false);
    },

    setPaused: function(paused) {
      AudioWebRuntime.paused = !!paused;
      if (AudioWebRuntime.paused) {
        AudioWebRuntime._cancelPendingResume();
        AudioWebRuntime.gestureAccepted = false;
      }
      AudioWebRuntime._applyMix();
      AudioWebRuntime._suspendForPolicy();
      if (!AudioWebRuntime.paused) AudioWebRuntime._requestResume(false);
    },

    userGesture: function() {
      AudioWebRuntime._retryRebuild();
      AudioWebRuntime._applyMix();
      return AudioWebRuntime._requestResume(true);
    }
  },

  audio_web_init__deps: ["$AudioWebRuntime", "audio_core_web_pump"],
  audio_web_init: function() { return AudioWebRuntime.init() ? 1 : 0; },
  audio_web_shutdown__deps: ["$AudioWebRuntime"],
  audio_web_shutdown: function() { AudioWebRuntime.shutdown(); },
  audio_web_update__deps: ["$AudioWebRuntime"],
  audio_web_update: function() { AudioWebRuntime.update(); },
  audio_web_decode_begin__deps: ["$AudioWebRuntime"],
  audio_web_decode_begin: function(pointer, size) { return AudioWebRuntime.decodeBegin(pointer, size); },
  audio_web_decode_state__deps: ["$AudioWebRuntime"],
  audio_web_decode_state: function(handle) { return AudioWebRuntime.decodeState(handle); },
  audio_web_clip_destroy__deps: ["$AudioWebRuntime"],
  audio_web_clip_destroy: function(handle) { AudioWebRuntime.clipDestroy(handle); },
  audio_web_voice_play__deps: ["$AudioWebRuntime"],
  audio_web_voice_play: function(clip, bus, gain, loop) {
    return AudioWebRuntime.voicePlay(clip, bus, gain, loop);
  },
  audio_web_voice_active__deps: ["$AudioWebRuntime"],
  audio_web_voice_active: function(handle) { return AudioWebRuntime.voiceActive(handle) ? 1 : 0; },
  audio_web_voice_stop__deps: ["$AudioWebRuntime"],
  audio_web_voice_stop: function(handle) { AudioWebRuntime.voiceStop(handle); },
  audio_web_voice_set_gain__deps: ["$AudioWebRuntime"],
  audio_web_voice_set_gain: function(handle, gain) { AudioWebRuntime.voiceSetGain(handle, gain); },
  audio_web_voice_set_pitch__deps: ["$AudioWebRuntime"],
  audio_web_voice_set_pitch: function(handle, pitch) { AudioWebRuntime.voiceSetPitch(handle, pitch); },
  audio_web_set_mix__deps: ["$AudioWebRuntime"],
  audio_web_set_mix: function(master, music, sfx) { AudioWebRuntime.setMix(master, music, sfx); },
  audio_web_set_enabled__deps: ["$AudioWebRuntime"],
  audio_web_set_enabled: function(enabled) { AudioWebRuntime.setEnabled(enabled); },
  audio_web_set_paused__deps: ["$AudioWebRuntime"],
  audio_web_set_paused: function(paused) { AudioWebRuntime.setPaused(paused); },
  audio_web_user_gesture__deps: ["$AudioWebRuntime"],
  audio_web_user_gesture: function() { return AudioWebRuntime.userGesture() ? 1 : 0; },
  audio_web_stream_open__deps: ["$AudioWebRuntime"],
  audio_web_stream_open: function(ready) { return AudioWebRuntime.streamOpen(ready); },
  audio_web_now__deps: ["$AudioWebRuntime"],
  audio_web_now: function() { return AudioWebRuntime.now(); },
  audio_web_stream_buffered_frames__deps: ["$AudioWebRuntime"],
  audio_web_stream_buffered_frames: function(handle) { return AudioWebRuntime.streamBufferedFrames(handle); },
  audio_web_stream_push__deps: ["$AudioWebRuntime"],
  audio_web_stream_push: function(handle, pointer, frames, channels, rate) {
    return AudioWebRuntime.streamPush(handle, pointer, frames, channels, rate);
  },
  audio_web_stream_end__deps: ["$AudioWebRuntime"],
  audio_web_stream_end: function(handle) { AudioWebRuntime.streamEnd(handle); },
  audio_web_stream_park__deps: ["$AudioWebRuntime"],
  audio_web_stream_park: function(handle) { AudioWebRuntime.streamPark(handle); },
  audio_web_context_epoch__deps: ["$AudioWebRuntime"],
  audio_web_context_epoch: function() { return AudioWebRuntime.contextEpoch(); },
  audio_web_stream_resume__deps: ["$AudioWebRuntime"],
  audio_web_stream_resume: function(handle) { AudioWebRuntime.streamResume(handle); },
  audio_web_is_unlocked__deps: ["$AudioWebRuntime"],
  audio_web_is_unlocked: function() {
    return AudioWebRuntime.context && AudioWebRuntime.context.state === "running" &&
      AudioWebRuntime.everUnlocked && AudioWebRuntime.gestureAccepted ? 1 : 0;
  }
});
