# Wi-Fi 重连稳定性测试记录（2026-10-08）

## 测试环境

- 设备：M5Stack StickS3
- 相机协议：GR4_FAMILY / GR4_LEGACY
- 相机网络：2.4 GHz，实际信道 11
- 测试动作：横屏 LiveView → 竖屏关闭 WLAN → 横屏 BLE WLAN-on → Wi-Fi/LiveView 恢复
- 固件分支：`codex/wifi-connection-stability`
- 说明：原始“BLE 已开启相机 Wi-Fi 但一直未连接”问题无法稳定复现，本记录用于稳定性与恢复路径验证。

## 六轮结果

| 轮次 | Wi-Fi 结果 | 成功阶段 | Wi-Fi 总耗时 | LiveView 恢复 | RSSI |
| --- | --- | --- | ---: | ---: | ---: |
| 1 | 成功 | SSID_ONLY | 8389 ms | 481 ms | -41 dBm |
| 2 | 成功 | SSID_ONLY | 8320 ms | 466 ms | -37 dBm |
| 3 | 成功 | SSID_ONLY | 8390 ms | 443 ms | -41 dBm |
| 4 | 成功 | SSID_ONLY | 8351 ms | 453 ms | -38 dBm |
| 5 | 成功 | SSID_ONLY | 8390 ms | 457 ms | -39 dBm |
| 6 | 成功 | SSID_ONLY | 8388 ms | 449 ms | -37 dBm |

汇总：

- 重连成功率：6/6（100%）。
- Wi-Fi 平均耗时：8371 ms；中位数约 8389 ms；范围 8320–8390 ms。
- LiveView 平均恢复耗时：458 ms；范围 443–481 ms。
- RSSI：-37 至 -41 dBm，信号强度良好。
- LiveView 持续运行期间未观察到丢帧、重启、重连风暴或持续内存下降。

## 一致失败模式

每轮均出现相同序列：

1. `BSSID_CHANNEL` 尝试约 3 秒超时，状态为 `DISCONNECTED`，没有 disconnect reason。
2. `CHANNEL_ONLY` 能看到相机 AP，但被断开，reason 固定为 `49 (INVALID_PMKID)`；当前实现继续等待到本阶段 3 秒预算结束。
3. `SSID_ONLY` 在约 0.95–0.96 秒完成关联和 GOT_IP，整次调用约 1.07–1.10 秒。

Espressif 将 reason 49 定义为无效的 Pairwise Master Key Identifier（PMKID）。本次数据说明
最终 SSID-only 回退能够避免“始终连不上”，但前两个阶段每轮都会消耗约 6.8 秒，导致
当前 P95 约 8.39 秒，未达到测试计划中的 P95 小于 4 秒目标。

## 后续优化候选

1. 收到 `INVALID_PMKID` 后立即结束当前阶段并跳到 SSID-only，不再等待剩余超时。
2. 核查 Arduino-ESP32 2.0.17 的 PMK/PMKSA 缓存和 Wi-Fi 持久化设置；应用已经自行保存
   SSID/密码/BSSID/信道时，评估将驱动配置改为 RAM-only，避免相机 AP 每次重启后复用
   不再有效的安全上下文。
3. 不将 reason 49 当作密码错误；SSID-only 能以同一密码成功，说明凭据本身有效。
4. 修改后使用同一相机和动作再执行至少 20 轮 A/B，比较成功率及 P50/P95。

## 独立硬件观察项

- USB 接入时出现持续轻微电流声/高频啸叫，拔掉 USB 后消失，再插入后复现。
- 日志中充电状态存在 `charging=0/1` 频繁跳变，但未见 brownout 或复位。
- 该现象暂按 USB/充电噪声经常开扬声器功放耦合处理，与本次 Wi-Fi 成功率分开记录。

## 冷启动测试

### 第 1 轮：首次失败，人工重启后恢复

- 第一次启动：保存地址 BLE 直连等待 6002 ms 后失败，NimBLE status 为 13；随后
  2 秒快速扫描未找到相机并重置 BLE stack。
- 用户确认在第一次失败后手动重启了 StickS3，因此不能视为固件自动恢复成功。
- 第二次启动：BLE 已连接后进入 Wi-Fi；`BSSID_CHANNEL` 超时，`CHANNEL_ONLY`
  在总计 4932 ms 时连接成功，RSSI -37 dBm。
