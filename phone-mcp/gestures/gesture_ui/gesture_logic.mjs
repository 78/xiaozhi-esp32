// Turns a stream of per-frame hand samples into discrete gesture events.
// Pure logic (no camera, no DOM) so it can be tested with node.
//
// A sample is: { t: ms, name: "Open_Palm" | "Closed_Fist" | ... | "None", score: 0..1,
//                x: wrist x 0..1 (raw camera image), y: wrist y 0..1 } or null when no hand is seen.
//
// Events returned from feed():
//   { gesture: "Open_Palm" }                      a pose held steady for holdMs
//   { gesture: "Pointing_Up", repeat: true }      same pose still held (every repeatMs, only for repeatable poses)
//   { gesture: "swipe_left" | "swipe_right" }     fast sideways move, as seen by the user
//   { gesture: "wave" }                           hand waved side to side

export const DEFAULTS = {
  minScore: 0.55,
  holdMs: 400,
  holdMsByGesture: { Closed_Fist: 1500 },  // fist mutes the music, so it must be held on purpose
  voteWindowMs: 240,      // the pose is the majority over this window, so one bad frame cannot reset a hold
  minVoteShare: 0.6,
  repeatMs: 700,
  repeatable: ["Pointing_Up"],
  cooldownMs: 1200,       // same gesture cannot fire again sooner than this (unless repeat)
  mirror: true,           // front camera: the user's right is the image's left
  swipeEnabled: false,    // swipes fired by accident and missed real ones; static signs are used instead
  swipeWindowMs: 450,
  swipeMinDx: 0.30,       // fraction of the image width
  swipeMaxDy: 0.18,
  waveWindowMs: 1400,
  waveMinReversals: 3,
  waveMinAmplitude: 0.05, // each leg of the wave must travel at least this far
  lostAfterMs: 400,       // hand missing this long resets the pose
};

// ---------------------------------------------------------------- hand geometry

const dist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y, ((a.z || 0) - (b.z || 0)) * 0.5);

/** Which fingers are stretched out, judged from the 21 landmarks alone (works at any rotation). */
export function fingerState(lm) {
  if (!lm || lm.length < 21) return null;
  const wrist = lm[0];
  const finger = (tip, pip) => dist(lm[tip], wrist) > dist(lm[pip], wrist) * 1.05;
  const index = finger(8, 6), middle = finger(12, 10), ring = finger(16, 14), pinky = finger(20, 18);
  // the thumb folds sideways, so compare against the base of the little finger instead of the wrist
  const thumb = dist(lm[4], lm[17]) > dist(lm[3], lm[17]) * 1.05 && dist(lm[4], lm[5]) > dist(lm[3], lm[5]) * 1.05;
  const count = [index, middle, ring, pinky].filter(Boolean).length;
  return { index, middle, ring, pinky, thumb, count };
}

/**
 * Cross-checks MediaPipe's pose label with the finger geometry. The classifier often calls a
 * tilted palm "None" or mixes up palm and fist; counting stretched fingers settles those cases.
 * Returns { name, score, fingers }.
 */
export function resolvePose(name, score, lm, minScore = DEFAULTS.minScore) {
  const fingers = fingerState(lm);
  let outName = name || "None";
  let outScore = score || 0;
  if (fingers) {
    if (outName === "Open_Palm" && fingers.count <= 1) {
      outName = "Closed_Fist"; outScore = Math.min(outScore, 0.7);
    } else if (outName === "Closed_Fist" && fingers.count >= 4) {
      outName = "Open_Palm"; outScore = Math.min(outScore, 0.7);
    } else if (outName === "None" || outScore < minScore) {
      if (fingers.count === 4 && fingers.thumb) { outName = "Open_Palm"; outScore = 0.65; }
      else if (fingers.count === 0 && !fingers.thumb) { outName = "Closed_Fist"; outScore = 0.65; }
    }
  }
  return { name: outName, score: outScore, fingers };
}

