# GR IIIx 配对后 StickS3 自动关机调查（2026-10-11）

## 实机证据

用户确认：只有 StickS3 黑屏，GR IIIx 仍可拍摄。
串口采用不切换 DTR/RTS 的读取方式，记录见同目录 `2026-10-11-gr3x-pairing-sleep.log`。

- BLE 已连接，进入 WiFi open 前的电源检查。
- Camera Power 返回 `0x01`（固件解读为 ON）。
- Operation Mode 返回 `0x02`（固件解读为 BLE_STARTUP）。
- 固件进入 CAMERA_POWER_OFF / Camera Sleep Guard，主动断开 BLE，停止自动扫描。
- 30 秒后打印 `Power: CameraSleep idle timeout reached`、`Power: shutdown requested`，随后 USB 串口消失。
- reason=533 是此分支传入的原因值，不能作为相机真实发送关机事件的证据；之后 0x216 是本地主动断开。

## 结论与限制

本次黑屏由待机保护的 30 秒关机策略触发，发生在 Wi-Fi / LiveView 启动之前，与 maxfps=7 尚无因果证据。
尚不能确定 0x02 是配对期间临时状态、连接内残留状态，还是 GR IIIx 枚举解释不适用。已有 GR IIIx v1.60 分析只恢复特征注册，明确未逐项恢复枚举，因此不能仅凭 GR IV 的解释放开 WLAN 写入。
当前实现 `cameraSleepGuardBlocksFlow()` 会一直阻止自动流程；文档所述 GR III 每 8 秒新连接探测并未由这个路径实现。

## 下一步实机验证

相机退出配对菜单并回到可拍摄画面后重新启动 StickS3，记录新连接的 Power / Operation Mode，并补充相机固件版本。
本轮尚未修改或烧录固件。

## 后续反馈与修改

用户确认 GR IIIx 固件 V1.50，退出配对菜单、返回拍摄画面后再次连接仍进入保护态。当前目录用于静态对照的是 V1.60，并非同版本样本。

已修改：GR III 系列在电源明确 ON、但模式/安全门控失败时仍阻止 WLAN，停止把该事件作为相机关机证据，不启动 StickS3 的 30 秒自动关机计时。真实电源事件和 GR IV 原有策略保留。Button A 仍可重新连接检查。此修改只处理误关机，不声称解决 V1.50 的连接失败。

验证：native 100 项测试通过；m5stack-sticks3 编译通过。等待设备重新开机以烧录和验证。

## V1.60 静态枚举复核

通过本地 LLVM 对 webapid 反汇编，operation_mode::GetCurrentValue 位于 0xe7644，读取属性 100，映射表位于 0x11227c。初始化函数 0xe76fc 的字符串/数值对为 capture=0、playback=1、bleStartup=2、other=3、powerOffTransfer=4（字符串地址分别 0x1023d0、0x1048d8、0x1048e4、0x1048f0、0xff974）。bled 0x5ee14 读取属性后直接返回低字节。此证据支持 V1.60 的现有枚举，但不能证明 V1.50 的运行状态含义完全一致。

测试版已成功烧录并校验哈希；串口正在观察。UI 对未就绪保护投影为错误提示，避免休眠动画和背光变暗。

## 烧录后实机进展

用户确认已显示遥控界面或相机画面。串口捕获 Power=0x01、OperationMode=0x00 CAPTURE，NETWORK_TYPE_UUID 启动 WLAN 成功，Wi-Fi 连接 2009ms，信道6、RSSI -48，LiveView maxfps=7 打开384ms，画布创建成功、720×480 图像。这说明 V1.50 同样可报告 CAPTURE；不能把此前 0x02 简单视作该版本的拍摄枚举。此次成功未走 auto_power_off=0 分支，故尚不能视作误判分支的实机回归通过。

120秒串口观察结束，日志见 `2026-10-11-gr3x-after-guard-fix.log`。捕获多次快门 START，但不据此单独断言拍照完成。横竖屏切换后 WLAN 重启一次，NO_AP_FOUND 后同预算重试成功，连接 RSSI=-83；随后预览恢复，采样窗口约2.3–5.8 FPS，记录的 MJPEG dropped=0。未见30秒关机、panic或LiveView看门狗恢复。距离及快门完成结果等待用户反馈。开头曾出现 M5PM1 button read err=-7，程序退回 M5Unified；USB供电状态多次变动，暂未确认原因。

## 用户实测确认

用户确认：A键可以实际拍摄，功能正常，约3米距离也正常。该结论适用于本次 GR IIIx V1.50 与已烧录测试版；约5米距离尚未验证。此前 Power=ON / Mode=0x02 的触发原因及未就绪分支的实机回归仍未完成，不将本次正常CAPTURE连接视作全部问题已彻底修复。