- 第二次启动从 boot uptime 到 LiveView 约 11633 ms，LiveView 打开耗时 521 ms。
- 本轮结论：冷启动首次成功率未通过；人工重启可恢复。下一轮必须只启动一次并至少等待
  30 秒，以验证 BLE 失败后的自动恢复能力。

### 第 2 轮：单次启动自动连接成功

- 用户只启动一次并等待，未进行人工二次重启。
- 开机早期 USB 串口重连导致 BLE 前半段日志缺失，但状态机自行到达
  `WIFI_CREDENTIALS_READY` 并继续连接。
- `BSSID_CHANNEL` 超时，`CHANNEL_ONLY` 在总计 4811 ms 时连接成功，RSSI -37 dBm。
- LiveView 打开耗时 586 ms；从 boot uptime 到 `PREVIEW_RUNNING` 为 21877 ms。
- 连接后帧率约 9.4–9.8 FPS，未观察到丢帧、复位或持续内存下降。
- 本轮结论：单次冷启动自动恢复通过，但总启动耗时偏长。
- 仍然复现 M5PM1 `Read Reg[0x48] failed`，随后固件禁用 M5PM1 polling 并降级到
  M5Unified；该问题与 Wi-Fi 结果分开跟踪。

### 第 3 轮：单次启动快速连接成功

- BLE 保存地址直连成功：fast path 253 ms（secure connect 240 ms）。
- `BSSID_CHANNEL` 超时，`CHANNEL_ONLY` 在总计 4811 ms 时连接成功，RSSI -54 dBm。
- LiveView 打开耗时 558 ms；从 boot uptime 到 `PREVIEW_RUNNING` 为 10794 ms。
- 连接后帧率约 9.2–9.8 FPS，未观察到丢帧或复位。
- 再次复现 M5PM1 `Read Reg[0x48] failed` 并自动降级。
- 本轮结论：首次启动自动连接通过。整机重启后 `CHANNEL_ONLY` 能成功，而不重启开发板、
  反复关闭/开启相机 WLAN 时稳定出现 `INVALID_PMKID`，进一步支持运行期 PMK/PMKSA
  缓存失效的判断。

### 第 4 轮：BLE 参数波动后 SSID-only 恢复

- BLE 保存地址直连完成：fast path 3052 ms。
- 缓存 `BSSID_CHANNEL` 快连 3202 ms 后失败，随后读取 fresh BLE 参数。
- fresh BLE 参数未包含 BSSID，并返回 channel 1；实际连接后 AP channel 为 11。
- `CHANNEL_ONLY` channel 1 超时；进入 `SSID_ONLY` 时连接已完成，fresh Wi-Fi 流程总计
  3611 ms，RSSI -42 dBm。
- LiveView 打开耗时 517 ms；从 boot uptime 到 `PREVIEW_RUNNING` 为 14133 ms，日志记录
  camera online total 为 12750 ms。
- 连接后帧率恢复到约 9.0–9.8 FPS，未观察到丢帧或复位。
- 本轮结论：首次启动自动连接通过；SSID-only 回退能够覆盖 BLE 返回 channel 与实际 AP
  channel 不一致的情况。

### 第 5 轮：重复复现 BLE channel 不一致并恢复

- BLE 保存地址直连完成：fast path 3152 ms。
- 缓存 `BSSID_CHANNEL` 快连 3203 ms 后失败，fresh BLE 参数再次未包含 BSSID，且返回
  channel 1；实际连接后 AP channel 仍为 11。
- `CHANNEL_ONLY` channel 1 超时；进入 `SSID_ONLY` 时连接已完成，fresh Wi-Fi 流程总计
  3606 ms，RSSI -29 dBm。
- LiveView 打开耗时 483 ms；从 boot uptime 到 `PREVIEW_RUNNING` 为 14280 ms，日志记录
  camera online total 为 12793 ms。
- 连接后帧率约 7.8–9.2 FPS，未观察到丢帧或复位。
- 本轮结论：首次启动自动连接通过；第 4、5 轮连续证明 BLE channel 字段不能作为唯一
  连接依据，必须保留 SSID-only 回退。

### 首批五轮冷启动汇总

- 首次启动自动完成：4/5（80%）。
- 失败的第 1 轮需要人工重启；其余四轮均能自动进入 LiveView。
- 四轮成功样本的 boot-to-LiveView：10.794 s、14.133 s、14.280 s、21.877 s；平均约
  15.271 s，中位数约 14.207 s。
