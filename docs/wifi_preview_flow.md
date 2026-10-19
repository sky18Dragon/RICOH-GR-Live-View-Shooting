# Wi-Fi and Preview Flow

## 已确认的端到端流程

从 `src/main.cpp`、`src/gr_wifi.cpp`、`src/gr_api.cpp`、`src/mjpeg_stream.cpp`、`src/jpeg_decoder.cpp` 确认：

```text
setup()
  -> ui / M5PM1 / buttons / decoder / Wi-Fi STA / profile init
  -> allocate FRAME_BUFFER_SIZE
  -> mjpeg.begin(frameBuffer)
  -> runCameraFlowOnce()

runCameraFlowOnce()
  -> BLE scan/direct connect
  -> BLE_READY
  -> ensureCameraPowerReadyForWifi()
  -> open Wi-Fi over BLE
  -> cached Wi-Fi connect or fresh BLE Wi-Fi params
  -> HTTP /v1/liveview
  -> LIVEVIEW_RUNNING
  -> render first frame
  -> deferred HTTP /v1/props

loop()
  -> ensureLiveView()
  -> grApi.readLiveView(streamReadBuffer, 8192)
  -> mjpeg.process()
  -> onJpegFrame()
  -> decoder.drawFrame()
  -> ui.drawOverlay()
  -> ui.pushCanvas()
```

## HTTP API

- Props：`GET /v1/props HTTP/1.1`，`Connection: close`。
- LiveView：`GET /v1/liveview HTTP/1.1`，`Connection: keep-alive`。
- 中心对焦：写参数放 body，不放 query。GR III/IV 在打开 LiveView 前 `PUT /v1/params/lens` body `focusSetting=spot`；仅竖持离开预览时还原。不在 MJPEG 上发 `POST /v1/lens/focus`（会掐掉 GR III keep-alive）。横持快门：当前 GR III/IV 关闭 `allowBlePreviewParking`，LiveView 期间保持 BLE，快门走 BLE（GR III 横屏 HTTP 快门出现断连后回滚）。以下 HTTP 快门路径代码保留但不启用：`POST /v1/camera/shoot` 表单体 `af=on&pos=50,50`（GR IIIx v1.60 固件确认解析 `af=on/off` 与 `pos`；空 body 缺省 `af=off`。GR III 上「先对焦再拍」仍需真机验证；`pos` 单位未核实）。GR III 对 query `?pos=&af=` 返回 400。spot 快门会掐掉 MJPEG。`af=on` 会很快返回 HTTP 成功，取景还在对焦/写文件；固件先等 2s 再 `GET /v1/liveview`，并等到 JPEG 才算重开成功。空 200 流走 Wi-Fi 重开，不拆 BLE。GR II 快门仍用官方 `af=camera`。
- 从 LiveView 进入 `WifiCredentialsReady`（蓝牙快门界面）时，先在 Wi-Fi
  仍连接期间发送 `POST /v1/device/wlan/finish`，再断开 StickS3 STA，最后按
  代际发送 BLE WLAN OFF 作为幂等兜底；相机可能在 HTTP 响应完成前主动断网。
- GR III/IIIx 的 BLE 兜底通过 `Network Type` UUID 写 `0x00`；BLE 连接保持
  用于对焦和快门。
- GR III/IIIx 从 `WifiCredentialsReady` 再次进入 LiveView 时，先通过 BLE
  写 `Network Type=0x01` 重新开启 AP，再使用缓存的 SSID/密码连接。
- GR IV 进入 `WifiCredentialsReady` 前，向严格识别后的 WLAN 固定 Handle
  `0x0135` 写 `0x00` 关闭 AP；再次进入 LiveView 时写 `0x01` 重新开启。
- 已绑定相机在竖持重连且本地 Wi-Fi 缓存有效时，直接发送 WLAN OFF 并进入
  `WifiCredentialsReady`，不再重复开启 AP、读取相同参数后又关闭；首次配对无缓存，
  仍执行一次 WLAN 开启、参数读取和关闭。
- 两个代际退出 LiveView 后都保持 BLE，用于对焦和快门；关闭 AP 失败只记录
  错误，不会把蓝牙快门状态降级为断连。
