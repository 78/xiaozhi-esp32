# Tuni P4 — Milestone 1 (Voice Loop on Onboard Audio) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a bare Waveshare ESP32-P4-NANO hold a continuous, always-on open-mic voice conversation with the `robo-worker` backend over WebSocket, using the NANO's onboard ES8311 audio.

**Architecture:** A new reduced `tuni-p4` firmware board (WifiBoard + ES8311 + NoDisplay) speaks the existing PCM-24k WS protocol to `robo-worker`. A fail-closed dev-only auth bypass in `robo-worker` mints a session token from the device MAC so no on-device crypto is needed yet. Wake word is disabled; the device auto-enters a VAD-bounded autostop listening loop.

**Tech Stack:** ESP-IDF (C++) + ESP-SR AFE (VADNet1 VAD) on the firmware side; Cloudflare Workers + Hono + Kysely/D1 + vitest-pool-workers on the backend side.

**Repos (two separate git repos — commit in the right one):**
- Backend: `/Users/tung/robo-worker` (Part A)
- Firmware: `/Users/tung/xiaozhi-esp32` (Part B)

**Spec:** `xiaozhi-esp32/docs/superpowers/specs/2026-06-24-tuni-p4-milestone1-voice-loop-design.md`

## Global Constraints

- **Protocol version MUST be 1** (raw PCM, no `BinaryProtocol2/3` header). Backend OTA returns numeric `version: 1`; firmware flashed with `erase-flash` first.
- **Audio format:** PCM, 24000 Hz, mono, 40 ms frames (already the firmware/backend default).
- **Half-duplex:** server-AEC and device-AEC OFF so `GetDefaultListeningMode()` resolves to `kListeningModeAutoStop`. No `CONFIG_USE_SERVER_AEC`.
- **No wake word, no press-to-talk:** `CONFIG_WAKE_WORD_DISABLED=y`; auto-start via `CONFIG_AUTO_START_LISTENING=y`.
- **VAD model:** neural `CONFIG_SR_VADN_VADNET1_MEDIUM=y` (else falls back to WebRTC default).
- **Auth bypass is fail-closed:** active only when `c.env.E2E_ENABLED === "1" && c.env.XIAOZHI_DEV_BYPASS === "1"`; mint-only (never writes device rows); requires an active device bound to `XIAOZHI_DEV_CHILD_ID`.
- **Silicon revision:** pick `tuni-p4` (rev<v3) vs `tuni-p4-p4x` (rev≥3) to match the physical chip (read from bootloader log).
- **Dev session TTL:** 30 days (`DEV_SESSION_TTL_SEC`).
- Backend lint/format = Biome (2-space, double quotes, 120 cols); schemas = Arktype; IDs = UUIDv7.

---

# Part A — Backend dev-bypass auth (`robo-worker`)

All Part A commits run in `/Users/tung/robo-worker`. Run a single test file with
`pnpm vitest run tests/<file>`.

### Task A1: Add `ttlSec` parameter to `mintSessionToken`

**Files:**
- Modify: `src/auth/device.ts` (the `mintSessionToken` function, ~line 70)
- Test: `tests/device_session_ttl.test.ts` (create)

**Interfaces:**
- Produces: `mintSessionToken(env, deviceId, childId, nowSec?, ttlSec?)` — `ttlSec` defaults to `DEVICE_SESSION_TTL_SEC`; token `exp = nowSec + ttlSec`.

- [ ] **Step 1: Write the failing test**

```ts
// tests/device_session_ttl.test.ts
import { env } from "cloudflare:workers";
import type { UUID } from "node:crypto";
import { describe, expect, it } from "vitest";
import { DEVICE_SESSION_TTL_SEC, mintSessionToken, verifySessionToken } from "../src/auth/device";

function decodeExp(jwt: string): number {
  const payload = JSON.parse(new TextDecoder().decode(
    Uint8Array.from(atob(jwt.split(".")[1].replace(/-/g, "+").replace(/_/g, "/")), (c) => c.charCodeAt(0)),
  ));
  return payload.exp;
}

describe("mintSessionToken ttlSec", () => {
  const deviceId = "aa:bb:cc:dd:ee:ff";
  const childId = "00000000-0000-7000-8000-000000000001" as UUID;

  it("defaults to DEVICE_SESSION_TTL_SEC", async () => {
    const nowSec = 1_750_000_000;
    const jwt = await mintSessionToken(env, deviceId, childId, nowSec);
    expect(decodeExp(jwt)).toBe(nowSec + DEVICE_SESSION_TTL_SEC);
  });

  it("honors an explicit ttlSec", async () => {
    const nowSec = 1_750_000_000;
    const thirtyDays = 30 * 24 * 60 * 60;
    const jwt = await mintSessionToken(env, deviceId, childId, nowSec, thirtyDays);
    expect(decodeExp(jwt)).toBe(nowSec + thirtyDays);
    const claims = await verifySessionToken(env, jwt, nowSec + 1);
    expect(claims.childId).toBe(childId);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pnpm vitest run tests/device_session_ttl.test.ts`