- 所有成功轮次均先经历 3 秒的固定 BSSID/channel 尝试失败。
- 第 3 轮使用正确 channel 11 在 `CHANNEL_ONLY` 成功；第 4、5 轮 BLE 返回 channel 1，
  最终依赖 SSID-only 恢复。
- 每轮均复现 M5PM1 Reg[0x48] 读取失败并自动降级；每轮 USB 供电下 charging 状态均有
  0/1 抖动。
- 结论：最终恢复能力明显改善，但首次启动成功率和连接时延尚未达到发布目标。

## 第二轮优化实现（等待同机 A/B）

首轮证据表明 BSSID/channel 快连并未带来收益，反而稳定增加约 6.8 秒延迟；BLE 返回
的 channel 也可能与相机实际 AP 不一致。因此在同一分支完成以下针对性调整：

1. GR III/IV 每次由 BLE 开启的新 WLAN 会话直接执行 SSID-only；已有 BSSID/channel
   继续保留在 Profile 中供日志、诊断及连接后更新，但不再约束关联。
2. Wi-Fi 初始化使用 `WiFi.persistent(false)`，不让 Arduino/ESP-IDF 驱动跨相机 WLAN
   会话持久化 STA 配置；应用自己的 ProfileStore 仍正常保存相机凭据。
3. 收到 `INVALID_PMKID(49)` 时立即清理 RAM-only STA 安全上下文，并在同一个总超时
   预算内重试一次；不会无限重试，也不把 reason 49 当作密码错误。
4. 缓存 SSID-only 总预算由 3000 ms 调整为 7000 ms，覆盖首轮实测约 4 秒的 AP 启动
   时间；fresh BLE 路径仍受 15000 ms 总预算约束。
5. GR II 的手动 AP 兼容路径仍保留 BSSID/channel/SSID-only 分阶段连接计划。

自动验证：Native 101/101；StickS3 发布构建成功，Flash 57.1%，静态 RAM 22.9%。
这些结果只证明代码和构建回归通过；连接耗时、PMKID 恢复和成功率需要烧录后用同一
相机动作做 A/B 才能确认。

### 第二轮实机 A/B：第 1 轮

- 烧录时保留 NVS 和既有相机配对信息；冷启动 BLE 安全连接成功。
- 横屏后 BLE WLAN-on 成功，连接计划只有一次 `SSID_ONLY`：channel 0、无绑定 BSSID。
- STA 在 4487 ms 内完成，应用记录总耗时 4489 ms；关联 4179 ms、GOT_IP 4282 ms，
  实际 channel 11，RSSI -40 dBm，IP `192.168.0.3`。
- 连接过程中出现一次 `NO_AP_FOUND(201)`，但同一连接流程随后成功；未出现
  `INVALID_PMKID(49)`，也未触发安全上下文重置。
- LiveView 打开耗时 530 ms，进入 `PREVIEW_RUNNING` 后约 7.6 FPS。
- 与第一轮旧策略 6 次重连平均 8371 ms 相比，本轮 Wi-Fi 4489 ms，缩短约 46%。单个
  样本只验证新路径生效，仍需姿态循环与更多冷启动样本确认成功率和 P95。

### 第二轮实机 A/B：第 2 轮与状态机追加修复

- 竖屏关闭 WLAN 后再次横屏，第一次 WLAN-on 的相机 AP 在 15 秒内始终不可见，连接以
  `NO_AP_FOUND(201)` 结束；状态机随后重新 WLAN-on，并执行 7 秒缓存尝试。
- 缓存尝试仍未看到 AP，读取 fresh BLE 参数后相机返回 channel 1，而实际 AP 仍为
  channel 11。SSID-only 在 118 ms 时收到 `INVALID_PMKID(49)`，立即清理 RAM-only STA
  安全上下文；第二次 begin 在 945 ms 内成功，整个 fresh 连接调用 1381 ms，RSSI
  -42 dBm。LiveView 再用 389 ms 打开。
- 本轮证明 reason 49 定向恢复有效：没有等待满 15 秒，也未将其误判为密码错误。但
  发现 `resumeFromWifiCredentialsReady()` 名为缓存恢复、实际却先调用 15 秒 fresh 连接；
  失败后又重新 WLAN-on，重复消耗 15 + 7 秒。
- 追加修复为同一次 WLAN-on 内按“7 秒缓存 SSID-only → 读取 fresh BLE 参数 → 15 秒
  兜底”执行，中间不再重复关闭/开启相机 WLAN。新增状态机回归测试，自动验证更新为
  Native 102/102，StickS3 发布构建成功（Flash 57.1%，静态 RAM 22.9%）。该追加修复
  需重新烧录后继续姿态循环。

