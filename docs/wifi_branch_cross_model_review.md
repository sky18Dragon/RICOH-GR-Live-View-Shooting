# codex/wifi-connection-stability 对 main 的跨机型影响分析

> 后续状态（2026-10-09）：已按用户要求移除检测功能及其文档/测试接线，恢复原镜像功能和 Button B 单击行为，保留 Wi-Fi 稳定性改动。下文为清理前的分析记录；文件统计、行号及检测源码链接描述的是清理前快照。

日期：2026-10-09。结论：没有发现移除 GR II/GR III 支持或将它们改走 GR IV 协议的改动；但连接策略、故障恢复和 UI 都有跨机型变化，不能将本工作区当作仅针对 GR IV 的修复。

## 比较基线

- 本地 `main` 与当前分支 HEAD 均为 `4d1aa81b92409d91399331d2b4a776c009ab7a8b`，已提交分支差异为零；未 fetch 远端。
- 本报告分析 `git diff main` 以及未跟踪源码。已有跟踪文件改动 28 个，887 行新增、207 行删除；另有 7 个新增源码/头文件：`wifi_connection_policy.*` 和 `detection/` 下 5 个文件。新增文档另计。
- 比较对象是当前未提交工作区；单独 checkout 已提交分支不会得到这些优化。
- 未修改实现或测试。本报告新增前的同份源码已在本次会话验证：Native 104/104 通过、StickS3 release 增量构建成功。未实测 GR II/GR III。

## 主要改动与作用范围

| 模块 | main | 当前工作区 | 影响机型 |
| --- | --- | --- | --- |
| `gr_wifi.*` | 默认驱动持久化，自动重连开启，连接期间轮询状态 | RAM-only、关闭常规自动重连、记录事件原因、201 限次重扫、49 限次清理/重启 STA | 全部 |
| `wifi_connection_policy.*`、`main.cpp` | 有信道时先提示连接；回退仍可能绑定旧 BSSID | BSSID+channel → channel-only → SSID-only，受总预算限制 | GR II 实际使用阶段策略；GR III/IV 直接走 SSID-only |
| 缓存连接 | GR III/IV 启动等缓存路径预算 1.9 秒 | GR III/IV 11 秒，GR II 保持 15 秒 | GR III/IIIx、GR IV 时序变化最大 |
| 新鲜凭据连接 | 可带 BSSID/channel | GR III/IV SSID-only、15 秒；GR II 保留提示策略 | 全部连接共用层 |
| `AppController` 姿态恢复 | 使用已有参数调用 fresh-connect 回调；失败返回后重试 | 先 cached-connect，失败后同会话读取 fresh 凭据再连接 | 正常横竖切换主要影响 GR III/IV |
| Wi-Fi 缓存 | BLE BSSID/channel 差异也触发覆盖 | 延迟刷新仅比较 SSID/密码；连接提示未变化则跳过 NVS 写入 | BLE 刷新影响 GR III/IV；连接后跳写影响全部 |
| `camera_protocol_profile.cpp` | GR III channel 接受 0–11 | 接受 0–13 | GR III/IIIx |
| `gr2_provisioning_portal.cpp` | 结束配网时开启自动重连 | 结束配网时维持关闭 | GR II |
| `ricoh_ble_client.cpp` | 不输出 security_type | 增加该日志字段 | GR III/IV；本次未改变 BLE 读写实现 |
| 显示、JPEG、Button B、NVS | 单击 B 切换镜像；保存 `disp_mirror` | 单击 B 切换 OFF/FACE/PERSON；移除镜像，保存 `detect_mode` | 全部 |
| `platformio.ini`、native 测试 | 93 个注册测试 | 104 个注册测试，加入 Wi-Fi 策略和检测几何源码 | 验证层；未变更板卡/框架/BLE 依赖配置 |

## GR III / GR IIIx：协议保留，连接策略已经改变

代码证据：[缓存/新鲜连接入口](../src/main.cpp:1907)、[姿态恢复](../src/app/AppController.cpp:231)、[信道校验](../src/camera_protocol_profile.cpp:277)。

未变化的关键能力：

- `src/ricoh/gr3/Gr3BleProtocol.*`、协议路由、配对安全配置及 GR III UUID 常量均未修改。
- Passkey 输入、已绑定设备重连、NetworkType UUID 开关 WLAN、OperationMode/电源门控仍沿用 main。
- 快门仍调用 GR III BLE 路径；没有新增 GR IV 固定句柄写入，没有把 GR III 改用 GR IV 配对方式。

实际影响与风险：

