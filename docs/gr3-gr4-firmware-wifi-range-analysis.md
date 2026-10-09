# GR II / GR IIIx / GR IV 固件与 main 遥控距离差异分析

日期：2026-10-09。症状：用户报告旧 main 固件在 GR IV 约 5 米仍可遥控，而 GR III 系列约 1 米后预览卡住、断开并重连。

## 范围与结论

- 遥控器基线：本地 main `4d1aa81b92409d91399331d2b4a776c009ab7a8b`；分析实际读取的是 `git show main:<path>`，未使用工作区新增连接策略归因。
- 相机样本：GR IIIx v1.60（内部 1.60.11.17）和 GR IV v1.11（内部 1.11.10.7）。目录中没有独立的 GR III / HDF 固件样本，本结论不能直接覆盖整个 III 系列。
- 资料根目录：`/Users/zhanghuilong/Documents/ricoh-gr4-firmware-analysis-and-feature-expansion`。
- 方法：读取已解包脚本、配置和静态协议报告；解析 GR IV 功率表 TLV；对 IIIx webapid 的 SendFrameLoop 做离线 ARM 反汇编。不执行相机二进制，不连接或修改相机。
- 两代相机确实使用不同无线平台和 AP 配置。现有材料不能证明“IIIx 射频只能到 1 米”，也不能证明 GR IV 有显著更高的实际发射功率。
- 最直接的软件优化线索是相机 LiveView 已有 `maxfps` 参数，而 main 未使用。应首先检验源端限帧，再检验 BLE 并行负载和恢复策略。

## 1. 无线平台

IIIx `output/gr3x-v160/rootfs/usr/bin/start_wlan.sh:4` 选择 `cyw43455_softap.bin`，指定 Murata Type1MW NVRAM 和 `43455_ricoh.clm_blob`，写入 `bcmdhd` 参数；AP 接口为 wlan0。

IV `output/http-analysis/initfs/etc/rc.local:40` 起按地区加载 mlan/moal，指定 NXP 功率配置。`lib/firmware/nxp/wifi_mod_para.conf` 的 SDIW612 配置指向 `sduart_nw61x_v1.bin.se`，与 NXP IW61x 平台相符；AP 接口为 uap0。该通用文件含多个芯片块，不能把所有块都认为是当前启用配置。没有以芯片理论能力替代相机实际能力。

上述差异意味着相机端驱动、速率控制、共存和省电实现不同；仅靠固件无法确定天线增益、实际接收灵敏度、板级校准和当前协商速率。

## 2. 发射功率：存在差异，但不能解释为五倍距离

IIIx 启动脚本明确选中 `bcm943455wlsagb_Type1MW_Common_MP_170921_US26.txt`，不是目录里另一个早期配置。

该文件关键字段：

```text
rxchain=1
txchain=1
maxp2ga0=44
cckbw202gpo=0x0000
ofdmlrbw202gpo=0x0000
dot11agofdmhrbw202gpo=0x0000
mcsbw202gpo=0x00000000
btc_mode=1
```

CYW43455 厂商 NVRAM 文档给出 maxp2ga0 的单位为 0.25 dB，并对 CCK 标称目标给出 `0.25 × value − 1.5` 公式。因此 44 对应 11 dBm 上限字段，CCK 标称目标按该公式为 9.5 dBm。不能把它报告成所有速率下测得的发射功率；OTP、CLM、校准、驱动和速率仍会影响运行值。

另一份 `bcm943455wlsagb_Type1MW_170408.txt` 是 74，但启动脚本没有选择它，不能用它声称这台相机按 18.5 dBm 工作，更不应据此修改校准文件。

