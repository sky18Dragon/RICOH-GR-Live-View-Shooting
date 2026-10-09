# StickS3 × RICOH GR Wi-Fi 连接稳定性提升方案

更新时间：2026-10-08

实现状态：`codex/wifi-connection-stability` 已完成 P0/P1 最小修复包，并完成首轮实机
基线：姿态重连 6/6 成功，但 BSSID/channel 阶段稳定增加延迟且出现
`INVALID_PMKID(49)`；5 轮冷启动首次自动完成 4/5，BLE channel 两次与实际 AP 不符。
第二轮已改为 GR III/IV 会话直接 SSID-only、RAM-only STA 配置及 PMKID 定向恢复，等待
同机 A/B。RF、供电和相机型号兼容性仍为 `TODO_HARDWARE`。下文“当前问题”描述的是
改造前基线，用于保留根因与验收依据。

## 1. 结论摘要

当前固件的基础方向是正确的：StickS3 以 `WIFI_STA` 连接相机 2.4 GHz AP，保留
modem-sleep 以兼顾 BLE/Wi-Fi 共存，缓存 SSID、BSSID 和信道以加快重连，并在 Wi-Fi
丢失后关闭 LiveView socket、重建连接。

针对“BLE 已触发相机开启 Wi-Fi，但 StickS3 一直连接不上”的现场问题，建议按以下
优先级处理：

1. **P0：修复伪全信道回退。** 当前所谓 `fallback to full scan` 只把信道改成 `0`，
   仍把缓存 BSSID 传给 `WiFi.begin()`。只要 BSSID 缓存过期，回退仍被锁在旧 AP，
   不是完整的 SSID-only 扫描。
2. **P0：增加 Wi-Fi 事件与 reason code 取证。** 当前最终只记录
   `NO_SSID`、`CONNECT_FAILED` 或 `DISCONNECTED`，无法区分 AP 尚未启动、BSSID/信道
   错误、密码/握手失败、弱信号、DHCP 未完成。
3. **P0：连接重试只保留一个控制者。** 当前同时启用 Arduino Core
   `setAutoReconnect(true)` 和应用层三次恢复状态机。应由应用状态机统一控制重试，
   避免底层自动重连与缓存降级、BLE 重新开 AP、主动断网流程互相干扰。
4. **P1：把 1.9 秒缓存快连改为分阶段预算。** 当前 BLE WLAN-on 应答后无等待，立即
   发起 1.9 秒锁 BSSID/信道的连接；如果相机 AP 启动稍慢，就会误判缓存失效并进入
   较重的 BLE 参数重读和整链路恢复。
5. **P1：核查 GR III/IIIx 的 12/13 信道。** 当前凭据校验只接受 `channel <= 11`，
   但同一代码库的频率转换支持 1–13。若相机实际返回 12/13，会被当成无效凭据。
6. **P1：核对相机 WLAN Security 与 Arduino 最低安全等级。** GR IV 的 BLE 凭据
   已读取 `securityType`，但连接层没有记录或使用它；认证失败时无法判断是否存在安全
   模式不匹配。
7. **P1：做天线安装位置与电源对照测试。** StickS3 是内置天线的小型 2.4 GHz
   设备，安装在金属热靴和相机机身附近可能造成遮挡或失谐；电池供电时 Wi-Fi TX、
   LCD、BLE、扬声器又会叠加峰值负载。这两项都需要实机 A/B 数据确认。

预计最可能直接解决当前问题的是第 1、2、4 项；第 2 项能在一次现场复现后把问题明确
归类，不再依赖猜测。

### 1.1 首轮实机后的策略收敛

首轮数据已否定“GR III/IV 每轮优先绑定缓存 BSSID/channel 会更快”的假设：6 次姿态
重连均由 SSID-only 成功，前两阶段只增加约 6.8 秒；冷启动又两次出现 BLE channel 1、
实际 AP channel 11。当前实现因此对 BLE 管理的 GR III/IV WLAN 会话直接使用
SSID-only，把 BSSID/channel 降级为非绑定诊断信息。初版缓存预算 7000 ms 在 5 次连续
姿态循环中均早于 AP 可用时间耗尽，现按实测调整为 11000 ms，以覆盖约 8 秒 AP 启动及
同一次调用内的 PMKID 清理重试。

