import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import vm from "node:vm";

const LIBRARY_PATH = new URL("../web/audio_web.library.js", import.meta.url);

class FakeAudioParam {
  constructor() { this.value = 1; }
}

class FakeNode {
  constructor() {
    this.connections = [];
    this.disconnected = false;
  }

  connect(node) {
    this.connections.push(node);
    return node;
  }

  disconnect() { this.disconnected = true; }
}

class FakeGainNode extends FakeNode {
  constructor() {
    super();
    this.gain = new FakeAudioParam();
  }
}

class FakeBufferSourceNode extends FakeNode {
  constructor() {
    super();
    this.buffer = null;
    this.loop = false;
    this.onended = null;
    this.started = false;
    this.stopped = false;
    this.playbackRate = { value: 1 };
  }

  start(when = 0, offset = 0, duration = undefined) {
    this.started = true;
    this.when = when;
    this.offset = offset;
    this.duration = duration;
  }

  stop() {
    this.stopped = true;
    const ended = this.onended;
    if (ended) ended();
  }

  finish() {
    const ended = this.onended;
    if (ended) ended();
  }
}

class FakeAudioBuffer {
  constructor(channels, length, sampleRate) {
    this.numberOfChannels = channels;
    this.length = length;
    this.sampleRate = sampleRate;
    this.channels = Array.from({ length: channels }, () => new Float32Array(length));
  }

  getChannelData(channel) { return this.channels[channel]; }
}

class FakeAudioContext {
  constructor() {
    this.state = "suspended";
    this.sampleRate = 48000;
    this.currentTime = 0;
    this.destination = new FakeNode();
    this.gains = [];
    this.sources = [];
    this.pendingDecodes = [];
    this.resumeCalls = 0;
    this.rejectResume = false;
    this.holdResume = false; // a resume asked for without user activation never settles
    this.suspendCalls = 0;
    this.closeCalls = 0;
  }

  createGain() {
    const node = new FakeGainNode();
    this.gains.push(node);
    return node;
  }

  createBuffer(channels, length, sampleRate) { return new FakeAudioBuffer(channels, length, sampleRate); }

  createBufferSource() {
    const source = new FakeBufferSourceNode();
    this.sources.push(source);
    return source;
  }

  decodeAudioData(buffer) {
    const bytes = Array.from(new Uint8Array(buffer));
    return new Promise((resolve, reject) => {
      this.pendingDecodes.push({ bytes, resolve, reject });
    });
  }

  resume() {
    this.resumeCalls += 1;
    if (this.rejectResume) return Promise.reject(new Error("gesture rejected"));
    if (this.holdResume) return new Promise(() => {});
    return Promise.resolve().then(() => { this.state = "running"; });
  }

  suspend() {
    this.suspendCalls += 1;
    this.state = "suspended";
    return Promise.resolve();
  }

  close() {
    this.closeCalls += 1;
    this.state = "closed";
    return Promise.resolve();
  }
}

class FakeDocument {
  constructor() {
    this.hidden = false;
    this.listeners = new Map();
  }

  addEventListener(type, listener, options) { this.listeners.set(type, { listener, options }); }
  removeEventListener(type, listener, options) {
    const entry = this.listeners.get(type);
    if (entry && entry.listener === listener && entry.options === options) this.listeners.delete(type);
  }
  dispatch(type) {
    const entry = this.listeners.get(type);
    if (entry) entry.listener();
  }
}

async function flushPromises() {
  await Promise.resolve();
  await Promise.resolve();
}

function decodedBuffer(length = 1, numberOfChannels = 1, name = "decoded") {
  return { length, numberOfChannels, name };
}

