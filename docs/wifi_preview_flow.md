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
  -> grApi.readLiveView(streamReadBuffer, 2048)
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
- stream read buffer：2048 bytes。
- JPEG decode 使用 JPEGDEC `openRAM()`。
- Pixel type 设置为 `RGB565_BIG_ENDIAN`。
- JPEG scale：`JPEG_SCALE_POLICY`，当前 config 默认 `JPEG_SCALE_HALF`。
- 解码后绘制到 M5Canvas / LovyanGFX，之后 `ui.pushCanvas()` 上屏。

## 实时预览卡顿风险

后续优化 LiveView 时必须重点检查：

1. Wi-Fi 阻塞读取：`WiFiClient::read()`、connect timeout、HTTP header/body timeout。
2. JPEG 解码耗时：`JpegDecoder::_lastDecodeMs` 可作为观测点。
3. 屏幕刷新频率：`pushCanvas()` 每帧调用可能影响帧率。
4. buffer 过小：`STREAM_READ_BUFFER_SIZE=2048` 过小可能增加循环次数；`FRAME_BUFFER_SIZE=256KB` 不足会导致 dropped frame。
5. 频繁 malloc/free：当前主 frame buffer 只在 setup 分配；新增每帧分配是风险。
6. BLE/Wi-Fi 任务互相抢占：ESP32-S3 BLE + Wi-Fi 共存可能受 modem sleep、任务优先级影响。
7. 长时间 delay：连接/重试路径存在 delay，LiveView 运行路径应避免新增长 delay。
8. watchdog 风险：解码、网络读取、串口大量打印都可能造成长时间不 yield。
9. 串口日志过多：每帧打印会显著拖慢预览。

## TODO_UNVERIFIED

- 当前实际 FPS、平均 JPEG decode ms、丢帧率需要实机日志确认。
- 相机 LiveView MJPEG 分辨率、帧率和单帧最大大小需要采样确认。
- Wi-Fi RSSI 与卡顿关联阈值需要实测。

## 维护注意事项

- Preview 优化必须保留相机电源保护和 BLE guard。
- 不要为了流畅度删除 stall watchdog；可以调整但必须记录依据。
- 新增性能指标时优先低频统计，不要每帧串口打印。