同一批日志还稳定出现 `INVALID_PMKID(49)`。当前实现用 `WiFi.persistent(false)` 保持
驱动 STA 配置为 RAM-only，并在 reason 49 时清理安全上下文、在原总预算内重试一次。
GR II 由用户手动开启 AP，仍保留本方案后文的分阶段提示策略。该收敛策略需要第二轮
同机 A/B 验证，尚不能外推到所有 GR 型号或固件版本。

## 2. 当前硬件与软件基线

### 2.1 硬件

- M5Stack StickS3，主控 ESP32-S3-PICO-1-N8R8，240 MHz，8 MB Flash，8 MB Octal
  PSRAM。
- 仅支持 2.4 GHz Wi-Fi；ESP32-S3 的 Wi-Fi 与 BLE 共用一套 2.4 GHz RF，通过时分
  复用共存。
- 1.14 英寸 ST7789P3 LCD，250 mAh 电池，M5PM1 电源管理。
- 官方给出的整机满载功耗点为 4.2 V / 519.02 mA；官方也提示电池供电时扬声器功耗
  过高可能导致意外重启，并提醒拆机可能损伤天线 FPC。

### 2.2 固件

- PlatformIO `espressif32@6.12.0`，Arduino 框架；本地实际解析到
  `framework-arduinoespressif32 3.20017`（Arduino-ESP32 2.0.17）。
- 板级配置为 `esp32-s3-devkitc-1`、QIO/OPI、8 MB Flash、8 MB PSRAM，与 M5Stack
  官方 StickS3 PlatformIO 示例一致。
- M5Unified 与 M5PM1 当前用 Git URL、未锁 release/tag/SHA；本地解析版本分别为
  M5Unified 0.2.18、M5PM1 1.0.7。建议发布构建锁定已验证版本，避免未来依赖头部变化
  引入无线或板级初始化回归。
- BLE 使用 NimBLE-Arduino 2.5.0；Wi-Fi 使用 Arduino `WiFi.h`；LiveView 使用裸
  `WiFiClient`。

## 3. 改造前连接链路

```text
BLE 已连接
  -> 通过 BLE 写 WLAN ON
  -> 若有缓存：SSID + BSSID + channel，1.9 s 快连
  -> 快连失败：重新读取 BLE Wi-Fi 参数
  -> channel hint 连接，最多 6 s
  -> 失败后 channel=0 回退，使用剩余总预算
  -> 获得 IP
  -> TCP 连接 192.168.0.1:80
  -> GET /v1/liveview
```

当前成功样本说明正常路径本身可行：项目硬件记录中，GR IIIx HDF 在信道 6、RSSI
-28 dBm 时，Wi-Fi 约 655 ms 完成连接；断线恢复曾在 238–630 ms 完成。因此“始终
连接不上”更像异常路径处理不足，而不是 ESP32-S3 或相机协议天然不兼容。

## 4. 改造前已确认的问题点

### 4.1 P0：`full scan` 回退仍固定 BSSID

`connectWifiFromProfile()` 的首次尝试传入缓存的 BSSID 和信道；失败后的回退虽然把
信道设为 `0`，但仍传入同一个 BSSID。Arduino Wi-Fi Core 会设置
`wifi_config_t.sta.bssid_set = 1`，因此扫描仍只接受该 BSSID。

风险场景：

- NVS 中是旧相机、换机或清配对不彻底；
- 相机固件升级或 WLAN 配置变化导致 BSSID 改变；
- 读取新凭据前先使用旧缓存，而 AP 刚启动、首次连接超时；
- 相同 SSID 下实际应选择另一个 BSSID。

修复原则：真正的最终回退必须同时使用 `channel=0`、`bssid=nullptr`，只保留 SSID
和密码。

### 4.2 P0：没有断线原因，现场不可诊断

ESP-IDF 在 `WIFI_EVENT_STA_DISCONNECTED` 中提供 reason code，并提供断线时 RSSI。
官方明确区分 `NO_AP_FOUND`、`AUTH_EXPIRE`、`AUTH_FAIL`、`ASSOC_EXPIRE`、
`HANDSHAKE_TIMEOUT`、`BEACON_TIMEOUT` 等阶段。当前固件没有注册 Wi-Fi 事件，只轮询
`WiFi.status()`；不同根因最后会被折叠成相同状态。

