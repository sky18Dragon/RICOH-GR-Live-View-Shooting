# Changelog for AI Agents

本文件记录对后续 AI/Codex 有帮助的项目级事实变化。代码提交日志仍以 Git 为准。

## 2026-10-13 — LiveView BLE 快门回滚 + BLE/Wi-Fi 共存调优（待实机验证）

- 代码确认：GR III / GR IV `allowBlePreviewParking=false`，横屏 LiveView 期间保持 BLE、快门走 BLE（GR III 横屏 HTTP 快门出现断连后回滚）；`LIVEVIEW_MAX_FPS` 回到 7。HTTP 快门路径代码保留未启用。
- 代码确认：新增 BLE LiveView 低占空比（102.5 ms / latency 0 / 6 s）机制与开关 `BLE_LIVEVIEW_LOW_DUTY_ENABLED`，Normal 参数与 `connect()` 一致由 `static_assert` 保证；重连识别用 `connectionSerial()`。
- 实机确认（GR IIIx，用户日志 2026-10-13）：低占空比平均约 4.0 FPS 且出现一次快门写入无应答 → 监督超时；连接时参数平均约 6.2 FPS、8/8 快门确认。据此默认关闭低占空比。Wi-Fi RSSI −30～−52 dBm，无意外断开，Wi-Fi 链路不是瓶颈。
- 代码确认：相机状态通知订阅移到 BLE WLAN ON 之前；推流期间不再触发（原先 LiveView 打开后阻塞 4.3–4.8 s，误触发卡顿看门狗）。
- 代码确认：Wi-Fi 显式 `WIFI_PS_MIN_MODEM`、STA HT20、TX 19.5 dBm；新增 STA 断开原因记录与 `LINK:` 周期日志。
- 实机确认（GR IIIx，用户日志 2026-10-13 15:31–15:35）：状态通知订阅提前到 WLAN ON 之前后无误报卡顿，横屏 20 次 BLE 快门全部确认、无断连，FPS 约 5.5。
- 代码确认：相机 AP join 未关联 1.5 s 即重新 join（最多 3 次）；Passkey 输入窗口 45 s → 28 s 并显示倒计时（GR IIIx 约 30 s 以 0x213 放弃配对），提交后安全等待延长 `RICOH_BLE_PASSKEY_BOND_WAIT_MS`（10 s）。
- 文档修正：GR III WLAN 门控实际为"读取成功且非 BLE_STARTUP / POWER_OFF_TRANSFER"（代码早已放行 OTHER / PLAYBACK），`power_state_policy.md` 等已同步。
- 实机确认（GR IIIx，用户日志 2026-10-13 15:45–15:50）：Wi-Fi 快速重试每次在 1.53 s 触发、约 0.27 s 后连上，相机 AP join 由约 4.0 s 降到约 1.8 s；Passkey 在窗口第 ~26 s 提交、31.3 s 完成绑定，依赖新增的提交后等待。
- 代码确认：充电指示去抖（`charging_indicator_policy.h`）：开即时显示，关需原始值持续 12 s 为未充电；满电补充时的 3–6 s 来回跳动显示为充电中。
- 自动化：Native 121/121 通过。ESP32 固件编译未在本次会话验证（用户本机编译烧录）。
- TODO_UNVERIFIED：订阅提前后 GR IIIx 横屏拍摄断线是否复现；GR III / GR IV 回归；RSSI 与安装方式关系。

## 2026-09-04 — GR II 支持完成实机验证

- 用户确认当前 `codex/feat-gr-ii-support` 分支的 RICOH GR II 支持已在真实相机上验证通过。
- 已验证支持范围包括 Wi-Fi-only 手机配网、连接相机 AP、LiveView 和 HTTP 快门链路；GR II 不使用 BLE。
- 支持状态升级为“已实机验证”。相机固件版本、长期 FPS、MJPEG 响应细节和是否进入过快门 fallback 分支仍需在正式实机记录中补充，不能据此推断所有 GR II 固件版本均已回归。

## 2026-07-24 — GR IIIx 本分支固件实机验证

- 用户确认当前 `codex/feat-gr3-family-support` 分支固件已通过 RICOH GR IIIx 相机实机验证。
- 该结论只升级 GR IIIx，不自动覆盖 GR III、GR III HDF 或 GR IIIx HDF；这些机型仍需独立实机记录。
- 详细实机矩阵、相机固件版本、StickS3 commit、是否清空 NVS、FPS/耗时和去密日志仍待补充到 `docs/gr3_family_test_record.md`。

## 2026-07-18 — GR III Family 协议 Profile 实现（待实机验证）

- 新增 `Gr2Family` / `Gr3Family` / `Gr4Family` / `Unknown` 能力模型；Unknown 默认拒绝 WLAN、Power、Shutter 副作用写入，GR II 仅预留 ManualOnly/ManualConfiguration。
- GR IV 保留固定 Handle 路径；GR III WLAN/Power/凭据正式路径只使用 Service + Characteristic UUID，不做跨代盲写回退。
- 新增 GR III 六位 Passkey 设备端输入、地址类型归一化、受保护读取触发配对，以及只按明确安全错误删除旧 Bond 的恢复策略。
- NVS Profile schema 升至 v4，旧 v3 身份/Bond/Wi-Fi 数据可读取，缺少代际字段时安全地重新识别。
- GR III WLAN 必须 authenticated encryption + fresh `CAPTURE`；待机后断开，以 Profile 的 8 秒最小间隔建立只读探测连接。
- 2026-07-18 自动化结果：Native 27/27，StickS3 发布构建成功。GR III/IIIx 实机矩阵和本次 GR IV/IV HDF 回归尚未执行，不得标记为本分支已实机验证。
- 实机记录模板：`docs/gr3_family_test_record.md`。

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