async function loadLibrary() {
  const source = await readFile(LIBRARY_PATH, "utf8");
  const library = {};
  const document = new FakeDocument();
  const context = vm.createContext({
    ArrayBuffer,
    AudioContext: FakeAudioContext,
    HEAPU8: new Uint8Array(1024),
    HEAPF32: new Float32Array(4096),
    LibraryManager: { library },
    Uint8Array,
    console,
    document,
    mergeInto(target, additions) { Object.assign(target, additions); },
    window: { AudioContext: FakeAudioContext },
  });
  vm.runInContext(source, context, { filename: LIBRARY_PATH.pathname });
  context.AudioWebRuntime = library.$AudioWebRuntime;
  return { context, document, library };
}

test("init creates music and sfx buses under master without resuming", async () => {
  const { library } = await loadLibrary();

  assert.equal(library.audio_web_init(), 1);
  const runtime = library.$AudioWebRuntime;
  assert.equal(runtime.context.resumeCalls, 0);
  assert.deepEqual(runtime.musicNode.connections, [runtime.masterNode]);
  assert.deepEqual(runtime.sfxNode.connections, [runtime.masterNode]);
  assert.deepEqual(runtime.masterNode.connections, [runtime.context.destination]);
});

test("decode_begin immediately owns HEAPU8 bytes and exposes polled state", async () => {
  const { context, library } = await loadLibrary();
  library.audio_web_init();
  context.HEAPU8.set([10, 20, 30, 40], 100);

  const clip = library.audio_web_decode_begin(100, 4);
  assert.notEqual(clip, 0);
  assert.equal(library.audio_web_decode_state(clip), 0);
  context.HEAPU8.fill(99, 100, 104);
  assert.deepEqual(library.$AudioWebRuntime.context.pendingDecodes[0].bytes, [10, 20, 30, 40]);

  const decoded = decodedBuffer();
  library.$AudioWebRuntime.context.pendingDecodes[0].resolve(decoded);
  await flushPromises();
  assert.equal(library.audio_web_decode_state(clip), 1);
});

test("destroying a loading clip invalidates late Promise completion", async () => {
  const { context, library } = await loadLibrary();
  library.audio_web_init();
  context.HEAPU8.set([1, 2, 3], 10);

  const stale = library.audio_web_decode_begin(10, 3);
  library.audio_web_clip_destroy(stale);
  const replacement = library.audio_web_decode_begin(10, 3);
  assert.notEqual(replacement, stale);

  library.$AudioWebRuntime.context.pendingDecodes[0].resolve(
    decodedBuffer((128 * 1024 * 1024) / 4, 1, "late"),
  );
  await flushPromises();
  assert.equal(library.audio_web_decode_state(stale), 2);
  assert.equal(library.audio_web_decode_state(replacement), 0);
  assert.equal(library.$AudioWebRuntime.decodedPcmBytes, 0);

  library.$AudioWebRuntime.context.pendingDecodes[1].resolve(decodedBuffer(1, 1, "current"));
  await flushPromises();
  assert.equal(library.audio_web_decode_state(replacement), 1);
  assert.equal(library.$AudioWebRuntime.decodedPcmBytes, 4);
  library.audio_web_clip_destroy(replacement);
  assert.equal(library.$AudioWebRuntime.decodedPcmBytes, 0);
});

test("late decode from a previous init cannot complete a reused slot", async () => {
  const { context, library } = await loadLibrary();
  library.audio_web_init();
  context.HEAPU8.set([7], 0);
  const oldContext = library.$AudioWebRuntime.context;
  const stale = library.audio_web_decode_begin(0, 1);

  library.audio_web_shutdown();
  library.audio_web_init();
  const currentContext = library.$AudioWebRuntime.context;
  const replacement = library.audio_web_decode_begin(0, 1);
  assert.notEqual(replacement, stale);

  oldContext.pendingDecodes[0].resolve(
    decodedBuffer((128 * 1024 * 1024) / 4, 1, "stale"),
  );
  await flushPromises();
  assert.equal(library.audio_web_decode_state(replacement), 0);
  assert.equal(library.$AudioWebRuntime.decodedPcmBytes, 0);
  currentContext.pendingDecodes[0].resolve(decodedBuffer(1, 1, "current"));
  await flushPromises();
  assert.equal(library.audio_web_decode_state(replacement), 1);
  assert.equal(library.$AudioWebRuntime.decodedPcmBytes, 4);
});