### 最终固件验证

- 重新烧录追加修复后，保留 NVS 的横屏冷启动走 7 秒缓存预算，并在 4486 ms 内以
  SSID-only 成功；LiveView 402 ms，RSSI -29 dBm。
- 最后一轮竖屏约 9 秒后横屏：HTTP/BLE 正常关闭并重新开启 WLAN，全程只发送一次
  WLAN-on。
- 缓存 SSID-only 在 7050 ms 后以 `NO_AP_FOUND(201)` 结束；同一 WLAN 会话直接读取
  fresh BLE 参数，没有中途 WLAN-off/on。fresh 参数仍报告 channel 1，关联继续使用
  SSID-only。
- fresh 连接在 118 ms 时遇到 `INVALID_PMKID(49)`，立即清理 RAM-only STA 安全上下文；
  第二次 begin 945 ms 成功，fresh 调用总计 1379–1381 ms，实际 channel 11，RSSI
  -32 dBm；LiveView 424 ms。
- 从 WLAN-on 完成到 LiveView 恢复约 9.73 秒；追加修复前的同类失败恢复约 26 秒，缩短
  约 63%。预览随后稳定在约 7.0–8.3 FPS，未丢帧、未复位。
- 最终结论：SSID-only、7 秒缓存预算、同会话 fresh 兜底及 PMKID 定向恢复均已在当前
  GR4_FAMILY 相机上得到实机路径证据。仍需更多循环统计 P95，且不能外推到其他机型。

### 最终固件连续 5 轮姿态重连

| 轮次 | 最终结果 | 缓存阶段 | fresh 连接 | Wi-Fi 总耗时 | LiveView | RSSI |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| 1 | 成功 | 7052 ms / NO_AP_FOUND | 1381 ms | 9274 ms | 365 ms | -39 dBm |
| 2 | 成功 | 7055 ms / NO_AP_FOUND | 1853 ms | 9739 ms | 342 ms | -42 dBm |
| 3 | 成功 | 7054 ms / NO_AP_FOUND | 1969 ms | 9888 ms | 393 ms | -40 dBm |
| 4 | 成功 | 7057 ms / NO_AP_FOUND | 1381 ms | 9219 ms | 406 ms | -46 dBm |
| 5 | 成功 | 7051 ms / NO_AP_FOUND | 1969 ms | 9860 ms | 300 ms | -43 dBm |

- 成功率 5/5；Wi-Fi P50 9739 ms，样本 P95/最慢 9888 ms；从首次连接开始到画面恢复
  P50 约 10.12 秒，样本最慢约 10.32 秒。
- 5/5 均只发送一次 WLAN-on，均在同一会话完成 fresh 兜底；无重启、无丢帧、无重复
  WLAN 开关。fresh 阶段 5/5 出现 `INVALID_PMKID(49)`，定向清理后全部恢复。
- 7 秒缓存预算 5/5 在 AP 可用前耗尽；约 0.74–0.80 秒 fresh BLE 读取后立即出现 PMKID，
  说明相机重复 WLAN-off/on 的 AP 可用窗口约为 8 秒。每轮 fresh 参数还触发一次凭据
  保存、成功后再保存实际 BSSID/channel，形成不必要的 NVS 写入。
- 下一版把缓存预算调整为 11000 ms，使约 8 秒出现的 PMKID 与随后的 1–2 秒恢复留在
  同一次 SSID-only 调用，目标是在保持成功率的同时避免 fresh 读取与重复 NVS 写入。

### 11000 ms 预算复测与根因修正

- 烧录后冷启动确认 `budget=11000ms` 生效，SSID-only 4487 ms 成功，LiveView 477 ms。
- 随后 3 轮姿态循环中，缓存阶段 3/3 都在约 3.4 秒收到 `NO_AP_FOUND(201)`，但没有
  再发起扫描，而是继续空等至约 11.03 秒；之后仍需 fresh BLE 参数读取，且 fresh
  阶段继续由 PMKID 定向清理恢复。因此单纯延长预算没有消除 fresh 路径与 NVS 写入。
- 第 1 轮最终进入稳定 LiveView；第 2、3 轮初次 LiveView 分别约 3275 ms、4264 ms 才
  打开，随后出现 `LiveView closed` 或 5 秒帧 stall 并触发自动恢复。Wi-Fi 最终均能恢复，
  但这两轮不能计为一次稳定画面恢复，说明长时间网络等待和连续 WLAN 恢复对相机 HTTP
  服务也有压力。
