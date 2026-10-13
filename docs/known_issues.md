# Known Issues and Open Questions

## 支持与证据限制

1. **GR IIIx 已在本分支固件上实机验证，GR III 与 HDF 版本仍需独立验证**：设备端 Passkey、UUID WLAN、凭据、Capture 门控、Bond 自愈已经实现并通过 Native/编译验证；GR IIIx 详细矩阵与去密日志待补充，GR III/GR III HDF/GR IIIx HDF 不应仅凭 GR IIIx 结果升级为已验证。
2. **GR IV 已在本分支固件上实机回归（2026-10-14）**：首次配对、竖屏 BLE 快门、横屏 LiveView + BLE 快门、横竖反复切换、关机保护与手动唤醒、重启后快速重连、长按 B 重置配对均通过。GR IV HDF 仍未在本分支验证。
3. **GR III HDF / GR IIIx HDF 只有实验性支持**：没有独立实机 GATT 证据，不能仅按产品名称假设与非 HDF 版本相同。
4. **GR II 已完成实机验证**：Wi-Fi-only 配网、相机 AP 连接、LiveView 与 HTTP 快门链路已在真实相机上验证通过；实现不依赖 BLE，也不包含虚构的 BLE UUID/Handle。**`RicohDriver` 迁移（2026-10-13）后尚未回归：目前没有 GR II 实机可测（2026-10-14 记录）。** 有机器后按顺序回归：长按 B 重置后选 GR II → 网页配网保存 → 加入相机 AP 并出 LiveView → 竖屏仍保持预览、Button A HTTP 快门（`shootGr2`）→ 重启后用 NVS 中的手动凭据直接连接 → 长按 B 取消配网 / 清除 WLAN 配置。重点核对 `handleGr2ProvisioningInput()` 与宿主 `serviceProvisioning()` 的拆分，以及 `setUiPreviewIntent()` 中 GR II 竖屏仍请求预览的规则。

## 运行行为与待调优点

1. GR IIIx 外部实机记录显示，休眠时建立的连接可能一直返回 `BLE_STARTUP`。当前策略断开并以不短于 8 秒的间隔建立只读探测连接；安全但可能带来约 9 秒自动发现延迟。
2. 自动删除旧 Bond 只在连续 3 次明确安全恢复失败或 2 次明确 ATT 认证不足后发生。不同相机固件的错误码需实机确认，普通超时/断连不会触发删除。
3. Passkey 输入只有 Button A：短按循环数字、长按确认。功能已做纯逻辑测试，但实际按键手感和用户误操作率需要真机/设备测试。输入窗口已按相机侧约 30 秒的放弃时间缩到 28 秒并显示倒计时；按键输入本身偏慢的问题仍在（见 `gr3_iiix_hardware_verification_en.md`）。
4. GR III Power Notify 通过 UUID subscribe 尝试，失败不会绕过 Power/Operation Mode 读取门控；不同固件是否提供 notify 尚不确定。
5. GR III 首次参数没有已验证的 Security/Frequency/BSSID 特征。当前接受 SSID+Passphrase、Channel 可选，连接成功后回采实际 BSSID/Channel；需实机验证相机在自动 Channel 下的 AP 启动延迟。
6. Wi-Fi 缓存快速连接首次失败可能是相机 AP 尚未准备好；流程会回退到 BLE 重新读取参数。
7. Windows PlatformIO monitor 的 `ClearCommError failed` 属于历史串口监视/重连问题，不等同于固件崩溃。
8. GR II 的 SSID/密码是相机专属值，通过 StickS3 临时热点上的本地网页写入 NVS。为避免 ESP32-S3 在 AP+STA 全信道扫描时阻塞网页或切换 AP 信道，固件在启动 SoftAP 前以 STA 模式完成一次扫描并缓存结果；相机 AP 仍必须由用户在机身上手动开启。

## 已知且暂缓处理（2026-10-14 记录）

以下问题来自 `RicohDriver` 迁移后 GR III 系列 / GR IV 的实机日志，均为迁移前已存在的行为，评估后暂时接受、不修。重新处理前先按「复现方式」确认仍然存在。

