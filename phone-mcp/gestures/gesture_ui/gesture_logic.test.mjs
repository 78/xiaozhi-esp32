// Run:  node gesture_logic.test.mjs
import assert from "node:assert/strict";
import { GestureEngine } from "./gesture_logic.mjs";

const FPS = 30;
const STEP = 1000 / FPS;

// pose(name, ms) -> samples of a still hand
function still(name, ms, x = 0.5, y = 0.5, t0 = 0, score = 0.9) {
  const out = [];
  for (let t = 0; t <= ms; t += STEP) out.push({ t: t0 + t, name, score, x, y });
  return out;
}
// move wrist along x from a to b over ms
function move(a, b, ms, t0 = 0, name = "Open_Palm", y = 0.5) {
  const out = [];
  for (let t = 0; t <= ms; t += STEP) out.push({ t: t0 + t, name, score: 0.9, x: a + ((b - a) * t) / ms, y });
  return out;
}
function run(engine, samples) {
  const ev = [];
  for (const s of samples) ev.push(...engine.feed(s, s.t));
  return ev.map((e) => (e.repeat ? e.gesture + "+" : e.gesture));
}

let passed = 0;
function test(name, fn) {
  fn();
  passed++;
  console.log("ok  -", name);
}

test("held pose fires once after holdMs", () => {
  const e = new GestureEngine();
  assert.deepEqual(run(e, still("Open_Palm", 2000)), ["Open_Palm"]);
});

test("pose shorter than holdMs does not fire", () => {
  const e = new GestureEngine();
  assert.deepEqual(run(e, still("Closed_Fist", 300)), []);
});

test("low confidence never fires", () => {
  const e = new GestureEngine();
  assert.deepEqual(run(e, still("Victory", 2000, 0.5, 0.5, 0, 0.3)), []);
});

test("None pose never fires", () => {
  const e = new GestureEngine();
  assert.deepEqual(run(e, still("None", 2000)), []);
});

test("same pose does not re-fire inside the cooldown, fires again after", () => {
  const e = new GestureEngine();
  let s = still("Thumb_Up", 600); // fires at ~467ms
  s = s.concat(still("Open_Palm", 60, 0.5, 0.5, 634)); // brief other pose
  s = s.concat(still("Thumb_Up", 1000, 0.5, 0.5, 720)); // second hold would fire at ~1200ms, only ~730ms later
  const ev = run(e, s);
  assert.equal(ev.filter((g) => g === "Thumb_Up").length, 1); // blocked by the 1200ms cooldown
  const e2 = new GestureEngine();
  let s2 = still("Thumb_Up", 1000);
  s2 = s2.concat(still("None", 100, 0.5, 0.5, 1034));
  s2 = s2.concat(still("Thumb_Up", 1000, 0.5, 0.5, 3000));
  assert.equal(run(e2, s2).filter((g) => g === "Thumb_Up").length, 2);
});

test("Pointing_Up repeats while held, others do not", () => {
  const e = new GestureEngine();
  const ev = run(e, still("Pointing_Up", 3000));
  assert.equal(ev[0], "Pointing_Up");
  assert.ok(ev.filter((g) => g === "Pointing_Up+").length >= 2, JSON.stringify(ev));
  const e2 = new GestureEngine();
  assert.deepEqual(run(e2, still("Open_Palm", 3000)), ["Open_Palm"]);
});

test("user's hand moving right (raw x decreasing, mirrored camera) is swipe_right", () => {
  const e = new GestureEngine({ swipeEnabled: true });
  assert.deepEqual(run(e, move(0.8, 0.4, 300)), ["swipe_right"]);
});

test("user's hand moving left is swipe_left", () => {
  const e = new GestureEngine({ swipeEnabled: true });
  assert.deepEqual(run(e, move(0.4, 0.8, 300)), ["swipe_left"]);
});

test("mirror:false flips the direction", () => {
  const e = new GestureEngine({ mirror: false, swipeEnabled: true });
  assert.deepEqual(run(e, move(0.8, 0.4, 300)), ["swipe_left"]);
});

test("slow drift is not a swipe (the held open palm still counts as a pose)", () => {
  const e = new GestureEngine({ swipeEnabled: true });
  const ev = run(e, move(0.7, 0.4, 3000));
  assert.ok(!ev.some((g) => g.startsWith("swipe") || g === "wave"), JSON.stringify(ev));
  assert.deepEqual(ev, ["Open_Palm"]);
});

test("small move is not a swipe", () => {
  const e = new GestureEngine({ swipeEnabled: true });
  assert.deepEqual(run(e, move(0.55, 0.45, 300)), []);
});

test("diagonal move is not a swipe", () => {
  const e = new GestureEngine({ swipeEnabled: true });
  const s = move(0.8, 0.4, 300).map((p, i, a) => ({ ...p, y: 0.2 + (0.5 * i) / a.length }));
  assert.deepEqual(run(e, s), []);
});

