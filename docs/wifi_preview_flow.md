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

- `WiFi.persistent(false)` 后进入 `WiFi.mode(WIFI_STA)`；驱动侧 STA 配置只保留在 RAM，
  避免相机 AP 重启后复用上一轮 WLAN 会话的安全上下文。
- `WiFi.setSleep(true)`，注释说明 BLE + Wi-Fi 共存需要 modem sleep。
- `WiFi.setAutoReconnect(false)`；连接重试、退避和相机 WLAN 生命周期由应用状态机统一控制。
- GR III/IV 由 BLE 明确开启的新 WLAN 会话直接使用 SSID-only；缓存 BSSID/channel
  只用于诊断和成功后的回采，不作为关联约束。GR II 手动开启 AP 的兼容路径仍支持
  `BSSID+channel → channel-only → SSID-only` 逐级放松缓存约束。
- 收到 `INVALID_PMKID(49)` 时清理 RAM-only STA 安全上下文，并在原连接总预算内立即
  重试一次；不会把它误判为密码错误，也不会无限重试。
- 收到 `NO_AP_FOUND(201)` 时不再空等到总预算结束，而是在同一预算内立即重新扫描，
  最多两次；相机 AP 慢启动时，后续扫描可以直接衔接 PMKID 清理与关联。
- 注册 STA connected/GOT_IP/disconnected/lost-IP 事件，失败日志包含 reason code、
  reason name、断线 RSSI、关联耗时和 DHCP 耗时。
- `ConnectGuard` 可在连接轮询中检查 BLE 是否仍连接，失败时提前断开 Wi-Fi。
- 缓存连接在 WLAN-on 应答后立即开始，无固定等待：`WIFI_CACHED_CONNECT_GRACE_MS=0`。
- GR III/IV 缓存连接总预算：`WIFI_CACHED_CONNECT_TIMEOUT_MS=11000`。冷启动 AP 约 4 秒，
  连续 WLAN-off/on 姿态循环约 8 秒后才可关联；该预算还覆盖同一次 SSID-only 调用内的
  一次 PMKID 清理重试，避免正常恢复落入 fresh BLE 参数读取和重复 NVS 写入。
- BSSID 与信道提示阶段各最多 3000 ms，阶段间退避 400 ms，退避计入总预算。
- 总连接超时：`WIFI_CONNECT_TIMEOUT_MS=15000`。
- 缓存连接成功后延迟刷新 BLE Wi-Fi 参数：`WIFI_CACHE_REFRESH_DELAY_MS=5000`。
  BLE 返回的 BSSID/channel 仅为非绑定提示；SSID/密码未变化时保留连接实测值，实际
  BSSID/channel 也未变化时跳过 NVS 写入。

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

- GR IIIx 以外机型的实际 FPS、JPEG 分辨率、平均 decode ms 与丢帧率需要实机确认。
- Wi-Fi RSSI 与卡顿关联阈值需要实测。

## 后续 Codex 修改代码时必须注意

- Preview 优化必须保留相机电源保护和 BLE guard。
- 不要为了流畅度删除 stall watchdog；可以调整但必须记录依据。
- 新增性能指标时优先低频统计，不要每帧串口打印。