test("decoded PCM enforces a 128 MiB per-clip limit with overflow-safe math", async () => {
  const { context, library } = await loadLibrary();
  library.audio_web_init();
  context.HEAPU8[0] = 1;
  const atLimit = library.audio_web_decode_begin(0, 1);
  const overLimit = library.audio_web_decode_begin(0, 1);
  const unsafe = library.audio_web_decode_begin(0, 1);
  const framesAtLimit = (128 * 1024 * 1024) / 4;

  const pending = library.$AudioWebRuntime.context.pendingDecodes;
  pending[0].resolve(decodedBuffer(framesAtLimit, 1));
  pending[1].resolve(decodedBuffer(framesAtLimit + 1, 1));
  pending[2].resolve(decodedBuffer(Number.MAX_SAFE_INTEGER, 2));
  await flushPromises();

  assert.equal(library.audio_web_decode_state(atLimit), 1);
  assert.equal(library.audio_web_decode_state(overLimit), 2);
  assert.equal(library.audio_web_decode_state(unsafe), 2);
  assert.equal(library.$AudioWebRuntime.decodedPcmBytes, 128 * 1024 * 1024);
});

test("decoded PCM enforces a 256 MiB aggregate limit and destroy frees budget", async () => {
  const { context, library } = await loadLibrary();
  library.audio_web_init();
  context.HEAPU8[0] = 1;
  const first = library.audio_web_decode_begin(0, 1);
  const second = library.audio_web_decode_begin(0, 1);
  const excess = library.audio_web_decode_begin(0, 1);
  const frames128MiB = (128 * 1024 * 1024) / 4;
  const pending = library.$AudioWebRuntime.context.pendingDecodes;
  pending[0].resolve(decodedBuffer(frames128MiB, 1));
  pending[1].resolve(decodedBuffer(frames128MiB, 1));
  pending[2].resolve(decodedBuffer(1, 1));
  await flushPromises();

  assert.equal(library.audio_web_decode_state(first), 1);
  assert.equal(library.audio_web_decode_state(second), 1);
  assert.equal(library.audio_web_decode_state(excess), 2);
  assert.equal(library.$AudioWebRuntime.decodedPcmBytes, 256 * 1024 * 1024);

  library.audio_web_clip_destroy(first);
  assert.equal(library.$AudioWebRuntime.decodedPcmBytes, 128 * 1024 * 1024);
  const afterFree = library.audio_web_decode_begin(0, 1);
  library.$AudioWebRuntime.context.pendingDecodes.at(-1).resolve(decodedBuffer(1, 1));
  await flushPromises();
  assert.equal(library.audio_web_decode_state(afterFree), 1);
  assert.equal(library.$AudioWebRuntime.decodedPcmBytes, 128 * 1024 * 1024 + 4);

  library.audio_web_shutdown();
  assert.equal(library.$AudioWebRuntime.decodedPcmBytes, 0);
});

