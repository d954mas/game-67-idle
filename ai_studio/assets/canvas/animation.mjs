// Canvas procedural animation: the ONE shared, pure, dependency-free contract the
// preview (increment 2, canvas rAF) and the bake (increment 3, frames via
// tools/render_group.py) both sample, so a channel animates IDENTICALLY on the
// browser page and in the PIL bake (the slice9.mjs/fonts.mjs/tree.mjs pattern —
// imported by ops.mjs in node AND served to the site over /ai_studio/). The exact
// Python twin is tools/animation.py; it arrives WITH the bake increment (increment 3)
// and must mirror the sampler math line-for-line — parity is the contract.
//
// v1 model (schema `ai_studio.canvas.animation.v1`, T0260 Track A). An element carries
// an optional `element.animation = { v:1, channels:[...] }`. Absent = a static element,
// byte-identical to today's saves (additive, zero migration). Image AND text elements
// both allow it — animation is a GEOMETRY/OPACITY-level transform (offset/rotation/
// scale/opacity), never pixel-level, so it applies to either type.
//
// A channel targets exactly ONE property and drives it over time; at most ONE channel
// per property (a duplicate is a loud error). Properties + their identity (the value a
// missing channel yields, so an unanimated property never moves):
//   - off_x, off_y : world-unit offset ADDED to the element x/y      (identity 0)
//   - rot          : degrees ADDED to the element rotation           (identity 0)
//   - scale        : MULTIPLIER on the element display scale         (identity 1)
//   - opacity      : MULTIPLIER on the element opacity, clamped [0,1] (identity 1)
// The application of these composed values to an element (x+offX, rotation+rot,
// scale*scale, opacity*opacity) lives in the preview/bake increments; this module only
// SAMPLES the animation to those composed values — it never reads or writes an element.
//
// Two channel kinds:
//   - osc  {prop, kind:"osc", amplitude, period_ms, phase?, center?}
//       value(t) = center + amplitude * sin( 2*PI * (t/period_ms + phase) )
//       period_ms > 0 (loud otherwise). `phase` is a fraction of a full cycle in [0,1)
//       (default 0, stored absent). `center` is the oscillation midpoint (default = the
//       property's IDENTITY, so an osc wobbles around the resting value — 0 for offsets/
//       rot, 1 for scale/opacity — stored absent when it equals that identity).
//   - keyframes {prop, kind:"keyframes", points:[{t_ms, v}, ...]}
//       linear interpolation between points; t_ms strictly increasing and STARTING at 0;
//       >= 2 points. The animation LOOPS over the LAST point's t_ms (tMs is taken modulo
//       it), so for a seamless loop the last point's v should match the first's — the
//       last->first wrap segment is never interpolated (t modulo stays in [0, loop)).
//
// The sampler is LOOP-STABLE for arbitrarily large tMs: each channel reduces t into its
// own period/loop with a modulo BEFORE any sin() or lerp, so there is no float drift and
// sampling at t and at t + N*loop returns identical values.

// Sampler-facing identity per property (also the default `center` of an osc channel).
const IDENTITY = { off_x: 0, off_y: 0, rot: 0, scale: 1, opacity: 1 };
const PROPS = new Set(["off_x", "off_y", "rot", "scale", "opacity"]);
const KINDS = new Set(["osc", "keyframes"]);