Expected: FAIL on the second test — `exp` is `nowSec + DEVICE_SESSION_TTL_SEC`, not `nowSec + thirtyDays` (ttlSec ignored).

- [ ] **Step 3: Add the `ttlSec` parameter**

In `src/auth/device.ts`, change the signature and `exp`:

```ts
export async function mintSessionToken(
  env: DeviceSessionEnv,
  deviceId: string,
  childId: UUID,
  nowSec = Math.floor(Date.now() / 1000),
  ttlSec = DEVICE_SESSION_TTL_SEC,
): Promise<string> {
  const header = { alg: "HS256", typ: "JWT" };
  const payload = {
    sub: deviceId,
    childId,
    iat: nowSec,
    exp: nowSec + ttlSec,
  };
  const signingInput = `${b64url(jsonBytes(header))}.${b64url(jsonBytes(payload))}`;
  const sig = await signHs256(env, signingInput);
  return `${signingInput}.${b64url(new Uint8Array(sig))}`;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pnpm vitest run tests/device_session_ttl.test.ts`
Expected: PASS (both tests).

- [ ] **Step 5: Commit**

```bash
git add src/auth/device.ts tests/device_session_ttl.test.ts
git commit -m "feat(auth): add ttlSec param to mintSessionToken"
```

### Task A2: Declare dev-bypass env vars

**Files:**
- Modify: `wrangler.jsonc` (the `env.test.vars` block, ~line 159; and top-level `vars` if present)
- Modify: `.dev.vars` (local dev)
- Modify: `worker-configuration.d.ts` (regenerated, do not hand-edit)

**Interfaces:**
- Produces: `c.env.XIAOZHI_DEV_BYPASS: string` and `c.env.XIAOZHI_DEV_CHILD_ID: string` typed on `Cloudflare.Env`.

- [ ] **Step 1: Add vars to the test env**

In `wrangler.jsonc`, extend `env.test.vars`:

```jsonc
"vars": {
  "E2E_ENABLED": "1",
  "DEVICE_SESSION_SECRET": "test-device-session-secret",
  "XIAOZHI_DEV_BYPASS": "1",
  "XIAOZHI_DEV_CHILD_ID": "00000000-0000-7000-8000-0000000000aa"
}
```

- [ ] **Step 2: Add to local dev vars**

Append to `.dev.vars`:

```
XIAOZHI_DEV_BYPASS=1
XIAOZHI_DEV_CHILD_ID=00000000-0000-7000-8000-0000000000aa
```

- [ ] **Step 3: Regenerate types**

Run: `pnpm types`
Expected: `worker-configuration.d.ts` now contains `XIAOZHI_DEV_BYPASS: string;` and `XIAOZHI_DEV_CHILD_ID: string;`.

Verify: `grep -E "XIAOZHI_DEV_(BYPASS|CHILD_ID)" worker-configuration.d.ts`

- [ ] **Step 4: Type-check**

Run: `pnpm type-check`
Expected: no errors.

- [ ] **Step 5: Commit**

```bash
git add wrangler.jsonc .dev.vars worker-configuration.d.ts
git commit -m "chore(config): declare XIAOZHI_DEV_BYPASS + XIAOZHI_DEV_CHILD_ID"
```

### Task A3: `evaluateDevBypass` pure decision function (fail-closed)

**Files:**
- Create: `src/xiaozhi/dev-bypass.ts`
- Test: `tests/xiaozhi_dev_bypass.test.ts` (create)