test("play creates one source and gain per voice and ended state is polled", async () => {
  const { context, document, library } = await loadLibrary();
  library.audio_web_init();
  context.HEAPU8.set([1], 0);
  const clip = library.audio_web_decode_begin(0, 1);
  library.$AudioWebRuntime.context.pendingDecodes[0].resolve(decodedBuffer(1, 1, "clip"));
  await flushPromises();
  assert.equal(library.audio_web_voice_play(clip, 1, 0.25, 1), 0);
  document.dispatch("pointerdown");
  assert.equal(library.audio_web_user_gesture(), 1);

  const voice = library.audio_web_voice_play(clip, 1, 0.25, 1);
  assert.notEqual(voice, 0);
  const source = library.$AudioWebRuntime.context.sources.at(-1);
  const voiceGain = source.connections[0];
  assert.equal(source.started, true);
  assert.equal(source.loop, true);
  assert.equal(voiceGain.gain.value, 0.25);
  assert.deepEqual(voiceGain.connections, [library.$AudioWebRuntime.sfxNode]);
  assert.equal(library.audio_web_voice_active(voice), 1);

  library.audio_web_voice_set_gain(voice, 0.6);
  library.audio_web_voice_set_pitch(voice, 1.3);
  assert.equal(voiceGain.gain.value, 0.6);
  assert.equal(source.playbackRate.value, 1.3);
  library.audio_web_voice_set_pitch(voice, 0);
  library.audio_web_voice_set_pitch(voice, NaN);
  assert.equal(source.playbackRate.value, 1.3);

  source.finish();
  assert.equal(library.audio_web_voice_active(voice), 0);
  library.audio_web_voice_set_gain(voice, 0.1);
  assert.equal(voiceGain.gain.value, 0.6);
});

test("pointerdown resumes synchronously and the C hook reports the attempt", async () => {
  const { document, library } = await loadLibrary();
  library.audio_web_init();
  const audioContext = library.$AudioWebRuntime.context;

  assert.equal(audioContext.resumeCalls, 0);
  document.dispatch("pointerdown");
  assert.equal(audioContext.resumeCalls, 1);
  assert.equal(library.audio_web_user_gesture(), 1);
  assert.equal(library.audio_web_is_unlocked(), 0);
  await flushPromises();
  assert.equal(library.audio_web_is_unlocked(), 1);
});

test("keydown is also a synchronous unlock source", async () => {
  const { document, library } = await loadLibrary();
  library.audio_web_init();
  const audioContext = library.$AudioWebRuntime.context;

  document.dispatch("keydown");
  assert.equal(audioContext.resumeCalls, 1);
  assert.equal(library.audio_web_user_gesture(), 1);
});

test("ending ad pause resumes previously unlocked audio without another gesture", async () => {
  const { document, library } = await loadLibrary();
  library.audio_web_init();
  const audioContext = library.$AudioWebRuntime.context;
  document.dispatch("pointerdown");
  await flushPromises();

  library.audio_web_set_paused(1);
  assert.equal(audioContext.suspendCalls, 1);
  assert.equal(library.audio_web_user_gesture(), 0);
  assert.equal(library.audio_web_is_unlocked(), 0);
  assert.equal(audioContext.resumeCalls, 1);
  library.audio_web_set_paused(0);
  assert.equal(audioContext.resumeCalls, 2);
  assert.equal(library.audio_web_user_gesture(), 1);
  assert.equal(library.audio_web_is_unlocked(), 0);
  await flushPromises();
  assert.equal(audioContext.state, "running");
  assert.equal(library.audio_web_is_unlocked(), 1);

  document.hidden = true;
  document.dispatch("visibilitychange");
  assert.equal(library.audio_web_user_gesture(), 0);
  document.hidden = false;
  document.dispatch("visibilitychange");
  assert.equal(audioContext.resumeCalls, 3);
  assert.equal(library.audio_web_user_gesture(), 1);

  library.audio_web_set_enabled(0);
  assert.equal(library.audio_web_user_gesture(), 0);
  library.audio_web_set_enabled(1);
  assert.equal(audioContext.resumeCalls, 4);
});

test("a rejected resume releases voices queued by that gesture", async () => {
  const { context, document, library } = await loadLibrary();
  library.audio_web_init();
  context.HEAPU8[0] = 1;
  const clip = library.audio_web_decode_begin(0, 1);
  library.$AudioWebRuntime.context.pendingDecodes[0].resolve(decodedBuffer());
  await flushPromises();
  library.$AudioWebRuntime.context.rejectResume = true;

  document.dispatch("pointerdown");
  assert.equal(library.audio_web_user_gesture(), 1);
  assert.equal(library.audio_web_is_unlocked(), 0);
  const voice = library.audio_web_voice_play(clip, 1, 1, 0);
  assert.notEqual(voice, 0);
  await flushPromises();
  assert.equal(library.audio_web_is_unlocked(), 0);
  assert.equal(library.audio_web_voice_active(voice), 0);
});