// Validate + normalize an osc channel to {prop, kind:"osc", amplitude, period_ms[,
// phase][, center]} — phase/center present only when != their default (mirrors the
// slice9 scale / rotation:0 "absent = default" convention). Loud on a non-finite
// amplitude/center, a period_ms <= 0, or a phase outside [0,1).
function validateOscChannel(raw, prop, index) {
  const amplitude = Number(raw.amplitude);
  if (!Number.isFinite(amplitude)) {
    throw new Error(`animation channel ${index} (${prop}) osc amplitude must be a finite number, got ${JSON.stringify(raw.amplitude)}`);
  }
  const period = Number(raw.period_ms);
  if (!Number.isFinite(period) || !(period > 0)) {
    throw new Error(`animation channel ${index} (${prop}) osc period_ms must be a finite number > 0, got ${JSON.stringify(raw.period_ms)}`);
  }
  const out = { prop, kind: "osc", amplitude, period_ms: period };
  if (raw.phase !== undefined && raw.phase !== null) {
    const phase = Number(raw.phase);
    if (!Number.isFinite(phase) || phase < 0 || phase >= 1) {
      throw new Error(`animation channel ${index} (${prop}) osc phase must be a number in [0,1), got ${JSON.stringify(raw.phase)}`);
    }
    if (phase !== 0) out.phase = phase; // default 0 -> stored absent
  }
  if (raw.center !== undefined && raw.center !== null) {
    const center = Number(raw.center);
    if (!Number.isFinite(center)) {
      throw new Error(`animation channel ${index} (${prop}) osc center must be a finite number, got ${JSON.stringify(raw.center)}`);
    }
    if (center !== IDENTITY[prop]) out.center = center; // default (the property identity) -> stored absent
  }
  return out;
}

// Validate + normalize a keyframes channel to {prop, kind:"keyframes", points:[{t_ms,
// v}]}. Loud on < 2 points, a non-object point, a non-finite/negative t_ms or non-finite
// v, a first point whose t_ms is not 0, or a t_ms sequence that is not strictly
// increasing.
function validateKeyframesChannel(raw, prop, index) {
  if (!Array.isArray(raw.points) || raw.points.length < 2) {
    throw new Error(`animation channel ${index} (${prop}) keyframes points must be an array of >= 2 {t_ms, v}, got ${JSON.stringify(raw.points)}`);
  }
  const points = raw.points.map((point, p) => {
    if (!point || typeof point !== "object" || Array.isArray(point)) {
      throw new Error(`animation channel ${index} (${prop}) keyframe point ${p} must be an object {t_ms, v}, got ${JSON.stringify(point)}`);
    }
    const t = Number(point.t_ms);
    const v = Number(point.v);
    if (!Number.isFinite(t) || t < 0) {
      throw new Error(`animation channel ${index} (${prop}) keyframe point ${p} t_ms must be a finite number >= 0, got ${JSON.stringify(point.t_ms)}`);
    }
    if (!Number.isFinite(v)) {
      throw new Error(`animation channel ${index} (${prop}) keyframe point ${p} v must be a finite number, got ${JSON.stringify(point.v)}`);
    }
    return { t_ms: t, v };
  });
  if (points[0].t_ms !== 0) {
    throw new Error(`animation channel ${index} (${prop}) keyframes must start at t_ms 0, got ${points[0].t_ms}`);
  }
  for (let i = 1; i < points.length; i += 1) {
    if (!(points[i].t_ms > points[i - 1].t_ms)) {
      throw new Error(`animation channel ${index} (${prop}) keyframe t_ms must be strictly increasing (point ${i} t_ms ${points[i].t_ms} <= point ${i - 1} t_ms ${points[i - 1].t_ms})`);
    }
  }
  return { prop, kind: "keyframes", points };
}

