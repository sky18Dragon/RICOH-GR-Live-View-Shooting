# Project Overview

## 目标

本项目是 RICOH GR Live View Shooting 固件。目标设备为 ESP32-S3 / M5Stack StickS3，使用 PlatformIO Arduino framework。固件通过 BLE 发现和控制 RICOH GR 相机，通过相机 Wi-Fi AP 使用 HTTP API 打开 LiveView，将 MJPEG 解码显示到 StickS3 屏幕，并提供 Button A BLE AF 快门。

## 已从代码确认的事实

- `platformio.ini` 默认环境：`m5stack-sticks3`。
- Platform：`espressif32@6.12.0`。
- Board：`esp32-s3-devkitc-1`。
- Framework：Arduino。
- 依赖：M5Unified、M5PM1、JPEGDEC、ArduinoJson、NimBLE-Arduino 2.5.0、WiFi、Preferences、Wire。
- 显示默认尺寸：`DISPLAY_WIDTH=240`、`DISPLAY_HEIGHT=135`。
- LiveView frame buffer：`FRAME_BUFFER_SIZE=256 * 1024`。
- Stream read buffer：`STREAM_READ_BUFFER_SIZE=2048`。
- Camera HTTP 默认地址：`GR_HOST=192.168.0.1`，`GR_PORT=80`。
- 主状态机枚举：`BleScan`、`CameraSleepGuard`、`BleReady`、`WifiConnecting`、`HttpProbe`、`LiveViewRunning`。
- 主循环顺序：`handleButtons()`、`serviceCameraFlowIfNeeded()`、`ensureWiFi()`、`refreshPropsIfDue()`、`ensureLiveView()`、`refreshWifiCacheIfDue()`、`updateStatusUiIfDue()`、`delay(1)`。
- 当前没有 `include/` 和 `lib/` 目录。
- `docs/` 原有内容仅发现 `docs/images/hardware_setup.jpg` 和 `docs/images/liveview_action.jpg`。

## 机型支持状态

README 声明：

| 机型 | 状态 |
| --- | --- |
| RICOH GR IV HDF | 已验证可用 |
| RICOH GR IV 系列 | 理论可用，仍需实机确认 |
| RICOH GR III / GR IIIx | 当前不可用 |
| RICOH GR II | 当前不可用 |

## 功能模块

- BLE：`src/ricoh_ble_client.*`
- Wi-Fi：`src/gr_wifi.*`
- HTTP/LiveView：`src/gr_api.*`
- MJPEG：`src/mjpeg_stream.*`
- JPEG 解码：`src/jpeg_decoder.*`
- UI：`src/display.*`
- 按钮：`src/buttons.*`
- 电源和主状态机：`src/main.cpp`
- NVS profile：`src/camera_profile_store.*`
- 相机身份推导：`src/camera_identity.*`

## TODO_UNVERIFIED

- GR IV 非 HDF 机型的完整兼容性：README 标记为理论可用，需实机确认。

## 后续 Codex 修改代码时必须注意

- 修改任何流程前先确认状态机入口和恢复路径。
- 涉及相机唤醒的改动必须同时读 `power_state_policy.md`。
