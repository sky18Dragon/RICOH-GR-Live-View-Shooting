# Test Plan

## 自动化验证

```powershell
platformio test -e native
platformio run -e m5stack-sticks3
```

Native 测试必须覆盖：GR III/IV/Unknown 检测、Unknown 副作用拒绝、GR II ManualOnly、地址类型归一化、六位 Passkey 完成/重置/超时、安全恢复与 ATT 认证失败计数、普通错误不删 Bond、Operation Mode 安全决策、GR III 可选 Channel 凭据、NVS v3 升级和 v4 往返，以及既有 MJPEG/身份/监督器测试。

诊断代码还必须单独编译一次：

```powershell
$env:PLATFORMIO_BUILD_FLAGS='-DRICOH_BLE_GATT_DIAGNOSTICS=1'
platformio run -e m5stack-sticks3
Remove-Item Env:PLATFORMIO_BUILD_FLAGS
```

通过条件：Native 全部成功；发布环境和诊断开关环境均 SUCCESS；`git diff --check` 无错误；源码审查确认没有 Passphrase/Passkey 明文日志。自动化结果不等于实机验证。

## 烧录与记录

```powershell
platformio run --target upload --upload-port COM6
platformio device monitor --port COM6 --baud 115200 --filter time
```

未经设备所有者确认不要覆盖设备。每次实机运行记录：相机型号、相机固件、StickS3 固件 commit、是否清空 NVS、结果、耗时和去密后的关键日志。使用 `docs/gr3_family_test_record.md` 记录 GR III Family。

## GR IV / GR IV HDF 回归矩阵

1. 全新烧录首次配对（特别确认 `KEYBOARD_DISPLAY` 没有改变 Numeric Comparison UX）。
2. StickS3 重启后自动重连。
3. 相机重启后自动重连。
4. 相机关机时不被误唤醒。
5. WLAN 快速连接缓存。
6. LiveView 连续运行并记录 FPS/stall。
7. Button A AF 快门且 LiveView 保持。
8. Button B 长按清配对后重新配对。
9. 相机端删除 StickS3 配对后的自愈。
10. Wi-Fi/LiveView 断线恢复。

除 Profile 识别与安全状态日志外，GR IV 应继续走 `FIXED_HANDLE`，不得访问 GR III WLAN UUID。

## GR III / GR IIIx 实机矩阵

1. 相机菜单进入配对模式；StickS3 扫描并识别 `GR3_FAMILY`。
2. 相机显示随机六位码。
3. 不接电脑，仅用 Button A 成功输入并 Bond。
4. 重启 StickS3 后无需再次输入 Passkey。
5. 读取 SSID/Passphrase/Channel，日志只显示 present 标记。
6. `CAPTURE` 下通过 Network Type UUID 开 AP 并连接 Wi-Fi。
7. `/v1/props` 成功。
8. `/v1/liveview` 持续输出画面，记录 FPS/stall/heap/PSRAM。
9. Button A BLE AF 快门可用且不破坏 LiveView。
10. `BLE_STARTUP`/休眠时不写 WLAN、不伸出镜头。
11. 用户打开相机后，Button A 建立新连接并在 `CAPTURE` 后继续。
12. 自动只读重试间隔不短于 8 秒，不出现 2 秒高频骚扰。
13. 相机端删除配对后，旧 Bond 达阈值被清除并重新显示六位码。
14. Button B 长按清 Profile/Bond/cache 后重新配对。
15. StickS3 与相机交替重启后恢复。
16. Wi-Fi/LiveView 断线后安全恢复。

## 安全负向用例

- Unknown Profile：尝试 WLAN、Power、Shutter 动作均被拒绝。
- GR III 的 `PLAYBACK`、`BLE_STARTUP`、`POWER_OFF_TRANSFER`、`OTHER`、`UNKNOWN`：Network Type 不得被写入。
- 只有 encrypted、没有 authenticated：受保护读取/写入不得继续，达到明确阈值后才删除 Bond。
- 普通超时、`0x208`、资源不足：不得删除有效 Bond。
- GATT diagnostics：只含表结构和安全元数据，不含任何特征值或秘密。

## Preview 性能记录

记录 FPS、dropped frames、`JpegDecoder::lastDecodeMs()`、Wi-Fi RSSI、LiveView stall/重连次数，以及 heap/PSRAM。优化必须提供同一机型、同一场景的前后对比。