test("a rejected automatic resume cleans voices queued by that attempt", async () => {
  const { context, document, library } = await loadLibrary();
  library.audio_web_init();
  context.HEAPU8[0] = 1;
  const clip = library.audio_web_decode_begin(0, 1);
  library.$AudioWebRuntime.context.pendingDecodes[0].resolve(decodedBuffer());
  document.dispatch("pointerdown");
  await flushPromises();

  library.audio_web_set_paused(1);
  library.$AudioWebRuntime.context.rejectResume = true;
  library.audio_web_set_paused(0);
  const queued = library.audio_web_voice_play(clip, 1, 1, 0);
  assert.notEqual(queued, 0);
  await flushPromises();
  assert.equal(library.audio_web_voice_active(queued), 0);
  assert.equal(library.$AudioWebRuntime.resumePending, false);
});

test("fixed pools refuse excess clips and voices", async () => {
  const { context, document, library } = await loadLibrary();
  library.audio_web_init();
  context.HEAPU8[0] = 1;
  const clips = Array.from({ length: 64 }, () => library.audio_web_decode_begin(0, 1));
  assert.ok(clips.every((handle) => handle !== 0));
  assert.equal(library.audio_web_decode_begin(0, 1), 0);
  for (const pending of library.$AudioWebRuntime.context.pendingDecodes) pending.resolve(decodedBuffer());
  await flushPromises();
  document.dispatch("pointerdown");

  const voices = Array.from({ length: 32 }, () => library.audio_web_voice_play(clips[0], 0, 1, 0));
  assert.ok(voices.every((handle) => handle !== 0));
  assert.equal(library.audio_web_voice_play(clips[0], 0, 1, 0), 0);
});

test("shutdown stops voices, removes all DOM listeners, disconnects graph, and closes context", async () => {
  const { context, document, library } = await loadLibrary();
  library.audio_web_init();
  context.HEAPU8[0] = 1;
  const clip = library.audio_web_decode_begin(0, 1);
  library.$AudioWebRuntime.context.pendingDecodes[0].resolve(decodedBuffer());
  await flushPromises();
  document.dispatch("pointerdown");
  const voice = library.audio_web_voice_play(clip, 1, 1, 0);
  const runtime = library.$AudioWebRuntime;
  const source = runtime.context.sources.at(-1);
  const master = runtime.masterNode;
  const audioContext = runtime.context;

  library.audio_web_shutdown();
  assert.equal(source.stopped, true);
  assert.equal(master.disconnected, true);
  assert.equal(audioContext.closeCalls, 1);
  assert.equal(document.listeners.has("visibilitychange"), false);
  assert.equal(document.listeners.has("pointerdown"), false);
  assert.equal(document.listeners.has("keydown"), false);
  assert.equal(library.audio_web_voice_active(voice), 0);
  assert.equal(library.audio_web_decode_state(clip), 2);
});

test("a touch whose resume stays pending is asked again by the next activating gesture", async () => {
  const { document, library } = await loadLibrary();
  library.audio_web_init();
  const audioContext = library.$AudioWebRuntime.context;
  audioContext.holdResume = true;
  document.dispatch("pointerdown"); // a touch's pointerdown carries no activation: this resume never settles
  assert.equal(audioContext.resumeCalls, 1);
  document.dispatch("touchend"); // within the same 250 ms window: still the in-flight resume
  assert.equal(audioContext.resumeCalls, 1);
  library.$AudioWebRuntime.resumeAskedAt -= 1000;
  audioContext.holdResume = false;
  document.dispatch("touchend");
  assert.equal(audioContext.resumeCalls, 2);
  await flushPromises();
  assert.equal(audioContext.state, "running");
  assert.equal(library.audio_web_user_gesture(), 1);
  assert.equal(library.audio_web_is_unlocked(), 1);
});

