# RICOH BLE Protocol Notes

## 适用范围

以下信息从当前代码、README 和 GR IV Profile 提取。协议以 RICOH GR IV HDF 实测为准；GR IIIx 当前只有枚举预留，没有可连接 Profile、实际 GATT Handle、Passkey 输入 UI、WLAN 或 LiveView 实现。

## BLE 服务 UUID

UUID 的唯一生产代码来源是 `src/protocol/profiles/Gr4ProtocolProfile.cpp`：

| 名称 | UUID |
| --- | --- |
| Info Service | `9A5ED1C5-74CC-4C50-B5B6-66A48E7CCFF1` |
| Camera Service | `4B445988-CAA0-4DD3-941D-37B4F52ACA86` |
| Operation Mode | `1452335A-EC7F-4877-B8AB-0F72E18BB295` |
| Shooting Service | `9F00F387-8345-4BBC-8B92-B87B52E3091A` |
| Shooting Flavor | `B29E6DE3-1AEC-48C1-9D05-02CEA57CE664` |
| Operation Request | `559644B8-E0BC-4011-929B-5CF9199851E7` |
| Control Service | `0F291746-0C80-4726-87A7-3C501FD3B4B6` |

## GR IV WLAN / Power handles

Handle 和协议 Value 的唯一生产代码来源是 `src/protocol/profiles/Gr4ProtocolProfile.cpp`：

| 功能 | Handle / Value | 代码含义 |
| --- | --- | --- |
| WLAN Power | `0x0135` | 写 `0x01` 请求打开相机 Wi-Fi |
| WLAN ON value | `0x01` | Profile 的 `wlanEnableValue` |
| WLAN SSID | `0x0138` | 读取 SSID |
| WLAN Passphrase | `0x013A` | 读取密码 |
| WLAN Security | `0x013C` | 读取安全类型 |
| WLAN Frequency | `0x013E` | 读取频率，并推导信道 |
| WLAN BSSID | `0x0140` | 读取或辅助解析 BSSID |
| Power State | `0x00EB` | 读取/通知相机电源状态 |
| Power State CCCD | `0x00EC` | 写 `0x01 0x00` 订阅通知 |
| Power ON value | `0x01` | 代码映射为 `RicohCameraPowerState::On` |
| Power OFF value | `0x00` | 代码映射为 `OffOrShuttingDown` |

## Operation Mode 映射

从 `src/ricoh_ble_client.cpp::readOperationMode()` 确认：

| Value | Mode |
| --- | --- |
| `0x00` | `CAPTURE` |
| `0x01` | `PLAYBACK` |
| `0x02` | `BLE_STARTUP` |
| `0x03` | `OTHER` |
| `0x04` | `POWER_OFF_TRANSFER` |
| other | `UNKNOWN` |

`src/main.cpp::isCameraStandbyOperationMode()` 将 `BLE_STARTUP` 和 `POWER_OFF_TRANSFER` 视为待机/关机相关状态。

## 扫描与连接

从 `src/ricoh_ble_client.cpp` 确认：

- 候选设备通过广告服务或名称判断。
- `CameraDiscoveryRegistry` 从全部已注册且已校验的 Profile 枚举发现 UUID；`advertisesAnyRicohService()`、广告元数据和候选评分不依赖当前活动 Profile。
- 当前 Registry 只注册 GR IV HDF，因此发现集合仍是其 Info/Camera/Shooting/Control 四个服务；此分离不等于自动机型探测。
- `nameLooksLikeRicoh()` 接受 `GR`、`GR_`、包含 `RICOH`、`PENTAX`、`GRIII`、`GR III` 等名称特征。
- 连接后调用 `secureConnection(true)` 等待加密。
- Security wait 默认来自 `RICOH_BLE_SECURITY_WAIT_MS`，bonded 直连使用 `RICOH_BLE_BONDED_SECURITY_WAIT_MS`。
- `ExistingDefault` 保持 `BLE_HS_IO_DISPLAY_YESNO`、固定 Passkey `123456` 和确认流程；`PasskeyEntry` 尚未实现，会明确阻止初始化/连接，不会回退到默认安全模式。

## Profile 解析与生命周期

- `CameraProtocolRegistry::resolve()` 同时记录 `requestedModel`、`resolvedModel` 和 `usedFallback`。GR IIIx、Unknown 和非法值当前都回退到 GR IV HDF。
- Client/Service 只按 `CameraModel` 选择 Registry 内静态 Profile，不接收或保存外部 Profile 引用/裸指针。
- BLE 连接、扫描、连接建立或安全协商期间禁止切换 Model。
- GAP 回调只读取原子机型值，并在回调内解析 Registry 静态 Profile。
- Registry 生产 Profile 必须通过 `CameraProtocolValidator` 的能力/Handle/UUID 一致性校验。
- 连接成功后，持久化 Model 必须无条件等于 Client 的活动 Model；fallback 请求因此会被修正为实际 GR IV Model。

## Wi-Fi 参数读取

只有 `supportsWifiLiveView=true` 且必需 Handle 完整时才允许 WLAN ON 和参数读取。不支持 Wi-Fi 会立即返回明确错误，不会伪装成普通超时。

`waitForWifiCredentials()` 会在超时窗口内轮询 WLAN SSID/PASSPHRASE/SECURITY/FREQUENCY/BSSID handles。每个 handle 读取后有短 `delay(20)` 和 `yield()`；未得到 valid credentials 时按 `RICOH_BLE_WIFI_CREDENTIAL_POLL_MS` 延迟重试。

## 快门控制

从 `src/ricoh_ble_client.cpp::shoot()` 确认：

- Shooting Service：`9F00F387-8345-4BBC-8B92-B87B52E3091A`
- 写 Shooting Flavor：`0x00`，含义为 IMMEDIATE。
- 写 Operation Request：`{0x01, param}`。
- `param=0x01` 表示 autofocus，`param=0x00` 表示 no AF。
- 当前 Button A 调用 `bleCamera.shoot(true)`。

## 相机电源状态判断规则

- Wi-Fi ON 前必须先读 Power State。
- Power State `0x01` 不足以证明相机处于拍摄可用状态；还必须读取 Operation Mode。
- `BLE_STARTUP` / `POWER_OFF_TRANSFER` 下自动流程必须进入 `CAMERA_SLEEP_GUARD`，不得写 WLAN ON。
- Power State `0x00` 通知会触发 guard。
- 断连 reason `0x213` 或 `0x215` 会被视为 power-off/user remote disconnect 候选，并触发 guard（具体含义以当前代码常量为准）。

## TODO_UNVERIFIED

- UUID/handle 是否适用于所有 GR IV 非 HDF 机型。
- GR IIIx / GR III / GR II 的等价协议、handle 和状态值。
- `0x03 OTHER` 的具体相机语义。
- Passkey 的相机侧交互细节；代码中存在 passkey request 和 confirm passkey 流程，但完整 UX 需实机日志确认。

## 后续 Codex 修改代码时必须注意

- 不得新增或修改 UUID/handle，除非有抓包、官方资料或实机日志证据。
- 任何 BLE 重连优化不得绕过 `ensureCameraPowerReadyForWifi()`。
- BLE 回调只做轻量状态记录，禁止耗时操作。
