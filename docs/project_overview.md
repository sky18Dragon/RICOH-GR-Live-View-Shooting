# Project Overview

## 项目目标

RICOH GR Live View Shooting 是面向 ESP32-S3 / M5Stack StickS3 的 PlatformIO Arduino 固件。固件以 BLE 完成相机发现、配对、状态读取、Wi-Fi 激活和快门控制，连接相机 Wi-Fi AP 后通过 HTTP 打开 MJPEG LiveView，并将解码后的画面显示在 StickS3 上。

项目同时包含相机待机/关机保护、连接恢复、NVS 相机 Profile 和主机侧 Native 测试。UI 已与业务流程分离，可在编译期选择 Ricoh、Minimal、Debug、Kawaii 或 Rabbit 界面。

## 当前代码结构

核心运行链路：

```text
AppController / Services
  -> UiRuntimeSnapshot
  -> UiPresenter
  -> UiModel
  -> UiManager<ActiveUiRenderer>
  -> M5DisplaySurface
  -> M5Canvas / M5.Display
```

- `AppController` 负责状态机、业务事件、恢复和用户命令，不知道当前 UI Variant。
- Services 负责 BLE、相机、Wi-Fi/HTTP LiveView、快门和预览缓冲等能力。
- `UiPresenter` 根据 `AppState` 和结构化运行态生成强类型 `UiModel`，不通过展示字符串推断状态。
- `UiManager` 负责页面选择、Dirty 检测、非 LiveView 刷新节流和统一上屏。
- `ActiveUiRenderer` 在编译期解析为 Ricoh、Minimal、Debug、Kawaii 或 Rabbit Renderer，只绘制传入的 `UiModel`。
- `M5DisplaySurface` 独占 M5 初始化、Canvas 生命周期和 `present()`。

UI 的详细职责、字段、六方法 Renderer 契约、扩展方式和约束见 [ui_architecture.md](./ui_architecture.md)。Kawaii 的代码绘制视觉与限制见 [ui_kawaii_theme.md](./ui_kawaii_theme.md)，Wi-Fi、MJPEG、JPEG 和上屏时序见 [wifi_preview_flow.md](./wifi_preview_flow.md)。

## 从当前配置与代码确认的事实

- 默认 PlatformIO 环境：`sticks3-ui-ricoh`。
- 第四套 UI 环境：`sticks3-ui-kawaii`，定义 `UI_VARIANT=4`。
- 第五套 UI 环境：`sticks3-ui-rabbit`，定义 `UI_VARIANT=5`。
- 兼容环境：`m5stack-sticks3`，继承 Ricoh UI。
- Platform：`espressif32@6.12.0`。
- Board：`esp32-s3-devkitc-1`。
- Framework：Arduino，C++17。
- 主要依赖：M5Unified、M5PM1、JPEGDEC、ArduinoJson、NimBLE-Arduino 2.5.0。
- 逻辑显示尺寸：240 × 135；`M5DisplaySurface` 在启动时按实际 rotation 创建 Canvas。
- LiveView frame buffer：256 KiB，优先 PSRAM、失败时尝试内部 RAM。
- Stream read buffer：2048 bytes。
- Camera HTTP 默认地址：`192.168.0.1:80`。
- LiveView JPEG scale 默认由 `JPEG_SCALE_POLICY` 决定，当前配置为 `JPEG_SCALE_HALF`。
- 主循环由 `AppController::planTick()` 规划按钮、相机流程、Wi-Fi 监视、属性刷新和 LiveView 监视，并运行 Supervisor、Wi-Fi Profile 刷新和状态 UI 更新。

- `UiScreen` 包含 `Settings`，所有 Renderer 都实现 `renderSettings()`；当前 Presenter 和按钮流程尚未导航到该页面。

这些代码级事实不代表具体相机、帧率、长期稳定性或五套 UI 已在当前改动中完成实机验证。Kawaii 与 Rabbit 的屏幕视觉和相机链路仍待 StickS3 实测。

## 业务流程

典型连接主路径：

```text
BleScan
  -> BLE 扫描或已保存地址直连
  -> BleReady
  -> 检查 Power State / Operation Mode
  -> 通过 BLE 激活相机 Wi-Fi
  -> 缓存 Wi-Fi 参数短超时连接，失败则读取最新参数回退
  -> HttpProbe
  -> 打开 /v1/liveview
  -> LiveViewRunning
```

`AppState` 还包含更细粒度的 Boot、BLE 连接、相机电源检查、Wi-Fi 激活、HTTP 探测、Preview 启停、拍摄、断开和错误状态。`UiPresenter` 将这些状态统一映射为 `UiScreen` 和 `UiPhase`。

当检测到 `BLE_STARTUP`、`POWER_OFF_TRANSFER` 或相机关机语义时，流程进入待机/关机保护，不应为继续预览而自动开启相机 Wi-Fi。涉及唤醒或保护逻辑的修改还必须阅读 [power_state_policy.md](./power_state_policy.md)。

## LiveView 显示路径

```text
WifiPreviewService / GrApi 读取 MJPEG 字节
  -> MjpegStream 按 SOI/EOI 组帧
  -> JpegDecoder 写入 displaySurface.canvas()
  -> ActiveUiRenderer 叠加 Overlay
  -> displaySurface.present() 一次
```