test("side-to-side wave fires wave, not a swipe", () => {
  const e = new GestureEngine({ swipeEnabled: true });
  let s = [];
  let t = 0;
  let x = 0.4;
  for (let leg = 0; leg < 5; leg++) {
    const to = leg % 2 === 0 ? 0.55 : 0.4;
    s = s.concat(move(x, to, 250, t));
    t += 250 + STEP;
    x = to;
  }
  const ev = run(e, s);
  assert.ok(ev.includes("wave"), JSON.stringify(ev));
  assert.ok(!ev.some((g) => g.startsWith("swipe")), JSON.stringify(ev));
});

test("hand lost resets the held pose", () => {
  const e = new GestureEngine();
  const ev = [];
  for (const s of still("Open_Palm", 300)) ev.push(...e.feed(s, s.t));
  for (let t = 400; t < 1200; t += STEP) ev.push(...e.feed(null, t)); // gone
  for (const s of still("Open_Palm", 300, 0.5, 0.5, 1300)) ev.push(...e.feed(s, s.t));
  assert.deepEqual(ev, []); // neither 300ms stretch reached 450ms
});

test("a swipe blocks pose events during its cooldown", () => {
  const e = new GestureEngine({ swipeEnabled: true });
  const s = move(0.8, 0.4, 300).concat(still("Open_Palm", 800, 0.4, 0.5, 340));
  assert.deepEqual(run(e, s), ["swipe_right"]);
});

test("swipes are off by default", () => {
  const e = new GestureEngine();
  assert.ok(!run(e, move(0.8, 0.4, 300)).some((g) => g.startsWith("swipe")));
});

test("fist needs a 1.5 s hold, other poses keep the normal hold", () => {
  assert.deepEqual(run(new GestureEngine(), still("Closed_Fist", 1200)), []);
  assert.deepEqual(run(new GestureEngine(), still("Closed_Fist", 2200)), ["Closed_Fist"]);
  assert.deepEqual(run(new GestureEngine(), still("Thumb_Up", 700)), ["Thumb_Up"]);
});

// ------------------------------------------------------------ accuracy additions
import { fingerState, resolvePose, OneEuro, LandmarkSmoother } from "./gesture_logic.mjs";

// Synthetic right-hand-facing-camera landmarks, wrist at (0.5, 0.8), fingers pointing up.
function hand({ index = true, middle = true, ring = true, pinky = true, thumb = true, angle = 0 } = {}) {
  const lm = new Array(21);
  lm[0] = { x: 0.5, y: 0.8, z: 0 };
  const cols = { index: [5, 0.44, index], middle: [9, 0.48, middle], ring: [13, 0.52, ring], pinky: [17, 0.56, pinky] };
  for (const [, [base, x, ext]] of Object.entries(cols)) {
    lm[base] = { x, y: 0.65, z: 0 };
    if (ext) { // straight up
      lm[base + 1] = { x, y: 0.57, z: 0 };
      lm[base + 2] = { x, y: 0.52, z: 0 };
      lm[base + 3] = { x, y: 0.47, z: 0 };
    } else {   // curled back into the palm
      lm[base + 1] = { x, y: 0.58, z: 0 };
      lm[base + 2] = { x, y: 0.62, z: 0 };
      lm[base + 3] = { x, y: 0.67, z: 0 };
    }
  }
  if (thumb) {
    lm[1] = { x: 0.42, y: 0.74, z: 0 }; lm[2] = { x: 0.37, y: 0.70, z: 0 };
    lm[3] = { x: 0.33, y: 0.66, z: 0 }; lm[4] = { x: 0.29, y: 0.62, z: 0 };
  } else {
    lm[1] = { x: 0.42, y: 0.74, z: 0 }; lm[2] = { x: 0.40, y: 0.70, z: 0 };
    lm[3] = { x: 0.43, y: 0.67, z: 0 }; lm[4] = { x: 0.47, y: 0.66, z: 0 };
  }
  if (angle) {
    const c = Math.cos(angle), s = Math.sin(angle);
    return lm.map((p) => ({ x: 0.5 + (p.x - 0.5) * c - (p.y - 0.8) * s, y: 0.8 + (p.x - 0.5) * s + (p.y - 0.8) * c, z: 0 }));
  }
  return lm;
}
const OPEN = () => hand();
const FIST = () => hand({ index: false, middle: false, ring: false, pinky: false, thumb: false });

test("fingerState: open hand has 4 fingers and a thumb out", () => {
  const f = fingerState(OPEN());
  assert.equal(f.count, 4);
  assert.equal(f.thumb, true);
});

test("fingerState: fist has none out", () => {
  const f = fingerState(FIST());
  assert.equal(f.count, 0);
  assert.equal(f.thumb, false);
});

