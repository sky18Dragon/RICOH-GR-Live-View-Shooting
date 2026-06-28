# RICOH GR StickS3 Remote Viewfinder

Firmware for using an M5Stack StickS3 as a wireless live-view monitor and shutter remote for RICOH GR cameras.

The firmware treats **BLE as the only camera-presence entry point**. It connects to the camera over BLE, asks the camera to temporarily enable Wi-Fi, reads the dynamic Wi-Fi credentials returned by the camera, then opens the HTTP LiveView stream.

[中文 README](README.md)

---

## Stable Features

- BLE-first camera discovery and secure connection.
- Dynamic Wi-Fi credentials read from the camera over BLE; no fixed SSID/password is required in `platformio.ini`.
- HTTP MJPEG LiveView preview on the StickS3 display.
- GPIO11 external shutter button.
- Camera power-off guard to avoid waking the camera while it is shutting down.
- Manual wake from guard state via G11 or Button B.
- StickS3 reboot starts fresh and automatically reconnects from BLE scan.

---

## Workflow

```text
StickS3 boot
  -> initialize display, buttons, NVS, BLE, Wi-Fi
  -> load camera profile
  -> scan for GR / RICOH BLE device
  -> connect and secure BLE
  -> write WLAN ON over BLE (handle 0x0135, value 0x01)
  -> read Wi-Fi SSID / passphrase / BSSID from BLE
  -> connect to camera Wi-Fi AP
  -> probe camera HTTP API
  -> open LiveView and render MJPEG frames
```

### Camera Power-Off Guard

```text
LiveView running
  -> camera powers off / remote BLE disconnect
  -> BLE disconnect reason 531 or 533
  -> enter CAMERA_SLEEP_GUARD
  -> block BLE scan, reconnect, and Wi-Fi ON for 15 seconds
  -> remain idle after cooldown
  -> user presses G11 or Button B
  -> rebuild NimBLE stack and reconnect
```

This prevents the StickS3 from sending a Wi-Fi wake command while the camera is still shutting down or advertising in low-power standby.

---

## Controls

| Control | Function |
| --- | --- |
| G11 external button | BLE shutter while LiveView is running; manual wake/reconnect while in guard state |
| Button B | Pause/resume LiveView; manual wake/reconnect while in guard state |
| Button A | Reserved |

G11 wiring: connect a button between **GPIO11 (G11)** and **GND**. The firmware uses `INPUT_PULLUP` and a falling-edge interrupt.

---

## BLE Handles

| Feature | Handle | Operation | Description |
| --- | ---: | --- | --- |
| Wi-Fi enable | `0x0135` | Write | Write `0x01` to enable the camera AP |
| Wi-Fi SSID | `0x0138` | Read | Read camera AP SSID |
| Wi-Fi passphrase | `0x013A` | Read | Read camera AP password |
| Wi-Fi BSSID | `0x0140` | Read | Read camera AP MAC address |
| Shutter control | `0x0099` | Write | `0x01` focus, `0x02` shoot, `0x00` release |

---

## Build and Flash

```bash
platformio run
platformio run -t upload
platformio device monitor
```

Serial baud rate: `115200`.

---

## Notes

- Fixed `GR_WIFI_SSID` / `GR_WIFI_PASSWORD` build flags are not needed.
- Camera guard state is RAM-only; rebooting the StickS3 starts from BLE scan and reconnects automatically.
- If the shutter command logs a BLE write failure but the camera actually takes the photo, the failure is usually from the release/follow-up write after the camera has already accepted the shot.