**Interfaces:**
- Produces:
  ```ts
  export const DEV_SESSION_TTL_SEC = 30 * 24 * 60 * 60;
  export type DevBypassInput = {
    e2eEnabled: string | undefined;
    bypassEnabled: string | undefined;
    devChildId: string | undefined;
    deviceId: string | undefined;
    deviceStatus: "unclaimed" | "active" | "revoked" | undefined; // undefined = device not found
    boundChildId: string | undefined;                             // undefined = no binding
  };
  export type DevBypassDecision = { ok: true; deviceId: string; childId: string } | { ok: false; reason: string };
  export function evaluateDevBypass(input: DevBypassInput): DevBypassDecision;
  export function devBypassActive(e2eEnabled: string | undefined, bypassEnabled: string | undefined): boolean;
  ```

- [ ] **Step 1: Write the failing tests**

```ts
// tests/xiaozhi_dev_bypass.test.ts
import { describe, expect, it } from "vitest";
import { devBypassActive, evaluateDevBypass, type DevBypassInput } from "../src/xiaozhi/dev-bypass";

const base: DevBypassInput = {
  e2eEnabled: "1",
  bypassEnabled: "1",
  devChildId: "child-1",
  deviceId: "aa:bb:cc:dd:ee:ff",
  deviceStatus: "active",
  boundChildId: "child-1",
};

describe("devBypassActive (fail-closed conjunction)", () => {
  it("true only when both flags are exactly '1'", () => {
    expect(devBypassActive("1", "1")).toBe(true);
    expect(devBypassActive(undefined, "1")).toBe(false); // production: E2E off
    expect(devBypassActive("1", undefined)).toBe(false); // bypass flag missing
    expect(devBypassActive("0", "1")).toBe(false);
    expect(devBypassActive("1", "true")).toBe(false);
  });
});

describe("evaluateDevBypass", () => {
  it("mints for an active, correctly-bound device", () => {
    expect(evaluateDevBypass(base)).toEqual({ ok: true, deviceId: base.deviceId, childId: "child-1" });
  });
  it("rejects when devChildId is unset", () => {
    expect(evaluateDevBypass({ ...base, devChildId: undefined }).ok).toBe(false);
  });
  it("rejects a missing Device-Id", () => {
    expect(evaluateDevBypass({ ...base, deviceId: undefined }).ok).toBe(false);
  });
  it("rejects an unknown device", () => {
    expect(evaluateDevBypass({ ...base, deviceStatus: undefined }).ok).toBe(false);
  });
  it("rejects a non-active device", () => {
    expect(evaluateDevBypass({ ...base, deviceStatus: "revoked" }).ok).toBe(false);
  });
  it("rejects an unbound device", () => {
    expect(evaluateDevBypass({ ...base, boundChildId: undefined }).ok).toBe(false);
  });
  it("rejects a binding to a different child", () => {
    expect(evaluateDevBypass({ ...base, boundChildId: "child-2" }).ok).toBe(false);
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pnpm vitest run tests/xiaozhi_dev_bypass.test.ts`
Expected: FAIL — `src/xiaozhi/dev-bypass.ts` does not exist.

- [ ] **Step 3: Implement the pure functions**

```ts
// src/xiaozhi/dev-bypass.ts
// Dev-only bring-up auth decision. Pure + fail-closed: see Milestone-1 spec §5.
export const DEV_SESSION_TTL_SEC = 30 * 24 * 60 * 60; // 30 days

export type DevBypassInput = {
  e2eEnabled: string | undefined;
  bypassEnabled: string | undefined;
  devChildId: string | undefined;
  deviceId: string | undefined;
  deviceStatus: "unclaimed" | "active" | "revoked" | undefined;
  boundChildId: string | undefined;
};

export type DevBypassDecision =
  | { ok: true; deviceId: string; childId: string }
  | { ok: false; reason: string };

export function devBypassActive(e2eEnabled: string | undefined, bypassEnabled: string | undefined): boolean {
  return e2eEnabled === "1" && bypassEnabled === "1";
}

export function evaluateDevBypass(input: DevBypassInput): DevBypassDecision {
  if (!devBypassActive(input.e2eEnabled, input.bypassEnabled)) return { ok: false, reason: "bypass_inactive" };
  if (!input.devChildId) return { ok: false, reason: "dev_child_unset" };
  if (!input.deviceId) return { ok: false, reason: "missing_device_id" };
  if (input.deviceStatus === undefined) return { ok: false, reason: "unknown_device" };
  if (input.deviceStatus !== "active") return { ok: false, reason: input.deviceStatus };
  if (input.boundChildId === undefined) return { ok: false, reason: "unbound" };
  if (input.boundChildId !== input.devChildId) return { ok: false, reason: "child_mismatch" };
  return { ok: true, deviceId: input.deviceId, childId: input.devChildId };
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pnpm vitest run tests/xiaozhi_dev_bypass.test.ts`
Expected: PASS (all cases).