- 根因修正：收到 reason 201 后立即结束当前 begin 并在同一总预算内重新扫描，最多两
  次；PMKID 仍只允许一次安全上下文清理。预期路径为“约 3.4 秒 NO_AP → 立即重扫 →
  PMKID 清理 → 关联”，而不是空等到 11 秒。
- 自动验证：Native 103/103；StickS3 发布构建成功（Flash 57.1%，静态 RAM 22.9%）。

### NO_AP 立即重扫实机结果

- 烧录后冷启动：首次扫描 3414 ms 返回 `NO_AP_FOUND(201)`，立即重扫后直接连接；
  Wi-Fi 总耗时 5490 ms，LiveView 442 ms，RSSI -31 dBm，无 fresh BLE 兜底。
- 随后 3 轮姿态重连全部成功：

| 轮次 | NO_AP 重扫 | PMKID 清理 | Wi-Fi | LiveView | RSSI | fresh 兜底 |
| --- | --- | --- | ---: | ---: | ---: | --- |
| 1 | 1 次 | 是 | 5510 ms | 402 ms | -44 dBm | 否 |
| 2 | 1 次 | 是 | 4921 ms | 465 ms | -45 dBm | 否 |
| 3 | 1 次 | 是 | 5512 ms | 441 ms | -43 dBm | 否 |

- 成功率 3/3；Wi-Fi P50 5510 ms，样本最慢 5512 ms；LiveView 均在 465 ms 内打开并持续
  出帧，没有复现上一版的关闭或 stall。每轮只发送一次 WLAN-on，没有失败后的 fresh
  参数读取，也没有重复 WLAN 开关。
- 对比 7 秒版本的 5 轮 P50 9739 ms，新策略 P50 缩短约 43%；对比最初旧策略 8371 ms
  均值，当前最慢样本也缩短约 34%。
- 仍观察到连接后延迟 BLE 刷新返回 channel 1/空 BSSID，随后用实际 channel 11/BSSID
  覆盖并写 NVS。收尾优化把 BLE 的 BSSID/channel 视为非绑定提示：SSID/密码不变时不
  覆盖实测值；实际 BSSID/channel 未变化时也跳过 NVS 写入。
- 收尾自动验证：Native 104/104；StickS3 发布构建成功（Flash 57.1%，静态 RAM
  22.9%）。

### NVS 跳写收尾验证

- 最终固件冷启动首次扫描 3412 ms 返回 NO_AP，立即重扫后 Wi-Fi 总计 4905 ms 成功，
  LiveView 441 ms，RSSI -51 dBm；随后稳定约 8.6–9.5 FPS，无丢帧或 stall。
- 连接后的实际 BSSID/channel 与缓存一致，日志确认
  `WiFi cache: unchanged after cached connect; NVS write skipped`。
- 5 秒后延迟 BLE 刷新仍返回 channel 1/空 BSSID，但 SSID/密码未变化，日志确认保留
  已连接的实际 BSSID/channel，且没有触发 NVS 写入。
- 当前实机最终结论：NO_AP 立即重扫、PMKID 限次恢复、同会话 fresh 兜底、SSID-only、
  非绑定提示处理及 NVS 跳写均已获得路径证据。样本规模仍不足以替代 20/100 轮发布
  回归，不应外推到其他相机型号。

### 最终固件 10 轮姿态压力测试

| 轮次 | Wi-Fi | LiveView | RSSI |
| --- | ---: | ---: | ---: |
| 1 | 5508 ms | 409 ms | -52 dBm |
| 2 | 5389 ms | 481 ms | -54 dBm |
| 3 | 4920 ms | 442 ms | -41 dBm |
| 4 | 4918 ms | 392 ms | -45 dBm |
| 5 | 4920 ms | 510 ms | -43 dBm |
| 6 | 4920 ms | 3185 ms | -61 dBm |
| 7 | 4918 ms | 471 ms | -57 dBm |
| 8 | 5508 ms | 404 ms | -52 dBm |
| 9 | 4920 ms | 369 ms | -49 dBm |
| 10 | 4919 ms | 387 ms | -56 dBm |