function near(actual, expected) {
  assert.ok(Math.abs(actual - expected) < 1e-9, `${actual} is not ${expected}`);
}

async function unlockedStreamVoice(loop = 1) {
  const loaded = await loadLibrary();
  const { document, library } = loaded;
  library.audio_web_init();
  document.dispatch("pointerdown");
  await flushPromises();
  const clip = library.audio_web_stream_open(1);
  assert.equal(library.audio_web_decode_state(clip), 1);
  const voice = library.audio_web_voice_play(clip, 0, 0.5, loop);
  assert.notEqual(voice, 0);
  return { ...loaded, clip, voice };
}

// One mono chunk of `frames` frames plus its guard frame, at HEAPF32 index 0.
function mono(context, frames) {
  context.HEAPF32.set(Array.from({ length: frames + 1 }, (_, i) => i / 100), 0);
}

test("a stream clip is ready at once and its voice holds a gain but no source", async () => {
  const { library, voice } = await unlockedStreamVoice();
  const runtime = library.$AudioWebRuntime;
  assert.equal(runtime.context.sources.length, 0);
  assert.equal(library.audio_web_voice_active(voice), 1);
  assert.equal(library.audio_web_stream_buffered_frames(voice), 0);
  runtime.context.currentTime = 2.5;
  assert.equal(library.audio_web_now(), 2.5);
  const voiceGain = runtime.context.gains.at(-1);
  assert.deepEqual(voiceGain.connections, [runtime.musicNode]);
  assert.equal(library.audio_web_decode_state(library.audio_web_stream_open(0)), 2);
});

test("stream chunks play at the track's rate back to back on its own timeline", async () => {
  const { context, library, voice } = await unlockedStreamVoice();
  const audioContext = library.$AudioWebRuntime.context;
  audioContext.currentTime = 1.5;
  mono(context, 3);
  assert.equal(library.audio_web_stream_push(voice, 0, 3, 1, 32000), 1);
  assert.equal(library.audio_web_stream_push(voice, 0, 3, 1, 32000), 1);
  const [first, second] = audioContext.sources;
  assert.equal(first.buffer.sampleRate, 32000);
  assert.equal(first.buffer.length, 4, "a guard frame past the chunk");
  assert.deepEqual(Array.from(first.buffer.channels[0]), [0, 0.01, 0.02, 0.03].map(Math.fround));
  near(first.when, 1.55);
  near(first.duration, 3 / 32000 - 0.5 / 48000);
  near(second.when, 1.55 + 3 / 32000);
  assert.equal(library.audio_web_stream_buffered_frames(voice), Math.floor((0.05 + 6 / 32000) * 32000));

  library.audio_web_voice_set_pitch(voice, 2);
  assert.equal(first.playbackRate.value, 1);
  audioContext.currentTime = 10; // a stall past the queue: the timeline moves ahead of the output latency
  audioContext.baseLatency = 0.01;
  audioContext.outputLatency = 0.04;
  library.audio_web_stream_push(voice, 0, 3, 1, 32000);
  near(audioContext.sources.at(-1).when, 10.1);
  library.audio_web_stream_push(voice, 0, 3, 1, 32000);
  near(audioContext.sources.at(-1).when, 10.1 + 3 / 32000);
});

test("a thousand chunks land where the track's timeline puts them", async () => {
  const { context, library, voice } = await unlockedStreamVoice();
  mono(context, 2048);
  for (let i = 0; i < 1000; ++i) library.audio_web_stream_push(voice, 0, 2048, 1, 32000);
  const sources = library.$AudioWebRuntime.context.sources;
  const drift = Math.abs(sources[999].when - (0.05 + 999 * 2048 / 32000));
  assert.ok(drift < 1e-9, `drift ${drift}`);
});