- [ ] **Step 5: Commit**

```bash
git add src/xiaozhi/dev-bypass.ts tests/xiaozhi_dev_bypass.test.ts
git commit -m "feat(xiaozhi): fail-closed dev-bypass decision function"
```

### Task A4: Wire dev-bypass into `authenticateOta`

**Files:**
- Modify: `src/xiaozhi/router.ts` (`authenticateOta`, ~lines 70-98; imports)
- Test: `tests/xiaozhi_ota_bypass.test.ts` (create)

**Interfaces:**
- Consumes: `evaluateDevBypass`, `DEV_SESSION_TTL_SEC` (Task A3); `mintSessionToken(…, ttlSec)` (Task A1); existing `getDevice`, `getBinding`.
- Produces: `/xiaozhi/ota` returns a non-empty `websocket.token` for a provisioned dev device when the flags are on.

- [ ] **Step 1: Write the failing integration test**

```ts
// tests/xiaozhi_ota_bypass.test.ts
import { SELF } from "cloudflare:test";
import { env } from "cloudflare:workers";
import type { UUID } from "node:crypto";
import { describe, expect, it } from "vitest";
import { initDB_D1 } from "../src/datastore";
import { claimDevice, upsertDevice } from "../src/datastore/d1/devices";

const DEV_CHILD = "00000000-0000-7000-8000-0000000000aa" as UUID; // matches env.test XIAOZHI_DEV_CHILD_ID

async function seedChild(childId: UUID) {
  await env.D1_ROBO.prepare("INSERT OR IGNORE INTO children (id, name, age, cefr_level) VALUES (?, ?, ?, ?)")
    .bind(childId, "Bypass Child", 6, "pre-a1").run();
}
const DUMMY_JWK = { kty: "EC", crv: "P-256", x: "a".repeat(43), y: "b".repeat(43) } as JsonWebKey;

async function ota(deviceId: string) {
  return SELF.fetch("https://example.com/xiaozhi/ota", {
    method: "POST",
    headers: { "Content-Type": "application/json", "Device-Id": deviceId, "Client-Id": "test-client" },
    body: JSON.stringify({ application: { version: "2.2.6" } }),
  });
}

describe("/xiaozhi/ota dev-bypass", () => {
  it("mints a token for a provisioned+bound dev device", async () => {
    const db = initDB_D1(env);
    const deviceId = "11:22:33:44:55:66";
    await seedChild(DEV_CHILD);
    await upsertDevice(db, { deviceId, publicKey: DUMMY_JWK, status: "unclaimed" });
    await claimDevice(db, deviceId, DEV_CHILD, "test"); // sets status=active + binding
    const resp = await ota(deviceId);
    const body = (await resp.json()) as { websocket: { token: string; version: number }; device: { status: string } };
    expect(resp.status).toBe(200);
    expect(body.websocket.token.length).toBeGreaterThan(0);
    expect(body.device.status).toBe("ready");
  });

  it("returns setup_required (empty token) for an unprovisioned device", async () => {
    const resp = await ota("99:99:99:99:99:99");
    const body = (await resp.json()) as { websocket: { token: string }; device: { status: string } };
    expect(body.websocket.token).toBe("");
    expect(body.device.status).toBe("setup_required");
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pnpm vitest run tests/xiaozhi_ota_bypass.test.ts`
Expected: FAIL — first test gets an empty token (bypass not wired yet).

- [ ] **Step 3: Wire the bypass into `authenticateOta`**

In `src/xiaozhi/router.ts`, add the import near the top:

```ts
import { DEV_SESSION_TTL_SEC, evaluateDevBypass } from "./dev-bypass";
```

Replace the body of `authenticateOta` so the bypass runs first (the existing JWT `try` block stays unchanged below it):