必须记录：

- 单调递增的 `attempt_id`；
- 目标模式：`BSSID+channel`、`channel-only`、`SSID-only`；
- WLAN-on 应答到开始连接、STA_CONNECTED、GOT_IP 的耗时；
- disconnect reason 数值和名称、断线 RSSI；
- 目标 channel、是否设置 BSSID（日志中不必完整输出用户 MAC）；
- 获得的 IP、网关和实际 channel/BSSID；
- BLE 在整个连接期间是否仍在线；
- 自由 internal heap、最大连续块、复位原因和供电状态。

事件回调只写入轻量状态/队列，不在 Wi-Fi event task 中操作 UI、BLE、NVS 或执行阻塞
重连。

### 4.3 P0：自动重连和应用重试重叠

`GrWifi::begin()` 打开 `WiFi.setAutoReconnect(true)`，应用层 `AppController` 同时有三次
WLAN/Wi-Fi 恢复流程。当前 Arduino Core 在部分非主动断线原因上还会自行
`WiFi.disconnect(); WiFi.begin();`。这会让一次应用尝试中出现不可见的额外连接，且可能
在应用准备放松 BSSID/信道限制时仍使用旧配置。

建议连接与预览全程设置 `setAutoReconnect(false)`，由应用状态机根据 reason code
重试。主动离开预览、相机 AP 关闭和用户转竖屏时也更容易保证不会被底层重新拉起。

### 4.4 P1：AP 启动窗口与 1.9 秒硬超时

当前 `WIFI_CACHED_CONNECT_GRACE_MS=0`、缓存预算 1900 ms。BLE 写 WLAN ON 成功只表示
相机接收了命令，不保证 AP 已开始发 beacon、完成认证服务并启动 DHCP。

建议不要恢复固定 700 ms 的盲等，而是采用事件驱动的分阶段预算：快速路径仍可立即
开始，但 `NO_AP_FOUND` 时短暂退避后放松约束；已关联但未 GOT_IP 时继续等待 DHCP，
不要重复 BLE 开关 WLAN。

### 4.5 P1：信道 12/13 的策略不一致

`validGr3WifiChannel()` 当前只允许 0–11，而 `frequencyMhzForChannel()` 支持 1–13，
连接后的缓存逻辑还接受到 14。应先在 GR III/IIIx 实机日志确认相机允许的信道集合，
再统一校验；如果相机可能返回 12/13，当前逻辑会让凭据轮询直到超时。

### 4.6 P1：WLAN Security 已读取但没有进入 Wi-Fi 连接决策

GR IV 凭据读取会保存 `securityType`，但 `applyBleWifiCredentials()` 没有把它带入运行时
profile，Wi-Fi 层也没有根据它设置或至少记录 auth mode。当前 Arduino Core 在有密码时
默认使用 `WIFI_AUTH_WPA2_PSK` 作为最低安全等级。如果实机相机都是 WPA2，此项不会
造成问题；但现场若出现 `AUTH_FAIL`，必须同时记录 BLE `securityType`、扫描到的 AP
auth mode 和 Core 最低安全等级，再根据 RICOH 实测映射决定是否调整。不要在没有映射
依据时直接放宽到任意开放网络。

### 4.7 P1：LiveView socket 恢复方向正确，但应由事件触发得更早

乐鑫明确说明 STA 断开时 LwIP 会清理 TCP/UDP 连接，旧 socket 不能因为稍后重新取得
同一 IP 就继续使用。当前 `WifiPreviewService::disconnectWifi()` 会关闭 LiveView 并
reset MJPEG，恢复时重建 socket，这一点正确。应在收到 disconnect/GOT_IP changed
事件后尽早标记 socket 失效，不只等主循环中的 `WiFi.status()` 发现。

### 4.8 P2：modem-sleep 不应作为首轮修改项

