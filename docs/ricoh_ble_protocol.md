# RICOH BLE Protocol Notes

## 证据范围

本文只记录当前源码、原项目实机记录和已审查的外部逆向资料。它不是 RICOH 官方协议保证。

- GR IV / GR IV HDF：原项目已有实机验证记录；当前代码继续使用固定 Handle Profile。
- GR IIIx：旧 GR3 功能分支曾由用户确认通过；本次双协议重构分支仍需重新执行完整矩阵。
- GR III：当前分支已实现但仍需 GR III 机身独立验证；配对顺序参考 `mic-kul` 的 BlueZ/真机分析。
- GR III HDF / GR IIIx HDF：无独立实机证据，只能视为实验性支持。
- GR II：未实现协议，仅有无 BLE、ManualOnly/ManualConfiguration 的扩展占位。

## 运行时协议识别

安全原则是“先识别，再执行该 Profile 唯一允许的动作”，禁止先写 GR IV Handle、失败后再试 GR III UUID。

1. 扫描阶段只按保存身份、广播服务和名称选择候选设备。
2. 未绑定设备建立只读发现连接；交换 MTU 并完整发现 GATT，不发起安全过程。
3. 同时发现 WLAN Service `F37F568F-9071-445D-A938-5441F2E82399` 与 Network Type Characteristic `9111CDD0-9F01-45C4-A2D4-E09E8FB0424D` 时，识别为 `Gr3Family`。
4. GR IV 必须同时具备 Camera/Operation Mode、Shooting/Flavor/Operation Request、Control Service，以及多个已验证固定 Handle 的 GATT 结构；识别阶段不读取 `0x00EB` 的值。
5. GR III 与 GR IV 证据同时成立时返回 `Unknown`，不尝试任一协议写入。
6. 发现后断开、清理 Client，重建 NimBLE Stack 并应用对应 Security Profile，再进行正式配对连接。
7. 已绑定设备直接使用 NVS 保存的 generation/security profile，不在每次启动时重新识别。
8. 无法识别时为 `Unknown`，只允许诊断、断开和重新扫描，禁止 WLAN、Power 和快门写入。

Profile 定义和纯逻辑检测位于 `src/camera_protocol_profile.*`。`IRicohBleProtocol` 由 `Gr3BleProtocol` 和 `Gr4LegacyBleProtocol` 分别实现，路由器在正式连接前只选择一个实例；`ricoh_ble_client.*` 只执行该实例声明的传输计划，不用失败回退识别代际。上层 `BleCameraService` 不知道具体 Handle/UUID。

## 共用 Camera / Shooting UUID

| 名称 | UUID |
| --- | --- |
| Info Service | `9A5ED1C5-74CC-4C50-B5B6-66A48E7CCFF1` |
| Camera Service | `4B445988-CAA0-4DD3-941D-37B4F52ACA86` |
| Camera Power | `B58CE84C-0666-4DE9-BEC8-2D27B27B3211` |
| Operation Mode | `1452335A-EC7F-4877-B8AB-0F72E18BB295` |
| Shooting Service | `9F00F387-8345-4BBC-8B92-B87B52E3091A` |
| Shooting Flavor | `B29E6DE3-1AEC-48C1-9D05-02CEA57CE664` |
| Operation Request | `559644B8-E0BC-4011-929B-5CF9199851E7` |

Operation Mode 映射：`0x00=CAPTURE`、`0x01=PLAYBACK`、`0x02=BLE_STARTUP`、`0x03=OTHER`、`0x04=POWER_OFF_TRANSFER`，其他值为 `UNKNOWN`。

快门复用同一套 Shooting Service：Shooting Flavor 写 `0x00`；Operation Request 写 `{0x01,0x01}`（Start + AF）或 `{0x01,0x00}`（Start + No AF）。Profile 必须声明支持 BLE shutter，Unknown/GR II 不允许写。

## GR III Family UUID 协议

正式访问只使用 Service UUID + Characteristic UUID。表中的 GR IIIx Handle 仅用于实机日志对照，不是跨型号契约。

| 功能 | UUID | 数据 | GR IIIx 参考 Handle |
| --- | --- | --- | ---: |
| WLAN Service | `F37F568F-9071-445D-A938-5441F2E82399` | WLAN 配置服务 | — |
| Network Type | `9111CDD0-9F01-45C4-A2D4-E09E8FB0424D` | `0=Off`、`1=AP` | `0x00F0` |
| SSID | `90638E5A-E77D-409D-B550-78F7E1CA5AB4` | UTF-8，可读 | `0x00F3` |
| Passphrase | `0F38279C-FE9E-461B-8596-81287E8C9A81` | UTF-8，可读、禁止输出明文 | `0x00F5` |
| Channel | `51DE6EBC-0F22-4357-87E4-B1FA1D385AB8` | `0=Auto`、`1..11` | `0x00F7` |
| Camera Power | `B58CE84C-0666-4DE9-BEC8-2D27B27B3211` | `0=Off`、`1=On`、`2=Sleep` | `0x00BC` |

GR III Family WLAN 写入必须同时满足：

```text
BLE encrypted AND authenticated
AND detected profile == GR3_FAMILY
AND a fresh Operation Mode read succeeded
AND Operation Mode == CAPTURE
```