```ts
async function authenticateOta(
  c: Context<AppContext>,
  authHeader: string | undefined,
): Promise<{ ok: true; sessionToken: string; verifiedDeviceId: string } | { ok: false; reason: string }> {
  const db = c.get("d1");
  if (!db) return { ok: false, reason: "db_unavailable" };

  // Dev-only bring-up bypass (fail-closed; trusts spoofable Device-Id, never reachable in prod).
  if (c.env.E2E_ENABLED === "1" && c.env.XIAOZHI_DEV_BYPASS === "1") {
    const deviceId = c.req.header("Device-Id");
    const device = deviceId ? await getDevice(db, deviceId) : undefined;
    const binding = deviceId ? await getBinding(db, deviceId) : undefined;
    const decision = evaluateDevBypass({
      e2eEnabled: c.env.E2E_ENABLED,
      bypassEnabled: c.env.XIAOZHI_DEV_BYPASS,
      devChildId: c.env.XIAOZHI_DEV_CHILD_ID,
      deviceId,
      deviceStatus: device?.status,
      boundChildId: binding?.child_id,
    });
    if (!decision.ok) return { ok: false, reason: `dev_bypass:${decision.reason}` };
    const sessionToken = await mintSessionToken(
      c.env, decision.deviceId, decision.childId as UUID, undefined, DEV_SESSION_TTL_SEC,
    );
    return { ok: true, sessionToken, verifiedDeviceId: decision.deviceId };
  }

  try {
    const deviceJwt = extractBearer(authHeader);
    const deviceId = getUnverifiedDeviceId(deviceJwt);
    const device = await getDevice(db, deviceId);
    if (!device) return { ok: false, reason: "unknown_device" };
    if (device.status !== "active") return { ok: false, reason: device.status };

    const publicJwk = JSON.parse(device.public_key) as JsonWebKey;
    const verified = await verifyDeviceJwt(deviceJwt, publicJwk);

    const binding = await getBinding(db, verified.deviceId);
    if (!binding) return { ok: false, reason: "unbound" };

    await consumeDeviceReplayNonce(c.env, verified);
    const sessionToken = await mintSessionToken(c.env, verified.deviceId, binding.child_id as UUID);
    return { ok: true, sessionToken, verifiedDeviceId: verified.deviceId };
  } catch (err) {
    const message = (err as Error).message;
    console.warn(`[xiaozhi-ota] auth failed: ${JSON.stringify(message)}`);
    return { ok: false, reason: err instanceof DeviceReplayError ? "replay" : "invalid_auth" };
  }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pnpm vitest run tests/xiaozhi_ota_bypass.test.ts`
Expected: PASS (both). Then `pnpm vitest run tests/xiaozhi_auth.test.ts` to confirm the JWT path is unbroken.
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/xiaozhi/router.ts tests/xiaozhi_ota_bypass.test.ts
git commit -m "feat(xiaozhi): wire fail-closed dev-bypass into OTA auth"
```

### Task A5: Return `version: 1` in the OTA websocket block

**Files:**
- Modify: `src/xiaozhi/router.ts` (`otaHandler`, the `responseBody` object, ~line 49)
- Test: `tests/xiaozhi_ota_bypass.test.ts` (extend)

**Interfaces:**
- Produces: `/xiaozhi/ota` response `websocket` object includes numeric `version: 1`.

- [ ] **Step 1: Add the failing assertion**

Append to the first test in `tests/xiaozhi_ota_bypass.test.ts` (after the token assertion):

```ts
    expect(body.websocket.version).toBe(1);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pnpm vitest run tests/xiaozhi_ota_bypass.test.ts`
Expected: FAIL — `websocket.version` is `undefined`.

- [ ] **Step 3: Add `version: 1` to the response**

In `otaHandler`, change the `websocket` field of `responseBody`:

```ts
    websocket: { url: wsUrl, token: authResult.ok ? authResult.sessionToken : "", version: 1 },
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pnpm vitest run tests/xiaozhi_ota_bypass.test.ts`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/xiaozhi/router.ts tests/xiaozhi_ota_bypass.test.ts
git commit -m "feat(xiaozhi): pin protocol version=1 in OTA websocket block"
```

### Task A6: Provisioning runbook + deploy notes

**Files:**
- Create: `docs/xiaozhi-tuni-p4-provisioning.md`

**Interfaces:** none (operational doc).

- [ ] **Step 1: Write the runbook**

