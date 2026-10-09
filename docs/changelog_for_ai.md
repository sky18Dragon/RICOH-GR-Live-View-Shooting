# Changelog for AI Agents

本文件记录对后续 AI/Codex 有帮助的项目级事实变化。代码提交日志仍以 Git 为准。

## 2026-10-09 — 移除 Wi-Fi 分支混入的检测功能

- 移除 FACE/PERSON 检测源码、后台任务、模型初始化、检测框、模式持久化和对应文档/测试。
- 恢复 main 的 Button B 单击镜像切换、JPEG 镜像渲染及 `disp_mirror` 设置恢复；双击锁定与长按清配对沿用 main。
- 保留 Wi-Fi 连接计划、SSID-only、NO_AP 重扫、PMKID 限次恢复、同会话 fresh 兜底、NVS 跳写及其测试。
- 相机配对/凭据存储格式未改变。旧设备即使留有 `detect_mode`，新实现也不再读取或启动检测；无需为本次清理擦除 NVS。
- 两份可行性/跨机型报告保留为历史分析，并标注清理后状态。
- 清理后验证：Native 100/100 通过；StickS3 clean 后完整 release 构建成功，Flash 1,467,421 B（43.9%），静态 RAM 66,492 B（20.3%）。ELF 无 `DetectionManager`/`HumanFaceDetectMSR01` 符号，保留镜像接口。未烧录或进行新的实机验证。

## 2026-10-08 — Wi-Fi 连接稳定性改造与首轮实机收敛

- 新增纯逻辑连接计划：缓存快连按 `BSSID+channel`、`channel-only`、`SSID-only`
  逐级放松约束；最终全扫描明确清除 BSSID 和 channel，修复旧实现只清 channel 的
  “伪全扫描”问题。
- Arduino Wi-Fi 自动重连已关闭，由 AppController 统一控制相机 WLAN 生命周期、连接
  重试和阶段退避；缓存快连预算由 1900 ms 调整为 3000 ms，总预算仍为 15000 ms。
- `GrWifi` 注册 connected/GOT_IP/disconnected/lost-IP 事件，输出 reason code/name、
  断线 RSSI、关联耗时和 DHCP 耗时；event task 只保存原子诊断状态。
- GR III/IIIx Wi-Fi 凭据校验接受 0（无提示）及 1–13 信道；GR IV BLE WLAN 日志新增
  `security_type`，尚未在无实测映射时自动放宽 auth mode。
- 首轮实机：姿态触发重连 6/6 成功，但每轮均先经历 BSSID 超时、
  `INVALID_PMKID(49)`，最终由 SSID-only 恢复，平均 Wi-Fi 耗时 8.371 秒；冷启动 5 轮
  首次自动完成 4/5，且 BLE channel 两次返回 1、实际 AP channel 为 11。
- 基于上述证据，GR III/IV 的 BLE 管理 WLAN 会话改为直接 SSID-only；设置
  `WiFi.persistent(false)`，reason 49 时清理 RAM-only STA 安全上下文并在原预算内重试
  一次。GR II 手动 AP 路径保留分阶段提示兼容策略；缓存连接预算调整为 7000 ms。
- 第二轮实机触发一次 `INVALID_PMKID(49)`，RAM-only 安全上下文清理后 fresh 连接调用
  1381 ms 内恢复，证明定向恢复路径生效；同时发现姿态恢复错误地先使用 15 秒 fresh
  预算、失败后又重开 WLAN。现已改为同一次 WLAN-on 内按“7 秒缓存 → fresh 参数
  15 秒兜底”执行。
- 最新自动化结果：Native 102/102；StickS3 发布构建成功（Flash 57.1%，静态 RAM
  22.9%）。追加修复已重新烧录验证：只进行一次 WLAN-on，7 秒缓存失败后在同会话
  fresh 兜底，PMKID 清理后 1381 ms 连接，WLAN-on 至 LiveView 约 9.73 秒；上一版同类
  路径约 26 秒。RF、供电和不同机型兼容性不能由本轮结果外推。
- 随后 5 轮连续姿态重连 5/5 成功，但 7 秒缓存预算每次都在 AP 约 8 秒可用之前耗尽，
  导致每轮 fresh BLE 读取及重复 NVS 写入。缓存预算据此调整为 11000 ms，让 PMKID
  清理重试留在同一次 SSID-only 调用。
- 11000 ms 版本 3 轮复测仍全部空等到超时，证明根因是驱动在首次
  `NO_AP_FOUND(201)` 后不会自行开始下一次扫描，而非预算不足；后两轮还出现 LiveView
  打开慢、关闭或 stall。现改为 reason 201 后在原预算内立即重新 `WiFi.begin()`，最多
  两次。
