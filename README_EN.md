# RICOH GR StickS3 Remote Viewfinder

[![PlatformIO](https://img.shields.io/badge/PlatformIO-v6.12.0-blue.svg)](https://platformio.org/)
[![Board](https://img.shields.io/badge/Board-ESP32--S3-orange.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/index.html)
[![Hardware](https://img.shields.io/badge/Hardware-M5Stack--StickS3-red.svg)](https://docs.m5stack.com/en/core/m5stamp_s3)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

[中文版](README.md)

An intelligent, wireless electronic viewfinder (EVF) and shutter remote control firmware running on the **M5Stack StickS3** for RICOH GR cameras.

---

## 📸 Showcase / Field Demo

> 💡 **Tip**: Replace or add photos of your hardware assembly and working setup here. We recommend saving them under the `docs/images/` directory in your repository.

| 📸 Remote Monitor & Shutter | 🎬 Waist-Level Viewfinder In Action |
| :---: | :---: |
| ![RICOH GR StickS3 Remote Monitor & Shutter](docs/images/hardware_setup.jpg) | ![Waist Level Viewfinder Action](docs/images/liveview_action.jpg) |

---

## 🎯 Purpose & Primary Use Cases

This firmware is designed to turn your M5Stack StickS3 into a **mini wireless field monitor** for RICOH GR cameras, catering to street photography and creative perspectives:

1. **Waist-Level Viewfinder (Waist-Level EVF)**: Mount the StickS3 on the camera's hot shoe or hold it in your hand. Frame low-angle, street stealth, or waist-level shots perfectly using the handheld display without needing to bend down or lie on the ground.
2. **Remote Monitor & Controller**: Mount the camera on a tripod and monitor the frame from up to 10 meters away. Wirelessly trigger the shutter, making it perfect for group photos, self-portraits, or complex tripod placements.

---

## 🚀 Core Automatic Workflow

The firmware handles connection bootstrapping with **zero manual configuration required**:

```text
[Power On] 
   │
   ▼
[Auto BLE Scan] ────────► Discovers preferred RICOH camera
   │
   ▼
[Auto BLE Secure Link] ──► Authenticates using Ricoh validation Passkey 123456
   │
   ▼
[Auto Activate Camera Wi-Fi] ──► Sends WLAN power ON command & reads dynamic SSID/passphrase
   │
   ▼
[Auto Connect Wi-Fi] ────► Switches ESP32 to STA mode and connects to camera AP
   │
   ▼
[Enter LiveView EVF] ────► Decodes HTTP /v1/liveview stream & enables interrupt shutter (GPIO11)
```

---

## ✨ Features

- **Zero-Configuration Dynamic Credentials**: Since the camera rotates its Wi-Fi passwords, the firmware reads SSID, password, and BSSID credentials dynamically via BLE on every boot for a seamless **turn-on-and-connect** experience.
- **Hardware Interrupt Shutter (GPIO11)**: Configures GPIO11 with an internal pull-up and a falling-edge interrupt. Pressing the external shutter button latches the trigger instantly, ensuring zero shutter lag even during heavy JPEG decoding.
- **Flicker-Free Double Buffering**: Uses the highly optimized `JPEGDEC` library to decode RGB565 big-endian frames into PSRAM. LovyanGFX draws a transparent HUD overlay (FPS, battery icon, Wi-Fi RSSI bars, frame stats) and flushes to the screen buffer to eliminate flicker.
- **RF Coexistence & Lockup Watchdogs**:
  - Explicitly toggles Wi-Fi modem sleep (`WiFi.setSleep(true)`) to maintain connection stability while sharing the single-antenna radio on the ESP32-S3.
  - Automatically resets the BLE stack (`resetStack`) to recover from hardware stack locks if consecutive BLE connection attempts fail.
- **Persistent Profile Storage**: Automatically saves the matched camera's BLE address and name to the ESP32 NVS using the Preferences library. Connects automatically on boot.

---

## 🛠️ Hardware Requirements

1. **Microcontroller**: [M5Stack StickS3](https://docs.m5stack.com/en/core/m5stamp_s3)
2. **Camera**: RICOH GR III / GR IIIx / GR IV or compatible BLE-controlled Ricoh cameras.
3. **External Shutter Button**: A tactile button connected between **GPIO11 (G11)** and **GND** (internal `INPUT_PULLUP` configured in firmware). Can be built into custom handles or grips.

---

## 💻 Building & Flashing

Developed under the PlatformIO environment. Configuration is located in [platformio.ini](platformio.ini).

```bash
# 1. Compile project
platformio run

# 2. Upload to StickS3 board
platformio run -t upload

# 3. Monitor serial output (Baudrate: 115200)
platformio device monitor
```

---

## ⚙️ Development Environment & Dependencies

This project is developed and compiled using **PlatformIO IDE** (either via VS Code extension or the command-line interface). Config files can be found in [platformio.ini](platformio.ini).

### 1. Hardware Specifications
* **SoC**: ESP32-S3-PICO-1-N8R8 (Dual-core Xtensa LX7, clocked at 240MHz)
* **Memory**: 8MB QSPI Flash + 8MB OPI PSRAM (`qio_opi` memory type config)
* **Partition Table**: 8MB Default Layout (`default_8MB.csv`)

### 2. Software Frameworks & Platform
* **Platform**: Espressif32 BSP v6.12.0
* **Framework**: Arduino Core for ESP32

### 3. Core Library Dependencies
* **[M5Unified](https://github.com/m5stack/M5Unified)**: Unified hardware abstraction wrapper for screen display, backlight, and built-in button interactions.
* **[M5PM1](https://github.com/m5stack/M5PM1)**: Power management controller library for M5Stack StickS3.
* **[NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) (v2.5.0)**: Lightweight BLE stack replacement for enhanced stability and lower RAM footprint, preventing stack lockups.
* **[JPEGDEC](https://github.com/bitbank2/JPEGDEC) (v1.8.2+)**: Highly optimized JPEG decoder yielding high-speed pixel output directly to PSRAM.
* **[ArduinoJson](https://github.com/bblanchon/ArduinoJson) (v7.0.0+)**: Used to serialize and deserialize HTTP JSON API payloads from RICOH GR cameras.

---

## 🕹️ Controls & Interactions

- **G11 External Button**: Executes the BLE shutter sequence (Focus $\rightarrow$ Expose $\rightarrow$ Release). If pressed while disconnected, it initiates the camera connection recovery workflow.
- **Button B (Side)**: Toggles/Pauses LiveView. Pausing teardowns Wi-Fi and HTTP client to maximize battery life. Pressing B again issues a BLE Wi-Fi wake command and connects.
- **Button A (Front)**: Reserved for future UI/menu expansions.

---

## 🔗 RICOH BLE GATT Protocol Specs

The firmware reads and writes characteristics from the following services (tailored for the RICOH GR3/GR4 protocol):

| Feature | GATT Handle | Operation | Payload |
| :--- | :---: | :---: | :--- |
| **WLAN Power Toggle** | `0x0135` | Write | Write `0x01` to enable camera AP |
| **WLAN SSID** | `0x0138` | Read | Read dynamic Wi-Fi SSID |
| **WLAN Passphrase** | `0x013A` | Read | Read dynamic Wi-Fi Password |
| **WLAN BSSID** | `0x0140` | Read | Read AP physical address (BSSID) |
| **Shutter Control** | `0x0099` | Write | Sequence: Focus `0x01` $\rightarrow$ Shoot `0x02` $\rightarrow$ Release `0x00` |

---

## 🛡️ Fault Tolerance & Recovery

1. **LiveView Stall Watchdog**: If no valid JPEG is successfully parsed within `LIVEVIEW_STALL_TIMEOUT_MS` (5 seconds), the orchestrator triggers a connection teardown and warm recovery.
2. **Warm Reconnection**: If the Wi-Fi link drops but BLE is still alive, the system skips scanning, issues a Wi-Fi ON command, and attempts to re-associate Wi-Fi up to `WIFI_OPEN_ATTEMPTS = 3` times.
3. **Cold Reset**: If BLE is disconnected, all connections are torn down, the BLE stack is re-initialized, and the system falls back to `BleScan` to perform a full reconnect.
4. **First-Boot Pairing**: If no previous profile exists in NVS, the device conducts up to `FIRST_BOOT_BLE_PAIRING_ATTEMPTS = 12` rounds of scans, injecting Passkey `123456` to establish encryption.
