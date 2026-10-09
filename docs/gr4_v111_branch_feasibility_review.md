# GR IV v1.11 文档与当前分支优化可行性分析

> 后续状态（2026-10-09）：已按用户要求移除混入的 FACE/PERSON 检测及配套 UI/持久化改动，恢复 main 的镜像预览，保留 Wi-Fi 优化。下文保留清理前的分析快照；其中检测文件路径和行号可能已失效，检测相关风险不再属于当前实现。

评估日期：2026-10-09。结论：Wi-Fi 稳定性优化值得保留；当前证据支持已测机身上的有效性，但不足以宣称 GR IV 全系列、全固件兼容。优先核实 BLE 字段语义，再继续优化 HTTP 流量。检测功能须独立验收。

## 评估范围与证据边界

- 实际代码仓库为工作区下的同名子目录；分支 `codex/wifi-connection-stability`，HEAD `4d1aa81`，与本地 `main` 相同。被评估的实现来自未提交工作区差异及新增文件，不是已提交的分支增量。
- 工作区还包含 FACE/PERSON 检测、按键行为和 JPEG/显示修改；它们不能因 Wi-Fi 测试通过而一并视为通过。
- 固件依据：[HTTP 分析](/Users/zhanghuilong/Documents/ricoh-gr4-firmware-analysis-and-feature-expansion/docs/gr4-v111-http-api-analysis.md:21)、[BLE 分析](/Users/zhanghuilong/Documents/ricoh-gr4-firmware-analysis-and-feature-expansion/docs/gr4-v111-bluetooth-firmware-analysis.md:3)。其对象为 GR IV 1.11.10.7；BLE 文档明确说明是静态分析，未做真机读写验证。
- 硬件结果引用本仓库 [2026-10-08/09 实机记录](../logs/2026-10-08-wifi-reconnect-stability.md:267)，不是本次重新测得。记录以 `GR4_FAMILY` 标识相机，缺少可将样本完整绑定到该固件包的 model/firmwareVersion 记录；不能自动将其等同于 v1.11 的兼容验证。
- 本次仅新增本报告，未修改实现或烧录设备。

## 当前优化的判断

| 优化 | 判断 | 依据与限制 |
| --- | --- | --- |
| GR III/IV 直接 SSID-only，BSSID/channel 不绑定 | 保留 | 避免可疑 BLE 提示及旧缓存限制关联；已有最终策略姿态重连 13/13 的记录。GR III 仍须独立回归。 |
| NO_AP_FOUND 后在总预算内立即重扫，最多两次 | 保留 | AP 尚未出现时，等超时不等于继续扫描。当前实现确实重新 begin，已有实机路径证据。 |
| reason 49 后清理 STA 配置并重启 Wi-Fi，限一次 | 有条件保留 | 已有恢复成功记录；属于客户端恢复措施，不能据此证明相机 PMKSA 缓存机制或唯一根因。 |
| RAM-only Wi-Fi 配置 | 保留 | ProfileStore 管持久凭据，驱动配置放 RAM；但 persistent(false) 本身不等于清空运行期 PMKSA。 |
| 缓存失败后同一 WLAN 会话读 fresh 参数 | 保留 | 避免反复关闭/开启相机 AP，AppController 已实现。 |
| 相同 SSID/密码不覆盖实测 BSSID/channel，未变化不写 NVS | 保留 | 消除延迟 BLE 刷新反复覆盖实际关联信息的问题；不意味着 BLE 提示解析已经正确。 |
| 缓存预算增加至 11 秒 | 可保留为当前实测参数 | 起作用的是“限时内主动重扫及安全恢复”的组合。文档未证明相机 AP 固定需要 8 秒，不宜把预算值写成协议要求。 |
| channel 校验放宽到 1–13 | 合理但有边界 | 只改变输入校验；不等于保证区域设置、ESP32 扫描或每台相机都支持 12/13 信道。 |
| 关闭 Arduino 自动重连 | 改善但未完全实现单一控制者 | 当前 Arduino 2.0.17 仍有首次失败的一次内部重试，见下文。 |
| 关闭 LiveView 后结束 WLAN 会话 | 协议支持 | 固件有 `/v1/device/wlan/finish`，成功时可能立即断链；连接中断不能简单算作失败。 |
| FACE/PERSON 检测 | 客户端方案可行，尚待独立实测 | 固件文档没有确认提供人脸框；当前代码在 StickS3 上推理，PERSON 是人脸锚定的上半身框。 |

