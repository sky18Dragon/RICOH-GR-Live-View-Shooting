# Changelog for AI Agents

本文件记录对后续 AI/Codex 有帮助的项目级事实变化。代码提交日志仍以 Git 为准。

## 2026-07-03 — 建立 Codex 嵌入式项目记忆系统

新增/完善：

- `AGENTS.md`
- `docs/project_overview.md`
- `docs/hardware.md`
- `docs/pin_map.md`
- `docs/ricoh_ble_protocol.md`
- `docs/wifi_preview_flow.md`
- `docs/power_state_policy.md`
- `docs/known_issues.md`
- `docs/codex_review_checklist.md`
- `docs/test_plan.md`
- `logs/README.md`
- `logs/issue_template.md`
- `logs/2026-06-28-camera-power-state-investigation.md`
- `logs/2026-07-02-preview-lag-investigation.md`

核心记忆：

- 当前固件已在 RICOH GR IV 与 RICOH GR IV HDF 上实机验证。
- BLE/Wi-Fi/LiveView 以 GR IV BLE-first 流程为核心。
- Power State `0x00EB` + Operation Mode 是防止误唤醒相机的核心策略。
- `BLE_STARTUP` / `POWER_OFF_TRANSFER` 不得自动 Wi-Fi ON。

## 历史事实索引

- 2026-06-27/28：代码注释记录 GR IV WLAN handles 与 Power State handles 来自 Android app / HCI logs 抓包。
- 当前 README：GR IV 与 GR IV HDF 已验证可用，GR III/GR II 不可用。
- 当前代码：Button A 使用 RICOH Shooting Service 的 ShootingFlavor + OperationRequest。

## TODO_UNVERIFIED

- GR IV / GR IV HDF 长时间运行和不同相机固件版本兼容性。
- 预览性能基线数据。

## 后续 Codex 修改代码时必须注意

- 每次重要功能改动后更新本文件一条 AI-readable 记录。
- 记录应区分代码确认、实机确认和 TODO_UNVERIFIED。
