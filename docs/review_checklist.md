# 代码审查清单

## 修改前

- [ ] 明确本次任务是否允许改代码；若用户限定文档-only，不得触碰 `src/`、`platformio.ini`。
- [ ] 阅读 `PROJECT_NOTES.md`。
- [ ] 根据影响范围阅读相关文档：
  - BLE：`ricoh_ble_protocol.md`
  - Wi-Fi/Preview：`wifi_preview_flow.md`
  - Power：`power_state_policy.md`
  - GPIO：`pin_map.md`
- [ ] 用 `git status --short` 识别已有未提交改动，避免混入本任务。
- [ ] 用 `rg` 找最近调用者和恢复路径。

## 代码风险检查

- [ ] 是否可能在相机关机/待机时自动 Wi-Fi ON？
- [ ] 是否绕过 `ensureCameraPowerReadyForWifi()` 或 `CAMERA_SLEEP_GUARD`？
- [ ] 是否在 BLE 回调里新增耗时操作？
- [ ] 是否在主循环或 LiveView 路径增加长时间阻塞？
- [ ] 是否在每帧路径新增 malloc/free 或大量日志？
- [ ] 是否改变 Wi-Fi cache fallback 语义？
- [ ] 是否影响 Button A 手动唤醒/AF 快门？
- [ ] 是否修改未确认的 UUID/handle/GPIO？

## Preview 专项

- [ ] 记录 `STREAM_READ_BUFFER_SIZE`、`FRAME_BUFFER_SIZE` 影响。
- [ ] 评估 JPEG decode ms。
- [ ] 评估 `pushCanvas()` 频率。
- [ ] 检查 dropped frames 和 stall watchdog。
- [ ] 避免 BLE/Wi-Fi 任务互相抢占。

## Power 专项

- [ ] Power State `0x00EB` 和 Operation Mode 均保留。
- [ ] `BLE_STARTUP` / `POWER_OFF_TRANSFER` 仍阻止自动 Wi-Fi ON。
- [ ] Power notify `0x00` 仍进入 guard。
- [ ] 断连 reason `0x213` / `0x215` 仍进入 guard。
- [ ] 冷却结束后仍要求 Button A 手动唤醒。

## 验证

- [ ] 仅文档改动：`git diff --stat`、`git diff -- PROJECT_NOTES.md docs logs`。
- [ ] native 逻辑：`platformio test -e native`。
- [ ] 固件构建：`platformio run`。
- [ ] 实机：串口日志包含启动、BLE、Power/Operation Mode、Wi-Fi、HTTP、LiveView。
- [ ] 若改 BLE/Wi-Fi/Power，必须记录相机关机状态下 StickS3 重启不会自动唤醒相机。

## 输出

- [ ] 修改文件列表。
- [ ] 已确认事实。
- [ ] `TODO_UNVERIFIED`。
- [ ] 验证命令和结果。
- [ ] 风险和下一步。

## 审查重点

- 审查时优先查误唤醒、卡顿、阻塞、内存和 GPIO 冲突。
- 对没有实测证据的协议结论必须打回。