## 最重要的协议疑点：固定句柄的字段含义

[当前常量](../src/config.h:115) 把 `0x013C / 0x013E / 0x0140` 分别命名为 Security / Frequency / BSSID；[凭据解析](../src/ricoh_ble_client.cpp:646) 将 Frequency 的整数 1–14 当作 Wi-Fi 信道。

而 [v1.11 WLAN 注册表](/Users/zhanghuilong/Documents/ricoh-gr4-firmware-analysis-and-feature-expansion/docs/gr4-v111-bluetooth-firmware-analysis.md:277) 的六个特征是：NetworkType、SSID、Passphrase、Channel、Available5GHz、WlanFrequency。该表没有 BSSID，且明确说明 SecurityType 未注册。

如果真机的句柄排列与表中注册顺序一致，则存在以下待验证映射：

| 现有句柄 | 代码解释 | v1.11 候选实际含义 |
| --- | --- | --- |
| `0x013C` | SecurityType | Channel |
| `0x013E` | 频率或信道 | Available5GHz |
| `0x0140` | BSSID | WlanFrequency |

这是基于顺序与现象的推断，不是文档已提供的 handle→UUID 结论。现有 [协议识别](../src/ricoh_ble_client.cpp:1091) 只验证六个位置存在，并校验 NetworkType、SSID、Passphrase 三个 UUID 的映射，没有验证后三项语义。

若 Available5GHz 的原始值为 `0x01`，当前 `normalizeFrequencyOrChannel()` 会将其解释为 2412 MHz / channel 1。这与“BLE channel 1、实际 AP channel 11、BSSID 为空”的既有日志高度吻合。因此应先排查客户端读错字段，不能继续认定是相机返回过期信道。

影响有两层：

1. SSID-only 已绕开错误提示对关联的影响，解释了当前优化为何有效；但新增的 security_type 日志仍可能误导诊断。
2. 固件出现句柄布局变化时，现有固定句柄识别可能拒绝连接或误分类。该问题原已存在，不是本次 Wi-Fi 重试改动引入。

建议先使用已有 `RICOH_BLE_GATT_DIAGNOSTICS` 只读输出 UUID、handle、读写属性，记录 ModelName/FirmwareRevision，并在受控诊断中只读取非敏感的 Channel、Available5GHz、WlanFrequency 值；SSID/密码不写入诊断日志。确认后为 GR IV 增加独立的 UUID/字段解析，保留已验证旧版本路径。

**不能直接复用 GR III 的 Channel 常量：**当前 GR III 使用 `51DE6EBC-0F22-4357-87E4-B1FA1D385AB8`，文档给出的 GR IV v1.11 为 `63BC8463-228F-4698-B30D-FAF8E3D7BD88`。共享服务 UUID 不代表共享所有字段编码。

SecurityType 不是当前凭据成功的强制条件：代码只要有 SSID、可用密码即可判有效，因此不能把“未注册 SecurityType”直接说成必然无法连接。应先修复字段认知，避免将错误值用于未来安全模式决策。

## 另一个实现限制：首次内部重试仍存在

[GrWifi::begin](../src/gr_wifi.cpp:80) 设置 `setAutoReconnect(false)`，但本机 Arduino-ESP32 2.0.17 的 [WiFiGeneric.cpp](/Users/zhanghuilong/.platformio/packages/framework-arduinoespressif32/libraries/WiFi/src/WiFiGeneric.cpp:1076) 在首次非主动断线时仍通过 `first_connect` 执行一次 `WiFi.disconnect(); WiFi.begin();`，该分支不受 AutoReconnect 开关约束。

因此，第一次 reason 201/49 发生时，底层可能已发起连接，应用又触发重扫/重启。现有硬件结果未证明这必然失败，但“重试只有应用一个控制者”目前不成立。建议优先固定依赖版本、补首次失败的事件序列验证；若需要严格单一控制者，再用版本受控的框架修补或底层事件方案处理，不要只增加延时掩盖竞争。