ESP32-S3 只有一套 2.4 GHz RF，BLE 和 Wi-Fi 由共存模块分时使用。乐鑫说明默认
`WIFI_PS_MIN_MODEM`，关闭 Wi-Fi 省电可降低实时收包延迟，但共存时即使
`WIFI_PS_NONE` 也仍受 RF 时间片约束。项目记录还显示在 BLE 控制器已启用后关闭
modem-sleep 曾触发 Wi-Fi 栈 abort。

因此生产方案应保留当前 `WiFi.setSleep(true)`。只有在 P0/P1 完成且具备自动化压力
测试后，才可建立实验分支，在无线栈初始化顺序可控的前提下 A/B 测试；不要现场直接
关闭。

### 4.9 P2：热靴安装与供电需要排除

这是基于设备结构和安装方式的工程推断，需要实测确认：StickS3 紧靠相机金属热靴、
机身或手掌时，内置天线可能被遮挡或失谐。M5Stack 官方明确提到天线 FPC，且设备只
支持 2.4 GHz。

电源方面，当前声音默认启用、音量 40%，背光为 180，同时运行 BLE、Wi-Fi 和 LCD。
虽然低于官方的扬声器 75%提示阈值，但低电量、老化电池或连接瞬间 TX 峰值仍应通过
USB 供电对照和 reset reason 排除。

## 5. 推荐连接状态机

总连接预算保持在 15 秒左右，但把它分成有意义的阶段：

| 阶段 | 目标参数 | 建议预算 | 进入条件 | 失败动作 |
| --- | --- | ---: | --- | --- |
| A 快连 | SSID + 缓存 BSSID + 缓存 channel | 2.5–3.0 s | BSSID、channel 都有效 | `NO_AP_FOUND` 后短退避并进入 B |
| B 放松 BSSID | SSID + channel，BSSID=null | 2.0–3.0 s | channel 有效 | 仍找不到 AP 则进入 C |
| C 完全扫描 | SSID，channel=0，BSSID=null | 7–9 s | A/B 失败或缓存不完整 | 按 reason 分类处理 |

补充规则：

- WLAN-on 后第一次 `NO_AP_FOUND`：退避 300–500 ms；第二次 800–1200 ms。不要每秒
  重复 BLE WLAN OFF/ON。
- `AUTH_FAIL` / `HANDSHAKE_TIMEOUT`：只重新读取一次 BLE 密码，然后重试；仍失败则
  明确提示凭据问题，不进行无限扫描。
- `BEACON_TIMEOUT` / RSSI 较低：关闭旧 socket，退避后 SSID-only 重连；同时提示
  用户调整距离/安装方向。
- 已 `STA_CONNECTED` 但未 `GOT_IP`：归类为 DHCP 阶段，保留关联并等待剩余预算；
  不要立即重新认证。
- 主动 `disconnect()` 必须设置 `intentional_disconnect`，忽略相应的 `ASSOC_LEAVE`
  自动恢复。
- 达到本轮预算后指数退避，例如 1 s、2 s、4 s，上限 8 s；用户主动操作可立即开启
  新一轮。
- 只有 `GOT_IP` 后才允许创建 `WiFiClient`。当前 Arduino Core 的 `WL_CONNECTED`
  实际在 GOT_IP 时设置，但状态机仍应直接使用事件，避免以后升级 Core 后语义变化。

## 6. 针对当前现场问题的最小修复包

建议第一个固件版本只做以下改动，便于隔离效果：

1. 注册 `ARDUINO_EVENT_WIFI_STA_CONNECTED`、`ARDUINO_EVENT_WIFI_STA_GOT_IP`、
   `ARDUINO_EVENT_WIFI_STA_DISCONNECTED`、`ARDUINO_EVENT_WIFI_STA_LOST_IP`。
2. 保存并输出 disconnect reason/name/RSSI 和各阶段耗时。
3. 将应用控制期间的 `setAutoReconnect(true)` 改为 `false`。
4. 把连接 API 拆成明确的 `bssidPtr`/`channel` 参数，确保最终 SSID-only 回退传
   `nullptr, 0`。
5. 缓存快连预算由 1900 ms 提升到约 3000 ms；不增加固定 grace，按
   `NO_AP_FOUND` 做短退避。
6. 每次成功后保存实际 BSSID/channel；连续两次 `NO_AP_FOUND` 时将缓存 BSSID 标记
   为可疑，本轮禁用，成功后再更新。
