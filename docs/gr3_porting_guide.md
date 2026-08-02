# RICOH GR III porting guide

## Status

The GR III protocol path is working on a physical RICOH GR III HDF. The firmware checks for the GR III WLAN service and Network Type characteristic before it uses that path. The existing GR IV path still uses its fixed GATT handles.

PR #8 contains an earlier GR III-family implementation. Its test record says a GR IIIx passed pairing, WLAN credential reads, `/v1/props`, `/v1/liveview`, and BLE shutter tests. PR #9 later reverted that work. This branch rebuilds the generation split and adds first-time passkey entry on the StickS3.

On August 1, 2026, a clean GR III HDF test completed the following path:

- camera-displayed passkey entry and authenticated BLE bonding;
- GR III protocol detection through targeted UUID discovery;
- camera Wi-Fi activation and credential reads;
- connection to the camera access point;
- a successful `/v1/props` request identifying `RICOH GR III HDF`;
- LiveView startup with 30 rendered frames and no drops during the recorded check.

The same run reached the shared BLE shutter service and completed shutter warm-up. A separate button-press log should be attached if shutter dispatch is claimed as part of a release test.

Implemented now:

- post-connect protocol detection instead of choosing GR III/IV from the advertised name;
- GR III WLAN service and characteristic discovery by UUID;
- GR III AP activation through the Network Type characteristic;
- GR III SSID, passphrase, and channel reads through UUID characteristics;
- GR III Camera Power read/notification through its characteristic UUID;
- an authenticated-bond requirement before any GR III Wi-Fi write;
- a fresh `CAPTURE` operation-mode requirement before the GR III AP write;
- reuse of the existing Shooting Service and HTTP `/v1/liveview` path;
- a six-digit passkey screen operated entirely with the StickS3 buttons.

For the first pairing, copy the six-digit code shown by the camera. Button A changes the highlighted digit and a short press of Button B accepts it. USB serial entry remains available for development, but it is not required.

## BLE differences between GR III and GR IV

The downstream Wi-Fi/HTTP/display path is shared. The incompatible part is the BLE bootstrap:

| Function | GR IV path | GR III path |
| --- | --- | --- |
| Turn camera AP on | Write `0x01` to fixed handle `0x0135` | Write `0x01` to Network Type UUID |
| Read SSID | Fixed handle `0x0138` | SSID UUID |
| Read passphrase | Fixed handle `0x013A` | Passphrase UUID |
| Read channel | Fixed handle `0x013E` | Channel UUID |
| Read power | Fixed handle `0x00EB` | Camera Power UUID |
| Pairing | Current GR IV secure-link flow | Authenticated GR III bond with camera-displayed passkey |
| Live view | `GET /v1/liveview` | Same endpoint |

GATT handles depend on the server layout, so the firmware must not use GR IV handles on a GR III. It enables the GR III path only after finding both the WLAN service and Network Type characteristic.

## Known GR III UUIDs

| Purpose | UUID |
| --- | --- |
| WLAN service | `F37F568F-9071-445D-A938-5441F2E82399` |
| Network Type / AP on | `9111CDD0-9F01-45C4-A2D4-E09E8FB0424D` |
| SSID | `90638E5A-E77D-409D-B550-78F7E1CA5AB4` |
| Passphrase | `0F38279C-FE9E-461B-8596-81287E8C9A81` |
| Channel | `51DE6EBC-0F22-4357-87E4-B1FA1D385AB8` |
| Camera service | `4B445988-CAA0-4DD3-941D-37B4F52ACA86` |
| Camera Power | `B58CE84C-0666-4DE9-BEC8-2D27B27B3211` |
| Operation Mode | `1452335A-EC7F-4877-B8AB-0F72E18BB295` |
| Shooting service | `9F00F387-8345-4BBC-8B92-B87B52E3091A` |
| Operation Request | `559644B8-E0BC-4011-929B-5CF9199851E7` |

Sources:

- <https://github.com/dm-zharov/ricoh-gr-bluetooth-api>
- repository history: <https://github.com/sky18Dragon/RICOH-GR-Live-View-Shooting/pull/8>
- GR IIIx HTTP exploration: <https://notes.secretsauce.net/notes/2022/06/16_ricoh-gr-iiix-80211-reverse-engineering.html>

