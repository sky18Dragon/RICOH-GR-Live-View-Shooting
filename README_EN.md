<p align="center">
  <a href="./README.md"><img alt="简体中文" src="https://img.shields.io/badge/简体中文-EAEAEA?style=for-the-badge&labelColor=EAEAEA&color=111111" /></a>
  <a href="./README_EN.md"><img alt="English" src="https://img.shields.io/badge/English-111111?style=for-the-badge" /></a>
</p>

<p align="center">
  <img src="docs/images/FinalOutput_EN.png" alt="RICOH GR Live View Shooting" width="100%" />
</p>

<h1 align="center">RICOH GR Live View Shooting</h1>

<p align="center">
  A remote shutter and wireless live-view firmware for RICOH GR cameras, running on the M5Stack StickS3.
</p>

GR III/IV use BLE for identification, pairing, WLAN control, and shutter. GR II follows the official GR Remote design and uses only manually enabled Wi-Fi, HTTP shutter commands, and MJPEG LiveView.

> [!IMPORTANT]
> Select the correct camera generation in the StickS3 guide before first use. Select **GR II** for GR II, **GR III** for the GR III/IIIx families, and **GR IV** for the GR IV family.

## Support Status

| Camera | Current Status | Transport |
| :--- | :---: | :--- |
| RICOH GR III | Implemented; full independent regression still required | GR III Family UUID protocol and dynamic six-digit passkey |
| RICOH GR IIIx | Hardware verified | Uses the same pairing and communication parameters as the GR III |
| RICOH GR III HDF / GR IIIx HDF | Experimental | Currently treated as GR III Family; no independent hardware evidence |
| RICOH GR IV | Hardware verified | GR IV Legacy fixed-handle protocol |
| RICOH GR IV HDF | Hardware verified | Uses the same generation protocol as the GR IV |
| RICOH GR II | Hardware verified | No Bluetooth; manually enabled Wi-Fi and official GR Remote HTTP API |

See the [project architecture](docs/project_overview.md), [BLE protocol notes](docs/ricoh_ble_protocol.md), and [known issues](docs/known_issues.md) for evidence boundaries and safety rules.

## Features

- First-time model guide with an isolated GR II Wi-Fi-only path and GR III/IV BLE paths.
- Persistent camera BLE identity, bond, protocol profile, and Wi-Fi parameters, with direct-address reconnect on later boots.
- One Button A press sends one autofocus-and-capture operation; the GR III no longer needs a second press.
- Portrait keeps BLE shutter control available and turns the camera AP off; landscape re-enables the AP, joins Wi-Fi, and starts LiveView.
- Both GR III and GR IV keep BLE connected during LiveView for deterministic autofocus and shutter operation.
- Leaving LiveView first attempts HTTP WLAN Finish, disconnects the StickS3 Wi-Fi STA, then turns the camera AP off over BLE.
- Button B single-click mirror, double-click LiveView Lock, and three-second hold to clear pairing.
- IMU low-pass filtering, hysteresis, and stabilization timing to prevent orientation chatter.
- 256 KB PSRAM frame buffer, 8192-byte stream buffer, and ESP32-S3-optimized JPEG decoding.
- Wi-Fi, MJPEG stream, and valid-frame watchdogs with automatic LiveView recovery.
- Camera standby guard that prevents repeated scanning or WLAN writes from waking the camera unexpectedly.
- StickS3 battery/charging indication, camera status, connection animation, shutter feedback, and sound.

## Hardware and Software

- M5Stack StickS3 (ESP32-S3-PICO-1, 8 MB Flash, 8 MB PSRAM)
- A supported RICOH GR camera
- USB-C data cable
- A prebuilt merged BIN and M5Burner, or PlatformIO Core

The default build uses Arduino, `espressif32@6.12.0`, M5Unified, M5PM1, NimBLE-Arduino 2.5.0, ArduinoJson 7.x, and Espressif `esp_new_jpeg`.

## Install the Firmware

### M5Burner

No programming tools or source-code download are required. Prepare a StickS3, a USB-C data cable, and Chrome or Edge, then follow these steps:

1. Open the official M5Stack online burner: [https://burner.m5stack.com/](https://burner.m5stack.com/).
2. Enter `GR` in the search box at the top of the page and click Search.
3. Find the firmware card named **“理光 GR 实时取景拍摄”**. Confirm that the target device is **StickS3** and the author is **TinkerZhang**.
4. Open the firmware details and click the Burn button shown on the page.
5. Connect the StickS3 with a USB-C data cable. When the browser requests serial permission, select the corresponding **USB JTAG/serial debug unit** or StickS3 serial port and allow access.
6. Follow the on-screen instructions to start flashing. Do not unplug the cable or close the page until M5Burner reports success and the StickS3 restarts.
7. On first boot, select the camera generation: Button B selects GR II/GR III/GR IV, and Button A confirms.

> [!NOTE]
> GR II credentials are unique to each camera. The firmware does not embed them; selecting GR II starts a phone-based setup portal on StickS3.

![Search for and select the RICOH GR Live View firmware in M5Burner](docs/images/M5Burner_Search_GR.png)

> [!TIP]
> If you find the firmware useful, please click the **heart/Like** button on its card or detail page. Your Like helps **TinkerZhang** earn M5Stack community points and supports continued maintenance and updates. Thank you!

> [!WARNING]
> Verify the StickS3 target and TinkerZhang author so that you do not install a similarly named package for another device. The published M5Burner community release already contains the complete flash image; regular users do not need to set an offset or Flash Mode. Only developers importing a BIN manually need the merged Bootloader, partition table, `boot_app0`, and application image flashed at `0x0000` with DIO parameters. Never use `.pio/build/m5stack-sticks3/firmware.bin` as a complete image.

### Build from Source

No GR II Wi-Fi configuration file is required. Build and upload normally:

```bash
# Build and upload to StickS3
pio run -e m5stack-sticks3 -t upload

# Monitor serial output
pio device monitor -b 115200
```

If automatic port detection fails, append `--upload-port <port>`.

## First-time Connection

### GR II (No Bluetooth)

GR II support has been verified on real camera hardware.

1. Enable Wi-Fi on the camera body; StickS3 cannot turn on the GR II access point remotely.
2. Select `GR II` in the guide. StickS3 scans for the camera Wi-Fi first, then displays a `GR-II-Setup-xxxx` setup network.
3. Join that network from a phone using password `GR288888`, then open `http://192.168.4.1`.
4. Select the camera's `RICOH_*` network, enter the Wi-Fi password shown by the camera, and submit. If the camera is missing from the list, enter its SSID manually.
5. StickS3 saves the credentials to NVS, closes the setup network, and joins the camera without any BLE scan or pairing. Hold Button B to clear the profile and configure it again.
6. Following the [official GR Remote](https://www.ricoh-imaging.co.jp/english/products/gr_remote/index.html), LiveView uses `/v1/liveview`. Button A uses `/v1/camera/shoot?af=camera`, with the official `shoot/start` + `shoot/finish` fallback.

GR II keeps its HTTP stream alive in portrait so the Wi-Fi shutter remains available, while skipping JPEG decode and drawing. The firmware never sends the official `/v1/device/finish` command during handoff because that command powers the camera off.

### GR III / GR IV Pairing

1. Turn on the camera and open its Bluetooth pairing screen.
2. With no saved camera, the StickS3 opens the model-selection guide.
3. Press **Button B** to switch among `GR II`, `GR III`, and `GR IV`; press **Button A** to confirm.
4. The firmware scans for and accepts only a camera matching the selected generation. A generation mismatch cannot perform WLAN, power, or shutter writes.
5. After pairing, camera identity, protocol profile, bond, and usable Wi-Fi parameters are stored in NVS.

The pairing guide owns both buttons, so model selection cannot accidentally trigger the shutter, display mirror, LiveView Lock, or pairing reset.

### GR III / GR IIIx Six-digit Code

GR III Family uses the dynamic six-digit passkey shown on the camera:

- Short A press: increment the current digit (cycles 0–9).
- Short B press: confirm the current digit and move to the next position.
- Long A press: submit the current six digits immediately.
- Hold B for three seconds: cancel entry and enter the pairing-reset flow.
- Entry window: 45 seconds.

### GR IV / GR IV HDF

GR IV Family retains its Legacy security configuration and fixed passkey `123456`. GR III KeyboardDisplay settings, signing-key distribution, and UUID WLAN path are never applied to GR IV.

## Controls

| Button | Context | Action |
| :--- | :--- | :--- |
| Button A | Pairing guide | Confirm the selected camera generation |
| Button B | Pairing guide | Switch among GR II, GR III, and GR IV |
| Button A | Camera ready | Trigger one AF + shutter command on press; holding does not repeat it |
| Button A | Camera sleep guard | Request one safe manual reconnect without taking a picture |
| Button B single-click | Normal UI | Toggle and persist LiveView image mirroring |
| Button B double-click | LiveView | Toggle LiveView Lock; a locked preview stays active in portrait |
| Hold Button B for three seconds | Paired state | Clear camera profile, Wi-Fi cache, and BLE bonds, then return to the pairing guide |
| Hold the power button for about 1.2 seconds | Any state | Power off the StickS3 |

Holding Button A for more than 300 ms shows focus animation and sound feedback, but the camera command is still sent only once.

## Orientation and Camera AP Lifecycle

### Portrait: BLE Shutter

- BLE remains connected for autofocus, shutter, and camera-state handling.
- With a valid Wi-Fi cache, the firmware does not enable the AP merely to read the same values again.
- First pairing may briefly enable the AP to retrieve parameters, then immediately turns it off over BLE.
- Returning from LiveView closes the HTTP stream, disconnects StickS3 Wi-Fi, and sends the generation-specific WLAN OFF command for both GR III and GR IV.

### Landscape: LiveView

- The camera AP is re-enabled over BLE.
- Cached SSID, BSSID, and channel are tried first; a failed fast path falls back to fresh BLE parameters.
- The firmware opens the `/v1/liveview` MJPEG stream and defers `/v1/props` until after the first frame.
- BLE stays connected on both GR III and GR IV, so Button A continues to use the BLE AF shutter.

Orientation must remain stable for about 500 ms before switching. If the IMU is unavailable, the firmware falls back to landscape preview mode. With LiveView Lock enabled by a Button B double-click, portrait orientation no longer closes the preview.

## GR III and GR IV Protocol Differences

| Item | GR III / GR IIIx | GR IV / GR IV HDF |
| :--- | :--- | :--- |
| Pairing | Camera-generated six-digit code, KeyboardDisplay | Legacy fixed `123456`, DisplayYesNo |
| WLAN control | Service and Characteristic UUIDs | Verified fixed handles |
| Wi-Fi parameters | SSID, passphrase, optional channel; BSSID learned after connection | SSID, passphrase, security, frequency, and BSSID |
| Shutter preparation | Direct Operation Request `{START, AF}` | Legacy Shooting Flavor write, then Operation Request |
| BLE during LiveView | Kept connected | Kept connected |
| Leave LiveView | HTTP Finish + BLE UUID WLAN OFF | HTTP Finish + BLE handle WLAN OFF |

GR III and GR IIIx are one `GR3_FAMILY` in the current implementation and do not have separate pairing or transport parameters. GATT characteristics are discovered by UUID; reference handles recorded from a GR IIIx are not treated as a cross-model contract.

## Runtime Flow

```mermaid
flowchart TD
    A[StickS3 starts] --> B{Saved camera?}
    B -->|No| C[Model guide: B selects / A confirms]
    B -->|Yes| Q{GR II?}
    C --> Q
    Q -->|Yes| P{GR II WLAN saved?}
    P -->|No| S[Phone joins StickS3 setup AP and submits credentials]
    P -->|Yes| R[Join the manually enabled GR II AP]
    S --> R
    Q -->|No, saved| D[Fast reconnect by saved BLE identity]
    Q -->|No, first use| E[Scan, identify protocol, and pair securely]
    R --> M[Start HTTP MJPEG LiveView]
    D --> F[Read Power / Operation Mode]
    E --> F
    F --> G{Camera state allows WLAN?}
    G -->|No| H[Camera Sleep Guard]
    G -->|Yes| I[Read or reuse Wi-Fi parameters]
    I --> J{Landscape or LiveView Lock?}
    J -->|No| K[Camera AP off / BLE shutter ready]
    J -->|Yes| L[Enable AP and join camera Wi-Fi]
    L --> M
    M -->|Portrait and unlocked| N[HTTP Finish / Wi-Fi disconnect / BLE WLAN OFF]
    N --> K
    K -->|Landscape| L
```

## Build and Test

```bash
# Run host-side Native tests
pio test -e native

# Build the StickS3 release firmware
pio run -e m5stack-sticks3
```

Current commit baseline:

- Native: 86 / 86 passed.
- StickS3: release build succeeded.
- RAM: 66,444 / 327,680 bytes (20.3%).
- Flash: 1,455,789 / 3,342,336 bytes (43.6%).

Automated coverage includes protocol detection, security profiles, pairing recovery, NVS migration, orientation transitions, button isolation, AP lifecycle, BLE shutter behavior, MJPEG parsing, UI, and runtime watchdogs. Automated tests do not replace hardware regression across camera bodies and firmware versions.

## Troubleshooting

### Black Screen after Flashing

- Confirm that a full merged image was flashed at `0x0000`.
- Confirm that the Bootloader Flash Mode stayed DIO and was not forced to QIO by the merge tool.
- Erase Flash and install a hardware-verified release package again.

### Camera Not Found or Pairing Fails

- Confirm that the camera is on its Bluetooth pairing screen and the correct generation was selected in the guide.
- If the StickS3 was removed on the camera, also hold B for three seconds on the StickS3 to clear the stale local bond.
- GR III Family must use the six-digit code currently shown by the camera, not the GR IV fixed code.

### Landscape Does Not Start LiveView

- Confirm that the device is not in the camera sleep guard or pairing guide.
- Hold landscape for at least 500 ms; optionally double-click B to enable LiveView Lock.
- At 115200 baud, check the log for BLE WLAN ON, Wi-Fi connection, and `/v1/liveview` status.

### Camera AP Remains after Returning to Portrait

The normal log shows HTTP WLAN Finish, local Wi-Fi disconnect, then BLE WLAN deactivation. The camera may need a short time to stop advertising its AP. If it persists, record the camera model, firmware version, and complete orientation-transition log.

## Project Layout

- [src/main.cpp](src/main.cpp): hardware initialization, main loop, and business-action adapters.
- [src/app/](src/app/): connection, orientation, AP, and LiveView state machine.
- [src/ricoh/](src/ricoh/) and [src/ricoh_ble_client.cpp](src/ricoh_ble_client.cpp): GR III/GR IV protocol routing and BLE implementation.
- [src/services/](src/services/): BLE, Wi-Fi, LiveView, and shutter services.
- [src/ui/](src/ui/): pairing guide, buttons, orientation, animation, sound, and scene coordination.
- [src/camera_profile_store.cpp](src/camera_profile_store.cpp): camera profile, display settings, and Wi-Fi cache persistence.
- [src/jpeg_decoder.cpp](src/jpeg_decoder.cpp) and [src/mjpeg_stream.cpp](src/mjpeg_stream.cpp): JPEG decoding and MJPEG frame parsing.
- [test/test_native/](test/test_native/): host-side Native automated tests.
- [docs/](docs/): protocol, state-machine, hardware-verification, and test material.
- [cad/](cad/): printable StickS3 hot-shoe adapters and editable models.

## Accessories and Contributions

Printable StickS3 hot-shoe adapters are included; see [cad/README.md](cad/README.md). Please use [Issues](https://github.com/sky18Dragon/RICOH-GR-Live-View-Shooting/issues) to share the camera model, camera firmware, StickS3 commit, and redacted logs, or submit a Pull Request.

This project was developed collaboratively by the author and the Codex AI assistant. Thanks to everyone who contributed protocol analysis, hardware verification, or mechanical design.

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE). Modified and redistributed versions must comply with GPL-3.0.
