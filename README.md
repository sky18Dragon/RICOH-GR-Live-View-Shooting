# RICOH GR StickS3 Remote Viewfinder

M5Stack StickS3 上运行的 RICOH GR 无线实时取景与遥控快门固件。

它通过 **BLE 作为唯一在线入口** 识别并连接相机，再通过 BLE 临时开启相机 Wi-Fi，读取相机回传的动态 Wi-Fi 参数，最后通过 HTTP LiveView 显示实时预览。

[English README](README_EN.md)

---

## 当前稳定特性

- **BLE 优先连接流程**：上电后扫描 `GR_` / RICOH 设备，优先使用 NVS 中保存的相机 BLE 地址和名称。
- **动态 Wi-Fi 参数**：不再依赖 `platformio.ini` 中固定 SSID / Password；相机 Wi-Fi 唤醒后，通过 BLE 读取 SSID、密码和 BSSID。
- **LiveView 实时预览**：连接相机 Wi-Fi 后访问 RICOH HTTP API，打开 `/v1/liveview` 并在 StickS3 屏幕显示 MJPEG 画面。
- **G11 外接快门**：GPIO11 接 GND 的按键按下即触发 BLE 快门序列。
- **相机关机保护**：检测到相机主动断开 BLE 后进入保护态，避免 StickS3 在相机关机流程中再次唤醒相机。
- **手动唤醒恢复**：保护态冷却结束后，按 G11 或 Button B 才会重新唤醒并连接相机。
- **StickS3 重启自动连接**：保护态只保存在 RAM 中，StickS3 自身重启后会重新从 BLE 扫描开始自动连接相机。

---

## 工作流程

```text
StickS3 上电 / 重启
  ↓
初始化屏幕、按键、NVS、BLE、Wi-Fi
  ↓
读取 Camera Profile（BLE 地址、相机名、相机 IP）
  ↓
BLE 扫描 GR / RICOH 设备
  ↓
连接相机 BLE 并完成安全配对 / 加密
  ↓
通过 BLE 写入 WLAN ON 指令（handle 0x0135, value 0x01）
  ↓
通过 BLE 读取相机回传的 Wi-Fi 参数
  ↓
连接相机 Wi-Fi AP
  ↓
HTTP Probe 相机状态
  ↓
打开 LiveView，读取 MJPEG，解码显示
```

### 相机关机保护流程

```text
LiveView 运行中
  ↓
相机关机 / 相机主动断开 BLE
  ↓
收到 BLE disconnect reason 531 / 533
  ↓
进入 CAMERA_SLEEP_GUARD
  ↓
15 秒冷却期内禁止 BLE 扫描、BLE 重连、Wi-Fi ON
  ↓
冷却结束后仍不自动唤醒相机
  ↓
用户按 G11 或 Button B
  ↓
重建 NimBLE 栈，重新扫描并连接相机
```

这样可以避免相机正在关机或已进入低功耗广播状态时，被 StickS3 自动发送 Wi-Fi ON 指令强行唤醒。

---

## 按键说明

| 按键 | 功能 |
| --- | --- |
| G11 外接按键 | LiveView 中触发 BLE 快门；在相机保护态下作为手动唤醒 / 重连按键 |
| Button B | 暂停 / 恢复 LiveView；在相机保护态下作为手动唤醒 / 重连按键 |
| Button A | 保留 |

G11 硬件接线：按键连接在 **GPIO11 (G11)** 与 **GND** 之间，固件使用 `INPUT_PULLUP` 和下降沿中断。

---

## RICOH BLE GATT 句柄

| 功能 | Handle | 操作 | 说明 |
| --- | ---: | --- | --- |
| Wi-Fi 开启 | `0x0135` | Write | 写入 `0x01` 唤醒相机 Wi-Fi |
| Wi-Fi SSID | `0x0138` | Read | 读取相机 AP 名称 |
| Wi-Fi 密码 | `0x013A` | Read | 读取相机 AP 密码 |
| Wi-Fi BSSID | `0x0140` | Read | 读取相机 AP MAC 地址 |
| 快门控制 | `0x0099` | Write | `0x01` 对焦，`0x02` 拍摄，`0x00` 释放 |

