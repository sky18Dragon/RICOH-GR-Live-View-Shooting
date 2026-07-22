# StickS3 UI 与交互设计

## 范围与边界

本次重构只改变表现层、输入时序适配、IMU 姿态识别、声音反馈和 `main.cpp` 中必要的 UI 集成。相机连接和控制仍由现有 `AppController` 与服务层负责。

受保护且不得改变语义的模块：

- `src/ricoh_ble_client.*`
- `src/gr_api.*`
- `src/gr_wifi.*`
- `src/mjpeg_stream.*`
- `src/jpeg_decoder.*`
- `src/services/*`
- `src/supervisor/*`
- `src/app/AppController.*`
- NVS profile、BLE UUID/Characteristic/payload、Wi-Fi/HTTP/MJPEG/JPEG、恢复与休眠保护策略

拍摄入口仍为 `bleCamera.shoot(true)`。UI 的 300 ms 长按阈值只控制光圈收缩和提示音，不产生半按、独立 AF 或第二次拍摄命令。

## 参考原型

用户提供的 HTML 已逐字节归档到：

`docs/ui-reference/StickS3_Interaction_Prototype.html`

HTML 只作为尺寸、颜色和动效节奏参考，不进入固件运行时。网页外壳、鼠标/触摸、演示按钮、渐变占位图和 DOM reflow 不映射到设备。

### 参数映射

| 原型参数 | 固件位置 | 实现 |
| --- | --- | --- |
| 竖屏 135×240 | `UiTheme::kPortraitWidth/Height` | `DisplayUi` rotation 0/2，单 Canvas 135×240 |
| 横屏 240×135 | `UiTheme::kLandscapeWidth/Height` | `DisplayUi` rotation 1/3，单 Canvas 240×135 |
| 黑色背景 | `UiTheme::kBlack` | 所有非 LiveView 场景纯黑底 |
| 绿色 `#4CAF50` | `UiTheme::kGreen` | RGB565 `0x4D6A` |
| 20 px 连接圆点 | `kConnectDotDiameter` | Pairing/Connecting 两实心圆 |
| 2 s 连接融合 | `kConnectMergeMs` | `millis()` 归一化进度 + smoothstep；业务阶段可提高进度下限 |
| 60 px、2 px 光圈 | `kRemoteApertureDiameter`、`kRemoteApertureLineWidth` | 竖屏中央双线圆环 |
| 20 px 中心圆 | `kRemoteCoreDiameter` | 白色实心圆 |
| 300 ms 长按阈值 | `kFocusHoldThresholdMs` | 仅开启 FocusVisual 和提示音 |
| 0.8 倍、300 ms 收缩 | `kRemoteFocusScalePct`、`kFocusTransitionMs` | 60 px 平滑收缩至 48 px并变绿 |
| 300 ms 竖屏闪光 | `kPortraitShutterFlashMs` | RGB565 亮度衰减 overlay |
| 15 px、100 ms 横屏快门框 | `kShutterFrameWidth`、`kLandscapeShutterCurtainMs` | JPEG 之后绘制的最上层四边框 |
| 40 px 重置圆 | `kResetCircleDiameter` | 重置场景中央绿色融合圆 |
| 80%×4 px、底部 20 px | `kResetProgressWidthPct/Height/Bottom` | 与 Button B 同一计时源的连续进度条 |
| 3 s 重置 | `kResetHoldMs` | 达到阈值只产生一次 `ResetPairing` |
| 左右 30 px、300 ms 分裂 | `kResetSplitDistance`、`kResetSplitMs` | 重置后两个红点弹开 |

## 单向 UI 架构

```text
AppController/AppState + 连接只读值 + ButtonEvents + IMU
                         |
                         v
                    UiSnapshot
                         |
                         v
                   UiCoordinator
                         |
                         v
                    UiViewModel
                         |
                         v
                     DisplayUi
```

- `UiSnapshot` 只引用当前稳定字符串，避免每帧复制 `String`。
- `UiCoordinator` 集中维护场景优先级、动画生命周期、方向选择和一次性声音请求。
- `DisplayUi` 只管理 Canvas、背光和绘制，不调用 BLE、Wi-Fi 或 HTTP。
- `OrientationTracker`、`UiAnimator`、`ButtonInput` 和 `UiCoordinator` 是无 Arduino/M5Unified 依赖的纯 C++，纳入 Native 测试。

场景优先级：

```text
Error > ResetPairing > CameraSleep > Pairing/Connecting
      > 横握 LivePreview / 竖握 RemoteReady > Disconnected
```

拍摄是当前场景上的临时 overlay，不替换 `AppState`，也不改变控制器状态迁移。

## 姿态识别

`OrientationTracker` 使用 M5Unified 加速度计的 X/Y 绝对值判断静态方向，不使用陀螺仪角速度。

- 采样间隔：40 ms；
- 一阶低通系数：0.20；
- X/Y 差值滞回：0.18 g；
- 有效平面轴最小值：0.35 g，设备近似平放时维持当前方向；
- 候选方向稳定：500 ms；
- 切换后最短保持：500 ms；
- StickS3 实机 `abs(X) > abs(Y) + 0.18 g` 为竖握；
- StickS3 实机 `abs(Y) > abs(X) + 0.18 g` 为横握。