// -------------------------------------------------------------------- smoothing

/** One Euro filter: strong smoothing when the hand is still, little lag when it moves fast. */
export class OneEuro {
  constructor(minCutoff = 1.5, beta = 5, dCutoff = 1.0) {
    this.minCutoff = minCutoff;
    this.beta = beta;
    this.dCutoff = dCutoff;
    this.reset();
  }
  reset() { this.x = null; this.dx = 0; this.t = null; }
  static alpha(cutoff, dt) {
    const tau = 1 / (2 * Math.PI * cutoff);
    return 1 / (1 + tau / dt);
  }
  filter(value, tMs) {
    const t = tMs / 1000;
    if (this.x === null || this.t === null || t <= this.t) {
      this.x = value; this.t = t; this.dx = 0;
      return value;
    }
    const dt = t - this.t;
    const rawDx = (value - this.x) / dt;
    this.dx += OneEuro.alpha(this.dCutoff, dt) * (rawDx - this.dx);
    const cutoff = this.minCutoff + this.beta * Math.abs(this.dx);
    this.x += OneEuro.alpha(cutoff, dt) * (value - this.x);
    this.t = t;
    return this.x;
  }
}

/** Smooths all 21 landmarks (for drawing only; gesture timing uses the raw wrist). */
export class LandmarkSmoother {
  constructor(minCutoff, beta) {
    this.filters = Array.from({ length: 21 }, () => [0, 1, 2].map(() => new OneEuro(minCutoff, beta)));
  }
  reset() { this.filters.forEach((trio) => trio.forEach((f) => f.reset())); }
  smooth(lm, tMs) {
    return lm.map((p, i) => ({
      x: this.filters[i][0].filter(p.x, tMs),
      y: this.filters[i][1].filter(p.y, tMs),
      z: this.filters[i][2].filter(p.z || 0, tMs),
    }));
  }
}

// ----------------------------------------------------------------------- engine

export class GestureEngine {
  constructor(options = {}) {
    this.o = { ...DEFAULTS, ...options };
    this.reset();
  }

  reset() {
    this.pose = null;
    this.poseSince = 0;
    this.poseFired = false;
    this.lastRepeat = 0;
    this.votes = [];        // recent [{t, name}] (name null = no confident pose)
    this.track = [];        // recent wrist positions [{t, x, y}]
    this.lastSeen = -Infinity;
    this.lastFired = {};    // gesture -> time
    this.motionLockUntil = 0;
  }

  _ok(gesture, t) {
    const last = this.lastFired[gesture];
    return last === undefined || t - last >= this.o.cooldownMs;
  }

  _fire(events, gesture, t, extra = {}) {
    this.lastFired[gesture] = t;
    events.push({ gesture, ...extra });
  }

  _clearPose() {
    this.pose = null;
    this.poseFired = false;
    this.votes = [];
  }

  /** Majority pose over the vote window, or null when no pose has enough support. */
  _votedPose(name, t) {
    const o = this.o;
    this.votes.push({ t, name });
    while (this.votes.length && this.votes[0].t < t - o.voteWindowMs) this.votes.shift();
    if (this.votes.length < 2) return name;
    const counts = new Map();
    for (const v of this.votes) if (v.name) counts.set(v.name, (counts.get(v.name) || 0) + 1);
    let best = null, bestN = 0;
    for (const [n, c] of counts) if (c > bestN) { best = n; bestN = c; }
    return best && bestN / this.votes.length >= o.minVoteShare ? best : null;
  }