- 成功率 10/10；Wi-Fi 平均 5084 ms、P50 4920 ms、样本 P95/最慢 5508 ms。
- LiveView 平均 705 ms、P50 约 426 ms；9/10 在 510 ms 内。第 6 轮 RSSI 降至
  -61 dBm，LiveView 打开耗时 3185 ms，但随后正常持续出帧，未触发 watchdog。
- 每轮均为一次 NO_AP 立即重扫、一次 PMKID 安全上下文清理后连接成功；没有进入 fresh
  失败兜底，没有重复 WLAN-on，没有重启、断流或 stall。
- 10/10 连接成功后均确认 BSSID/channel 未变化并跳过 NVS 写入；延迟 BLE 刷新也只
  确认 SSID/密码未变化，不覆盖实际 channel 11/BSSID。
- 加上此前同一最终连接策略的 3 轮，姿态重连累计 13/13 成功。下一阶段进入持续预览
  soak，重点观察帧率、断流、watchdog、复位和内存趋势。

### 最终固件 15 分钟持续预览

- 实际日志窗口约 16 分钟（22:53:01–23:09:15）。stream 帧计数从 2554 连续增长到
  11117，rendered 从 604 增长到 9167，两者均增加 8563；全程 `dropped=0`。
- FPS 大多为 8.0–9.6；周期性 Props 请求附近短暂降至约 6.8–7.8，随后自行恢复。
- JPEG decode 通常约 39–40 ms，render 通常约 52–53 ms；未见持续变慢。
- internal free heap 在约 52.4–53.4 KB 范围波动，largest internal block 保持约
  20.5 KB；free PSRAM 约 8.04 MB，没有持续下降趋势。
- 未观察到复位、Wi-Fi 重连、LiveView 关闭、frame/stream stall watchdog 或恢复风暴。
- USB 供电期间 charging 状态仍频繁 0/1 跳变；与此前高频啸叫观察一致，继续作为独立
  电源/PMIC 项处理，不计为本次 Wi-Fi/LiveView 失败。
- 结论：15 分钟 USB 供电、横屏持续预览通过；尚不能替代计划中的 60 分钟 soak。

### 最终固件 3 轮冷启动回归

| 轮次 | BLE 就绪 | Wi-Fi 路径 | Wi-Fi | LiveView | 启动至画面 | RSSI | 结果 |
| --- | ---: | --- | ---: | ---: | ---: | ---: | --- |
| 1 | 3451 ms | 首次 SSID-only 直连 | 3775 ms | 4084 ms | 14223 ms | -35 dBm | 成功，持续出帧 |
| 2 | 3502 ms | NO_AP 后立即重扫 | 4906 ms | 410 ms | 19832 ms | -47 dBm | 成功；含约 8 秒竖屏等待 |
| 3 | 302 ms | NO_AP 后立即重扫 | 5377 ms | 408 ms | 11257 ms | -41 dBm | 成功，持续出帧 |

- 冷启动成功率 3/3；Wi-Fi 平均 4686 ms、P50 4906 ms、样本最慢 5377 ms。三轮均
  使用缓存 SSID-only 完成，没有进入 fresh BLE 凭据兜底，没有重复 WLAN-on，也没有
  Wi-Fi 断连或设备复位。
- 第 1 轮 LiveView 打开为 4084 ms，明显高于约 0.4 秒常态，但打开后稳定；第 2、3 轮
  分别为 410 ms、408 ms，未复现该延迟。该单次长尾保留为观察项，不据此判断回归失败。
- 第 2 轮启动后约 8 秒保持竖屏，故 19832 ms 总启动耗时不与全程横屏样本比较；第 3 轮
  启动初期也短暂走过竖屏缓存状态，切换横屏后正常恢复。
- 三轮均确认连接后 BSSID/channel 未变化并跳过 NVS 写入；延迟 BLE 刷新返回 channel 1/
  空 BSSID 时继续保留实际连接的 channel 11/BSSID。
- 每轮仍出现 M5PM1 Reg `0x48` I2C 读取失败，固件随后禁用 M5PM1 polling 并回退到
  M5Unified；本次未影响 Wi-Fi 或 LiveView，但与 USB charging 状态跳变一起继续作为独立
  电源/PMIC 问题跟踪。
- 综合验收：最终连接策略已通过 13/13 姿态重连、3/3 冷启动及约 16 分钟持续预览；当前
  GR4_FAMILY 实机上未复现“蓝牙开启相机 Wi-Fi 后一直连不上”。样本不能外推到其他相机
  型号或替代更长时间/更大规模发布回归。

### 2026-10-09 相机关机防误唤醒与手动恢复

