# CLAUDE.md — `src/` 模块详尽

[← 根目录 CLAUDE.md](../CLAUDE.md) ｜ 生成时间：2026-06-29 17:19:32 CST

`src/` 扁平存放全部固件源码（10 个 `.cpp`/`.h` 对 + `config.h`，约 3711 行）。本文按层记录每个模块的职责、对外接口、依赖与实现要点。

## 文件清单

| 文件 | 行数(.cpp/.h) | 层 | 一句话职责 |
| --- | ---: | --- | --- |
| `main.cpp` | 1011 | 编排 | 主状态机 + 连接流程 + 保护态 + 按键分发 |
| `ricoh_ble_client.{cpp,h}` | 1009 / 59 | BLE | RICOH BLE 全协议：扫描/连接/凭据/快门/电源通知 |
| `gr_wifi.{cpp,h}` | 189 / 28 | Wi-Fi | ESP32 STA 连相机 AP |
| `gr_api.{cpp,h}` | 342 / 35 | HTTP | `/v1/props` + `/v1/liveview` MJPEG |
| `camera_identity.{cpp,h}` | 49 / 5 | 相机身份 | 从 Wi-Fi SSID 推导候选 BLE 名称 |
| `display.{cpp,h}` | 336 / 50 | 渲染 | 屏幕 UI：boot/status/error/overlay |
| `jpeg_decoder.{cpp,h}` | 172 / 49 | 渲染 | JPEGDEC 解码到 RGB565 画布 |
| `mjpeg_stream.{cpp,h}` | 110 / 39 | 渲染 | MJPEG 字节流切分为单帧 |
| `camera_profile_store.{cpp,h}` | 62 / 31 | 持久化 | NVS 相机身份存储 |
| `buttons.{cpp,h}` | 16 / 15 | 输入 | 轮询 `M5.BtnA` |
| `config.h` | 85 | 配置 | 全局常量与 BLE GATT 句柄 |

---

## 编排层 — `main.cpp`

匿名命名空间内持有所有单例（`grWifi`/`grApi`/`mjpeg`/`ui`/`buttons`/`decoder`/`profileStore`/`cameraProfile`/`ricohBle`）。

`preferredBleName()` 会优先使用 NVS 中保存的相机名；无保存名时，调用 `camera_identity` 纯逻辑模块从 `GR_` / `GR_H` Wi-Fi SSID 推导候选 BLE 名称。

### 状态机 `CameraFlowState`

```
BleScan → BleReady → WifiConnecting → HttpProbe → LiveViewRunning
                ↑                              ↓
            (恢复)                        (链路丢失/恢复)
                ←──── CameraSleepGuard（保护态，旁路）───→
```

- `setCameraFlowState(state, reason)` 统一打印 `Flow: A -> B (reason)`。
- `runCameraFlowOnce()`：BLE 发现 → 开 Wi-Fi → HTTP probe → 开 LiveView 的完整一次推进。
- `resumeFromBleReady(reason)`：BLE 仍在时从 `BleReady` 重试 Wi-Fi/HTTP/LiveView（最多 `WIFI_OPEN_ATTEMPTS=3`）。
- `recoverCameraConnection(reason)`：恢复入口；按 `reasonRequiresBleRescan()` 决定走 `resumeFromBleReady` 还是 `runCameraFlowOnce`，链路已断时先 `resetBleStackBeforeScanAfterLinkLoss()`。

### 相机关机保护态 `CameraSleepGuard`

- 触发：`consumeCameraPowerOffDisconnect()`（断连 reason `0x213`/`0x215`）或 `consumeCameraPowerOffNotification()`（BLE 电源通知 `0x00`）→ `enterCameraSleepGuard()`。
- 期间 `cameraAutoWakeBlocked=true`，`cameraSleepGuardBlocksFlow()` 拦截一切自动流程；`cameraSleepGuardCooldownActive()` 判断 15s 冷却。
- 冷却结束**仍不自动唤醒**；须 `requestManualCameraWake(source)`：`clearCameraSleepGuard()` + `cameraManualWakeOverride=true` + `ricohBle.resetStack(true)` + 等 `BLE_MANUAL_WAKE_REINIT_SETTLE_MS` + `runCameraFlowOnce()`。
- 保护态不写 NVS → StickS3 重启即重新扫描。

### 电源门控

`ensureCameraPowerReadyForWifi(source)`：开 Wi-Fi 前必须 `ricohBle.readPowerState()`（最多 `RICOH_BLE_POWER_READ_RETRIES=2` 次）确认为 `On`，并 `enablePowerStateNotify()` 订阅电源 CCCD。`Off/Unknown` → 进保护态；`cameraManualWakeOverride` 可旁路。