1. **GR IV 实测收敛出的 SSID-only 也直接应用到了 GR III/IIIx。** 当前判断是“非 GR II”，而不是“仅 GR IV”。原本准确的 BSSID/channel 不再用于快连，可能牺牲 GR III 的最佳连接速度；也可能提升旧提示失效时的成功率。需要同机 A/B，不能由 GR IV 日志判定净收益。
2. **旧凭据失效时等待 fresh 读取更久。** 冷启动/恢复的缓存预算从 1.9 秒变为 11 秒。若 SSID/密码已变、BLE 仍在线且未取消，缓存失败后才读取新凭据；缓存阶段预算增加 9.1 秒。11 秒是上限，不是每次成功连接都固定等待 11 秒。
3. **姿态恢复多了同会话 fresh fallback。** 以前单次直接连接预算 15 秒；现在一次恢复可经历 11 秒缓存 + 最多 10 秒 BLE 参数等待 + 15 秒 fresh 连接，另有开 AP/HTTP 等成本。成功常态可提前返回，但失败路径更长。连接 guard 仍可因姿态变化取消关联。
4. **姿态快连成功后多了延迟 BLE 凭据刷新。** 新路径调用 `onCachedWifiConnected` 并安排 5 秒后的刷新；原姿态路径调用 `onFreshWifiConnected`。应观察预览刚恢复后 BLE 读取是否引入短暂掉帧。
5. **12/13 信道仅放宽输入校验。** 不会让已有 1–11 信道失效；但未同步改变 Wi-Fi 区域配置，不能以此宣称 12/13 实机连接已验证。

兼容性判断：核心功能路径保留；GR III/IIIx 的连接速度、AP 开启时序、重连和共存性能需回归。若要求尽量保持 main 的行为，建议将 SSID-only 默认值和缓存预算配置到机型策略，先只对已有实测依据的 GR IV 启用，而不是撤销公共的错误回退修复。

## GR II：Wi-Fi-only 保护仍在，但网络底层不是原样

代码证据：[逻辑连接前置条件](../src/main.cpp:1491)、[HTTP 快门](../src/main.cpp:1749)、[GR II 缓存连接](../src/main.cpp:1907)、[竖屏保持 Wi-Fi](../src/main.cpp:2158)。

确认保留：

- 启动发现遇到 GR II 直接进入逻辑就绪，不实际扫描 BLE；`bleLinkHealthy()` 对 GR II 返回逻辑就绪。
- 手动开启相机 AP、网页录入 SSID/密码的流程与凭据存储仍存在。
- 名为 `readFreshWifiCredentials` 的回调对 GR II 仅复制手动配置，不访问 BLE。
- GR II 专属 `shootGr2`、one-shot/press-release fallback 及相关响应判断未修改。
- 竖屏时 `previewRequested` 仍保持 true，保留 Wi-Fi/HTTP 供快门使用；因此不能把 GR III/IV 的姿态 AP-off/on 变化直接套到 GR II。
- 结束会话逻辑仍避开会使 GR II 关机的接口；HTTP API 文件未改。

实际变化：

- GR II 未改为 11 秒 SSID-only。其缓存和 fresh 连接仍使用 15 秒总预算，有提示时逐步放宽限制。
- 当同时存在 BSSID/channel 时，新方案通常为最多 3 秒 BSSID+channel、3 秒 channel-only、剩余时间 SSID-only。阶段间退避也从总预算扣除，所以最后阶段实际少于名义 9 秒；main 是约 6 秒提示阶段再回退，且回退可能保留旧 BSSID。
- AP 改信道/旧 BSSID 失效时，新方案具有更完整的回退能力；弱信号或关联较慢时，3 秒阶段也可能提前打断原本可以成功的提示连接。需实机比较。
- `GrWifi::begin()` 和配网结束均关闭常规自动重连。现在更依赖应用状态机检测 Wi-Fi 丢失并重建 LiveView；代码仍有 `monitorWifi → recoverCameraConnection → runCameraFlowOnce` 恢复通路，不能说关闭驱动重连就一定失去 GR II 恢复能力。
- 201 重扫、49 安全上下文恢复均在公共底层，GR II 也会触发。安全恢复操作对象是 StickS3 STA，不会自动删除 BLE Bond 或相机 Profile。

兼容性判断：没有发现“GR II 被要求先连 BLE”或“HTTP 快门被改坏”的直接回归；需要重点验证配网交接、短暂掉线、AP 关闭后手动重开和弱信号关联。由于 GR II 快门完全依赖 Wi-Fi，这些行为变化也会影响断线后的快门可用时间。

## 跨机型的确定性功能变化与共用风险