IV 的 `txpower_EU.bin` / `txpower_US.bin` / `txpower_JP.bin` 实际为 ASCII 十六进制命令表。对 `txpwrlimit_2g_cfg_set` 解码：跳过 12 字节命令头，按 little-endian type/length 读取 TLV；type=0x0189，长度44，字段为 start frequency=2407、bandwidth=20、channel，然后是 rate-group/power 字节对。三份文件在信道1–11的 group 0–6 均为12。按 NXP AN13009 的通用组定义，覆盖 CCK、legacy OFDM 和 HT20 MCS0–7，单位 dBm。EU/JP 表在12/13信道同为12；US表对应为0。未把其他速率组、RU表或零值扩展成未验证的运行行为。

这给出 IV 常用2.4GHz速率组12 dBm的配置限制，与 IIIx 的11 dBm上限字段只是接近量级；两者定义也不完全相同。不能据此宣称实际仅差1 dB，也不能声称 IV 比 IIIx 高十几 dB。自由空间同频同天线理想模型中，距离5倍对应约14 dB路径损耗差；该模型只用于数量级检验，不预测室内实际距离。

厂商字段依据：[CYW43455 NVRAM](https://community.infineon.com/gfawx74859/attachments/gfawx74859/CommunityTranslations/5357/1/002-30546_00_V.pdf)、[NXP AN13009](https://www.nxp.com/docs/en/application-note/AN13009.pdf)。

## 3. AP 模板：值得测量，但不是运行时结果

| 字段 | IIIx `/etc/hostapd-wpa2psk.conf` | IV `/etc/hostapd_24GHz.conf` |
|---|---|---|
| hw_mode | g | g |
| ieee80211n | 未显式设置 | 1 |
| ht_capab | 未显式设置 | SHORT-GI-20 |
| wmm_enabled | 1 | 1 |
| channel | 6 | 0，chanlist=1 6 11 |
| beacon_int | 100 | 100 |
| dtim_period | 2 | 未显式设置 |
| ieee80211ax | 未设置 | 1 |

IIIx netmgrd 字符串包含对 hostapd 配置的读取/写入和信道管理，因此不能把模板channel=6等同于始终固定在6。IV也会从模板生成 `/tmp/hostapd.conf`。尚未读取两代实际运行配置和AP能力信息，不能断定IIIx实际只协商11g，或断定运行时DTIM分别为2和1。

如果运行时确认IIIx缺少HT，而IV使用HT20，则聚合、空口效率等差异值得测试；若两台实际信道不同，干扰也可能解释部分差异。ESP32-S3自身不能使用5GHz/802.11ax，因此不把IV的5GHz/ax作为本遥控器链路的解释。

## 4. LiveView 参数是确定的新线索

本地 `docs/gr3x-v160/http-api-analysis.md:180` 确认：

- `maxfps`：整数1–90，缺省90；这是软件上限，不是实际90FPS。
- `reso`：按支持的分辨率列表验证；通过相机 `resoList` 确定支持值，不猜分辨率。
- 本次额外反汇编 IIIx `SendFrameLoop`（0x88504）看到传入帧率参与1000/帧率的间隔计算和休眠路径；这不只是孤立字符串。
- IV `docs/gr4-v111-http-api-analysis.md:235` 同样记录 `reso` / `maxfps`。

main 的 `src/gr_api.cpp:349` 只发 `GET /v1/liveview HTTP/1.1`，没有请求低帧率或低分辨率。仓库旧预览记录中“没有已知可配置帧率/分辨率”的认识应由这些新静态证据更新，但参数在用户当前机型/固件上是否有效仍需实机确认。

首选A/B：仅把IIIx请求改为 `/v1/liveview?maxfps=5`，保留其他设置，验证HTTP响应、实际FPS、KiB/s、距离和首次故障原因。限帧从相机发送端降低流量；在遥控器解码后丢帧不会省掉已传输的数据。若相机实际默认约10FPS，5FPS有机会显著降低流量，但不能保证精确减半。

## 5. 与旧 main 的故障放大机制结合

main 中 GR III 与 GR IV 均保留BLE连接；GR III的 `allowBlePreviewParking=false` 是既有行为。IIIx NVRAM中确有BT共存字段，但无法单凭btc_mode=1推断无线时间片比例、天线布线或实测性能。

main `src/app/AppController.cpp:698` 以frame/stream任一超过5秒为恢复条件，即使Wi-Fi仍关联、仍有零散数据但没有完整帧，也会恢复。BLE先掉线同样会恢复。恢复可能经过 `disconnectWifiLiveViewToBleReady`，关闭本机Wi-Fi连接并发BLE WLAN-off，再重新连接。

因此合理但未证实的故障链为：无线有效吞吐下降或BLE不稳 → 图像无法及时完整到达/链路失效 → main触发整条连接恢复 → 用户观察到距离很短且反复重连。当前新增SSID-only、11秒预算、201/49处理不能作为最初卡顿原因。

## 6. 验证优先级

1. 同姿态、同供电、同环境记录0.5/1/2/5米：Wi-Fi RSSI、实际channel/PHY能力、FPS、KiB/s和恢复前第一条原因。不要把应用主动disconnect之后的事件当成起因。
2. 单变量测试 `maxfps=5`；保持BLE连接与现有恢复阈值不变。
3. 单变量测试预览期间BLE parking及HTTP快门；退出预览/掉线后的BLE恢复必须一起验证。
4. 若Wi-Fi关联仍在，仅视频流停顿，测试先重开HTTP流、失败后才重开WLAN的分级恢复。
5. 如果同信道、同姿态下IIIx RSSI已明显低于IV，进一步确认相机个体、天线遮挡和实际发射/接收条件；不能用代码分析替代射频测量。

本次未改变固件实现、相机参数或射频校准，也未进行距离实测。

## 7. 加入 GR II v3.00 对照后的修正

用户进一步确认：GR II 在约5米处，实时预览和快门均正常；不是仅小流量快门测试。这是用户实测描述，本次没有独立复测。增加的本地样本为 `output/gr2-v300/`，详细协议入口为资料仓库 `docs/gr2-v300/http-api-analysis.md`。

### 7.1 新核实的静态证据

- 直接读取 `components/b01firm6.decoded.bin`，文件偏移 `0x6f4610` 是 `MARVELL iTRON AP DRIVER VER.0101`，`0x709d90` 是 `Marvell Micro AP`。这支持 Marvell/iTRON AP 驱动实现，不能据此确定具体芯片型号、天线或功率。
- `components/rg2_v001up/b01firm8.decoded.bin` 中 `0xa1389` 为 `wlanBeaconInterval`，`0xa139c` 为 `wlanDtimPeriod`，`0xa13b9` 起有 `wlanTxPowerTbl[0]` 至 `[25]`。这些是配置名称字符串；未恢复有效数值、单位及加载链，不能将字符串地址或附近字节解读为发射功率。
- HTTP LiveView 参数表文件偏移 `0x369cf4` 的六个大端32位字段为 `a0342f6d 00000000 a02bc1bf 00000000 a074e684 0000001e`。名称为maxfps，默认字0x1e=30；已保存的解析反汇编 `a02bc1be.asm` 设置上下界0和240，随后调用范围解析函数。
- GR II 的 `/v1/liveview` 路由登记maxfps，没有登记IIIx/IV的reso参数。这不构成全程序不存在其他分辨率控制的证明。
- GR II 的流采用JPEG multipart，边界字符串与IIIx/IV不同；遥控器按JPEG SOI/EOI切帧，不依赖固定multipart边界，因此边界名称不是距离差异的解释。

### 7.2 三代对照

| 比较项 | GR II | GR IIIx | GR IV |
|---|---|---|---|
| 用户描述距离 | 约5米，预览/快门均正常 | III系列约1米后卡顿重连；样本为IIIx | 约5米 |
| 相机固件无线实现 | Marvell iTRON AP | CYW43455 / bcmdhd | NXP / mlan / moal |
| main的相机控制链路 | Wi-Fi/HTTP | BLE控制 + Wi-Fi预览 | BLE控制 + Wi-Fi预览 |
| main预览时相机BLE连接 | 不建立 | 保持 | 保持 |
| BLE掉线能否触发预览恢复 | 逻辑前置条件直接通过，无相机BLE掉线来源 | 能 | 能 |
| AP开启 | 机身手动开启 | BLE开启 | BLE开启 |
| main竖屏时 | 保留预览传输请求供HTTP快门；显示另行处理 | 停预览并管理WLAN关闭 | 同IIIx |
| main周期Props请求 | GR II分支跳过网络请求 | 有 | 有 |
| main延迟BLE凭据刷新 | 不安排 | 缓存连接后安排 | 同IIIx |
| main Wi-Fi sleep | true | true | true |
| main完整帧stall阈值 | 5秒 | 5秒 | 5秒 |
| 相机maxfps参数 | 默认30，解析0–240 | 默认90，解析1–90 | 已确认解析1–90；本次不补猜默认值 |
| 实际帧大小/输出FPS | 本次没有同条件测量 | 旧IIIx HDF记录720×480、30–35KiB/帧、约10FPS | 本次没有三机同条件测量 |

main代码核查依据：`runBleDiscoveryAtBoot` 对GR II提前返回；`bleLinkHealthy` 对GR II逻辑为true；`shootAutofocusForController` 调用 `shootGr2`；`refreshPropsIfDue` 对GR II提前返回；`finishCameraWifiOverHttpForController` 避免对GR II发送结束相机接口；`updateUi` 对GR II不以横屏作为previewRequested必要条件。没有把逻辑BLE就绪误当成实际BLE连接，也不声称任意切换机型路径下ESP32蓝牙控制器必然已彻底反初始化。

### 7.3 由对照能推导和不能推导的结论

1. “老相机/旧无线标准天然只能近距离”解释不足：GR II更旧且相同遥控器能在5米持续预览。
2. `setSleep(true)`、5秒阈值以及不带参数的LiveView请求是main公共行为，单独拿其中一项不能解释为何只有III系列差。它们与相机行为的交互仍可能影响结果。
3. BLE并行和BLE掉线门控是GR II与IIIx之间最明确、可隔离的差异，值得提高验证优先级。但IV也保持BLE且表现正常，因此“只要BLE并行就只能1米”同样不成立；需要验证IIIx平台/连接参数下的具体表现。
4. 不可根据默认maxfps=30与90声称IIIx流量是GR II三倍；参数是上限，JPEG大小、实际FPS及压缩程度未知。也不能声称GR II图像更小或更模糊。
5. GR II AP生命周期较简单，且跳过周期Props/延迟BLE凭据刷新，减少了主循环阻塞和AP重启的来源。它可能恢复得更平顺，但AP启动/重连差异不是持续预览首次卡顿的直接证明。
6. GR II实际功率未知，不能用它的5米表现推出它发射功率一定高于IIIx；Marvell与NXP名称也不能证明GR II/IV射频结构相同。

### 7.4 最小鉴别实验

同一台出现问题的III系列相机、同一距离/方向/供电/信道，依次测试四种组合，优先只改变遥控器：

| 组合 | 预览时BLE | LiveView参数 | 判别目的 |
|---|---|---|---|
| A | 保持 | 原请求 | 重现基线 |
| B | parking | 原请求 | 隔离BLE链路/共存影响；快门改HTTP并验证恢复 |
| C | 保持 | maxfps=5 | 隔离持续预览流量影响 |
| D | parking | maxfps=5 | 前两项均有改善或单项不足时评估叠加效果 |

如B显著改善，仍需用原始故障日志区分“BLE断开门控消失”与“吞吐改善”：parking会同时移除BLE数据活动和BLE掉线前置条件，不能仅凭距离变长宣称是射频共存已被证明。若C改善，需确认实测KiB/s确实降低。若A–D均在类似RSSI/距离失效，进一步排查信道干扰、相机个体/天线和底层AP链路。所有组合均记录首次恢复原因，而不是应用主动断开后的事件。