### `setup()` / `loop()`

- `setup()`：`Serial` → `ui.begin()` → boot 画面 → 等串口 → `buttons/decoder/grWifi.begin()` → `applyDefaultProfile()` → PSRAM 检查 → 分配 `frameBuffer`（PSRAM 优先，回退内部 RAM，失败则停机）→ `mjpeg.begin(frameBuffer, ..., onJpegFrame)` → `runCameraFlowOnce()`。
- `loop()`（每轮 `delay(1)`）：
  1. `handleButtons()` — 仅 `M5.BtnA`：保护态则 `requestManualCameraWake`，否则 `triggerShutterFromButtonA()`（`ricohBle.shoot(true)`）。
  2. `serviceCameraFlowIfNeeded()` — 非运行态按 `BLE_SCAN_RETRY_INTERVAL_MS` 周期推进。
  3. `ensureWiFi()` — LiveView 中 Wi-Fi 掉线即恢复。
  4. `refreshPropsIfDue()` — 每 `PROPS_REFRESH_INTERVAL_MS=60s` 刷新 `/v1/props`。
  5. `ensureLiveView()` — 读 MJPEG → `mjpeg.process()`；处理 BLE/Wi-Fi/LiveView 掉线与 stall 看门狗。
  6. `updateStatusUiIfDue()` — 每 `UI_STATUS_INTERVAL_MS=1s` 刷新非 LiveView 状态。

### 帧回调 `onJpegFrame(data, len, _)`

`lastFrameAt` 更新 → `decodedFrames++` → `updateFps()` → `decoder.drawFrame(ui.getCanvas(), ...)` → 可选 `ui.drawOverlay(...)` → `ui.pushCanvas()`。

### 名字推导

`deriveBleNameFromWifiSsid(ssid)`：`GR_H<数字>` → 推导下一序列号作为期望 BLE 名（用于扫描时偏好匹配）。

---

## BLE 层 — `ricoh_ble_client.{cpp,h}`

基于 **NimBLE-Arduino**，封装 RICOH GR BLE 控制协议（句柄定义见 `config.h`）。

### 对外接口（`RicohBleClient`）

| 方法 | 说明 |
| --- | --- |
| `begin()` | 初始化 NimBLE 栈 |
| `scanForCamera(preferredAddress, preferredName, scanSeconds)` | 扫描并打分挑最佳 RICOH 候选（`RicohBleDeviceInfo`） |
| `connect(info, timeoutMs)` | 连接 + 安全配对/加密 |
| `isConnected()` / `shutterReady()` | 连接态 / 可拍快门 |
| `shoot(autofocus=true)` | 写 `0x0099`：`0x01`对焦→`0x02`拍摄→`0x00`释放 |
| `openWifi()` | 写 `0x0135`=`0x01` 唤醒相机 Wi-Fi |
| `readPowerState(&state)` / `enablePowerStateNotify()` | 读/订阅电源状态 `0x00EB`/CCCD `0x00EC` |
| `consumePowerOffNotification()` | 取走一次电源 `0x00` 通知 |
| `waitForWifiCredentials(&cred, timeoutMs)` | 轮询读取 `0x0138` SSID / `0x013A` 密码 / `0x0140` BSSID |
| `disconnect()` / `resetStack(clearObjects=false)` | 断连 / 重建 BLE 栈（手动唤醒用 `clearObjects=true`） |
| `consumeDisconnectReason()` / `clearDisconnectReason()` | 取走/清除 GAP 断连 reason |
| `statusText()` / `lastError()` | 状态文本 / 最近错误 |

### 实现要点

- `ricohGapEventHandler`：底层 GAP 事件回调，捕获断连 reason（`0x213` 远端 user / `0x215` 远端 power-off）。
- `RicohScanCallbacks`：`onDiscovered`/`onResult` 回调，`candidateScore()` 按「地址匹配 > 名字匹配 > 服务匹配」打分，`foundPreferred()` 标记命中偏好项。
- 候选识别：`advertisesAnyRicohService()`（Info/Camera/Shooting/Control 四服务任一）或 `nameLooksLikeRicoh()`。
- Wi-Fi 凭据解析：`printableText`/`maybeQuotedValue`/`macFromText`/`macFromRaw6`/`mergeCredentialValue` —— 容错地从 BLE 读回的文本/裸字节里提取 SSID、密码、MAC，处理加密 passphrase 标记。
- 句柄读写超时：`HANDLE_WRITE_TIMEOUT_MS=3000`、`HANDLE_READ_TIMEOUT_MS=800`。

