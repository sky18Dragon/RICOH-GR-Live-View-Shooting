# Camera protocol profiles

This layer separates model-specific RICOH BLE protocol data from connection,
Wi-Fi, LiveView, shutter, recovery, UI, and state-machine flows. GR IV HDF is
the only registered and connectable protocol. `RicohGr3x` is an enum reservation
only: it has no GATT profile, passkey-entry UI, WLAN flow, or LiveView support.

## Runtime resolution and selection

At startup the v4 camera profile store validates the optional `camera_model`
value. `CameraProtocolRegistry::resolve()` returns all four parts of the
decision:

- `requestedModel`: the validated model requested by NVS, or `Unknown`;
- `resolvedModel`: the registered model used at runtime;
- `profile`: the static registered profile;
- `usedFallback`: whether the request was replaced by the GR IV HDF default.

GR IV HDF resolves to itself. GR IIIx, `Unknown`, and illegal values resolve to
GR IV HDF with `usedFallback=true`. The startup log records requested, resolved,
fallback, and source values. A successful BLE connection persists the client's
active model unconditionally, so an unregistered or stale requested model cannot
remain associated with the connected camera identity.

`RicohBleClient` and `CameraProtocolSelection` store a `CameraModel`, not a
caller-owned `CameraProtocolProfile` pointer. Each protocol access resolves the
model through `CameraProtocolRegistry`, whose profiles have static lifetime.
Model changes are rejected while BLE is connected, scanning, connecting, or
performing security negotiation. The GAP handler stores only an atomic raw model
value and resolves a validated static profile when handling a notification.

## Validation and capabilities

Every production profile exposed by the registry must pass
`validateCameraProtocolProfile()`. Validation checks model/name, base service
UUIDs, and capability-dependent handles and UUIDs. A capability cannot be true
when the data required to implement it is missing.

Runtime code also enforces profile capabilities:

- Wi-Fi activation and credential reads require `supportsWifiLiveView` plus the
  required WLAN handles;
- power notification requires `hasPowerStateNotify` plus power/CCCD handles;
- shutter requires `supportsBleShutter` plus all shooting UUIDs;
- `ExistingDefault` retains the current display/confirm security behavior;
- `PasskeyEntry` is explicitly unsupported and prevents BLE initialization or
  connection. It does not fall back to `ExistingDefault`.

## Discovery versus active protocol

`CameraDiscoveryRegistry` enumerates discovery service UUIDs from every
registered and validated profile. BLE advertisement matching and candidate
metadata use this complete set, not only the active protocol. Discovery does not
perform automatic model detection in this phase; selection still happens before
connection.

## Persistence compatibility

Only an exact `proto_ver=4` with a valid `camera_model` is accepted. Older,
future, missing, and illegal model metadata resolve from `Unknown` without
deleting the stored BLE identity or Wi-Fi cache. After connection,
`saveConnectedCamera()` writes version, active model, name, address/type, bond,
and camera IP through one store API call. It is not an atomic NVS transaction.
A BLE address change invalidates a Wi-Fi cache associated with the previous
address. Every Preferences write result is checked and a failed write is
reported without dropping the live BLE link.

## Adding a future model

1. Add or enable its `CameraModel` enum value.
2. Add a static profile under `src/protocol/profiles/`.
3. Make the profile pass `CameraProtocolValidator` and register it.
4. Add its discovery UUIDs through the registered profile.
5. Add model detection and migration policy outside the common transport flow.
6. Implement and test its pairing strategy before enabling connection.
7. Add profile value, capability, registry, migration, and lifecycle tests.

The AppController, Wi-Fi preview service, HTTP API, MJPEG decoder, common state
machine, and generic shutter flow must remain free of model-specific branches.

## GR IV HDF hardware regression checklist

### First connection

- Clear StickS3 NVS and the camera pairing record.
- Confirm discovery, ExistingDefault pairing, encryption, and bond creation.
- Confirm the saved model is GR IV HDF.

### Migration and fallback

- With legacy v3 data, confirm runtime fallback to GR IV without losing the BLE
  address or Wi-Fi cache, then confirm connection upgrades model metadata to v4.
- With v4 model value 2, confirm requested GR IIIx resolves to GR IV with
  fallback logged and the stored model is corrected after connection.

### WLAN, LiveView, and shutter

- Confirm existing WLAN ON and credential reads, including BSSID/channel cache.
- Confirm `/v1/props` and `/v1/liveview`, frame rate, memory, and UI behavior.
- Confirm Button A autofocus/shutter and preview recovery are unchanged.

### Power, recovery, and switching

- Reboot with the existing bond, exercise standby and recovery, and confirm the
  existing power/operation-mode safety flow.
- Attempt a model change while connected and confirm it fails without disconnect
  or changing the active handles.

### Pairing reset

- Long-press Button B and confirm BLE bonds, camera identity, `camera_model`, and
  associated Wi-Fi cache are cleared before returning to pairing scan.
