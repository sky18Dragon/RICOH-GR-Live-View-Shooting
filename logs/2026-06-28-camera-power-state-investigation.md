# 2026-06-28 Camera Power State Investigation

## 问题现象

StickS3 在重启或 BLE 重连过程中，可能连接到处于关机/待机相关状态的 RICOH GR，相机仍允许 BLE 连接。如果固件只根据 BLE 连接成功或 Power State `0x01` 判断相机可用，可能错误发送 Wi-Fi ON，导致相机被自动唤醒。

## 复现步骤

TODO_UNVERIFIED：需要补充完整实机步骤。建议记录：

1. 相机处于关机/待机状态。
2. StickS3 重启。
3. 串口监视启动。
4. 观察是否出现 Operation Mode 为 `BLE_STARTUP` / `POWER_OFF_TRANSFER`。
5. 确认是否出现 `BLE: Wi-Fi open requested`。

## 相关日志

README 中已有示例：

```text
BLE: power handle=0x00EB read value=0x01
BLE: operation mode read value=0x02 state=BLE_STARTUP
WiFi blocked: camera operation mode=BLE_STARTUP while power=ON source=WiFi open
Flow: BLE_READY -> CAMERA_SLEEP_GUARD (BLE operation mode standby)
BLE guard: remote disconnect reason=533; auto wake paused for 15s, then manual wake required
```

TODO_UNVERIFIED：需要补充原始完整串口日志和对应固件 commit。

## 相关代码位置

- `src/config.h`：Power State handle、CCCD、Operation Mode UUID、disconnect reason、guard 配置。
- `src/ricoh_ble_client.cpp::readPowerState()`
- `src/ricoh_ble_client.cpp::readOperationMode()`
- `src/ricoh_ble_client.cpp::enablePowerStateNotify()`
- `src/main.cpp::ensureCameraPowerReadyForWifi()`
- `src/main.cpp::enterCameraSleepGuard()`
- `src/main.cpp::requestManualCameraWake()`

## 初步结论

当前代码通过 Power State + Operation Mode 双判断降低误唤醒风险。`BLE_STARTUP` / `POWER_OFF_TRANSFER` 会进入 `CAMERA_SLEEP_GUARD`，自动流程不会发送 Wi-Fi ON。冷却结束后仍需要 Button A 手动唤醒。

## 未确认事项

- `0x213` / `0x215` 在所有相机状态下的精确含义。
- GR IV 非 HDF 的 Operation Mode 值是否一致。
- 相机固件版本差异是否影响 Power State 行为。

## 后续验证步骤

- [ ] 相机关机状态下 StickS3 重启，确认无 `BLE: Wi-Fi open requested`。
- [ ] 待机状态下直连，确认进入 `CAMERA_SLEEP_GUARD`。
- [ ] 冷却中按 Button A，确认不会立即唤醒。
- [ ] 冷却后按 Button A，确认手动唤醒成功。
- [ ] LiveView 运行中关闭相机，确认 power notify 或 disconnect reason 进入 guard。

## 维护注意事项

- 任何连接加速都不得删除 Operation Mode 检查。
- 任何恢复流程都必须先检查 `cameraSleepGuardBlocksFlow()`。