---

## Wi-Fi 层 — `gr_wifi.{cpp,h}`

`GrWifi` 封装 ESP32 `WIFI_STA` 连接相机 AP。

- `begin()`：`WiFi.mode(WIFI_STA)` + `setSleep(true)` + `setAutoReconnect(true)`。
- `connect(...)` 四重重载，最完整签名：`(ssid, password, bssid, timeoutMs, ConnectGuard guard)`。
  - `bssid`：锚定相机 AP MAC（动态凭据场景防漫游）。
  - `ConnectGuard`：`bool(*)()` 回调，连接轮询期间若返回 `false`（如 BLE 已断）即提前中止 —— `main.cpp` 传 `bleStillConnectedForWifi`。
- `isConnected()`/`ssid()`/`rssi()`/`localIPString()`/`statusText()`：状态查询；`statusText()` 经 `wifiStatusToString` 映射 `wl_status_t`。
- `disconnect()`：`WiFi.disconnect(false, false)`（不清 SSID、不关 radio）。

---

## HTTP API 层 — `gr_api.{cpp,h}`

`GrApi` 用裸 `WiFiClient` 手写 HTTP（无 HTTPClient 库），适配相机 LiveView 长连接。

- `setEndpoint(host, port=80)` / 默认 `GR_HOST=192.168.0.1:80`。
- `fetchProps(props, timeoutMs)`：`GET /v1/props HTTP/1.1`（`Connection: close`），`parsePropsJson` 用 ArduinoJson 兼容多键名（`model`/`modelName`/`model_name`、`batteryLevel`/`battery_level`、`batteryState`/`battery_state`），输出 `CameraProps{model, battery}`。
- `openLiveView()` / `closeLiveView()` / `isLiveViewOpen()`：`GET /v1/liveview HTTP/1.1`，**保持连接**，`_liveClient` 复用。
- `readLiveView(dst, len)`：从 `_liveClient` 读 MJPEG 字节流；`>0` 字节数、`0` 暂无数据、`<0` 错误。
- 内部：`connectClient`（连接 + 超时）、`readHttpHeaders`/`parseHttpStatus`/`parseContentLength`、`lastError`。

---

## 解码渲染层

### `mjpeg_stream.{cpp,h}` — `MjpegStream`

状态机切分 MJPEG 字节流为完整 JPEG 帧。

- 状态 `SeekingSoi` ⇄ `InFrame`：识别 `FF D8`（SOI）开帧、`FF D9`（EOI）收帧。
- `appendByte` 写入外部 `frameBuf`（`main.cpp` 的 256KB PSRAM 缓冲）；超长 `_overflow` 丢帧。
- `finishFrame` 校验 `_len>=20` 后回调 `FrameCallback(data, len, user)`；`dropFrame` 仅计 `droppedFrames`。
- `process(data, len)` 返回本次新增帧数；`frames()`/`droppedFrames()`/`currentLength()` 状态查询。
- 头文件只依赖标准 C++ `<cstddef>` / `<cstdint>`，可被 host-side native 单元测试直接编译。
- 逐字节扫描，`_prev`/`_havePrev` 跨调用保留前一字节以识别 marker。

### `jpeg_decoder.{cpp,h}` — `JpegDecoder`

`JPEGDEC` 库解码单帧到 RGB565 画布。

- `drawFrame(LovyanGFX* dst, data, length)`：`_jpeg.openRAM` → `setPixelType(RGB565_BIG_ENDIAN)` → 按 `JPEG_SCALE_POLICY`（config.h = `JPEG_SCALE_HALF`）选缩放 → 居中算 `_drawX/_drawY` → `decode()`。
- `jpegDrawCallback`（静态）转发到实例 `drawBlock`，逐块写入目标画布。
- 裁剪：`visibleX/Y/W/H` 处理解码图大于屏幕的可见区。
- `lastDecodeMs`/`lastWidth`/`lastHeight`/`lastError` 状态查询。

### `display.{cpp,h}` — `DisplayUi`

`M5Canvas` 双缓冲 + `pushSprite` 上屏，240×135。