- NO_AP 立即重扫版本实机 3/3 姿态重连成功，Wi-Fi 4921–5512 ms，LiveView
  402–465 ms，均未进入 fresh 失败兜底或复现画面 stall。延迟 BLE 刷新现在只以
  SSID/密码判断稳定凭据是否变化；连接实测 BSSID/channel 未变化时跳过 NVS 写入。
  最新自动验证为 Native 104/104，StickS3 发布构建成功。最终烧录验证 Wi-Fi 4905 ms、
  LiveView 441 ms，并同时出现 `NVS write skipped` 与保留实测 BSSID/channel 日志。
- 最终固件追加 10 轮姿态压力测试 10/10 成功；Wi-Fi 平均 5084 ms、P50 4920 ms、
  样本最慢 5508 ms。LiveView 9/10 在 510 ms 内；一轮 RSSI -61 dBm 时为 3185 ms，
  随后正常出帧。未出现 fresh 兜底、重复 WLAN-on、重启或 stall，姿态样本累计 13/13。
- 15 分钟持续预览实际覆盖约 16 分钟，stream/render 计数同步增加 8563，dropped=0；
  FPS 大多 8.0–9.6，heap/PSRAM 无持续下降，未见复位、重连、断流或 watchdog。
- 最终固件冷启动回归 3/3 成功；Wi-Fi 平均 4686 ms、P50 4906 ms、最慢 5377 ms，均
  由缓存 SSID-only 完成且未进入 fresh 兜底。LiveView 两轮约 0.41 秒，一轮出现 4.08 秒
  单次长尾但随后稳定且未复现。综合实机覆盖为姿态重连 13/13、冷启动 3/3、约 16 分钟
  持续预览无断流；当前 GR4_FAMILY 上未复现原始“WLAN 已开启但一直未连接”问题。
- 每次冷启动仍可见 M5PM1 Reg `0x48` I2C 读取失败并回退 M5Unified，以及 USB charging
  状态跳变；它们未影响本轮网络验收，继续按独立电源/PMIC 问题跟踪。
- 2026-10-09 补充 GR4_FAMILY 关机安全回归：相机完全关机时 StickS3 只扫描、不连接且
  不发送 WLAN-on；LiveView 中关闭相机后，Power Notify `0x00`/Operation Mode `OTHER`
  在 Wi-Fi allow 前进入 `CAMERA_POWER_OFF` 并暂停扫描。保护态内 Button A 能清除 guard、
  重建 BLE，再次确认 Power `0x01` + `CAPTURE` 后恢复；Wi-Fi 4918 ms、LiveView 405 ms，
  未出现自动误唤醒或重复 WLAN-on。
- AP 启动时序扰动实机通过：横放触发连接约 1–2 秒后转竖屏，连接在 1777 ms 干净取消、
  发送一次 WLAN OFF 并返回 `WIFI_CREDENTIALS_READY`；再次横放后 Wi-Fi 5371 ms、
  LiveView 450 ms 恢复，无重复 WLAN-on、重连风暴或复位。计划中的 20 轮快速冒烟被用户
  跳过，姿态重连样本仍为 13/13。
- 纯电池供电目视回归通过：冷启动约 10 秒进入 LiveView，随后 3 轮姿态重连均完成，未
  报告卡死、重启或异响。因拔除 USB 而无串口分段耗时/RSSI/reset 数据，不与 USB 样本
  合并；USB 拔除后高频啸叫消失，继续支持将该现象按独立供电/充电/PMIC 问题跟踪。
- 纯电池、约 3 米距离下追加 3 轮姿态重连，用户目视确认连接与 LiveView 均正常；无串口
  RSSI/P95 数据，因此只作为距离裕量的功能证据。
- 新增默认关闭的 `m5stack-sticks3-wifi-cache-fault` 测试环境，仅在 GR4 Profile 加载后于
  RAM 同时注入错误 BSSID/channel。实机确认仍使用 SSID-only，2012 ms 连到实际 channel
  11 并恢复真实缓存；随后一次 HTTP header 超时由状态机自动恢复，首次 WLAN-on 至最终
  LiveView 约 13.25 秒。测试后已烧回正式固件，二进制和启动日志均确认不含 `TEST ONLY`。
- 追加仅 BSSID 错和仅 channel 错的独立实机注入，两项均在一次 `NO_AP_FOUND(201)` 后于
  原预算内重扫，Wi-Fi 分别 5371/5370 ms 连接到实际 channel 11，LiveView 分别 488/
  3329 ms；三种错误缓存组合全部通过。首版夹具可能因后续整 Profile 保存而间接持久化
  注入值，现已改为连接调用内的一次性局部副本，确保测试 Profile/NVS 不受污染。
- 单变量测试后两次烧回正式固件：产物及启动日志无 `TEST ONLY`，真实 BSSID/channel 11
  保持正确，均跳过无变化的 NVS 写入。夹具修正后的最新正式产物实机 Wi-Fi 4900 ms、
  LiveView 3193 ms，随后持续出帧。

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