- 相机完全关机时重启 StickS3：固件只扫描已保存的 BLE 身份；没有建立 BLE 连接、发送
  WLAN-on、连接 Wi-Fi 或请求 LiveView。观察期间相机保持关机。由于完全关机的相机不
  广播，该路径停留在安全扫描状态，没有进入已连接后的 `CAMERA_POWER_OFF`。
- 手动开启相机并按 Button A 后恢复成功：BLE 安全连接 774 ms；先读取 Power `0x01`、
  Operation Mode `CAPTURE` 并启用 Power Notify，随后只发送一次 WLAN-on。Wi-Fi 经一次
  NO_AP 即时重扫后 5375 ms 连接，LiveView 461 ms，RSSI -44 dBm。
- LiveView 运行期间关闭相机：固件先关闭 LiveView、发送一次 WLAN OFF，再收到 Power
  Notify `0x00`、读取 Power `0x00`，进入 `CAMERA_POWER_OFF`；扫描被暂停，未自动发送
  WLAN-on。30 秒无人工操作后 StickS3 按设计自动关机。
- 再次验证关机竞态时，电源读取短暂返回 `0x01`，但随后 Power Notify `0x00` 且
  Operation Mode 为 `OTHER`；状态机在 Wi-Fi allow 前进入保护态，未错误开启 WLAN。
- 在保护态内重新开启相机并单击 Button A：日志确认清除 guard、重建 BLE stack、重新
  扫描并安全连接；再次确认 Power `0x01`、`CAPTURE` 后才发送一次 WLAN-on。Wi-Fi 在
  NO_AP 重扫后遇到一次 `INVALID_PMKID(49)`，RAM-only 安全上下文清理后 4918 ms 成功；
  LiveView 405 ms，RSSI -70 dBm，从按键到画面约 13.9 秒。
- 恢复后跳过未变化的 NVS 写入，LiveView 持续出帧。结论：当前 GR4_FAMILY 实机的
  “完全关机冷启动不误唤醒”“已连接后关机保护”“关机通知竞态阻断 Wi-Fi”以及“Button A
  手动恢复”四条安全路径均通过。

### 2026-10-09 AP 时序扰动

- 用户选择跳过将姿态重连样本从 13 轮补齐到 20 轮；快速冒烟状态仍记录为 13/13，不能
  标记为计划中的 20/20。
- 从竖屏 `WIFI_CREDENTIALS_READY` 横放触发 WLAN-on 后约 1–2 秒立即转回竖屏：缓存
  SSID-only 连接在 1777 ms 以 `DISCONNECTED`/无 reason 结束，固件随后只发送一次
  WLAN OFF，并回到 `WIFI_CREDENTIALS_READY`；没有继续 fresh BLE 读取或形成重连风暴。
- 保持竖屏后再次横放：状态机重新检查 Power `0x01`、Operation Mode `CAPTURE`，只发送
  一次新的 WLAN-on。首次扫描收到 `NO_AP_FOUND(201)` 后立即重扫，Wi-Fi 5371 ms 成功，
  LiveView 450 ms，RSSI -46 dBm；随后持续出帧，无复位或 stall。
- 日志在姿态取消前输出了 `cached connect failed; reading fresh BLE params`，但控制流随即
  因竖屏取消，没有实际读取 fresh 参数。功能行为正确；日志措辞存在短暂误导，可在后续
  可观测性整理中把该日志移到真正开始 fresh 读取之后。
- 结论：WLAN/AP 启动窗口内被姿态中断时能干净取消，下一次 WLAN 会话可恢复；本轮验证
  了时序扰动恢复，但不等同于错误 BSSID/channel 的受控 NVS 注入测试。

### 2026-10-09 纯电池供电 A/B

- 拔除 USB 后使用 StickS3 内置电池冷启动，用户确认约 10 秒成功进入 LiveView；未出现
  USB 供电时观察到的持续细小电流声/高频啸叫。
- 随后在纯电池供电下完成 3 轮“竖屏关闭 WLAN → 横屏恢复 LiveView”，用户确认三轮均
  完成，未报告卡死、自动重启或异响。
- 本轮没有 USB 串口，因此属于用户目视功能验证，没有 BLE/Wi-Fi/LiveView 分段耗时、
  RSSI、disconnect reason、heap 或 reset reason 数据；不与有串口的耗时样本合并统计。