**镜像功能已移除，这是确定影响。** Button B、`DisplayUi`、`JpegDecoder` 的镜像接口和绘制分支均被删除，原 `disp_mirror` 不再读取。GR II/III/IV 的旧用户升级后都无法继续使用镜像预览，不能将这项变更表述为纯 Wi-Fi 稳定性优化。旧配对/凭据的 `profile_v4` 格式并未因此改变，也没有自动清除它们。

**检测 OFF 也不是完全没有资源开销。** [setup](../src/main.cpp:2845) 无条件启动 [DetectionManager 任务](../src/detection/DetectionManager.cpp:38)，使用 8192 的任务栈配置和互斥量；OFF 不构造模型，但后台任务仍存在。恢复 FACE/PERSON 时在连接流程早期加载模型，预览时增加 PSRAM 图像复制及推理负载。这会影响所有机型的堆余量和调度。当前检测文档仍保留推理尺寸崩溃修复待实机验证，104 个 native 测试不执行真实 ESP-DL 推理。

**重试控制仍有框架例外。** 本机 Arduino-ESP32 2.0.17 的 [首次失败重试](/Users/zhanghuilong/.platformio/packages/framework-arduinoespressif32/libraries/WiFi/src/WiFiGeneric.cpp:1076) 不受 `setAutoReconnect(false)` 约束；应用又处理 201/49，第一次失败存在重复启动连接的可能。此问题不只影响 GR IV，尚无证据证明它在 GR II/III 必然失败。

**上一份报告中的 GR IV 固定句柄语义疑点是 main 已有问题。** 本次 `ricoh_ble_client.cpp` 只新增日志；不能把继承的协议疑点作为当前分支新引入的 GR III/GR II 回归。

## 验证覆盖与合并建议

当前 104 个 native 测试相较 main 增加 11 个注册测试，覆盖姿态 fresh fallback、连接计划、reason 分类、NVS 跳写判定、检测几何/模式。Button B 原镜像测试改为检测测试。这证明新的期望行为通过，不证明旧镜像能力仍在。

Native 构建不包含真实 `main.cpp` 回调接线、`GrWifi`、配网 HTTP 服务器及 `DetectionManager` 推理实现，因此缺少以下关键覆盖：

| 机型 | 合并前优先验证 |
| --- | --- |
| GR III/IIIx | 配对/保存 Bond 重启；缓存首连 A/B；改 SSID/密码后恢复；横竖循环；关机门控；BLE 快门；目标区域 12/13 信道（如适用） |
| GR II | 新配网和保存配置重启；STA 交接；断网/关 AP 后重开；竖屏 HTTP 快门；单拍及 fallback 不重复拍照；旧 BSSID/信道回退 |
| 全部 | 新驱动重试在首次失败下的事件序列；60 分钟预览；USB/电池；OFF/FACE/PERSON 分开记录 FPS、heap、复位与快门延迟 |

建议先把工作区整理为两个可独立验证的改动集：Wi-Fi 稳定性与观测改动，以及检测/UI/取消镜像改动。保留 GR II 的独立分支；将 GR III/GR IV 的连接策略按 Profile 配置，或者在取得 GR III/IIIx A/B 数据后再共用现策略。整包合并目前会同时改变三个系列的行为，现有 GR IV 实测不足以给其他机型作兼容保证。

## 补充：混入的检测功能及拆分边界

混入的是独立的 LiveView 人脸/半身构图功能，包含检测实现、镜像功能替换、设置持久化、UI 和测试文档，不是 Wi-Fi 修复所必需的依赖。新增 `src/detection/` 共 5 个文件、566 行。源码直接包含 SDK 的 `human_face_detect_msr01.hpp`；并未在本次 diff 中新增 ESP-DL 的 lib_deps 下载项。

运行链路为：Button B → 持久化模式 → worker 加载模型 → JPEG 成功解码 → 主任务复制并转换 RGB565 字节序 → worker 推理 → 人脸框按比例扩展 → 映射到画布 → 绿色框与状态标签。

| 独立组成 | 当前行为 | 与 Wi-Fi 优化的关系 |
| --- | --- | --- |
| 检测模型 | FACE/PERSON 都使用 HumanFaceDetectMSR01，最多输出 4 个框 | 无协议依赖 |
| 构图几何 | FACE 框延伸到胸部；PERSON 扩展到上半身 | 不是独立人体模型，也没有驱动相机自动对焦 |
| 按键替换 | B 单击检测循环；双击锁定和长按清配对保留 | 产品交互变更，不是网络修复 |
| 镜像删除 | 删除镜像 API、JPEG 水平翻转分支及 `disp_mirror` 读写 | 会让所有机型失去原功能 |
| 持久化 | 新增 `detect_mode`，启动恢复；切换时先保存再异步请求模式 | 不改变 `profile_v4` 相机凭据结构 |
| 降级与诊断 | 加载等待 5 秒、连续三次推理超 600 ms 后关闭；输入尺寸过滤；红色提示 2.5 秒 | 检测自己的容错逻辑 |

