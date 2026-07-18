# Camera Power State Policy

## 目标

避免 StickS3 在相机处于关机/休眠时启动 WLAN 或让镜头意外伸出。策略由运行时 Profile 决定，优先级高于自动重连和 LiveView 恢复。

## 代际读法

| Profile | Power | Operation Mode | WLAN 写入 |
| --- | --- | --- | --- |
| GR IV | 固定 Handle `0x00EB`，CCCD `0x00EC` | 共用 UUID | 保留原 fixed-handle 路径与既有门控 |
| GR III | Camera Power UUID；`0=Off`、`1=On`、`2=Sleep` | 共用 UUID | 仅 authenticated + fresh `CAPTURE`，Network Type UUID 写 `0x01` |
| Unknown / GR II | 不执行有副作用探测 | 不进入在线控制 | 禁止 |

Operation Mode：`0x00=CAPTURE`、`0x01=PLAYBACK`、`0x02=BLE_STARTUP`、`0x03=OTHER`、`0x04=POWER_OFF_TRANSFER`。GR III 只有 `CAPTURE` 可开 WLAN；即使 Power 为 `0x01`，其他模式也不满足条件。

## Wi-Fi ON 前置条件

1. Camera Guard 不得阻止本次新连接探测。
2. BLE 已连接并识别出已知 Profile。
3. 按 Profile 读取 Power State；Power Off/Sleep/Unknown 进入 Guard。
4. 读取 Operation Mode。
5. GR III 要求连接 encrypted + authenticated、读取成功且为 `CAPTURE`；主状态机和 BLE 客户端双重检查。
6. GR IV 保留既有 Operation Mode 行为；`BLE_STARTUP` / `POWER_OFF_TRANSFER` 会阻止自动 WLAN。
7. 尝试订阅 Power Notify，并等待 `RICOH_BLE_POWER_NOTIFY_SETTLE_MS` 消费立即到来的关机通知。
8. 所有检查通过后，才调用 Profile 对应 WLAN 动作；绝不跨代失败回退写入。

## Camera Sleep Guard 与重新探测

进入 Guard 时关闭 LiveView、断开 Wi-Fi/BLE，并记录断连/电源原因。当前连接不再执行写操作。

GR III Family 的 Profile 设置 `standbyModeRequiresFreshReconnect=true`、最小探测间隔 8 秒。原因是休眠连接上的 Operation Mode 可能保持 `BLE_STARTUP`；必须断开并建立新连接重新采样。自动探测只允许：

- 扫描与连接；
- 服务/特征发现和 Profile 校验；
- Security/Power/Operation Mode 只读检查。

自动探测禁止 WLAN、Power 和 Shutter 写入。若仍非 `CAPTURE`，再次断开并等待下一间隔。Button A 可请求即时新连接，但依然不能绕过 Profile、认证和 fresh `CAPTURE` 门控。避免约 2 秒一次的高频重连；外部 GR IIIx 记录显示其可能造成 `0x216` 连接失败并妨碍相机启动。

GR IV 的既有固定 Handle、Power Notify、缓存连接与恢复策略不因 GR III Profile 改变。

## Bond 与关机断连

- Power Notify `0x00`（GR III 还接受 `0x02` Sleep）进入 Guard。
- `0x213` / `0x215` 仍作为远端关机/用户断连候选进入 Guard，但精确语义需更多固件证据。
- `0x216` 或普通临时断连不作为删除 Bond 的充分条件。
- 只有明确的安全恢复/ATT 认证错误达到阈值才删除旧 Bond。

## 必须保持的安全规则

- Camera Power `On` 不是充分条件。
- Unknown Profile 默认拒绝所有有副作用的 BLE 写入。
- GR III `PLAYBACK`、`BLE_STARTUP`、`POWER_OFF_TRANSFER`、`OTHER`、`UNKNOWN` 都拒绝 WLAN。
- 自动恢复不得把只读探测升级为唤醒写入。
- Power Notify settle 不得被连接速度优化移除。
- 日志不得包含 Passphrase、Passkey 或 NVS 明文凭据。

## TODO_UNVERIFIED

- 当前分支尚未完成 GR III/IIIx Guard 与 GR IV/IV HDF 防误唤醒实机回归。
- GR III Power Notify 在不同固件上的 notify 属性和值需验证。
- `PLAYBACK` 是否安全允许 WLAN 只有实机证明后才能单独放开。
