# GR IIIx hardware verification — `codex/feat-gr3-dual-protocol-compliance`

Testing carried out at the maintainer's request in PR #8: *"wait for an
experienced developer who actually owns a GR III to help properly test,
verify, and extend the support."*

| Field | Value |
| --- | --- |
| Date | 2026-07-28 |
| Branch tested | `codex/feat-gr3-dual-protocol-compliance` at `2d16189` |
| Camera | RICOH GR IIIx, firmware 1.60 |
| Board | M5Stack StickS3, `m5stack-sticks3` environment |
| NVS | Cleared — full first-time pairing exercised |
| Native tests | 61/61 passed |
| Build | RAM 76,924 / 327,680 (23.5%), Flash 1,348,397 / 3,342,336 (40.3%) |

Passkeys, passphrases and full SSIDs are omitted from every log excerpt below.

## Headline: protocol detection is correct, one gate blocks everything

Two-phase read-only detection classifies the GR IIIx correctly on the first
attempt, with no GR IV false positives:

```
BLE discovery: generation=GR3_FAMILY elapsed=16800ms complete=1 connected=1
  shared_wlan_service=1 shared_network_type=1
  gr4_power_handle=0 gr4_wlan_scope=0 gr4_uuid_map=0 gr4_wlan_handles=0
```

This was also confirmed offline beforehand against a full GATT dump of the
camera: no GR IIIx characteristic sits at any of the GR IV fixed handles. Its
Camera Power is at `0x00BB`, not `0x00EB`, and its attribute table ends at
`0x0109`, below every GR IV WLAN handle (`0x0135`–`0x0140`). So
`partialGr4FixedHandleEvidence` is false and the GR III branch is taken.

Pairing then completes properly:

```
BLE security: passkey submitted
BLE security: complete bonded=1 encrypted=1 authenticated=1 key_size=16
BLE profile selected=GR3_FAMILY security_profile=GR3_PASSKEY source=profile_or_readonly_discovery
BLE: connected secure connect_ms=1099 security_ms=12001 total_ms=13100
```

And then the session stops dead:

```
BLE: operation mode read value=0x03 state=OTHER
WiFi blocked: profile=GR3_FAMILY encrypted=1 authenticated=1 operation_mode=OTHER read_ok=1
Flow: CHECKING_CAMERA_POWER -> CAMERA_POWER_OFF (GR3 operation/security gate)
```

`operationModeAllowsWifi()` allows exactly one Operation Mode value on GR III:

```cpp
if (profile.generation == RicohProtocolGeneration::Gr3Family) {
  return operationModeReadSucceeded && mode == RicohCameraOperationMode::Capture;
}
```

A GR IIIx sitting ready to shoot reports **`Other` (0x03)**, not `Capture`
(0x00). The read succeeds, the value is never `0x00`, so Wi-Fi activation is
refused for the entire session — no WLAN, no `/v1/props`, no LiveView. This
reproduces on every single attempt.

`0x03` is a normal, capture-ready state on this generation. Independent
evidence from a separate headless probe on the same camera: mode `0x03` was
read, and the full chain then completed — WLAN raised over the Network Type
UUID, credentials read, `/v1/props` answered, LiveView sustained at 11.4 fps
with zero dropped frames. It is not a standby state.

This is a plausible cause of the GR III "connection issue" reported against
PR #8, and it is invisible on GR IV, whose gate is a denylist of the two
standby modes rather than an allowlist of one.

### The fix

Keep the stricter GR III rule that the mode read must succeed — an unknown
mode must still never authorise a WLAN write on this generation — and reject
the same two standby modes GR IV rejects:

```cpp
if (profile.generation == RicohProtocolGeneration::Gr3Family) {
  return operationModeReadSucceeded &&
         mode != RicohCameraOperationMode::BleStartup &&
         mode != RicohCameraOperationMode::PowerOffTransfer;
}
```

With that change, the same board and camera reach LiveView in 59 seconds from
power-on:

```
BLE WiFi params profile=GR3_FAMILY ssid_present=1 passphrase_present=1 bssid_present=0 channel=6 wait_ms=1902
WiFi: connect completed in 655ms channel=6 status=CONNECTED
WiFi: connected ip=192.168.0.5 rssi=-28
[I][HTTP] props fetched in 581ms
[I][PREVIEW] liveview opened in 503ms
Flow: PREVIEW_STARTING -> PREVIEW_RUNNING (LiveView opened)
[I][FRAME] stream=191 rendered=191 dropped=0
```

Verified on the panel, not only in the statistics.

## Test matrix

