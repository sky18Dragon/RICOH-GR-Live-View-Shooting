# Camera Power State Policy

## 目标

避免 StickS3 在相机处于关机/待机状态时自动唤醒 RICOH GR。该策略对 BLE 重连、Wi-Fi ON、LiveView 恢复都具有最高优先级。

## 已确认的电源相关常量

从 `src/config.h` 确认：

| 常量 | 值 | 含义 |
| --- | --- | --- |
| `RICOH_BLE_GR4_POWER_STATE_HANDLE` | `0x00EB` | Power State read/notify handle |
| `RICOH_BLE_GR4_POWER_STATE_CCCD_HANDLE` | `0x00EC` | Power State notify CCCD |
| `RICOH_BLE_GR4_POWER_STATE_ON_VALUE` | `0x01` | powered on / controllable |
| `RICOH_BLE_GR4_POWER_STATE_OFF_VALUE` | `0x00` | power-off or shutting down |
| `RICOH_BLE_DISCONNECT_REMOTE_USER` | `0x213` | power-off/user remote disconnect candidate |
| `RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF` | `0x215` | power-off disconnect candidate |
| `CAMERA_POWER_OFF_COOLDOWN_MS` | `15000` | guard cooldown |
| `RICOH_BLE_POWER_NOTIFY_SETTLE_MS` | `30` | 订阅 Power Notify 后的短等待窗口，用于在 Wi-Fi ON 前消费立即到来的 `0x00` 关机通知 |
| `RICOH_BLE_REQUIRE_POWER_ON_BEFORE_WIFI` | `true` | Wi-Fi ON 前必须读电源 |
| `RICOH_BLE_ALLOW_WIFI_WHEN_POWER_UNKNOWN` | `false` | power unknown 不允许 Wi-Fi ON |
| `RICOH_BLE_BLOCK_WIFI_IN_STANDBY_OPERATION_MODE` | `true` | standby operation mode 阻止 Wi-Fi ON |

## Operation Mode 保护

从代码确认：

- `0x00` = `CAPTURE`
- `0x01` = `PLAYBACK`
- `0x02` = `BLE_STARTUP`
- `0x03` = `OTHER`
- `0x04` = `POWER_OFF_TRANSFER`

`BLE_STARTUP` 和 `POWER_OFF_TRANSFER` 被 `isCameraStandbyOperationMode()` 视为待机/关机相关状态。

## Wi-Fi ON 前置条件

`ensureCameraPowerReadyForWifi()` 执行顺序：

1. 如果 `CAMERA_SLEEP_GUARD` active，直接阻止。
2. BLE 未连接，返回 false。
3. 读取 Power State，最多 `RICOH_BLE_POWER_READ_RETRIES=2`。
4. 如果 Power State 为 On 且启用 standby check，则读取 Operation Mode，最多 `RICOH_BLE_OPERATION_MODE_READ_RETRIES=2`。
5. 如果 Operation Mode 是 `BLE_STARTUP` / `POWER_OFF_TRANSFER` 且不是 manual wake override，则进入 `CAMERA_SLEEP_GUARD`，不打开 Wi-Fi。
6. 如果 Power State On，订阅 power notify。
7. 订阅后等待 `RICOH_BLE_POWER_NOTIFY_SETTLE_MS=30`，并消费这段时间内到达的 Power Off notify；若收到 `0x00`，进入 `CAMERA_SLEEP_GUARD`，不打开 Wi-Fi。
8. 没有收到 Power Off notify 时才允许 Wi-Fi ON。
9. Power Off 或 Unknown 且没有 override 时进入 guard。

## CAMERA_SLEEP_GUARD 行为

进入 guard 时：

- `cameraAutoWakeBlocked=true`。
- `cameraAutoWakeCooldownUntil = millis() + 15000`。
- 关闭 LiveView。
- 断开 Wi-Fi。
- 断开 BLE。
- 状态转为 `CameraSleepGuard`。
- 15 秒冷却期间禁止自动扫描/自动重连/Wi-Fi ON。
- 冷却结束后仍不自动唤醒；需要 Button A 手动唤醒。

Button A 手动唤醒：

- 冷却未结束：只提示，不唤醒。
- 冷却结束：清除 guard，设置 `cameraManualWakeOverride=true`，断开链路，重建 BLE stack，等待 `BLE_MANUAL_WAKE_REINIT_SETTLE_MS=3000`，再运行相机连接流程。

## 必须保持的安全规则

- Power State `0x01` 不是充分条件；必须同时考虑 Operation Mode。
- 自动恢复路径不得绕过 `cameraSleepGuardBlocksFlow()`。
- BLE 断连 reason `0x213` / `0x215` 在 ready/liveview 阶段必须进入 guard。
- Power notify `0x00` 必须进入 guard。
- 开 Wi-Fi 前不得跳过 Power Notify settle；否则相机刚关机时可能读到旧的 Power On 后立刻收到 `0x00`，导致误开 Wi-Fi。

## TODO_UNVERIFIED

- `0x213` / `0x215` 在所有固件版本和所有 GR IV 机型上的精确定义需要更多实机日志确认。
- Power State handle / Operation Mode UUID 在 GR IV 非 HDF 上的兼容性未确认。
- `OTHER` mode 是否允许 Wi-Fi ON 未由代码特殊处理，语义仍未确认。

## 后续 Codex 修改代码时必须注意

- 任何降低等待时间、减少读取次数、跳过 Operation Mode 的优化都可能重新引入误唤醒。
- 修复连接速度问题时不能以自动唤醒关机相机为代价。
- 修改 Button A 行为时必须保留手动唤醒语义。
