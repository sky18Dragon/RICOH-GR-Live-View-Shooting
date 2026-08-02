# 2026-07-02 Preview Lag Investigation

## 问题现象

TODO_UNVERIFIED：需要补充实机现象，例如 LiveView 延迟、卡顿、帧率低、画面停顿、重连频繁或显示刷新慢。

## 复现步骤

TODO_UNVERIFIED：建议记录：

1. 相机型号/固件版本。
2. StickS3 固件 commit。
3. 相机与 StickS3 距离、Wi-Fi RSSI。
4. 从启动到 LiveView 的完整串口日志。
5. LiveView 持续运行时长。
6. 是否按 Button A 或触发 Wi-Fi/BLE 恢复。

## 相关日志

```text
TODO_UNVERIFIED：粘贴串口日志片段，至少包含：
- Flow: ... -> LIVEVIEW_RUNNING
- WiFi RSSI
- LiveView stall watchdog
- dropped frame / decode timing（如后续添加）
```

## 相关代码位置

- `src/main.cpp::ensureLiveView()`
- `src/main.cpp::onJpegFrame()`（TODO_UNVERIFIED：请按当前代码定位具体行）
- `src/gr_api.cpp::readLiveView()`
- `src/mjpeg_stream.cpp::process()`
- `src/jpeg_decoder.cpp::drawFrame()`
- `src/display.cpp::drawOverlay()` / `pushCanvas()`
- `src/config.h`：`FRAME_BUFFER_SIZE`、`STREAM_READ_BUFFER_SIZE`、`JPEG_SCALE_POLICY`、`LIVEVIEW_STALL_TIMEOUT_MS`

## 初步结论

当前代码的潜在卡顿点包括：

- Wi-FiClient 可用数据不足导致读取碎片化。
- `STREAM_READ_BUFFER_SIZE=2048` 可能增加循环次数。
- JPEG decode 和 LCD pushImage/pushCanvas 会占用 CPU/SPI。
- 串口日志若加到每帧路径会拖慢。
- BLE/Wi-Fi 共存可能互相影响。
- 长时间 delay 或阻塞连接流程可能影响恢复体验。

## 未确认事项

- 当前实际 FPS。
- 平均/最大 JPEG decode ms。
- 单帧 MJPEG 大小分布。
- Wi-Fi RSSI 与卡顿的关系。
- 是否存在 heap/PSRAM 碎片。

## 后续验证步骤

- [ ] 增加低频性能统计日志，不要每帧打印。
- [ ] 记录 FPS、dropped frames、decode ms、RSSI。
- [ ] 比较不同 `STREAM_READ_BUFFER_SIZE` 的影响。
- [ ] 比较 `JPEG_SCALE_HALF` 与其他 scale 的显示/性能影响。
- [ ] 长时间运行 10/30/60 分钟观察 stall 和内存。

## 维护注意事项

- 不要直接改大 buffer 或 scale 后声称优化成功，必须有前后数据。
- 不要牺牲相机电源保护来换连接速度。