- `begin()` 初始化画布；`getCanvas()` 暴露 `LovyanGFX*` 供解码器直接绘制。
- `showBoot(msg)` / `showStatus(L1..L4)` / `showError(msg, detail)`：状态屏（顶部琥珀色标题、底部灰色提示；UI 文案标注「BtnA: shutter / wake」「Press BtnA to reconnect」）。
- `drawOverlay(wifiStatus, liveviewStatus, model, battery, fps, rssi, frames, droppedFrames)`：LiveView 上的半透明 overlay（WiFi 图标 `drawWifiIcon`、电池 `drawBatteryIcon`）。
- `pushCanvas()` 上屏；`width()`/`height()` 查询。
- 颜色常量 `COLOR_BG/COLOR_AMBER/COLOR_WHITE/COLOR_GRAY/COLOR_RED`。

---

## 持久化层 — `camera_profile_store.{cpp,h}`

`CameraProfileStore` 基于 `Preferences`（NVS）。

- namespace `"ricoh2"`，`profileVersion=3`。
- 键：`proto_ver`/`cam_name`/`ble_addr`/`ble_addr_type`/`ble_bonded`/`cam_ip`。
- `load()`/`save()`/`saveBleIdentity(name, addr[, type, bonded])`/`clear()`；`getStringIfPresent` 缺键返回空串。
- **只持久化 BLE 身份 + 相机 IP**，不存 Wi-Fi 凭据（每次运行时 BLE 动态获取）；保护态不写 NVS。

数据结构：`WifiCredential{ssid, passphrase, bssid, cameraIp}`、`CameraProfile{cameraName, bleAddress, bleAddressType, bleAddressTypeKnown, bleBonded, wifi, profileVersion}`。

---

## 输入层 — `buttons.{cpp,h}`

`Buttons` 仅 `M5.update()` + 轮询 `M5.BtnA.wasPressed()`，返回 `ButtonEvents{buttonA, any}`。

> ⚠️ **无 GPIO11/G11 外接按键、无 Button B**（README 描述已过时，见根级 CLAUDE.md「文档漂移」）。

---

## 配置 — `config.h`

全局 `constexpr` 常量与 `#define` 守卫。**改 BLE 行为前必读**。

- 显示/缓冲：`DISPLAY_WIDTH/HEIGHT`、`FRAME_BUFFER_SIZE=256KB`、`STREAM_READ_BUFFER_SIZE=2048`、`JPEG_SCALE_POLICY=JPEG_SCALE_HALF`。
- 超时/周期：`WIFI_CONNECT_TIMEOUT_MS`、`PROPS_TIMEOUT_MS`、`LIVEVIEW_STALL_TIMEOUT_MS`、`UI_STATUS_INTERVAL_MS`、`PROPS_REFRESH_INTERVAL_MS`。
- BLE 时序：`BLE_SCAN_SECONDS`、`BLE_CONNECT_ATTEMPTS`、`FIRST_BOOT_BLE_PAIRING_ATTEMPTS`、`BLE_STACK_RESET_AFTER_FAILURES`、`RICOH_BLE_SECURITY_WAIT_MS` 等。
- 电源/保护：`CAMERA_POWER_OFF_COOLDOWN_MS`、`BLE_MANUAL_WAKE_REINIT_SETTLE_MS`、`RICOH_BLE_DISCONNECT_REMOTE_USER=0x213`、`RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF=0x215`、`RICOH_BLE_REQUIRE_POWER_ON_BEFORE_WIFI=true`。
- **BLE GATT 句柄**（抓包验证 2026-06-27/28）：WLAN POWER `0x0135`(ON=`0x01`)、SSID `0x0138`、PASSPHRASE `0x013A`、SECURITY `0x013C`、FREQUENCY `0x013E`、BSSID `0x0140`、SHUTTER `0x0099`、POWER_STATE `0x00EB`/CCCD `0x00EC`(ON=`0x01`/OFF=`0x00`)。
- Service UUID：`RICOH_BLE_INFO/CAMERA/SHOOTING/CONTROL_SERVICE_UUID`。
- 默认端点：`GR_HOST=192.168.0.1`、`GR_PORT=80`。

## 依赖关系总览

```
main.cpp → 全部模块（编排）
ricoh_ble_client → NimBLE-Arduino, config.h
gr_wifi → WiFi.h (ESP32 Arduino)
gr_api → WiFiClient, ArduinoJson, config.h
camera_identity → 标准 C++ 头文件（纯逻辑，无硬件依赖）
mjpeg_stream → 标准 C++ 头文件（纯逻辑，无硬件依赖）
jpeg_decoder → JPEGDEC, M5Unified(LovyanGFX), config.h
display → M5Unified, config.h
camera_profile_store → Preferences
buttons → M5Unified
config.h → Arduino.h
```
