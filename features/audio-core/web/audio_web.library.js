mergeInto(LibraryManager.library, {
  $AudioWebRuntime: {
    CLIP_CAPACITY: 64,
    VOICE_CAPACITY: 32,
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
    visibilityListener: null,

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

    _releaseClip: function(index) {
      var slot = AudioWebRuntime.clips[index];
      if (!slot || !slot.occupied) return;
      slot.occupied = false;
      slot.state = 2;
      slot.buffer = null;
      slot.generation = AudioWebRuntime._nextGeneration(slot.generation);
    },

    _releaseVoice: function(index, stopSource) {
      var slot = AudioWebRuntime.voices[index];
      if (!slot || !slot.occupied) return;
      var source = slot.source;
      var gainNode = slot.gainNode;
      slot.occupied = false;
      slot.active = false;
      slot.source = null;
      slot.gainNode = null;
      slot.generation = AudioWebRuntime._nextGeneration(slot.generation);
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

    init: function() {
      if (AudioWebRuntime.context) AudioWebRuntime.shutdown();
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
      AudioWebRuntime.hidden = typeof document !== "undefined" && !!document.hidden;
      AudioWebRuntime.visibilityListener = function() {
        AudioWebRuntime.hidden = !!document.hidden;
        if (AudioWebRuntime.hidden) {
          AudioWebRuntime.gestureAccepted = false;
          AudioWebRuntime.gestureSerial += 1;
        }
        AudioWebRuntime._applyMix();
        AudioWebRuntime._suspendForPolicy();
      };
      if (typeof document !== "undefined" && document.addEventListener) {
        document.addEventListener("visibilitychange", AudioWebRuntime.visibilityListener);
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
      AudioWebRuntime.visibilityListener = null;
      for (var voiceIndex = 0; voiceIndex < AudioWebRuntime.voices.length; ++voiceIndex) {
        AudioWebRuntime._releaseVoice(voiceIndex, true);
      }
      for (var clipIndex = 0; clipIndex < AudioWebRuntime.clips.length; ++clipIndex) {
        AudioWebRuntime._releaseClip(clipIndex);
      }
      var nodes = [AudioWebRuntime.musicNode, AudioWebRuntime.sfxNode, AudioWebRuntime.masterNode];
      for (var i = 0; i < nodes.length; ++i) {
        if (nodes[i]) {
          try { nodes[i].disconnect(); } catch (ignored) {}
        }
      }
      var context = AudioWebRuntime.context;
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
      var handle = AudioWebRuntime._packHandle(index, slot.generation);
      try {
        AudioWebRuntime.context.decodeAudioData(ownedBytes.buffer).then(function(buffer) {
          var current = AudioWebRuntime._clip(handle);
          if (!current) return;
          current.slot.buffer = buffer;
          current.slot.state = 1;
        }, function() {
          var current = AudioWebRuntime._clip(handle);
          if (!current) return;
          current.slot.buffer = null;
          current.slot.state = 2;
        });
      } catch (error) {
        slot.state = 2;
      }
      return handle;
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
      if (!clip || clip.slot.state !== 1 || !clip.slot.buffer || (bus !== 0 && bus !== 1)) return 0;
      var index = -1;
      for (var i = 0; i < AudioWebRuntime.voices.length; ++i) {
        if (!AudioWebRuntime.voices[i].occupied) { index = i; break; }
      }
      if (index < 0) return 0;

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

    voiceActive: function(handle) {
      var entry = AudioWebRuntime._voice(handle);
      return !!(entry && entry.slot.active);
    },

    voiceStop: function(handle) {
      var entry = AudioWebRuntime._voice(handle);
      if (entry) AudioWebRuntime._releaseVoice(entry.index, true);
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
        AudioWebRuntime.gestureAccepted = false;
        AudioWebRuntime.gestureSerial += 1;
      }
      AudioWebRuntime._applyMix();
      AudioWebRuntime._suspendForPolicy();
    },

    setPaused: function(paused) {
      AudioWebRuntime.paused = !!paused;
      if (AudioWebRuntime.paused) {
        AudioWebRuntime.gestureAccepted = false;
        AudioWebRuntime.gestureSerial += 1;
      }
      AudioWebRuntime._applyMix();
      AudioWebRuntime._suspendForPolicy();
    },

    userGesture: function() {
      var context = AudioWebRuntime.context;
      if (!context || !AudioWebRuntime.enabled || AudioWebRuntime.paused ||
          AudioWebRuntime.hidden || context.state === "closed") return false;
      AudioWebRuntime._applyMix();
      AudioWebRuntime.gestureAccepted = true;
      var gestureSerial = ++AudioWebRuntime.gestureSerial;
      if (context.state === "running") return true;
      try {
        var result = context.resume();
        if (result && typeof result.catch === "function") result.catch(function() {
          if (AudioWebRuntime.context !== context || AudioWebRuntime.gestureSerial !== gestureSerial) return;
          AudioWebRuntime.gestureAccepted = false;
          for (var i = 0; i < AudioWebRuntime.voices.length; ++i) {
            AudioWebRuntime._releaseVoice(i, true);
          }
        });
        return true;
      } catch (error) {
        AudioWebRuntime.gestureAccepted = false;
        return false;
      }
    }
  },

  audio_web_init__deps: ["$AudioWebRuntime"],
  audio_web_init: function() { return AudioWebRuntime.init() ? 1 : 0; },
  audio_web_shutdown__deps: ["$AudioWebRuntime"],
  audio_web_shutdown: function() { AudioWebRuntime.shutdown(); },
  audio_web_update__deps: ["$AudioWebRuntime"],
  audio_web_update: function() {},
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
  audio_web_set_mix__deps: ["$AudioWebRuntime"],
  audio_web_set_mix: function(master, music, sfx) { AudioWebRuntime.setMix(master, music, sfx); },
  audio_web_set_enabled__deps: ["$AudioWebRuntime"],
  audio_web_set_enabled: function(enabled) { AudioWebRuntime.setEnabled(enabled); },
  audio_web_set_paused__deps: ["$AudioWebRuntime"],
  audio_web_set_paused: function(paused) { AudioWebRuntime.setPaused(paused); },
  audio_web_user_gesture__deps: ["$AudioWebRuntime"],
  audio_web_user_gesture: function() { return AudioWebRuntime.userGesture() ? 1 : 0; },
  audio_web_is_unlocked__deps: ["$AudioWebRuntime"],
  audio_web_is_unlocked: function() {
    return AudioWebRuntime.context && AudioWebRuntime.context.state !== "closed" &&
      AudioWebRuntime.gestureAccepted ? 1 : 0;
  }
});
