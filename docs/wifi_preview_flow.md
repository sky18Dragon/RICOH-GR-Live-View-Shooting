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
  -> HTTP /v1/props
  -> HTTP /v1/liveview
  -> LIVEVIEW_RUNNING

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
- 缓存连接短超时：`WIFI_CACHED_CONNECT_TIMEOUT_MS=1200`。
- 使用信道提示连接超时：`WIFI_CHANNEL_HINT_CONNECT_TIMEOUT_MS=6000`。
- 总连接超时：`WIFI_CONNECT_TIMEOUT_MS=15000`。
- 缓存连接成功后延迟刷新 BLE Wi-Fi 参数：`WIFI_CACHE_REFRESH_DELAY_MS=5000`。

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
