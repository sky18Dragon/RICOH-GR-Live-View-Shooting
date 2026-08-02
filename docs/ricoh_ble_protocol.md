# RICOH BLE Protocol Notes

## 适用范围

以下信息从当前代码、README 和配置常量提取。GR IV 固定 handle 路径已在 RICOH GR IV / GR IV HDF 上实测，GR III UUID 路径已在 GR III HDF 上实测。GR II 当前不可用。

## BLE 服务 UUID

从 `src/config.h` 确认：

| 名称 | UUID |
| --- | --- |
| Info Service | `9A5ED1C5-74CC-4C50-B5B6-66A48E7CCFF1` |
| Camera Service | `4B445988-CAA0-4DD3-941D-37B4F52ACA86` |
| Operation Mode | `1452335A-EC7F-4877-B8AB-0F72E18BB295` |
| Shooting Service | `9F00F387-8345-4BBC-8B92-B87B52E3091A` |
| Shooting Flavor | `B29E6DE3-1AEC-48C1-9D05-02CEA57CE664` |
| Operation Request | `559644B8-E0BC-4011-929B-5CF9199851E7` |
| Control Service | `0F291746-0C80-4726-87A7-3C501FD3B4B6` |

## GR III WLAN UUID

| 功能 | UUID |
| --- | --- |
| WLAN Service | `F37F568F-9071-445D-A938-5441F2E82399` |
| Network Type / AP ON | `9111CDD0-9F01-45C4-A2D4-E09E8FB0424D` |
| SSID | `90638E5A-E77D-409D-B550-78F7E1CA5AB4` |
| Passphrase | `0F38279C-FE9E-461B-8596-81287E8C9A81` |
| Channel | `51DE6EBC-0F22-4357-87E4-B1FA1D385AB8` |
| Camera Power | `B58CE84C-0666-4DE9-BEC8-2D27B27B3211` |

## GR IV WLAN / Power handles

从 `src/config.h` 确认：

| 功能 | Handle / Value | 代码含义 |
| --- | --- | --- |
| WLAN Power | `0x0135` | 写 `0x01` 请求打开相机 Wi-Fi |
| WLAN ON value | `0x01` | `RICOH_BLE_GR4_WLAN_ON_VALUE` |
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
- `advertisesAnyRicohService()` 匹配 Info、Camera、Shooting、Control 或 GR III WLAN 服务。
- `nameLooksLikeRicoh()` 接受 `GR`、`GR_`、包含 `RICOH`、`PENTAX`、`GRIII`、`GR III` 等名称特征。
- 连接后调用 `secureConnection(true)` 等待加密。GR III 首次配对使用相机显示的六位 passkey，并要求 authenticated bond。
- Security wait 默认来自 `RICOH_BLE_SECURITY_WAIT_MS`，bonded 直连使用 `RICOH_BLE_BONDED_SECURITY_WAIT_MS`。

## Wi-Fi 参数读取

`waitForWifiCredentials()` 按检测到的协议选择读取方式。GR IV 使用固定 handles；GR III 按 UUID 读取 SSID、passphrase 和 channel。每次读取后有短 `delay(20)` 和 `yield()`，未得到 valid credentials 时按 `RICOH_BLE_WIFI_CREDENTIAL_POLL_MS` 延迟重试。

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
- 标准 GR III 与 GR IIIx 是否与已验证的 GR III HDF 完全一致。
- `0x03 OTHER` 的具体相机语义。
- GR III HDF 的 Button A 快门发送仍需独立日志确认。

## 维护注意事项

- 不得新增或修改 UUID/handle，除非有抓包、官方资料或实机日志证据。
- 任何 BLE 重连优化不得绕过 `ensureCameraPowerReadyForWifi()`。
- BLE 回调只做轻量状态记录，禁止耗时操作。