- HTTP host 默认 `192.168.0.1:80`。
- `readHttpHeaders()` 最多读取 2048 bytes header。
- Props body 上限：16KB。

## Wi-Fi 连接策略

从 `src/gr_wifi.cpp` 和 `src/main.cpp` 确认：

- `WiFi.mode(WIFI_STA)`。
- `WiFi.setSleep(true)`，注释说明 BLE + Wi-Fi 共存需要 modem sleep。
- `WiFi.setAutoReconnect(true)`。
- 支持 SSID/password、BSSID、channel hint。
- `ConnectGuard` 可在连接轮询中检查 BLE 是否仍连接，失败时提前断开 Wi-Fi。
- 缓存连接在 WLAN-on 应答后立即开始，无固定等待：`WIFI_CACHED_CONNECT_GRACE_MS=0`。
- 缓存连接总预算：`WIFI_CACHED_CONNECT_TIMEOUT_MS=1900`。
- 使用信道提示连接超时：`WIFI_CHANNEL_HINT_CONNECT_TIMEOUT_MS=6000`。
- 总连接超时：`WIFI_CONNECT_TIMEOUT_MS=15000`。
- 缓存连接成功后延迟刷新 BLE Wi-Fi 参数：`WIFI_CACHE_REFRESH_DELAY_MS=5000`。

## BLE / Wi-Fi 共存

ESP32-S3 与 GR IIIx 的 CYW43455 都是 Wi-Fi/BLE 共用一个 2.4 GHz 射频、按时分复用调度，LiveView 期间 BLE 越活跃，MJPEG 可用的空口时间越少。

- **BLE 低占空比（默认关闭）**：`BLE_LIVEVIEW_LOW_DUTY_ENABLED 1` 时，横持 LiveView 持续推流 `BLE_LIVEVIEW_LOW_DUTY_SETTLE_MS`（1.5 s）后，`serviceBleLinkMode()` 把 BLE 连接参数从连接时的 30–50 ms / latency 1 / 5.12 s 改为 102.5 ms / latency 0 / 6 s（`ble_link_mode_policy.h`）。2026-10-13 GR IIIx 实测：低占空比平均约 4.0 FPS，并出现一次快门写入无应答 → 监督超时；连接时参数平均约 6.2 FPS、快门全部确认。原因判断：latency 1 时相机本就每两个事件才应答一次，相机侧负担几乎不变；且相机自身就是 AP，102.5 ms 与其 102.4 ms 信标只差 0.1 ms，BLE 事件会长时间压在信标上。重新启用前应换一个远离信标倍数的间隔。
- 断线后的恢复可能在一次阻塞调用内完成重连，`serviceBleLinkMode()` 通过 `connectionSerial()` 识别新连接并按 Normal 处理。
- 推流短暂中断（重开、恢复）但仍横持时保持当前模式，不来回切换；转竖持或 BLE 断开立即回到 Normal（新连接本就使用 Normal 参数）。
- 更新参数时同时更新 NimBLE 的存储参数，相机若发起参数更新请求，会以当前模式参数应答；请求次数记录在 `peer_req`。
- `BLE_LIVEVIEW_LOW_DUTY_ENABLED 0` 可一键关闭。
- **Wi-Fi 链路设置**（`GrWifi::applyLinkTuning`，begin 与每次 join 前执行）：`WIFI_PS_MIN_MODEM`（BLE 开启时不允许 NONE；MAX_MODEM 会饿死推流）、STA 带宽 HT20、TX 功率上限 19.5 dBm 并回读。
- **状态通知订阅时机**：BatteryLevel / ShotCount / CaptureStatus 的订阅（GR IIIx 上 GATT 发现 4.3–4.8 s）在 `activateCameraWifiOverBle()` 中、BLE WLAN ON 之前完成，不再在 LiveView 打开后由主循环触发（那会阻塞取流、误触发 5 s 卡顿看门狗并多做一次重开）。竖持复用缓存凭据的路径由 `serviceCameraStatusNotify()` 兜底订阅，推流期间不执行。代价：横持首次进入预览会多等这段发现时间。
- **观测**：预览期间每 5 s 输出一行 `LINK:`，含 RSSI 平均/最小（1 s 采样）、信道、TX 功率、STA 断开次数与最近原因码、BLE 实际协商的间隔/latency/超时、相机参数请求次数、卡顿重开次数。STA 断开时另有 `WiFi: STA disconnected reason=` 即时日志。断开计数包含固件主动断开（竖持交接等）。