7. 启动日志增加 `esp_reset_reason()`、电池百分比/充电状态、internal heap 最大块。

该版本不要同时修改 modem-sleep、发射功率、静态 IP、TCP buffer 和 JPEG 流参数，
否则现场结果难以归因。

## 7. 现场验证矩阵

### 7.1 软件与协议

- 冷启动：相机先开、StickS3 后开；反向顺序；同时开机。
- 相机 AP 慢启动：连续做 100 次 BLE WLAN-on → Wi-Fi join → GOT_IP → HTTP probe。
- 缓存异常：手工写入错误 channel、错误 BSSID、两者都错，确认最终 SSID-only 能恢复。
- 密码异常：确认日志进入 AUTH/HANDSHAKE 分类，不误报为 AP 不存在。
- AP 关闭/相机睡眠/相机重启：确认旧 socket 关闭，重连无风暴。
- GR III、GR IIIx、GR IV 分别验证；记录各型号实际 channel 范围。

### 7.2 RF 与安装

在同一固件、同一相机下比较：

- StickS3 离开热靴 10–20 cm；
- 装到当前热靴支架；
- 旋转 90°/180°；
- 用户手握住设备与不遮挡；
- 距离 0.3 m、1 m、3 m。

记录 scan RSSI、关联时 RSSI、断线时 RSSI、连接耗时和成功率。如果离开热靴后显著
改善，应优先调整支架，使天线端远离金属面并保留空气间隙，而不是盲目提高发射功率。

### 7.3 电源

- 满电池；低电量电池；USB 5 V 供电。
- 声音开启/关闭；背光 180/较低亮度。
- 记录 brownout/reset reason、最低电池电压（若 M5PM1 API 可可靠读取）和连接失败率。

如果 USB 供电稳定、低电量电池失败，应先处理供电峰值：连接阶段暂停音效、降低背光，
并检查电池状态。不要仅通过延长网络超时掩盖电源问题。

## 8. 验收标准

- 100 次冷启动连接成功率不低于 99%。
- 200 次 WLAN OFF/ON 重连成功率不低于 99.5%。
- 正常近距离条件下 GOT_IP：P50 < 1.5 s，P95 < 4 s，最慢不超过 10 s。
- 错误 BSSID/channel 缓存下，15 s 内能够通过 SSID-only 回退连接。
- 每次失败都能被归入 AP 未发现、认证/握手、关联、DHCP、弱信号/掉 beacon、主动
  断开或资源/复位之一，不再只出现无上下文的 `DISCONNECTED`。
- 连续 60 分钟 LiveView 无重启、无内存持续下降、无重连风暴；Wi-Fi 临时断开后旧
  socket 必须关闭并重建。
- 电池与 USB 两种供电、热靴安装与离架两种形态均留有测试记录。

## 9. 不建议立即采用的措施

- **不建议先关闭 modem-sleep。** 会改变 BLE/Wi-Fi 共存和功耗特性，且项目已有 abort
  记录。
- **不建议先写死静态 IP。** 虽然相机常用 `192.168.0.1`，但客户端地址分配策略应按
  不同 GR 型号实测；静态地址可能造成冲突。
- **不建议无限自动重连。** 乐鑫明确提示持续重连会让扫描一直失败，应限制连续次数并
  退避。
- **不建议先增大发射功率。** 应先用 RSSI、安装位置和 USB/电池 A/B 确认 RF 或供电
  根因；功率上调会增加峰值电流和共存压力。
- **不建议同时升级 Arduino Core/M5Unified 并修改状态机。** 先锁定当前依赖完成 P0
  修复和基线测试，再单独评估升级。

## 10. 官方参考资料

- [M5Stack StickS3 官方产品与硬件资料](https://docs.m5stack.com/en/core/StickS3)
- [Espressif ESP32-S3 RF Coexistence](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/api-guides/coexist.html)
- [Espressif ESP32-S3 Wi-Fi Driver 指南](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/api-guides/wifi.html)
- [Espressif ESP32-S3 Wi-Fi Performance and Power Save](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/wifi-driver/wifi-performance-and-power-save.html)
- [Arduino-ESP32 Wi-Fi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html)
