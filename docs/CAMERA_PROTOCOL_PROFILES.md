# Camera protocol profiles

This phase separates model-specific RICOH BLE protocol data from the existing
connection, Wi-Fi, LiveView, shutter, recovery, UI, and state-machine flows.
GR IV HDF remains the only enabled runtime protocol. The GR IIIx enum is a
reservation only; there is no GR IIIx GATT profile, pairing flow, or connection
branch in this phase.

## Runtime selection

At startup, the v4 camera profile store validates the optional `camera_model`
value. A registered model selects its static protocol profile. Missing legacy
data, invalid values, `Unknown`, and unregistered models fall back to
`CameraProtocolRegistry::defaultProfile()`, which is always GR IV HDF.
Selection happens before the first BLE scan or connection and is not changed
while connected.

`BleCameraService` delegates profile selection to `RicohBleClient` without
copying protocol state. The client and its GAP callback read handles, values,
UUID strings, and capability flags from the same static profile. NimBLE types
are constructed only at the point of BLE use.

## Adding a future model

1. Add or enable its `CameraModel` enum value.
2. Add a static profile under `src/protocol/profiles/`.
3. Register the profile in `CameraProtocolRegistry`.
4. Add model detection and migration policy outside the common transport flow.
5. Add profile value, capability, registry, and migration tests.
6. If pairing differs, add a separate pairing strategy based on `pairingMode`.

The AppController, Wi-Fi preview service, HTTP API, MJPEG decoder, common state
machine, and generic shutter flow should not need model-specific changes.

## GR IV HDF hardware regression checklist

### First connection

- Clear StickS3 NVS and the camera pairing record.
- Confirm the camera is discovered, pairs without a new security-input UI, and
  saves its BLE address and bond.

### WLAN

- Confirm WLAN ON writes the existing GR IV HDF WLAN enable handle/value.
- Confirm SSID, passphrase, security, frequency, and BSSID are read over BLE.
- Confirm the StickS3 connects to the camera AP.

### LiveView and shutter

- Confirm `/v1/props` and `/v1/liveview` work.
- Compare frame rate, free memory, stream stability, and UI flicker with the
  pre-refactor firmware.
- Confirm Button A autofocus/shutter behavior and LiveView recovery are unchanged.

### Recovery and cached connection

- Reboot the StickS3 and reconnect with the existing bond.
- Put the camera in standby, restore it, and verify the existing recovery flow.
- Confirm cached Wi-Fi and BSSID/channel fast connection still work.

### Pairing reset

- Long-press Button B and confirm BLE bonds, camera identity, and associated
  Wi-Fi cache are cleared before returning to pairing scan.