本机 SDK 为 ESP-IDF 4.4.7，头文件确认 reason 49 为 INVALID_PMKID、201 为 NO_AP_FOUND。官方 [Wi-Fi 驱动说明](https://docs.espressif.com/projects/esp-idf/en/v5.0.4/esp32s3/api-guides/wifi.html) 也支持这两个 reason 的含义；其含义不证明具体相机的 PMK 缓存根因。

## 固件文档支持的下一步优化

1. **先完善能力与版本采样。** `/v1/props` 提供 model、firmwareVersion、channelList、resoList、liveState。当前 [parsePropsJson](../src/gr_api.cpp:481) 只提取型号和电池。首帧之后再读取、缓存能力，避免重新把 props 请求放回启动关键路径。
2. **按实际能力请求 LiveView 分辨率和帧率。** [固件支持 reso/maxfps](/Users/zhanghuilong/Documents/ricoh-gr4-firmware-analysis-and-feature-expansion/docs/gr4-v111-http-api-analysis.md:235)，当前 [openLiveView](../src/gr_api.cpp:341) 未传参数。可从 resoList 选最低可用且满足显示/检测的分辨率，再 A/B 比较字节率、FPS 和推理耗时。216×144 是当前解码结果，不代表相机接受该请求分辨率；降低分辨率也可能使当前检测输入低于安全尺寸。
3. **评估 `/v1/changes` 替代部分 props 轮询。** 文档确认状态推送，现有 soak 记录在 props 请求附近有短暂掉帧。需先验证事件覆盖、重连补同步及额外 socket/heap 成本，再决定是否迁移；不能保证换 WebSocket 就更快。
4. **若增加对焦/参数控制，使用已确认写入格式。** `/v1/lens/focus` 支持 `pos=x,y`，写接口通常是键值串而非 JSON。坐标范围未确定，须结合 focusEffectiveArea 验证；当前本地检测框不等于相机 AF 框。
5. **检查快门前的额外写入。** 当前 [shoot](../src/ricoh_ble_client.cpp:2228) 将 `B29E...` 当作 GR IV 的 LegacyShootingFlavor，并在快门前写入。v1.11 文档把同一 UUID 标为 DriveMode。值的枚举尚未解码，不能凭文档立即删除已有实测路径；应验证该写入是否改变用户驱动模式、是否必要。它是继承的兼容性疑点，不属于本次 Wi-Fi 改动的回归。

文档只确认 WlanFrequency 为一字节选择/状态，未提供枚举编码；不能直接把 `1` 等值解释成 2.4 GHz 或 5 GHz。SSID-only 也不能解决相机 AP 运行在 StickS3 不支持频段的情况，应单独验证频段兼容。

## 检测功能与发布验证

[DetectionManager](../src/detection/DetectionManager.cpp:174) 的 FACE/PERSON 都使用 HumanFaceDetectMSR01；PERSON 只是基于可见人脸扩大矩形。文档没有证明 HTTP/WebSocket 会输出相机内部人脸/人体框，也没有证明本地推理崩溃已修复。

[检测说明](detection.md:13) 明确保留了旧缩放尺寸导致崩溃的背景，以及新尺寸方案待实机验证的要求。因此建议 Wi-Fi 稳定性与检测功能分开形成提交和验收结果；尤其要分别测试 OFF/FACE/PERSON 下的内存、帧率、快门和断线恢复。

建议的验证顺序：

1. 固定机型、相机固件版本及 StickS3 构建标识，补只读 GATT 映射记录，确认上述三个候选字段。
2. 将现有 13 次姿态样本扩充到计划的 20/100 次，并覆盖 USB/电池供电、冷启动、关机竞态、姿态取消、错误缓存及弱信号；分别统计 Wi-Fi 完成、LiveView 打开和有效首帧时间。
3. 补 60 分钟持续预览；当前约 16 分钟不能替代它。单次 LiveView 3–4 秒长尾仍需统计。
4. 对 GR IV 各目标变体及固件分别验证；GR III/IIIx、GR II 使用独立回归结果，不能由 GR IV 固件文档推导兼容。
5. 保留既有关机电源门控及显式配对流程；静态文档不证明产品采用固定 PIN，也不支持主动改变现有配对安全配置。

## 本次自动验证

- `pio test -e native`：104/104 通过。
- `pio run -e m5stack-sticks3`：release 构建成功；静态 RAM 74,896 B / 22.9%，Flash 1,908,561 B / 57.1%。这是当前已有构建产物的增量构建与尺寸检查，未执行 clean 重编译。
- Native 测试覆盖策略、状态机及检测几何，未在主机上执行真实 GrWifi 驱动、BLE 设备交互或 ESP-DL 推理。上述构建尺寸不代表运行时检测内存峰值。

建议决策：接受 Wi-Fi 稳定性方案的技术方向；把 GR IV BLE 字段映射作为下一项优先核实工作。当前可宣称“已测 GR4_FAMILY 机身获得改善”，暂不宣称“GR IV 系列全面兼容”或“整个工作区已完成发布验收”。