## 相机 AP 连接快速重试

BLE WLAN ON 后立即 join 时相机 AP 往往还没开始发信标：首次尝试约 3.4 s 才报 `NO_AP_FOUND`，自动重连随后约 0.6 s 连上（GR IIIx，2026-10-13）。`GrWifi::connectTo()` 在 `WIFI_CONNECT_KICK_MS`（1.5 s）内仍未关联（`ARDUINO_EVENT_WIFI_STA_CONNECTED` 未到）时主动断开并重新 join，最多 `WIFI_CONNECT_MAX_KICKS` 次，且剩余超时不足 1 s 时不再重试。已关联（认证 / DHCP 进行中）的连接不会被打断。日志 `WiFi: not associated after ...; re-issuing join (kick n/3)`，完成行带 `kicks=`。

## MJPEG/JPEG/显示

- MJPEG 通过 SOI `0xFFD8` 和 EOI `0xFFD9` 切帧。
- frame buffer 容量：256KB。
- stream read buffer：8192 bytes（GR IIIx 实机 A/B 测试由 2048 提升后，稳定预览约从 7.5 fps 提升至 10.4 fps）。
- JPEG decode 使用 Espressif `esp_new_jpeg` 1.0.2（ESP32-S3 SIMD 优化）。
- 输出为 `RGB565_BE`，持久 216×144 buffer 优先放 PSRAM、失败时回退 internal RAM。
- GR IIIx LiveView 实测为 720×480；decoder 直接缩放到 216×144，居中裁掉上 4 / 下 5 行后以 216×135 显示，无二次缩放。
- 预览在 240×135 canvas 中水平居中（左右各 12 px），之后 `ui.pushCanvas()` 上屏。

## 实时预览卡顿风险

后续优化 LiveView 时必须重点检查：

1. Wi-Fi 阻塞读取：`WiFiClient::read()`、connect timeout、HTTP header/body timeout。
2. JPEG 解码耗时：`JpegDecoder::_lastDecodeMs` 可作为观测点；216×144 PSRAM-output 实测约 38 ms。
3. 屏幕刷新频率：`pushCanvas()` 每帧调用可能影响帧率。
4. buffer 过小：`STREAM_READ_BUFFER_SIZE=8192` 是 GR IIIx 实测后的折中；`FRAME_BUFFER_SIZE=256KB` 不足会导致 dropped frame。
5. 频繁 malloc/free：当前主 frame buffer 只在 setup 分配；新增每帧分配是风险。
6. BLE/Wi-Fi 任务互相抢占：ESP32-S3 BLE + Wi-Fi 共存可能受 modem sleep、任务优先级影响。
7. 长时间 delay：连接/重试路径存在 delay，LiveView 运行路径应避免新增长 delay。
8. watchdog 风险：解码、网络读取、串口大量打印都可能造成长时间不 yield。
9. 串口日志过多：每帧打印会显著拖慢预览。

## TODO_UNVERIFIED

- BLE 低占空比：已在 GR IIIx 上得到负面结果，默认关闭；若换间隔重试，需在 GR III / GR IIIx / GR IV 上对比 FPS、快门确认与断连。
- 状态通知订阅提前后，GR IIIx 横屏拍摄后无应答/断线是否仍出现；必要时用 `RICOH_BLE_SUBSCRIBE_CAMERA_STATUS 0` 做 A/B。
- 不同握持方式与热靴安装下的 RSSI 差异（天线朝向）。
- Wi-Fi 快速重试对横持进入预览耗时的实际改善（预期从约 4 s 降到 2 s 左右），以及 GR IV / GR II 上是否有副作用。

- GR IIIx 以外机型的实际 FPS、JPEG 分辨率、平均 decode ms 与丢帧率需要实机确认。
- Wi-Fi RSSI 与卡顿关联阈值需要实测。

## 后续 Codex 修改代码时必须注意

- Preview 优化必须保留相机电源保护和 BLE guard。
- 不要为了流畅度删除 stall watchdog；可以调整但必须记录依据。
- 新增性能指标时优先低频统计，不要每帧串口打印。