Create `docs/xiaozhi-tuni-p4-provisioning.md` with the exact one-time steps (replace `<MAC>` with the P4's `Device-Id`, and host with the deployed worker):

````markdown
# Tuni P4 bring-up provisioning (dev-bypass)

Set the worker vars (production must NOT have these):
```bash
wrangler deploy            # ensure latest code is live
# dev/staging only:
wrangler secret put XIAOZHI_DEV_BYPASS         # value: 1   (or set in env vars)
# E2E_ENABLED=1 and XIAOZHI_DEV_CHILD_ID must also be set on the same env
```

One-time device provisioning (E2E routes; throwaway P-256 JWK is fine):
```bash
HOST=https://robo-worker.taskfi.workers.dev
CHILD=00000000-0000-7000-8000-0000000000aa     # = XIAOZHI_DEV_CHILD_ID
MAC=<MAC>                                        # device's Device-Id

curl -s $HOST/api/devices/mock-child -H 'content-type: application/json' \
  -d "{\"childId\":\"$CHILD\"}"
curl -s $HOST/api/devices/register -H 'content-type: application/json' \
  -d "{\"deviceId\":\"$MAC\",\"status\":\"unclaimed\",\"publicKey\":{\"kty\":\"EC\",\"crv\":\"P-256\",\"x\":\"<43-char-b64url>\",\"y\":\"<43-char-b64url>\"}}"
curl -s $HOST/api/devices/$MAC/claim -H 'content-type: application/json' \
  -d "{\"childId\":\"$CHILD\"}"
```
Generate a throwaway JWK with: `node -e "crypto.subtle.generateKey({name:'ECDSA',namedCurve:'P-256'},true,['sign']).then(k=>crypto.subtle.exportKey('jwk',k.publicKey)).then(j=>console.log(JSON.stringify({x:j.x,y:j.y})))"`

To migrate to production auth later: `unclaim` (NOT revoke) then re-register with the real key — see spec §5.6.
````

- [ ] **Step 2: Commit**

```bash
git add docs/xiaozhi-tuni-p4-provisioning.md
git commit -m "docs(xiaozhi): tuni-p4 dev-bypass provisioning runbook"
```

---

# Part B — Firmware `tuni-p4` board (`xiaozhi-esp32`)

All Part B commits run in `/Users/tung/xiaozhi-esp32`. Firmware has no host unit-test harness; the
test cycle for code tasks is **`idf.py build` succeeds**, and the milestone deliverable is the
on-device bring-up in Task B5. Source ESP-IDF first: `. ~/esp/esp-idf/export.sh`.

### Task B1: Auto-start-listening option + hook

**Files:**
- Modify: `main/Kconfig.projbuild` (add `config AUTO_START_LISTENING` near the `WAKE_WORD_TYPE` choice, ~line 735)
- Modify: `main/application.cc` (`HandleActivationDoneEvent`, ends ~line 408)

**Interfaces:**
- Produces: `CONFIG_AUTO_START_LISTENING` — when set, the device auto-enters the autostop listening loop after activation, with no wake word or button.

- [ ] **Step 1: Add the Kconfig option**

In `main/Kconfig.projbuild`, after the `endchoice` that closes `choice WAKE_WORD_TYPE`, add:

```kconfig
config AUTO_START_LISTENING
    bool "Auto-start the listening loop after activation (always-on open-mic, no wake word/button)"
    default n
    help
        When enabled, the device enters the default listening mode immediately
        after activation completes, instead of waiting for a wake word or button.
        Intended for boards with no wake word and no push-to-talk (e.g. tuni-p4).
```

- [ ] **Step 2: Add the auto-start hook**

In `main/application.cc`, at the end of `HandleActivationDoneEvent()` (after the `Schedule([this]() { ... PlaySound ... });` block, before the closing `}` at ~line 408), add:

```cpp
#ifdef CONFIG_AUTO_START_LISTENING
    // Always-on open-mic: no wake word / no button. From Idle, ToggleChatState
    // opens the audio channel in GetDefaultListeningMode() (AutoStop when AEC is
    // off); the AutoStop + TTS-stop→Listening cycle then sustains the conversation.
    Schedule([this]() { ToggleChatState(); });
#endif
```

- [ ] **Step 3: Sanity-build against an existing P4 board (option compiles, default-off is a no-op)**

Run:
```bash
. ~/esp/esp-idf/export.sh
cd /Users/tung/xiaozhi-esp32
python scripts/release.py esp-p4-function-ev-board --name esp-p4-function-ev-board
```
Expected: build SUCCEEDS (the `#ifdef` is inactive when `CONFIG_AUTO_START_LISTENING` is unset).

- [ ] **Step 4: Commit**

```bash
git add main/Kconfig.projbuild main/application.cc
git commit -m "feat(app): CONFIG_AUTO_START_LISTENING for always-on open-mic boards"
```

### Task B2: Create the `tuni-p4` board files

**Files:**
- Create: `main/boards/tuni-p4/config.h`
- Create: `main/boards/tuni-p4/config.json`
- Create: `main/boards/tuni-p4/tuni_p4.cc`

**Interfaces:**
- Consumes: `CONFIG_AUTO_START_LISTENING` (Task B1).
- Produces: a `WifiBoard` subclass `TuniP4` registered via `DECLARE_BOARD`, with onboard ES8311 audio and `NoDisplay`.

- [ ] **Step 1: Write `config.h`**

```c
// main/boards/tuni-p4/config.h
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_13
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_10
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_12
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_11
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_9

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_53
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_7
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_8
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO  GPIO_NUM_35

#endif // _BOARD_CONFIG_H_
```

- [ ] **Step 2: Write `config.json`** (two silicon-revision variants)

```json
{
    "target": "esp32p4",
    "builds": [
        {
            "name": "tuni-p4",
            "sdkconfig_append": [
                "CONFIG_BOARD_TYPE_TUNI_P4=y",
                "CONFIG_SLAVE_IDF_TARGET_ESP32C6=y",
                "CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y",
                "CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y",
                "CONFIG_USE_AUDIO_PROCESSOR=y",
                "CONFIG_SR_VADN_VADNET1_MEDIUM=y",
                "CONFIG_WAKE_WORD_DISABLED=y",
                "CONFIG_AUTO_START_LISTENING=y",
                "CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y",
                "CONFIG_ESP32P4_REV_MIN_100=y"
            ]
        },
        {
            "name": "tuni-p4-p4x",
            "sdkconfig_append": [
                "CONFIG_BOARD_TYPE_TUNI_P4=y",
                "CONFIG_SLAVE_IDF_TARGET_ESP32C6=y",
                "CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y",
                "CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y",
                "CONFIG_USE_AUDIO_PROCESSOR=y",
                "CONFIG_SR_VADN_VADNET1_MEDIUM=y",
                "CONFIG_WAKE_WORD_DISABLED=y",
                "CONFIG_AUTO_START_LISTENING=y",
                "CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n",
                "CONFIG_ESP32P4_REV_MIN_300=y"
            ]
        }
    ]
}
```

- [ ] **Step 3: Write `tuni_p4.cc`**

```cpp
// main/boards/tuni-p4/tuni_p4.cc
#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "display/display.h"   // NoDisplay
#include "button.h"
#include "config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "TuniP4"

class TuniP4 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Display* display_ = nullptr;

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = { .enable_internal_pullup = 1 },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeButtons() {
        // No press-to-talk. BOOT only enters Wi-Fi config while still starting.
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
            }
        });
    }

public:
    TuniP4() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeButtons();
        display_ = new NoDisplay();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_1, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }
};

DECLARE_BOARD(TuniP4);
```

- [ ] **Step 4: Commit (board not yet registered — builds come after B3)**

```bash
git add main/boards/tuni-p4/
git commit -m "feat(board): add tuni-p4 board files (ES8311 + NoDisplay)"
```

### Task B3: Register the board in Kconfig + CMakeLists

**Files:**
- Modify: `main/Kconfig.projbuild` (the `BOARD_TYPE` choice, near the P4 boards ~line 374)
- Modify: `main/CMakeLists.txt` (board dispatch, near `BOARD_TYPE_WAVESHARE_ESP32_P4_NANO` ~line 474)

**Interfaces:**
- Consumes: `main/boards/tuni-p4/` (Task B2).
- Produces: selectable `CONFIG_BOARD_TYPE_TUNI_P4` mapping to `BOARD_TYPE "tuni-p4"`.

- [ ] **Step 1: Add the BOARD_TYPE config**

In `main/Kconfig.projbuild`, inside the `choice BOARD_TYPE`, near the other P4 boards (e.g. right before `config BOARD_TYPE_WAVESHARE_ESP32_P4_NANO`), add:

```kconfig
    config BOARD_TYPE_TUNI_P4
        bool "Tuni P4 (Waveshare ESP32-P4-NANO, onboard audio, no display)"
        depends on IDF_TARGET_ESP32P4
```

- [ ] **Step 2: Add the CMake dispatch branch**

In `main/CMakeLists.txt`, next to the `elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_NANO)` branch, add:

```cmake
elseif(CONFIG_BOARD_TYPE_TUNI_P4)
    set(BOARD_TYPE "tuni-p4")
```

- [ ] **Step 3: Commit**

```bash
git add main/Kconfig.projbuild main/CMakeLists.txt
git commit -m "feat(board): register BOARD_TYPE_TUNI_P4"
```

### Task B4: Build the `tuni-p4` firmware

**Files:** none (build verification).

**Interfaces:**
- Consumes: Tasks B1-B3.

- [ ] **Step 1: Determine the silicon revision**

Flash-less check from the bootloader log on the next boot, or:
```bash
esptool.py --port $(ls /dev/cu.usbmodem* | head -1) chip_id
```
Note whether the chip is rev `< v3` (use `--name tuni-p4`) or rev `≥ v3` (use `--name tuni-p4-p4x`).

- [ ] **Step 2: Build the matching variant**

```bash
. ~/esp/esp-idf/export.sh
cd /Users/tung/xiaozhi-esp32
python scripts/release.py tuni-p4 --name tuni-p4        # or: --name tuni-p4-p4x
```
Expected: compiles to completion and produces `releases/v2.2.6_tuni-p4.zip` (or `…_tuni-p4-p4x.zip`).

- [ ] **Step 3: Verify the effective config**

Run: `grep -E "CONFIG_BOARD_TYPE_TUNI_P4=y|CONFIG_WAKE_WORD_DISABLED=y|CONFIG_SR_VADN_VADNET1_MEDIUM=y|CONFIG_AUTO_START_LISTENING=y" sdkconfig`
Expected: all four present.

- [ ] **Step 4: Commit (lockfile/build artifacts only if the repo tracks them; otherwise skip)**

```bash
git status   # if dependencies.lock changed, commit it:
git add dependencies.lock 2>/dev/null && git commit -m "chore: tuni-p4 dependency lock" || echo "nothing to commit"
```

### Task B5: Flash + on-device bring-up (end-to-end)

**Files:** none (hardware-in-the-loop verification). **Prerequisite:** Part A deployed; device provisioned via `docs/xiaozhi-tuni-p4-provisioning.md` (Task A6).

**Interfaces:**
- Consumes: all prior tasks + a live, provisioned backend.

- [ ] **Step 1: Erase NVS, then flash**

```bash
PORT=$(ls /dev/cu.usbmodem* | head -1)
idf.py -p $PORT erase-flash        # clears stale protocol version / token
idf.py -p $PORT flash monitor
```

- [ ] **Step 2: Verify boot + Wi-Fi**

Expected serial log: clean boot, C6 associates. If no Wi-Fi creds, press BOOT while starting → config AP `…` → set creds at `http://192.168.4.1`.

- [ ] **Step 3: Verify OTA returns a token + version**

Expected serial log around the OTA call: a `websocket` config saved; no `No websocket section found!`. (Backend log shows `[xiaozhi-ota] … auth=ok`.)

- [ ] **Step 4: Verify WS + always-on VAD loop**

Expected: WS connects, `hello` exchanged, firmware logs `version: 1`; device auto-enters listening with **no** press. Speak → backend log shows a transcript → robot replies through the speaker → after TTS stops it auto-returns to listening. Speak again to confirm the loop repeats.

- [ ] **Step 5: Record the result**

Append a short "bring-up result" note (date, chip revision, variant, what worked / what didn't) to `DSOLUTION_HANDOFF.md`'s successor or a new `TUNI_P4_BRINGUP.md`, and commit:

```bash
git add TUNI_P4_BRINGUP.md
git commit -m "docs: tuni-p4 milestone-1 bring-up result"
```

---

## Notes for the executor

- Two repos: run each commit in the repo named at the top of its Part. Do not cross-commit.
- Backend tasks are strict TDD (vitest-pool-workers runs in workerd). Firmware "tests" are build + the Task B5 hardware checklist — do not fabricate firmware unit tests.
- If `idf.py`/`release.py` reports the `ESP_HOSTED_*` keys differ for this board, cross-check the building `esp32-p4-nano` variant and adjust `config.json` (spec §7 risk).
- Keep server-AEC and device-AEC OFF (no `CONFIG_USE_SERVER_AEC`) or the listening mode flips to realtime/full-duplex.
