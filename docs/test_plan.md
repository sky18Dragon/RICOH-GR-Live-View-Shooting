# Test Plan

## 文档-only 任务

```powershell
git diff --stat
git diff -- AGENTS.md docs logs
```

本类任务不需要 `platformio run`，除非用户明确要求。

## Native 逻辑测试

`platformio.ini` 配置了 `env:native`，包含：

- `ble_reconnect_policy.cpp`
- `camera_identity.cpp`
- `mjpeg_stream.cpp`
- `test/test_native/test_native_logic.cpp`

建议命令：

```powershell
platformio test -e native
```

TODO_UNVERIFIED：本文档创建时未运行该命令。

## 固件构建

```powershell
platformio run
```

通过条件：m5stack-sticks3 环境 SUCCESS，无新增编译 warning/error。

## 烧录与监视

```powershell
platformio run --target upload --upload-port COM6
platformio device monitor --port COM6 --baud 115200 --filter time
```

COM 端口按实际设备替换。

## BLE / Wi-Fi / Power 实机测试用例

1. **首次配对**
   - 擦除 NVS/flash 后启动。
   - 期望：扫描、配对、安全连接、保存 BLE 身份、打开 Wi-Fi、进入 LiveView。
2. **已配对直连**
   - StickS3 重启，相机处于工作状态。
   - 期望：优先直连保存的 BLE 地址，Wi-Fi cache 可短超时尝试，失败后 fresh BLE 参数回退。
3. **相机关机/待机保护**
   - 相机关机或进入 BLE_STARTUP / POWER_OFF_TRANSFER。
   - 期望：不发送 Wi-Fi ON，进入 CAMERA_SLEEP_GUARD。
4. **手动唤醒**
   - Guard 冷却结束后按 Button A。
   - 期望：重建 BLE stack，手动唤醒流程启动。
5. **Button A AF 快门**
   - LiveView 正常运行时按 Button A。
   - 期望：日志包含 `BLE: Ricoh shutter OperationRequest START param=1 autofocus=1`。

## Preview 性能测试

记录：

- FPS。
- dropped frames。
- `JpegDecoder::lastDecodeMs()`。
- Wi-Fi RSSI。
- LiveView stall 次数。
- 重连次数。
- heap/PSRAM 可用量（TODO_UNVERIFIED：当前代码未统一输出）。


## 后续 Codex 修改代码时必须注意

- 测试计划要随代码能力更新。
- 新增协议/外设必须新增对应实机测试用例。
- 优化预览必须提供前后对比数据。
