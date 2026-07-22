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
- `ui/ButtonInput.cpp`
- `ui/OrientationTracker.cpp`
- `ui/UiAnimator.cpp`
- `ui/UiCoordinator.cpp`
- `test/test_native/test_native_logic.cpp`

建议命令：

```powershell
platformio test -e native
```

UI 纯逻辑测试覆盖状态到场景映射、优先级、姿态稳定/滞回、动画和 `millis()` 溢出、Button A 单次 Shoot、Button B 进度/取消/单次重置，以及快门 overlay 生命周期。

`src/ui/NativeBuildMain.cpp` 只在 `RVF_NATIVE_BUILD && !PIO_UNIT_TESTING` 时提供 smoke `main()`，使 `platformio run -e native` 可验证全部纯逻辑对象的独立链接；Unity 测试和 StickS3 固件均不会使用该入口。

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
6. **姿态与 Canvas**
   - 横竖切换 20 次，记录每次 `UI Canvas: switch/ready` 的 heap 和 PSRAM。
   - 期望：无崩溃、花屏、持续内存下降；临界角度不会反复旋转。
7. **竖屏流保持**
   - LiveView 运行后竖握至少 30 秒，再恢复横握。
   - 期望：竖屏遥控器 UI 不被 JPEG 覆盖，Supervisor 不报 stall，横屏后画面继续。
8. **Button A 输入时序**
   - 分别短按和按住超过 300 ms 后松开。
   - 期望：每次完整操作只有一次 `Shoot`/一张照片；长按期间只有 UI/声音反馈。
9. **Button B 重置进度**
   - 先在 3 秒前释放，再完整按满 3 秒。
   - 期望：第一次不重置，第二次只重置一次，横竖屏均显示进度。
10. **休眠与反馈降级**
    - 验证低亮过渡、A 唤醒提亮、声音；再模拟 IMU/扬声器不可用。
    - 期望：失败时静默降级，不影响连接、预览和拍摄。

## Preview 性能测试

记录：

- FPS。
- dropped frames。
- `JpegDecoder::lastDecodeMs()`。
- Wi-Fi RSSI。
- LiveView stall 次数。
- 重连次数。
- 横竖 Canvas 切换前后 heap/PSRAM 可用量（串口 `UI Canvas:` 日志）。


## 后续 Codex 修改代码时必须注意

- 测试计划要随代码能力更新。
- 新增协议/外设必须新增对应实机测试用例。
- 优化预览必须提供前后对比数据。
