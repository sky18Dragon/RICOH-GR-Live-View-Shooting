# Test Plan

## 文档-only 任务

```powershell
git diff --stat
git diff -- AGENTS.md docs logs
```

本类任务不需要 `platformio run`，除非用户明确要求。

## 自动化验证

`platformio.ini` 配置了 `env:native`，包含：

- `ble_reconnect_policy.cpp`
- `ble_pairing_policy.cpp`
- `camera_identity.cpp`
- `camera_profile_schema.cpp`
- `camera_protocol_profile.cpp`
- `gr2_provisioning_logic.cpp`
- `gr2_shutter_policy.cpp`
- `mjpeg_stream.cpp`
- `ui/ButtonInput.cpp`
- `ui/OrientationTracker.cpp`
- `ui/UiAnimator.cpp`
- `ui/UiCoordinator.cpp`
- `test/test_native/test_native_logic.cpp`

建议命令：

```powershell
platformio test -e native
platformio run -e m5stack-sticks3
```

Native 测试必须覆盖：

- GR III/IV/冲突/Unknown 检测、Unknown 副作用拒绝、GR II ManualOnly、GR3/GR4 Security Profile 常量。
- GR II 官方 HTTP 快门端点、单次响应与 fallback 决策，以及配网表单边界、扫描缓存排序/转义和凭据延迟接管。
- 地址类型归一化、六位 Passkey 完成/重置/超时、Locked/BondInvalid 单相机策略、安全恢复与 ATT 认证失败计数、普通错误不删 Bond。
- Operation Mode 安全决策、GR III 可选 Channel 凭据、NVS v3→GR4 Legacy 升级和 v4 往返。
- 姿态门控状态机、MJPEG/身份/监督器、状态到 UI 场景映射、优先级、姿态稳定/滞回、动画和 `millis()` 溢出、Button A 单次 Shoot、Button B 单击/双击、延迟显示的重置进度/取消/单次重置，以及快门 overlay 生命周期。

`src/ui/NativeBuildMain.cpp` 只在 `RVF_NATIVE_BUILD && !PIO_UNIT_TESTING` 时提供 smoke `main()`，使 `platformio run -e native` 可验证全部纯逻辑对象的独立链接；Unity 测试和 StickS3 固件均不会使用该入口。

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

## GR II 实机矩阵

状态：**已通过真实 GR II 相机验证**。以下矩阵继续作为发布回归基线；本次确认未附相机固件版本、逐项日志和性能数据，后续记录时应补齐这些证据。

1. 相机机身手动开启 Wi-Fi，在引导中选择 GR II；期望 StickS3 先在纯 STA 模式扫描并缓存网络，再以纯 AP 模式启动 `GR-II-Setup-xxxx`、显示 `http://192.168.4.1`，无 BLE 扫描/配对日志。
2. 手机使用密码 `GR288888` 加入配网热点；确认串口出现 `connected phones=1` 且不输出密码，Android/iOS 能立即打开页面；HTTP 请求处理期间不得调用 `scanNetworks`，RICOH 网络应优先显示，缺失时可手动输入 SSID。
3. 提交后确认 HTTP 成功页有机会完成加载，随后配网热点关闭、手机断开；凭据写入 NVS，StickS3 连接 `RICOH_*` AP，串口不输出密码。
4. 请求 `GET /v1/liveview`；记录 HTTP 状态、MJPEG boundary、分辨率、稳定 FPS 和 stall。
5. 横持显示实时取景；竖持至少 30 秒不绘制 JPEG，但流与 Button A HTTP 快门保持可用。
6. Button A 短按和长按各 10 次；每次只生成一张照片。
7. 记录普通 `/v1/camera/shoot?af=camera` 响应；若返回 `Precondition Failed`，确认 `shoot/start` + `shoot/finish` fallback 成功且不会留下按键锁定状态。
8. 关闭相机 AP、再手动重开；期望看门狗仅恢复 Wi-Fi/HTTP，不初始化 BLE stack 或发送 GATT 写入。
9. 重启 StickS3，确认直接复用 NVS 凭据；长按 B 清除 Profile 后重新选择 GR II，确认重新进入配网页面、不初始化 BLE，也不调用 `/v1/device/finish`。
10. 在 GR II 固件 v1.10 与可取得的最新版分别执行上述矩阵。

## GR IV / GR IV HDF 回归矩阵

1. **首次配对**
   - 擦除 NVS/flash 后启动。
   - 期望：只读发现后重建为 `GR4_LEGACY`，扫描、配对、安全连接、保存 BLE 身份、打开 Wi-Fi、进入 LiveView；特别确认 GR3 的 `KEYBOARD_DISPLAY` 没有改变 GR4 Numeric Comparison UX。
2. **已配对直连**
   - StickS3 重启，相机处于工作状态。
   - 期望：优先直连保存的 BLE 地址，Wi-Fi cache 可短超时尝试，失败后 fresh BLE 参数回退。
3. **相机关机/待机保护**
   - 相机关机或进入 BLE_STARTUP / POWER_OFF_TRANSFER。
   - 期望：不发送 Wi-Fi ON，进入 CAMERA_SLEEP_GUARD。
