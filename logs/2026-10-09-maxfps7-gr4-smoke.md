# 全机型 maxfps=7：GR IV 烧录与冒烟验证

日期：2026-10-09（Asia/Shanghai）。实现基于 `d702b79` 工作区；本次将共用 LiveView 请求改为 `/v1/liveview?maxfps=7`。GR II、GR III/IIIx 和 GR IV 均使用同一路径。

## 构建与烧录

- StickS3 release 构建成功，Flash 1,467,761 B，静态 RAM 66,492 B。
- USB 烧录完成，镜像哈希校验通过，设备自动复位。
- 相机由用户确认为 GR IV，串口协议识别为 GR4_FAMILY。
- 请求日志确认 `HTTP: opening LiveView path=/v1/liveview?maxfps=7`，相机成功打开 HTTP 流。

## 首次黑屏与恢复

- 首次连接出现一次 HTTP headers 超时，应用重连后打开 LiveView。
- 用户报告屏幕黑屏；串口仍收到约229–237 KiB/s，但 PREVIEW 的解码/渲染统计为0，JPEG尺寸为0×0。
- 补充日志反复出现 `UI Canvas: internal allocation failed; retrying in PSRAM` 和 `UI Canvas: create failed; restoring previous orientation`。
- 当时仍有约8 MB空闲PSRAM，不能把这两条日志直接解释为物理内存耗尽。画布创建失败的底层触发原因未确定。
- 后续一次USB复位后，日志确认 `UI Canvas: ready 240x135 storage=internal`、`JPEG: viewport synced 240x135`，开始解码720×480 JPEG。用户确认屏幕已恢复画面。
- 本次未修改显示初始化或画布逻辑，因此黑屏只记为已恢复的异常，不能记为根因修复。也没有证据认定限帧参数直接导致该异常。

## 恢复后的120秒观察

监控使用原始串口读写，未切换DTR/RTS。清理标识/凭据后的记录为 `logs/2026-10-09-maxfps7-gr4-stability.log`。

- 前约83秒持续预览：17个统计窗口FPS为6.6–7.2，均值约6.98；数据率199.8–216.9 KiB/s，均值约212.7。短窗口出现7.2不表示长期超过请求上限。
- stream/rendered从1858同步增长到约2478，dropped保持0。JPEG为720×480，通常约30–32 KiB/帧；解码约37–39ms，整帧渲染约50–52ms。
- 周期Props读取附近短暂降至6.6 FPS，随后恢复。
- 约83秒发生横屏→竖屏，正常发送WLAN finish；之后横屏恢复，遇到一次NO_AP_FOUND重扫、一次INVALID_PMKID上下文清理后连接成功。Wi-Fi连接耗时5506ms，RSSI -53 dBm；LiveView打开3093ms。
- 此次重连仍发 `/v1/liveview?maxfps=7`；横屏画布创建成功。恢复后的窗口FPS为3.9–5.5，持续有完整帧且dropped=0。7为最大请求帧率，不保证恢复窗口达到7 FPS。
- 120秒内未观察到画布创建失败、JPEG解码错误、stall/watchdog、意外流关闭或设备复位。姿态造成的正常WLAN重连与“全程不重连”不是同一结论。
- 用户确认GR IV约5米时实时预览和快门均正常；距离和快门结果来自用户观察，本次日志过滤器未专门保留快门结果，不能冒充自动测距或独立拍摄验证。

## 验证边界

已确认GR IV能够接受参数并稳定显示接近7 FPS，重连后参数保持，约5米功能由用户验证。尚未对GR II/GR III做本版实测，不能宣称原GR III约1米的问题已经解决。首次黑屏底层原因及长期稳定性仍需后续复现与验证。