  feed(sample, t = sample ? sample.t : 0) {
    const events = [];
    const o = this.o;

    if (!sample) {
      if (t - this.lastSeen > o.lostAfterMs) {
        this._clearPose();
        this.track = [];
      }
      return events;
    }
    this.lastSeen = t;

    // ---- motion (swipe, wave) from the wrist path ----
    const x = o.mirror ? 1 - sample.x : sample.x;
    this.track.push({ t, x, y: sample.y });
    const keepFrom = t - Math.max(o.swipeWindowMs, o.waveWindowMs);
    while (this.track.length && this.track[0].t < keepFrom) this.track.shift();

    if (t >= this.motionLockUntil) {
      const wave = this._detectWave(t);
      if (wave) {
        if (this._ok("wave", t)) this._fire(events, "wave", t);
        this.motionLockUntil = t + o.cooldownMs;
        this.track = [];
        this._clearPose();
        return events;
      }
      const swipe = o.swipeEnabled ? this._detectSwipe(t) : null;
      if (swipe) {
        if (this._ok(swipe, t)) this._fire(events, swipe, t);
        this.motionLockUntil = t + o.cooldownMs;
        this.track = [];
        this._clearPose();
        return events;
      }
    }

    // ---- held poses ----
    const raw = sample.score >= o.minScore && sample.name && sample.name !== "None" ? sample.name : null;
    const name = this._votedPose(raw, t);
    if (name !== this.pose) {
      this.pose = name;
      this.poseSince = t;
      this.poseFired = false;
      this.lastRepeat = t;
      return events;
    }
    if (!name || t < this.motionLockUntil) return events;

    if (!this.poseFired && t - this.poseSince >= (o.holdMsByGesture[name] ?? o.holdMs)) {
      this.poseFired = true;
      this.lastRepeat = t;
      if (this._ok(name, t)) this._fire(events, name, t);
    } else if (this.poseFired && o.repeatable.includes(name) && t - this.lastRepeat >= o.repeatMs) {
      this.lastRepeat = t;
      this._fire(events, name, t, { repeat: true });
    }
    return events;
  }

  _detectSwipe(t) {
    const o = this.o;
    const pts = this.track.filter((p) => p.t >= t - o.swipeWindowMs);
    if (pts.length < 4) return null;
    const first = pts[0];
    const last = pts[pts.length - 1];
    const dx = last.x - first.x;
    const dy = Math.abs(last.y - first.y);
    if (Math.abs(dx) < o.swipeMinDx || dy > o.swipeMaxDy) return null;
    // must be mostly one direction, not a wave
    let reversals = 0;
    for (let i = 2; i < pts.length; i++) {
      const a = pts[i - 1].x - pts[i - 2].x;
      const b = pts[i].x - pts[i - 1].x;
      if (a * b < 0 && Math.abs(a) > 0.01 && Math.abs(b) > 0.01) reversals++;
    }
    if (reversals > 1) return null;
    return dx > 0 ? "swipe_right" : "swipe_left";
  }

  _detectWave(t) {
    const o = this.o;
    const pts = this.track.filter((p) => p.t >= t - o.waveWindowMs);
    if (pts.length < 8) return false;
    // count direction reversals where each leg travelled far enough
    let dir = 0;
    let legStart = pts[0].x;
    let extreme = pts[0].x;
    let reversals = 0;
    for (let i = 1; i < pts.length; i++) {
      const x = pts[i].x;
      if (dir >= 0 && x > extreme) extreme = x;
      if (dir <= 0 && x < extreme) extreme = x;
      const travelled = extreme - legStart;
      if (dir === 0) {
        if (Math.abs(x - legStart) >= o.waveMinAmplitude) {
          dir = x > legStart ? 1 : -1;
          extreme = x;
        }
        continue;
      }
      // moving in `dir`; a reversal is a pullback of at least the amplitude
      if (dir === 1 && extreme - x >= o.waveMinAmplitude && travelled >= o.waveMinAmplitude) {
        reversals++;
        dir = -1;
        legStart = extreme;
        extreme = x;
      } else if (dir === -1 && x - extreme >= o.waveMinAmplitude && -travelled >= o.waveMinAmplitude) {
        reversals++;
        dir = 1;
        legStart = extreme;
        extreme = x;
      }
    }
    return reversals >= o.waveMinReversals;
  }
}