`PLAYBACK`、`BLE_STARTUP`、`POWER_OFF_TRANSFER`、`OTHER`、`UNKNOWN` 或认证状态不明时都禁止写 Network Type。主状态机和 BLE 客户端各做一次门控。Network Type 使用带响应写入并检查返回结果。

GR III 凭据只要求非空 SSID + Passphrase；Channel `0` 或 `1..11` 有效。Security、Frequency、BSSID 特征不存在不是错误。首次 Wi-Fi 连接不依赖 BSSID；连接成功后由 ESP32 读取实际 BSSID/Channel，再写入 NVS 快速缓存。

## GR III 首次配对与 Bond 失效

共享安全设置为 Bonding + MITM + Secure Connections、`KEYBOARD_DISPLAY`，双方 key distribution 使用 `ENC | ID | SIGN`，Own Address 优先 Public。解析出的 `PUBLIC_ID`/`RANDOM_ID` 在连接前分别归一化为 `PUBLIC`/`RANDOM`。

未绑定且发现 GR III WLAN Service 时的顺序：

1. 交换 MTU并完整发现服务。
2. 对受保护 SSID 做一次未认证读取，触发相机等待配对。
3. 等待约 300 ms 后调用安全连接。
4. 相机显示六位码；StickS3 Button A 短按改数字、Button B 短按移动下一位、Button A 长按提交，Button B 长按取消，45 秒窗口内完成。串口六位数字输入仅为调试后备。
5. 只有 encrypted + authenticated + bonded 后才读取凭据或写控制特征。

配对界面和普通日志都不输出完整 Passkey。普通日志只输出 Passphrase 是否存在，不输出其内容。

Locked 状态出现新的 Passkey Entry、Numeric Comparison 或明确的 ATT Insufficient Authentication/Authorization/Encryption 时进入 `BondInvalid`。固件不删除 Bond、不覆盖 Profile、不连接其他相机，也不自动重新配对；只有用户先从相机端删除 StickS3，再长按 Button B，才会清 Profile、Wi-Fi 缓存和全部 NimBLE Bond。

Profile v4 使用单个 `profile_v4` NVS 字符串保存完整权威快照；旧的逐字段 Key 仅作为向后兼容镜像。权威快照写成功后才更新镜像，避免配对未完成或多 Key 写入中途掉电时覆盖原单相机 Profile。

## GR IV 固定 Handle 协议

GR IV Profile 保留原项目路径，不改成 GR III UUID，也不通过失败回退跨代写入。

| 功能 | Handle / Value |
| --- | --- |
| WLAN Power | `0x0135`，写 `0x01` |
| WLAN SSID | `0x0138` |
| WLAN Passphrase | `0x013A` |
| WLAN Security | `0x013C` |
| WLAN Frequency | `0x013E` |
| WLAN BSSID | `0x0140` |
| Power State | `0x00EB`；`0x01=On`、`0x00=Off` |
| Power State CCCD | `0x00EC`，写 `0x01 0x00` |

GR IV 的 Power State、Operation Mode、WLAN 缓存优先级、LiveView 和快门流程保持原有行为。`Gr4LegacyPairingStrategy` 冻结为 `DISPLAY_YESNO`、`ENC|ID`、固定 `123456`、`RPA_PUBLIC_DEFAULT`；GR III 的 `KEYBOARD_DISPLAY`、`ENC|ID|SIGN`、Public Own Address 仅在独立 Stack Profile 中生效。

## 休眠与重新探测

GR III Family 在 `BLE_STARTUP`/`POWER_OFF_TRANSFER` 或安全门控失败时断开当前连接。因为同一连接内 Operation Mode 可能保持陈旧，后续探测必须建立新连接。Profile 的最小自动只读探测间隔为 8 秒；探测只允许发现服务并读取 Power/Operation Mode，不允许 WLAN、Power 或快门写入。Button A 可请求即时新连接，但不能绕过 `CAPTURE` 门控。

## 可关闭诊断

`RICOH_BLE_GATT_DIAGNOSTICS=0` 默认关闭。设为 `1` 时输出 Service/Characteristic UUID、Handle 和 read/write/notify 属性，但故意不读取或输出特征值。常规日志还记录：Profile 识别来源、安全状态（bonded/encrypted/authenticated/key size）、ATT 错误名称，以及原始 disconnect reason。不得在诊断中添加 Passphrase、Passkey 或 NVS 明文凭据。

## TODO_UNVERIFIED

- 当前双协议重构分支尚未在 GR III、GR IIIx 及其 HDF 版本上执行完整实机矩阵；旧分支的 GR IIIx 用户确认只能作为历史证据。
- 双 Security Profile 与首次两阶段发现尚未对 GR IV/GR IV HDF 执行本次改动后的完整回归。
- GR III Family Power Notify 支持、不同相机固件的 UUID/属性一致性、`OTHER` 和 `PLAYBACK` 的精确语义仍需实机日志。
- `0x213`、`0x215`、`0x216` 在不同代际/固件中的精确定义仍需扩充证据；自动删除 Bond 不得仅依据这些普通断连值。

## 后续修改约束

- 新 UUID/Handle 必须有抓包、外部逆向资料或实机日志证据。
- 任何有副作用的 BLE 写入必须先过 Profile 和状态门控。
- BLE 回调只做轻量状态记录，禁止耗时操作和秘密输出。