连接、错误和休眠页保持竖屏。只有预览已经在底层运行时，姿态才在 `RemoteReady` 与 `LivePreview` 之间选择显示模式。IMU 不可用时自动降级为“预览运行时横屏，其余状态竖屏”，不停止业务流程。

方向轴已按 StickS3 实机校准；若后续硬件修订改变 IMU 安装方向，只调整姿态适配，不修改相机流程。

## Canvas 与内存安全

`DisplayUi` 常驻一个 16-bit `M5Canvas`，不会同时保留横竖两份全屏缓冲，也不会每帧创建 Sprite。

方向变化时：

1. JPEG 写入期间拒绝切换；
2. 记录当前方向以及 heap/PSRAM；
3. 删除旧 Sprite；
4. 设置 rotation 并按实际 `M5.Display.width()/height()` 创建新 Sprite；
5. 创建失败则恢复前一方向和尺寸；
6. 成功后清黑并打印切换后的 heap/PSRAM。

Canvas 在检测到 PSRAM 时显式使用 PSRAM，避免 LiveView、BLE/Wi-Fi 运行后内部 DMA heap 碎片化导致约 64.8 KB 的 16-bit Canvas 无法重建。若分配仍失败，保留并恢复旧方向 Canvas，同一目标方向最多每 2 秒重试一次，避免主循环逐帧重复分配和刷串口。

固件只有主循环线程会调用方向切换和 MJPEG 帧处理，`beginLiveFrame()/finishLiveFrame()` 仍显式标记 JPEG 写入窗口，防止以后引入异步调用时销毁正在写入的 Canvas。

## LiveView 显示门控

横屏且 `PreviewRunning` 时，MJPEG 回调沿用原 `JpegDecoder::drawFrame()`，完成后只绘制微型电量图标和快门 overlay。

`JpegDecoder` 会缓存 `begin()` 时的显示尺寸。横竖 Canvas 发生变化后，`main.cpp` 在第一帧解码前同步当前 Canvas 尺寸并重新初始化该显示视口，避免用竖屏 `135×240` 裁剪横屏 `240×135` Canvas 所造成的画面偏移；JPEG 解码实现本身保持不变。

竖屏时：

- MJPEG 网络读取和 parser 保持运行；
- `lastLiveViewActivityAt` 仍在每次收到字节时更新；
- 每个完整 JPEG 仍更新 `lastFrameAt`、帧数和 frame-buffer 统计；
- 仅跳过 JPEG 解码与屏幕提交，防止画面覆盖遥控器 UI；
- 不关闭 Wi-Fi，不停止 LiveView，不触发重连。

因此 Supervisor 仍能看到活跃的流和完整帧，不会把“竖屏不渲染”误判为网络卡死。

## 按键与一次性命令

`ButtonInput` 对原始按键电平生成 A 键 down/held/released/holdMs，以及 B 键 active/progress/holdMs。

- A 键只在从“按下”变为“释放”的边沿产生 `UserCommand::Shoot`；持续按住和重复轮询都不产生命令。
- 未满 300 ms 的短按仍在释放时拍摄。
- 满 300 ms 只改变 FocusVisual 并按限频播放 FocusTick，释放时仍只拍一次。
- CameraSleepGuard 中同一个 `Shoot` 用户命令仍由现有 `AppController` 解释为手动唤醒。
- B 键进度和 3 秒触发共用 `_buttonBPressedAtMs`；提前释放清零，达到阈值通过 `_resetReported` 锁存只触发一次。
- 电源键关机路径保持不变。

## 声音和背光

`UiSoundPlayer` 使用 M5Unified 异步 `M5.Speaker.tone()`，保守音量为 40/255。FocusTick、快门双段音、成功和错误音都只排一个短音；双段快门由 `millis()` 到期后启动第二段，不使用 `delay()`。扬声器不可用或初始化失败时静默降级。

`BacklightAnimator` 用 smoothstep 在 900 ms 内从 180 降到 24，在唤醒时用 180 ms 恢复。动画只更新 `M5.Display.setBrightness()`，不改变设备电源管理或关机策略。

## 刷新策略

- LiveView：JPEG 帧驱动；
- 配对、遥控、重置：最多 25 FPS；
- 休眠：最多 8 FPS；
- Error：状态变化或低频刷新；
- `UI_DEBUG_HUD` 默认关闭；正常 LiveView 不显示 FPS、RSSI、帧计数、型号或 AF 框。

## 自动验证与实机清单

Native 测试覆盖：AppState 映射、场景优先级、姿态稳定/滞回、动画端点和 `millis()` 溢出、Button A 单次 Shoot、Button B 进度/取消/单次触发、快门成功/失败生命周期、Sleep 和 Error 优先级。

以下项目必须在 StickS3 + 支持的 RICOH GR 相机上验证，未执行前均标记为 `TODO_HARDWARE`：

- 首次配对、已保存相机快速连接、Wi-Fi/HTTP/LiveView；
- IMU X/Y 方向与 rotation 0/1 的物理朝向；
- 临界角度防抖和连续横竖切换 20 次；
- 切换前后串口 heap/PSRAM 日志无持续下降；
- 竖屏期间流保持、横屏恢复画面；
- A 短按、长按、单次照片、成功/失败反馈；
- B 中途释放、满 3 秒重置；
- 休眠低亮、A 唤醒、BLE/Wi-Fi 自动恢复；
- 扬声器音量、音色和 IMU/扬声器故障降级；
- 同条件前后平均 FPS，目标下降不超过约 10%。