| # | 问题 | 证据 / 复现方式 | 原因与处理方向 |
| --- | --- | --- | --- |
| K1 | 横屏预览中 BLE 快门写入阻塞约 12 s，期间预览冻结 | GR IV：`OperationRequest START` 后 12.4 s 才 `writeValue failed, rc: 384`，预览 fps 降到 0.2。多次横竖切换（Wi-Fi 反复 `AUTH_FAIL`）后在横屏连续按 A | BLE 写同步执行、阻塞主循环。短期给快门写加约 2 s 超时；根治为多品牌计划阶段 8（驱动非阻塞） |
| K2 | 快门被误判为已确认 | `shot confirmed by camera notify after 3ms`，或确认紧跟上一张的 `[00 00 00 00]`。0.5 s 内连按 A 两次 | 任何 CaptureStatus / ShotCount 通知都会递增 `shotEventSequence`。只把 `[01 …]` 计为确认 |
| K3 | 快门失败提示为空 | `Button A: BLE shutter failed: `（冒号后为空）。连按 A 三次 | `retryShutterAfterGr3StartupMode()` 读 Operation Mode 成功时清空了 `lastError()`。读之前保存、失败后恢复 |
| K4 | 相机仍在处理上一张时再次快门被拒 | `writeValue failed, rc: 384`（ATT `0x80`）。连按 A | 相机行为。可在 CaptureStatus 未回到 `00` 前暂缓或提示“相机忙” |
| K5 | Wi-Fi 全信道扫描首次失败后空等 | GR IV：`STA disconnected reason=49` 后等到 4 s kick，本次连接 11.9 s | `gr_wifi.cpp` 只按固定间隔 kick。改为收到断开事件立即重发 join |
| K6 | 开机 M5PM1 按键寄存器读取失败 | 每次开机 `M5PM1 button read failed err=-7`，回退到 M5Unified，长按关机仍可用 | 疑似 I2C 时序或硬件，需单独排查 |
| K7 | 内部 RAM 碎片 | `UI Canvas: internal allocation failed; retrying in PSRAM`（`largest_int` 约 63 KB < 64.8 KB），解码/渲染每帧慢 7–8 ms，FPS 基本不变 | 多品牌计划阶段 8（去 `String`） |
| K8 | GR IV 打开 LiveView 约 1.0–1.2 s | 加快 Wi-Fi join 后由约 0.4 s 变长 | 推测相机 HTTP 服务尚未就绪；持续观察 |
| K9 | GR IV 首次配对前几次连接失败 | 前 8 次 `connect failed reason=574`，广播名为空，约 14 s 后成功 | 相机侧未就绪，自动重试可覆盖 |
| K10 | 日志文案误导 | 横屏用缓存参数恢复时也打印 `WiFi cache: saved (fresh BLE connect)` | 仅文案 |

## 安全不变量

- Unknown Profile 不得执行 WLAN、Power、Shutter 写入。
- GR III 只允许通过 UUID 访问 WLAN；不得新增“先写 GR IV Handle 再回退”的代码。
- GR III 只有 authenticated encryption + fresh Operation Mode 读取成功且非 `BLE_STARTUP` / `POWER_OFF_TRANSFER` 才能写 Network Type `0x01`。
- 普通日志、错误与 GATT dump 不得包含完整 Passphrase、Passkey 或 NVS 明文凭据。
- GR II 不得调用官方 `/v1/device/finish` 作为普通 WLAN handoff，因为该接口会关闭相机。
- 修复连接速度问题不能以高频骚扰休眠相机或跳过 Operation Mode 为代价。

## TODO_UNVERIFIED

- GR IIIx 实机验证的相机固件版本、详细矩阵、稳定 FPS、长时间内存碎片和 BLE/Wi-Fi 共存稳定性。
- `0x213` / `0x215` / `0x216` 在不同代际和固件上的精确定义。
- GR III `OTHER` 已在 GR IIIx 1.60 上实机确认可开 WLAN（待拍状态即报 `OTHER`）；`PLAYBACK` 当前代码也放行，但尚无专门的实机记录。
- 相机端删除配对后，各代际是否都能按当前阈值自动清除旧 Bond 并重新配对。
- GR II 已确认支持可用；实机记录仍需补充相机固件版本、稳定 FPS、MJPEG 响应格式，以及是否实际进入过 `shoot/start` + `shoot/finish` fallback。

实机问题先按 `logs/issue_template.md` 保存机型、相机固件、StickS3 commit 和去密后的关键日志；不得把单一机型结果扩展为其他机型或 HDF 版本的“已实机验证”。