4. **手动唤醒**
   - Guard 冷却结束后按 Button A。
   - 期望：重建 BLE stack，手动唤醒流程启动。
5. **Button A AF 快门**
   - LiveView 正常运行时按 Button A。
   - 期望：日志包含 `BLE: Ricoh shutter OperationRequest START param=1 autofocus=1`。
6. **姿态与 Canvas**
   - 横竖切换 20 次，记录每次 `UI Canvas: switch/ready` 的 heap 和 PSRAM。
   - 期望：无崩溃、花屏、持续内存下降；临界角度不会反复旋转。
7. **竖屏流保持**
   - LiveView 运行后竖握至少 30 秒，再恢复横握。
   - 期望：竖屏遥控器 UI 不被 JPEG 覆盖，Supervisor 不报 stall，横屏后画面继续。
8. **Button A 输入时序**
   - 分别短按和按住超过 300 ms 后松开。
   - 期望：每次完整操作只有一次 `Shoot`/一张照片；长按期间只有 UI/声音反馈。
9. **Button B 重置进度**
   - 先在 3 秒前释放，再完整按满 3 秒。
   - 期望：第一次不重置，第二次只重置一次，横竖屏均显示进度。
10. **休眠与反馈降级**
    - 验证低亮过渡、A 唤醒提亮、声音；再模拟 IMU/扬声器不可用。
    - 期望：失败时静默降级，不影响连接、预览和拍摄。

11. **竖屏启动的 Wi-Fi 门控**
    - 保持 StickS3 竖屏启动，等待 BLE 和相机 Wi-Fi 参数准备完成。
    - 期望：状态停在 `WIFI_CREDENTIALS_READY`，日志有 `WiFi cache: saved`，StickS3 不连接相机 AP，也不发起 HTTP/LiveView。
12. **竖转横恢复流程**
    - 从 `WIFI_CREDENTIALS_READY` 转为横屏。
    - 期望：不重新 BLE 扫描，直接进入 `CONNECTING_WIFI -> PREVIEW_STARTING -> PREVIEW_RUNNING`；首帧显示后才刷新 `/v1/props`。
13. **横转竖断开流程**
     - LiveView 运行时转为竖屏。
     - 期望：关闭 LiveView、断开相机 Wi-Fi、回到 `WIFI_CREDENTIALS_READY`，BLE 保持连接；重新横屏可再次恢复。
14. **Bond 失效**
     - 从相机端删除 StickS3 配对记录后重连。
     - 期望：进入 BondInvalid，不自动删除本地 Bond、不自动重新配对；长按 B 前不覆盖 Profile。
15. **相机与网络恢复**
     - 分别重启相机、断开 Wi-Fi/LiveView 后等待恢复。
     - 期望：自动重连保存身份并恢复预览，无跨协议 UUID/Handle 回退。

除 Profile 识别与安全状态日志外，GR IV 应继续走 `FIXED_HANDLE`，不得访问 GR III WLAN UUID。

## GR III / GR IIIx 实机矩阵

1. 相机菜单进入配对模式；StickS3 扫描并识别 `GR3_FAMILY`。
2. 相机显示随机六位码。
3. 不接电脑，使用 A 改数字、B 移位、长按 A 提交并完成 Bond。
4. 重启 StickS3 后无需再次输入 Passkey。
5. 读取 SSID/Passphrase/Channel，日志只显示 present 标记。
6. `CAPTURE` 下通过 Network Type UUID 开 AP 并连接 Wi-Fi。
7. `/v1/props` 成功。
8. `/v1/liveview` 持续输出画面，记录 FPS/stall/heap/PSRAM。
9. Button A BLE AF 快门可用且不破坏 LiveView。
10. `BLE_STARTUP`/休眠时不写 WLAN、不伸出镜头。
11. 用户打开相机后，Button A 建立新连接并在 `CAPTURE` 后继续。
12. 自动只读重试间隔不短于 8 秒，不出现 2 秒高频骚扰。
13. 相机端删除配对后进入 BondInvalid；长按 B 前不得清 Bond或重新显示六位码。
14. Button B 长按清 Profile/Bond/cache 后重新配对。
15. StickS3 与相机交替重启后恢复。
16. Wi-Fi/LiveView 断线后安全恢复。

## 安全负向用例

- Unknown Profile：尝试 WLAN、Power、Shutter 动作均被拒绝。
- GR III 的 `PLAYBACK`、`BLE_STARTUP`、`POWER_OFF_TRANSFER`、`OTHER`、`UNKNOWN`：Network Type 不得被写入。
- 只有 encrypted、没有 authenticated：受保护读取/写入不得继续；Locked 状态进入 BondInvalid，但不得自动删除 Bond。
- 普通超时、`0x208`、资源不足：不得删除有效 Bond。
- GATT diagnostics：只含表结构和安全元数据，不含任何特征值或秘密。

## Preview 性能记录

记录 FPS、dropped frames、`JpegDecoder::lastDecodeMs()`、Wi-Fi RSSI、LiveView stall/重连次数、横竖 Canvas 切换前后 heap/PSRAM 可用量（串口 `UI Canvas:` 日志）。优化必须提供同一机型、同一场景的前后对比。
