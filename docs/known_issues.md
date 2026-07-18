# Known Issues and Open Questions

## 支持与证据限制

1. **GR III Family 尚未在本分支实机验证**：设备端 Passkey、UUID WLAN、凭据、Capture 门控、Bond 自愈已经实现并通过 Native/编译验证，但 16 项实机矩阵均待执行。
2. **GR IV 本次回归尚未执行**：原项目已在 GR IV 与 GR IV HDF 验证完整链路；本次把共享 SMP IO Capability 改为 `KEYBOARD_DISPLAY`，因此首次配对、旧 Bond 重连、清 Bond 重配等必须复测，不能用历史结果替代。
3. **GR III HDF / GR IIIx HDF 只有实验性支持**：没有独立实机 GATT 证据，不能仅按产品名称假设与非 HDF 版本相同。
4. **GR II 暂不支持**：只有能力模型占位；没有推测 UUID、Handle、Wi-Fi 或快门实现。

## 运行行为与待调优点

1. GR IIIx 外部实机记录显示，休眠时建立的连接可能一直返回 `BLE_STARTUP`。当前策略断开并以不短于 8 秒的间隔建立只读探测连接；安全但可能带来约 9 秒自动发现延迟。
2. 自动删除旧 Bond 只在连续 3 次明确安全恢复失败或 2 次明确 ATT 认证不足后发生。不同相机固件的错误码需实机确认，普通超时/断连不会触发删除。
3. Passkey 输入只有 Button A：短按循环数字、长按确认。功能已做纯逻辑测试，但实际按键手感、45 秒窗口和用户误操作率需要真机/设备测试。
4. GR III Power Notify 通过 UUID subscribe 尝试，失败不会绕过 Power/Operation Mode 读取门控；不同固件是否提供 notify 尚不确定。
5. GR III 首次参数没有已验证的 Security/Frequency/BSSID 特征。当前接受 SSID+Passphrase、Channel 可选，连接成功后回采实际 BSSID/Channel；需实机验证相机在自动 Channel 下的 AP 启动延迟。
6. Wi-Fi 缓存快速连接首次失败可能是相机 AP 尚未准备好；流程会回退到 BLE 重新读取参数。
7. Windows PlatformIO monitor 的 `ClearCommError failed` 属于历史串口监视/重连问题，不等同于固件崩溃。

## 安全不变量

- Unknown Profile 不得执行 WLAN、Power、Shutter 写入。
- GR III 只允许通过 UUID 访问 WLAN；不得新增“先写 GR IV Handle 再回退”的代码。
- GR III 只有 authenticated encryption + fresh `CAPTURE` 才能写 Network Type `0x01`。
- 普通日志、错误与 GATT dump 不得包含完整 Passphrase、Passkey 或 NVS 明文凭据。
- 修复连接速度问题不能以高频骚扰休眠相机或跳过 Operation Mode 为代价。

## TODO_UNVERIFIED

- 当前实机稳定 FPS、长时间内存碎片和 BLE/Wi-Fi 共存稳定性。
- `0x213` / `0x215` / `0x216` 在不同代际和固件上的精确定义。
- GR III `PLAYBACK` / `OTHER` 是否可安全开 WLAN；当前一律拒绝。
- 相机端删除配对后，各代际是否都能按当前阈值自动清除旧 Bond 并重新配对。

实机问题先按 `logs/issue_template.md` 保存机型、相机固件、StickS3 commit 和去密后的关键日志；不得把“理论可用”升级为“已实机验证”。