资源与可靠性分析：

- “异步推理”不等于完全不占用预览主任务：输入缓冲扩容和整帧字节转换在 `submitRgb565BeFrame()` 内同步执行，再交给 worker 推理。需要单独测量对 MJPEG 帧处理延迟的影响。
- worker 在启动时无条件创建，OFF 仍保留任务及互斥量；曾开启后切回 OFF，只删除模型，没有释放 `_input`。216×144 RGB565 输入缓冲为 62,208 字节，留在 PSRAM 供复用。这不等于持续泄漏，但说明关闭检测不等于恢复全部内存。
- FACE 与 PERSON 只改变最终矩形扩展方式，却在每次模式切换时 `delete _model` 并重新构造同一种模型，产生不必要的加载时间和分配压力。后续可考虑复用模型，仅切换几何策略。
- `infer()` 返回后才检查 600 ms 阈值，不能将它视为可中断单次推理的硬超时；若模型 panic 或不返回，现有慢推理降级无法处理该路径。
- 模式在验证首次推理成功前已写 NVS。若实机仍存在推理崩溃，重启会自动恢复该模式，存在再次触发的条件；目前没有证据说明新版一定会重复崩溃，也不能把输入尺寸防护等同于实机修复完成。
- [检测文档](detection.md:13) 明确保留崩溃修复和精度/性能待实机验证的边界。Wi-Fi 模式下的稳定记录不能代替 FACE/PERSON 的验收。

拆分时可按下列文件边界操作；本次只分析，没有执行拆分、删除或回退：

| 归属 | 文件/改动 |
| --- | --- |
| 检测独占的新文件 | `src/detection/` 全部、`docs/detection.md` |
| 当前 diff 全部属于检测/镜像替换 | `src/camera_profile_store.*`、`src/display.*`、`src/jpeg_decoder.*`、`src/ui/ButtonInput.*`、`src/ui/UserCommand.h`、两份 README、`docs/ui_interaction_design.md` |
| 必须按 hunk 拆分 | `src/main.cpp`、`platformio.ini`、`test/test_native/test_native_logic.cpp`、`docs/test_plan.md`、`docs/changelog_for_ai.md` |
| 留在 Wi-Fi 改动集 | `gr_wifi.*`、`wifi_connection_policy.*`、`AppController.cpp`、`WifiPreviewService.cpp`、`config.h`、`AppConfig.h`、GR III 信道校验、GR II 配网结束的重连开关、BLE security_type 日志、Wi-Fi 方案/流程/实机记录及 `src/CLAUDE.md` 中本轮 diff |
| 本次会话的分析产物 | `docs/gr4_v111_branch_feasibility_review.md` 与本报告，不属于混入的运行功能 |

`main.cpp` 的检测 hunk 包括头文件/全局对象、镜像设置函数替换、按键分发、JPEG 输入提交/overlay、tick 降级和 setup 初始化；Wi-Fi 缓存、连接计划及回调 hunk 应保留。不能直接将整个 `main.cpp` 恢复到 main。移出检测时也要恢复镜像接口及测试，不能只删除 `src/detection/`。

测试拆分可识别为：main 原 93 项；检测新增 4 项并改写已有按键期望；Wi-Fi 新增 7 项；当前总计 104。若保留全部 Wi-Fi 测试并完整移出检测增量，预期注册数为 100，但该拆分版本尚未构造或验证。

现有 changelog 将检测记录在 2026-09-10，Wi-Fi 记录在 2026-10-08；本地也有 `codex/person-face-detection` 分支，但其 HEAD 同样是 `4d1aa81`。这些信息与两组工作混在未提交目录中的现象一致，不足以确定是谁、何时或通过哪次 Git 合并引入，不能把来源归因为已确认的错误合并。

进一步核查 reflog：2026-09-10 16:31:16 从 main 切到 `codex/person-face-detection`，2026-10-08 22:21:19 再切到 `codex/wifi-connection-stability`，两次均处于同一提交。结合检测文件尚未提交，最符合现状的解释是检测工作区改动随分支切换保留，而不是检测提交被 merge 进 Wi-Fi 分支。reflog 不记录当时文件内容，因此携带过程仍是推断。

另一个需考虑的恢复边界：长按 B 的重配对入口调用 `clearBlePairing()`，并未请求检测模式切换 OFF 或销毁检测资源。不要把“清相机配对”当成检测异常的可靠退出手段；如果后续加入崩溃自动恢复，应提供独立的启动禁用/异常复位降级机制。