- 结论：当前样本未发现纯电池供电导致连接退化，且异响只在 USB 接入时出现，进一步支持
  将异响和 charging 状态跳变作为 USB 供电/充电/PMIC 独立问题，而非 Wi-Fi 功能故障。
- 继续保持纯电池供电，将 StickS3 与相机距离拉开至约 3 米后完成 3 轮姿态重连；用户
  确认连接与 LiveView 均正常。该结果同样为无串口的目视验证，不包含 RSSI/P95 数据。

### 2026-10-09 错误 BSSID/channel 缓存注入

- 新增默认关闭的 `m5stack-sticks3-wifi-cache-fault` 硬件测试环境。它只在 GR4_FAMILY
  Profile 从 NVS 载入后，把运行时 BSSID 改为 `02:00:00:00:00:01`、channel 改为 1；
  不改 SSID/密码，不清 NVS/Bond，正式 `m5stack-sticks3` 构建不包含注入代码。
- 自动验证：Native 100/100；正式固件和注入固件均使用相同依赖版本构建成功。二进制
  字符串核验确认 `TEST ONLY` 仅存在于注入环境，正式固件中不存在。
- 实机日志确认原缓存为真实 BSSID、channel 11，随后 RAM 同时注入错误 BSSID/channel。
  连接仍明确执行 `SSID_ONLY channel=0 has_bssid=0`，2012 ms 获得 IP，实际 channel 11、
  RSSI -54 dBm；连接后回采真实 BSSID/channel 并写回正确 NVS。
- 首次 Wi-Fi 已成功，但 LiveView 等待 HTTP headers 5 秒超时；自动恢复关闭并重新开启一次
  WLAN，第二次 Wi-Fi 4679 ms（期间一次 `INVALID_PMKID(49)` 定向清理），LiveView
  599 ms。从首次 WLAN-on 到最终画面约 13.25 秒，无人工干预、复位或重连风暴。
- 测试后已重新烧录正式环境且保留 NVS。正式启动没有 `TEST ONLY`，缓存为真实 BSSID/
  channel 11；一次正式连接为 Wi-Fi 2605 ms、RSSI -46 dBm，LiveView 5006 ms 后成功并
  持续出帧。该约 5 秒 HTTP 打开长尾与此前一次 4084 ms 样本一起保留为观察项。
- 仅 BSSID 错误（mode 2）通过：保留 channel 11，仅将 BSSID 设为
  `02:00:00:00:00:01`。连接仍执行 `SSID_ONLY channel=0 has_bssid=0`；首次扫描收到
  `NO_AP_FOUND(201)` 后在同一 11 秒预算内重扫，Wi-Fi 5371 ms 成功，实际 channel 11、
  RSSI -43 dBm，LiveView 488 ms，并回采真实 BSSID。
- 仅 channel 错误（mode 3）通过：保留真实 BSSID，仅将 channel/frequency 设为
  1/2412 MHz。连接仍执行 `SSID_ONLY channel=0 has_bssid=0`；一次 NO_AP 后重扫，Wi-Fi
  5370 ms 成功，实际 channel 11、RSSI -42 dBm，LiveView 3329 ms。此前还出现一次 BLE
  direct connect `err=13`，后续扫描/重连恢复；不属于 Wi-Fi 缓存绑定失败。
- 单变量测试日志揭示首版注入夹具会修改内存中的 canonical `cameraProfile`；BLE 重连后的
  整 Profile 保存可能间接把注入值写入 NVS，因此当时的 `NVS unchanged` 日志不严谨。
  这没有削弱错误提示值下 SSID-only 成功连接的结论，但会污染下一次测试的起始缓存。
  测试后已把注入改成首次连接调用内的一次性局部副本，后续 Profile/NVS 保持原值。
- 首轮恢复正式环境后验收：构建产物及启动日志均无 `TEST ONLY`，缓存为真实 BSSID
  `F2:3E:05:CE:C6:96`、channel 11；Wi-Fi 5371 ms、RSSI -43 dBm，LiveView 509 ms，
  NVS 因内容未变化而跳过写入。夹具改成连接局部副本后再次构建、烧录最新正式产物；
  最新一轮 Wi-Fi 4900 ms、RSSI -43 dBm，LiveView 3193 ms，随后持续出帧。
- 结论：BSSID/channel 同时错误、仅 BSSID 错、仅 channel 错三种受控缓存失效均通过；
  GR4 新 WLAN 会话不会受缓存提示值绑定。三组测试均观察到自动恢复而无 Wi-Fi 无限重连、
  设备复位或人工重新配对。