test("fingerState: same answer at any rotation", () => {
  for (const angle of [0.6, 1.57, 3.14, 4.7]) {
    assert.equal(fingerState(hand({ angle })).count, 4, "open @" + angle);
    assert.equal(fingerState(hand({ index: false, middle: false, ring: false, pinky: false, thumb: false, angle })).count, 0, "fist @" + angle);
  }
});

test("fingerState: only index up counts 1", () => {
  assert.equal(fingerState(hand({ middle: false, ring: false, pinky: false, thumb: false })).count, 1);
});

test("fingerState: bad input returns null", () => {
  assert.equal(fingerState(null), null);
  assert.equal(fingerState([]), null);
});

test("resolvePose: classifier says palm but the hand is a fist -> fist", () => {
  assert.equal(resolvePose("Open_Palm", 0.9, FIST()).name, "Closed_Fist");
});

test("resolvePose: classifier says fist but fingers are out -> palm", () => {
  assert.equal(resolvePose("Closed_Fist", 0.9, OPEN()).name, "Open_Palm");
});

test("resolvePose: a tilted palm labelled None is rescued", () => {
  const r = resolvePose("None", 0.2, hand({ angle: 0.9 }));
  assert.equal(r.name, "Open_Palm");
  assert.ok(r.score >= 0.55);
});

test("resolvePose: a closed hand labelled None becomes fist", () => {
  assert.equal(resolvePose("None", 0.1, hand({ index: false, middle: false, ring: false, pinky: false, thumb: false, angle: 2 })).name, "Closed_Fist");
});

test("resolvePose: thumbs up (closed fingers, thumb out) is left alone", () => {
  const lm = hand({ index: false, middle: false, ring: false, pinky: false, thumb: true });
  assert.equal(resolvePose("Thumb_Up", 0.9, lm).name, "Thumb_Up");
  assert.equal(resolvePose("None", 0.1, lm).name, "None"); // not mistaken for a fist
});

test("resolvePose: other confident labels are untouched, no landmarks is safe", () => {
  assert.equal(resolvePose("Victory", 0.9, OPEN()).name, "Victory");
  assert.equal(resolvePose("Pointing_Up", 0.9, FIST()).name, "Pointing_Up");
  assert.equal(resolvePose("Open_Palm", 0.9, null).name, "Open_Palm");
});

test("voting: a pose that flickers to None now and then still fires", () => {
  const e = new GestureEngine();
  const s = still("Open_Palm", 2000).map((p, i) => (i % 5 === 4 ? { ...p, name: "None" } : p));
  assert.deepEqual(run(e, s), ["Open_Palm"]);
});

test("voting: a pose that flips between two labels half the time does not fire", () => {
  const e = new GestureEngine();
  const s = still("Open_Palm", 2000).map((p, i) => (i % 2 ? { ...p, name: "Closed_Fist" } : p));
  assert.deepEqual(run(e, s), []);
});

test("voting: switching from palm to fist is picked up promptly", () => {
  const e = new GestureEngine();
  const s = still("Open_Palm", 1000).concat(still("Victory", 1500, 0.5, 0.5, 1034));
  const ev = run(e, s);
  assert.deepEqual(ev, ["Open_Palm", "Victory"]);
});

test("OneEuro: removes jitter on a still hand", () => {
  let seed = 7;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647 - 0.5) * 0.02; // +-1% noise
  const f = new OneEuro();
  const raw = [], out = [];
  for (let i = 0; i < 200; i++) { const v = 0.5 + rnd(); raw.push(v); out.push(f.filter(v, i * 33)); }
  const sd = (a) => { const m = a.reduce((x, y) => x + y) / a.length; return Math.sqrt(a.reduce((x, y) => x + (y - m) ** 2, 0) / a.length); };
  assert.ok(sd(out.slice(30)) < sd(raw.slice(30)) * 0.5, sd(out.slice(30)) + " vs " + sd(raw.slice(30)));
});

test("OneEuro: follows a fast move with little lag", () => {
  const f = new OneEuro();
  let last = 0;
  for (let i = 0; i <= 10; i++) f.filter(0.2, i * 33);           // rest
  for (let i = 1; i <= 9; i++) last = f.filter(0.2 + (0.6 * i) / 9, 330 + i * 33); // 0.6 in 0.3s
  assert.ok(Math.abs(0.8 - last) < 0.12, "lag " + Math.abs(0.8 - last));
});

test("OneEuro: ignores out-of-order timestamps instead of blowing up", () => {
  const f = new OneEuro();
  f.filter(0.5, 100);
  const v = f.filter(0.6, 100);
  assert.ok(Number.isFinite(v));
});

test("LandmarkSmoother: 21 finite points, reset forgets history", () => {
  const sm = new LandmarkSmoother();
  const a = sm.smooth(OPEN(), 0);
  assert.equal(a.length, 21);
  sm.smooth(OPEN(), 33);
  sm.reset();
  const b = sm.smooth(FIST(), 66);
  assert.deepEqual(b.map((p) => p.x), FIST().map((p) => p.x));
});

console.log("\n" + passed + " tests passed");