| # | Test | GR IIIx | Evidence |
| ---: | --- | --- | --- |
| 1 | Pair from menu, identify `GR3_FAMILY` | PASS | `generation=GR3_FAMILY`, all four `gr4_*` evidence flags 0 |
| 2 | Camera shows a random six-digit code | PASS | Shown on camera; value not recorded |
| 3 | Enter the code and bond | PASS | `bonded=1 encrypted=1 authenticated=1 key_size=16` |
| 4 | StickS3 restart needs no re-entry | PASS | Survived a reflash; reconnected and read credentials with no prompt |
| 5 | Read SSID / passphrase / channel | PASS | `ssid_present=1 passphrase_present=1 bssid_present=0 channel=6` |
| 6 | Raise AP over UUID, join Wi-Fi | PASS **after fix** | Blocked by the Operation Mode gate before it |
| 7 | `/v1/props` | PASS | `props fetched in 581ms` |
| 8 | Sustained `/v1/liveview` | PASS | 3.2 fps at half scale, `stream=191 rendered=191 dropped=0`, confirmed on the panel |
| 9 | BLE AF shutter, LiveView holds | PARTIAL | Picture taken and confirmed on the camera; preview does not survive it — see below |
| 16 | Wi-Fi / LiveView disconnect recovery | PASS | Recovered via cached params in 238–630 ms |

`bssid_present=0` at test 5 is expected: the GR IIIx exposes neither WLAN
Security nor BSSID, unlike the GR IV. Only the fast-reconnect path is affected.

Tests 10–15 are recorded separately as they are exercised.

### Test 9: the shutter works, the preview does not survive it

The picture itself is taken correctly — confirmed on the camera, not only in
the log.

```
BLE: Ricoh shutter OperationRequest START param=1 autofocus=1
LiveView stall: frame_idle_ms=7498 stream_idle_ms=1 timeout_ms=5000
Camera recovery: LiveView frame stall watchdog
Flow: PREVIEW_RUNNING -> BLE_READY (LiveView frame stall watchdog)
   ... WLAN re-raised, Wi-Fi rejoined, /v1/props, LiveView reopened ...
Flow: PREVIEW_STARTING -> PREVIEW_RUNNING (LiveView opened)
```

While the GR IIIx takes the picture it stops serving `/v1/liveview` for longer
than `LIVEVIEW_STALL_TIMEOUT_MS` (5 s) — 7.5 s of frame idle here. The
watchdog reads that as a dead link and tears the whole chain down to
`BLE_READY`, rebuilding BLE, WLAN, Wi-Fi, HTTP and LiveView. The screen is
frozen for about eleven seconds after every shot.

Worth noting that `stream_idle_ms=1` at the moment the watchdog fires: bytes
were still arriving, only complete frames were not. A shutter-aware grace
period, or keying the watchdog off stream idleness rather than frame idleness,
would avoid the teardown. The recovery path itself behaved well.

## The Operation Mode value is not stable

Worth recording for anyone diagnosing this without the hardware:

```
BLE: operation mode read value=0x03 state=OTHER     <- first check after pairing; blocked
BLE: operation mode read value=0x00 state=CAPTURE   <- later checks, session established
```

The camera reports `Other` on the check that immediately follows pairing and
`Capture` on later checks within an established session. So the original
allowlist did not fail uniformly — it failed intermittently, and specifically
at the one moment the gate is consulted on a fresh connection. That is a
plausible reason the failure reports were hard to reproduce.

## Secondary findings

**GATT discovery takes 16.8 seconds**, measured consistently across four
separate connections (`elapsed=16800ms`, `16601ms`, `16601ms`). The camera
abandons its pairing ceremony after roughly a minute, so discovery consumes
about a third of the budget before the user is asked for anything. Combined
with the entry method below, first-time pairing failed repeatedly before it
succeeded.

**Button passkey entry is impractical at this timeout.** Entry is Button A to
increment a digit, Button B to advance, Button A held to submit, inside
`RICOH_BLE_PASSKEY_ENTRY_WAIT_MS` of 45 s — a worst case of 54 presses plus
holds, against a camera-side window that is already partly spent. Every
attempt via buttons failed; the first attempt via the serial path in
`waitForEncryptedConnection()` succeeded, with the code reaching the board
about two seconds after it appeared on the camera. The serial path works well
and is worth documenting; the button path would benefit from a longer window,
a coarse/fine step, or starting each digit from the previous one.

## Reproducing

The offline part of this — checking that the detection evidence resolves
correctly for a given camera — needs only a GATT dump and no firmware at all.
Happy to share the GR IIIx dump (53 characteristics across 6 services) if it
is useful for future generations.
