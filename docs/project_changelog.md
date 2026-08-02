# 项目变更记录

本文件记录会影响后续开发的项目级事实。具体代码变更仍以 Git 提交记录为准。

## 2026-08-01：合入 PR #14 的预览改进

感谢 Willy Hardy 在 PR #14 中完成 GR IIIx HDF 的预览性能测试和实现。当前分支保留自己的 GR III 协议代码，因为它直接基于当前 `main`，并已在 GR III HDF 上完成配对、Wi-Fi 和 LiveView 验证。

本次移植了以下部分：

- Espressif `esp_new_jpeg` 解码器和 216 x 144 RGB565 输出。
- 8 KiB 读流、内部 RAM 横屏 Canvas 和更完整的预览统计。
- Button B 短按切换镜像，设置保存到 NVS。
- 已绑定相机启动时先尝试 BLE 地址直连，失败后回退扫描。
- LiveView 首帧优先，相机属性请求延后执行。

代码已通过 51 项 native 测试和 StickS3 production build。优化后的 GR III HDF FPS 和长时间稳定性仍需实机复测。

## 2026-07-03：整理项目参考资料

新增/完善：

- `PROJECT_NOTES.md`
- `docs/project_overview.md`
- `docs/hardware.md`
- `docs/pin_map.md`
- `docs/ricoh_ble_protocol.md`
- `docs/wifi_preview_flow.md`
- `docs/power_state_policy.md`
- `docs/known_issues.md`
- `docs/review_checklist.md`
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

## 2026-08-01：GR III HDF 实机验证

- 擦除 StickS3 配置后完成首次 passkey 配对，连接达到 encrypted、authenticated、bonded。
- 检测为 `GR3_FAMILY`，通过 UUID 打开相机 Wi-Fi 并读取 SSID、passphrase 和 channel。
- 成功连接相机 AP，`/v1/props` 返回 `RICOH GR III HDF`，LiveView 正常启动。
- 记录窗口内渲染 30 帧、丢帧 0；预览约 3 FPS。
- 新增 StickS3 六位配对码界面：Button A 修改当前数字，短按 Button B 确认。
- 移除实机调试使用的全量 GATT 输出与临时 pairing 日志。

## 2026-07-30：GR III 支持代码评审加固（代码确认）

对 `feature/gr3-support` 分支的评审整改，影响后续开发的事实：

- 协议判定安全规则：发现 GR III WLAN service 即否决 GR IV 判定（`detectRicohProtocol`），
  且不再对疑似 GR III 的相机探测 GR IV fixed handle。GR III 判定仍需
  WLAN service + Network Type characteristic 双重 GATT 证据。
- SMP IO capability 按代际设置：GR IV / 未知代际恢复 `DISPLAY_YESNO`
  （与实机验证过的配对流程一致），仅 GR III 链路使用 `KEYBOARD_DISPLAY`。
- 连接流程不再做全量 service discovery（`getService(uuid)` 按需定向发现）；
  已绑定设备跳过加密前探测与 protected read，缩短重连耗时。
- GR III passkey：输入窗口对齐 SMP 30 s 超时（`RICOH_BLE_GR3_PASSKEY_ENTRY_WAIT_MS`）、
  提示时清空串口缓冲并检查 `injectPassKey` 返回值。USB 输入保留为开发备用路径。
- GR III 拒绝 numeric-comparison 配对（只接受 passkey entry），避免伪认证绑定。
- Power notify `0x02` 仅在 GR III 链路视为关机信号，GR IV 行为不变。
- GR III Wi-Fi 信道接受 0-14（欧日机型可用 12-14），信道字节兼容 ASCII/二进制
  （`parseGr3WifiChannel`）；SSID/passphrase 直接取 characteristic 值，
  不再套用 GR IV 的启发式解析（改名 SSID 也能通过）。
- `openWifi()` GR III 路径自行读取 Operation Mode，不再依赖
  `RICOH_BLE_BLOCK_WIFI_IN_STANDBY_OPERATION_MODE` 开关的调用顺序。
- 扫描评分：已存身份的地址/名称匹配（+2500）压过任何服务位组合，
  邻近陌生 GR 不再抢占重连。
- `CameraProtocolProfile` 收敛为真实被读取的字段（Wi-Fi 方法分发 +
  `requiresAuthenticatedLink`）；`hasRicohIdentitySignal` 由 client 与 main 共享。
- GATT 读写回调上下文改为 `std::atomic` + 单次所有权交接，消除双核竞态。
- `STREAM_READ_BUFFER_SIZE` 保持 8192：该常量经 `AppConfig::Buffer::kStreamReadBufferSize`
  决定 `main.cpp` 的 `streamReadBuffer` 大小（live view 读流缓冲），
  更大的读块减少每帧 readFrame/processFrameData 次数（预览性能改动，待实机测量基线）。

## 历史事实索引

- 2026-06-27/28：代码注释记录 GR IV WLAN handles 与 Power State handles 来自 Android app / HCI logs 抓包。
- 当前 README：GR IV、GR IV HDF 与 GR III HDF 已有实机验证；其他 GR III 系列机型仍需回归。
- 当前代码：Button A 使用 RICOH Shooting Service 的 ShootingFlavor + OperationRequest。

## TODO_UNVERIFIED

- GR IV / GR IV HDF 长时间运行和不同相机固件版本兼容性。
- 预览性能基线数据。
- 标准 GR III 与 GR IIIx 尚未在当前分支完成全流程实机验证。
- GR III HDF 的 Button A 快门发送仍需独立日志记录。
- 2026-07-30 连接流程改动（定向 discovery、绑定跳过预探测）需在 GR IV 实机回归。

## 维护要求

- 每次重要功能改动后更新本文件。
- 记录应区分代码确认、实机确认和 TODO_UNVERIFIED。