## Optional Android Image Sync capture

An Android phone is not required. Start with the StickS3 diagnostic firmware when the hardware arrives. If that test fails, an Android Bluetooth HCI capture can show the request order, authentication state, characteristic access, notification setup, and timing used with your camera.

1. Install the official Image Sync app, but do not pair the GR III yet.
2. On Android, enable Developer options and turn on **Bluetooth HCI snoop log**.
3. Restart Bluetooth so full HCI logging takes effect.
4. Remove any existing phone/camera pairing on both sides.
5. Start a fresh capture, then perform only this sequence:
   - discover the camera;
   - pair and enter/confirm the six-digit code;
   - open remote shooting;
   - wait until live view is stable;
   - take one autofocus photo;
   - leave remote shooting.
6. Immediately create an Android bug report (`adb bugreport` is fine).
7. Put the raw bug report and BTSnoop file under `captures/private/`. That directory is ignored by Git.

The HCI log can contain the camera Wi-Fi passphrase and pairing data. Do not upload the raw capture. Keep it in the private workspace and commit only a redacted operation timeline.

Android's official debugging documentation explains where BTSnoop logs are stored and how to extract them from a bug report:
<https://source.android.com/docs/core/connect/bluetooth/verifying_debugging>

### Static APK inspection

If the HCI trace leaves a specific question, pull the APK from your own Android device and inspect it locally with JADX. Search for the UUIDs above, then follow their call sites. Check:

- the write payload sent to Network Type;
- the first protected read that triggers pairing;
- required security/bond flags;
- retry delays after AP activation;
- any model checks distinguishing GR III from GR IIIx/HDF;
- the HTTP host and live-view request.

Do not add the APK, decompiled sources, signing material, or camera credentials to this repository.

## Seeed and Home Assistant Bluetooth proxies

A normal BLE proxy can report advertisement names, service UUIDs, addresses, and RSSI. It cannot normally inspect an encrypted, bonded GATT session, so it cannot show protected GR III reads and writes.

## Build and test on the StickS3

First run the native tests:

```sh
platformio test -e native
```

Build and upload the firmware:

```sh
platformio run -e m5stack-sticks3
platformio run -e m5stack-sticks3 --target upload
platformio device monitor --baud 115200
```

The serial monitor is optional. On the first bond, enter the camera's six-digit code on the StickS3.

Expected milestones:

1. `BLE: protocol profile=GR3_FAMILY`
2. secure link reports an authenticated bond
3. power is `0x01` and operation mode is `CAPTURE`
4. `BLE: Wi-Fi open requested method=GR3_NETWORK_TYPE_UUID`
5. credential log shows SSID/passphrase present without printing either secret
6. Wi-Fi connects to the camera
7. `/v1/props` returns successfully
8. `/v1/liveview` produces valid JPEG frames
9. Button A produces one AF capture without stopping live view

## Hardware test matrix

Record pass/fail and elapsed times without credentials:

| Test | Expected |
| --- | --- |
| Clean first pairing | Camera code accepted; authenticated bond saved |
| StickS3 reboot | Reconnects without asking for the code |
| Camera reboot | Reconnects after the camera is awake |
| Portrait startup | BLE and credentials ready; no Wi-Fi STA join |
| Rotate to landscape | Wi-Fi joins and live view starts |
| Rotate to portrait | Live view and Wi-Fi stop; BLE stays connected |
| Button A | One AF shot |
| Camera sleep/off | No automatic AP write or lens wake |
| Live-view stall | Existing supervisor recovers |
| Pairing reset | Old bond/profile removed; clean pairing works again |

## Remaining work

1. Run the full matrix on a standard GR III and a GR IIIx.
2. Record a Button A shutter dispatch on the GR III HDF.
3. Persist the detected protocol generation with the saved camera profile.
4. Add stale-bond recovery for the case where the camera-side bond is deleted.
5. Compare a GR III HCI trace with the prior GR IIIx evidence only if a remaining behavior differs.
6. Run the updated branch on GR IV hardware before release.