只有 JPEG 解码成功的帧才叠加 Overlay 并上屏。Overlay 不得清屏；Renderer 不得自行 `pushSprite()`。这条所有权规则确保所有 Variant 共用同一条 LiveView 业务和解码链路。

## UI Variants 与构建

| 环境 | Variant | 说明 |
| --- | --- | --- |
| `sticks3-ui-ricoh` | Ricoh | 默认视觉界面 |
| `sticks3-ui-minimal` | Minimal | 简洁状态页和 Overlay |
| `sticks3-ui-debug` | Debug | 强调状态与诊断指标 |
| `sticks3-ui-kawaii` | Kawaii | 代码绘制的柔和紫色背景、角色与装饰性 HUD |
| `sticks3-ui-rabbit` | Rabbit | 右侧像素兔子背景、左侧安全文本区与顶部 LiveView HUD |
| `m5stack-sticks3` | Ricoh | 保留旧环境名的兼容入口 |

```bash
pio run -e sticks3-ui-ricoh
pio run -e sticks3-ui-minimal
pio run -e sticks3-ui-debug
pio run -e sticks3-ui-kawaii
pio run -e sticks3-ui-rabbit
pio run -e m5stack-sticks3
```

六个通用 `UI_FEATURE_*` 宏可在编译期控制 FPS、帧统计、Wi-Fi RSSI、电量、相机型号和对焦框；Kawaii 另使用 `UI_FEATURE_MASCOTS` 与 `UI_FEATURE_PATTERN_BACKGROUND` 控制角色和图案背景。取值 `-1` 使用 Variant 默认、`0` 关闭、`1` 开启；这些宏只能影响绘制。

Kawaii 实现 Boot、Status、LiveView、Settings、Error、Shutdown 六个页面方法。Settings 当前使用固定示例值做静态渲染，不具备导航、按键交互、持久化或相机参数写入能力。

## 模块索引

| 模块 | 路径 | 主要职责 |
| --- | --- | --- |
| 装配与循环 | `src/main.cpp` | 对象装配、Snapshot 收集、帧回调和运行循环 |
| 业务状态机 | `src/app/AppController.*`、`src/app/AppState.h` | 连接流、恢复、命令和状态转换 |
| 业务事件 | `src/core/AppEvent.h`、`src/core/AppEventSink.h`、`src/core/AppMessage.h` | 语义事件发布 |
| BLE 服务 | `src/services/BleCameraService.*`、`src/ricoh_ble_client.*` | 扫描、配对、状态与快门协议 |
| Wi-Fi / Preview | `src/services/WifiPreviewService.*`、`src/gr_wifi.*`、`src/gr_api.*` | Wi-Fi 连接、HTTP Props 与 LiveView 数据读取 |
| MJPEG / JPEG | `src/mjpeg_stream.*`、`src/jpeg_decoder.*` | 帧边界解析与 JPEG 解码 |
| 帧缓冲 | `src/services/PreviewFrameBuffer.*` | 固定缓冲及帧/内存统计 |
| Display Surface | `src/display/DisplaySurface.h`、`src/display/M5DisplaySurface.cpp` | M5 初始化、Canvas 和上屏 |
| UI Model / Presenter | `src/ui/model/`、`src/ui/presenter/` | 强类型 UI 数据和状态映射 |
| UI Manager / 选择器 | `src/ui/core/` | 刷新策略、特性开关和 Variant 类型选择 |
| UI Renderers | `src/ui/variants/` | Ricoh、Minimal、Debug、Kawaii、Rabbit 纯绘制实现 |
| Supervisor | `src/supervisor/SystemSupervisor.*` | 预览健康检查和恢复事件 |
| Native 测试 | `test/test_native/`、`test/test_ui_presenter/`、`test/test_ui_variant_contract/` | 基础逻辑、Presenter 和 Profile 契约 |

## 验证范围

固件编译、Native 测试和实机回归是三类不同证据：

- 五个固件环境编译成功：证明四个 Variant 与兼容环境能链接，不证明屏幕视觉或相机通信实际可用。
- `pio test -e native` 成功：证明主机侧基础逻辑、Presenter 映射和五套 Profile 默认值符合测试，不覆盖 M5 硬件或网络时序。
- StickS3 + 相机实测：才能确认页面、Overlay、不闪屏、帧率、BLE/Wi-Fi/快门和电源保护行为。

执行方法和记录要求见 [test_plan.md](./test_plan.md)。未附带当次命令输出或设备记录时，不应把待验证项写成“已通过”。

## 修改时必须保持的边界

- 不为不同 UI 复制 BLE、Wi-Fi、HTTP、LiveView、快门或状态机代码。
- Renderer 不得访问 BLE、Wi-Fi、HTTP，不得调用 `M5.begin()`、`pushSprite()` 或 `delay()`。
- `renderLiveViewOverlay()` 不得清屏，成功解码帧只能统一上屏一次。
- UI 元素开关不得影响数据采集或业务能力。
- 修改连接或预览路径前同时核对错误、保护和恢复分支。