// Validate + normalize an animation spec against schema `ai_studio.canvas.animation.v1`
// and return a FRESH {v:1, channels:[...]} object (never shares references with the
// input). Loud on: a non-object spec, a missing/empty/non-array `channels`, a channel
// with an unknown/duplicate `prop`, an unknown `kind`, or any kind-specific violation
// (see validateOscChannel/validateKeyframesChannel). The op layer (setElementAnimation)
// is the loud gate at SET time, from either client; `null` to clear is handled there,
// not here (this only ever sees a real spec object).
export function validateAnimation(spec) {
  if (!spec || typeof spec !== "object" || Array.isArray(spec)) {
    throw new Error(`animation must be an object { channels: [...] }, got ${JSON.stringify(spec)}`);
  }
  if (!Array.isArray(spec.channels) || !spec.channels.length) {
    throw new Error(`animation.channels must be a non-empty array (use null to clear), got ${JSON.stringify(spec.channels)}`);
  }
  const seen = new Set();
  const channels = spec.channels.map((raw, index) => {
    if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
      throw new Error(`animation channel ${index} must be an object, got ${JSON.stringify(raw)}`);
    }
    const prop = raw.prop;
    if (!PROPS.has(prop)) {
      throw new Error(`animation channel ${index} prop must be one of ${[...PROPS].join("/")}, got ${JSON.stringify(prop)}`);
    }
    if (seen.has(prop)) {
      throw new Error(`animation has more than one channel targeting "${prop}" (at most one channel per property)`);
    }
    seen.add(prop);
    if (!KINDS.has(raw.kind)) {
      throw new Error(`animation channel ${index} (${prop}) kind must be osc or keyframes, got ${JSON.stringify(raw.kind)}`);
    }
    return raw.kind === "osc" ? validateOscChannel(raw, prop, index) : validateKeyframesChannel(raw, prop, index);
  });
  return { v: 1, channels };
}

// One osc channel's value at tMs. The time is reduced into a single cycle (tMs % period,
// then + phase, then mod 1) BEFORE the sin(), so an arbitrarily large tMs never feeds a
// huge argument into sin() (which would drift) — the loop-stability contract.
function oscValue(channel, tMs) {
  const period = channel.period_ms;
  const phase = channel.phase || 0;
  const center = channel.center !== undefined ? channel.center : IDENTITY[channel.prop] ?? 0;
  let frac = ((tMs % period) / period + phase) % 1;
  if (frac < 0) frac += 1;
  return center + channel.amplitude * Math.sin(2 * Math.PI * frac);
}

// One keyframes channel's value at tMs: linear interpolation within the segment holding
// (tMs modulo the loop length = the last point's t_ms). tMod is always in [0, loop), so
// it lands inside a defined [points[i], points[i+1]] segment — the wrap segment is never
// interpolated (see the module header's loop rule).
function keyframesValue(channel, tMs) {
  const points = channel.points;
  const loopLen = points[points.length - 1].t_ms;
  if (!(loopLen > 0)) return points[0].v; // degenerate; validation forbids it
  let tMod = tMs % loopLen;
  if (tMod < 0) tMod += loopLen;
  for (let i = 0; i < points.length - 1; i += 1) {
    const a = points[i];
    const b = points[i + 1];
    if (tMod >= a.t_ms && tMod <= b.t_ms) {
      const span = b.t_ms - a.t_ms;
      return span > 0 ? a.v + (b.v - a.v) * ((tMod - a.t_ms) / span) : a.v;
    }
  }
  return points[points.length - 1].v; // unreachable: tMod < loopLen
}

// Sample an animation spec at tMs (milliseconds) to the composed transform
// {offX, offY, rot, scale, opacity}. Pure + deterministic: same spec + same tMs always
// yields the same result, with no element/DOM/clock access. A property with no channel
// yields its identity (0 for offsets/rot, 1 for scale/opacity). `opacity` is clamped to
// [0,1] AFTER composition (it is a multiplier on the element opacity). Trusts a spec
// already validated by validateAnimation (the lenient-sampler stance slice9Patches
// takes) — it does not re-validate or throw.
export function sampleAnimation(spec, tMs) {
  const out = { offX: 0, offY: 0, rot: 0, scale: 1, opacity: 1 };
  const channels = spec && Array.isArray(spec.channels) ? spec.channels : [];
  const t = Number.isFinite(Number(tMs)) ? Number(tMs) : 0;
  for (const channel of channels) {
    const value = channel.kind === "osc" ? oscValue(channel, t) : keyframesValue(channel, t);
    switch (channel.prop) {
      case "off_x": out.offX = value; break;
      case "off_y": out.offY = value; break;
      case "rot": out.rot = value; break;
      case "scale": out.scale = value; break;
      case "opacity": out.opacity = value; break;
      default: break;
    }
  }
  out.opacity = Math.min(1, Math.max(0, out.opacity));
  return out;
}