test("a chunk that cannot be scheduled is reported to the decoder", async () => {
  const { library, voice } = await unlockedStreamVoice();
  library.$AudioWebRuntime.context.createBuffer = () => { throw new Error("out of memory"); };
  assert.equal(library.audio_web_stream_push(voice, 0, 3, 1, 32000), 0);
  assert.equal(library.audio_web_stream_push(0, 0, 3, 1, 32000), 0);
});

test("a parked stream keeps its unplayed chunks and resumes on the sample it stopped on", async () => {
  const { context, library, voice } = await unlockedStreamVoice();
  const audioContext = library.$AudioWebRuntime.context;
  mono(context, 3200);
  for (let i = 0; i < 3; ++i) library.audio_web_stream_push(voice, 0, 3200, 1, 32000); // 0.1 s each from 0.05 s
  const scheduled = [...audioContext.sources];
  audioContext.currentTime = 0.2; // 0.15 s in: halfway through the second chunk
  library.audio_web_stream_park(voice);
  assert.ok(scheduled.every((source) => source.stopped));
  assert.equal(library.audio_web_voice_active(voice), 1, "a parked voice is still the caller's");
  assert.equal(library.audio_web_stream_push(voice, 0, 3200, 1, 32000), 0, "nothing is decoded while parked");

  audioContext.currentTime = 5;
  library.audio_web_stream_resume(voice);
  const [resumedSecond, resumedThird] = audioContext.sources.slice(-2);
  assert.equal(resumedSecond.buffer, scheduled[1].buffer);
  near(resumedSecond.when, 5.05);
  near(resumedSecond.offset, 1600 / 32000);
  near(resumedSecond.duration, 1600 / 32000 - 0.5 / 48000);
  assert.equal(resumedThird.buffer, scheduled[2].buffer);
  near(resumedThird.when, 5.05 + 1600 / 32000);
  assert.equal(library.audio_web_stream_push(voice, 0, 3200, 1, 32000), 1);
  near(audioContext.sources.at(-1).when, 5.05 + 4800 / 32000);
});

test("stopping a stream voice stops every scheduled chunk", async () => {
  const { context, library, voice } = await unlockedStreamVoice();
  mono(context, 2);
  library.audio_web_stream_push(voice, 0, 2, 1, 32000);
  library.audio_web_stream_push(voice, 0, 2, 1, 32000);
  const sources = library.$AudioWebRuntime.context.sources;
  library.audio_web_voice_stop(voice);
  assert.ok(sources.every((source) => source.stopped));
  assert.equal(library.audio_web_voice_active(voice), 0);
  assert.equal(library.audio_web_stream_buffered_frames(voice), -1);
  library.audio_web_stream_push(voice, 0, 2, 1, 32000);
  assert.equal(sources.length, 2);
});

test("an ended stream voice goes inactive when its last chunk finishes", async () => {
  const { context, library, voice } = await unlockedStreamVoice(0);
  mono(context, 2);
  library.audio_web_stream_push(voice, 0, 2, 1, 32000);
  library.audio_web_stream_push(voice, 0, 2, 1, 32000);
  library.audio_web_stream_end(voice);
  const [first, second] = library.$AudioWebRuntime.context.sources;
  first.finish();
  assert.equal(library.audio_web_voice_active(voice), 1);
  second.finish();
  assert.equal(library.audio_web_voice_active(voice), 0);
});

test("destroying a stream clip frees its slot for reuse", async () => {
  const { library, clip } = await unlockedStreamVoice();
  library.audio_web_clip_destroy(clip);
  assert.equal(library.audio_web_decode_state(clip), 2);
  const again = library.audio_web_stream_open(1);
  assert.notEqual(again, 0);
  assert.notEqual(again, clip);
});