---

## 硬件需求

- M5Stack StickS3
- RICOH GR III / GR IIIx / GR IV 或兼容 RICOH BLE 控制协议的机型
- 可选：外接 G11 快门按键

---

## 构建与烧录

```bash
# 编译
platformio run

# 烧录
platformio run -t upload

# 查看串口日志
platformio device monitor
```

串口波特率：`115200`

---

## 关键配置

主要配置位于：

- `src/config.h`
- `platformio.ini`

当前固件通过 BLE 获取相机 Wi-Fi 参数，因此 `platformio.ini` 中不需要配置固定的 `GR_WIFI_SSID` / `GR_WIFI_PASSWORD`。

重要参数：

| 参数 | 默认值 | 说明 |
| --- | ---: | --- |
| `BLE_SCAN_SECONDS` | `2` | 单轮 BLE 扫描时间 |
| `BLE_CONNECT_ATTEMPTS` | `12` | 已有相机身份时的 BLE 重连尝试次数 |
| `FIRST_BOOT_BLE_PAIRING_ATTEMPTS` | `12` | 首次烧录 / 无 NVS 身份时的配对扫描次数 |
| `CAMERA_POWER_OFF_COOLDOWN_MS` | `15000` | 相机关机断连后的冷却时间 |
| `BLE_MANUAL_WAKE_REINIT_SETTLE_MS` | `3000` | 手动唤醒时 BLE 栈重建后的稳定等待时间 |
| `LIVEVIEW_STALL_TIMEOUT_MS` | `5000` | LiveView 无有效帧时的卡顿恢复阈值 |

---

## 典型日志

正常连接：

```text
BLE: connected secure
Flow: BLE_SCAN -> BLE_READY (BLE connected)
BLE: Wi-Fi open requested
BLE: Wi-Fi parameters received ssid='GR_H264456' bssid=''
WiFi: connected ip=192.168.0.4
HTTP: camera ready model='RICOH GR IV HDF'
LiveView: connected
```

相机关机保护：

```text
BLE: disconnected reason=531
Flow: BLE_READY -> CAMERA_SLEEP_GUARD (...)
BLE guard: remote disconnect reason=531; auto wake paused for 15s, then manual wake required
```

手动唤醒：

```text
BLE guard: manual wake requested (G11 manual wake), previous disconnect reason=531
BLE guard: manual wake BLE stack rebuild (G11 manual wake)
BLE: resetting stack (clear objects)
BLE: scanning for GR camera
```

---

## 故障排查

### 相机关机后不希望被唤醒

这是当前固件的默认行为。收到 `531 / 533` 断连后会进入 `CAMERA_SLEEP_GUARD`，不会自动发送 Wi-Fi ON。冷却结束后需要按 G11 或 Button B 才会手动唤醒。

### StickS3 重启后是否会自动连接

会。保护态不写入 NVS，StickS3 重启后从 BLE 扫描流程重新开始。

### G11 快门日志显示 BLE shutter failed，但相机实际拍摄成功

RICOH 相机可能在拍摄瞬间断开或拒绝后续 release 写入，日志可能显示写入失败。只要相机实际完成拍摄且 LiveView 可恢复，就不影响使用。

---

## 项目结构

```text
src/
  main.cpp                 主状态机、连接流程、保护态、按键逻辑
  ricoh_ble_client.*       RICOH BLE 扫描、连接、Wi-Fi 参数读取、快门写入
  gr_wifi.*                ESP32 Wi-Fi STA 连接
  gr_api.*                 RICOH HTTP API 与 LiveView
  mjpeg_stream.*           MJPEG 流解析
  jpeg_decoder.*           JPEG 解码与显示输出
  display.*                StickS3 屏幕 UI
  camera_profile_store.*   NVS 相机身份存储
  g11_button.*             G11 外接按键中断
```