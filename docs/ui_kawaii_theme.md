# Kawaii Purple UI Theme

## 当前范围

Kawaii 是第四套编译期 UI Variant：

| 项目 | 值 |
| --- | --- |
| Variant 宏 | `UI_VARIANT_KAWAII=4` |
| `UI_VARIANT` 数值 | `4` |
| PlatformIO 环境 | `sticks3-ui-kawaii` |
| Renderer | `rvf::ui::KawaiiUiRenderer` |
| Profile | `rvf::ui::KawaiiUiProfile` |

它只替换显示层，不复制或改变 BLE、Wi-Fi、HTTP、MJPEG、JPEG、快门、配对重置、恢复和相机电源保护流程。`UiVariant.h` 在编译期把 `ActiveUiRenderer` 解析为 `KawaiiUiRenderer`，没有运行时 Renderer 分配或切换。

当前代码提供完整的页面绘制实现和编译入口，但没有附带 StickS3 实机照片、录像或性能数据。屏幕方向、RGB565 颜色、文字/角色裁切、Overlay 可读性、FPS 和真实相机链路均仍待实机验证。

## 代码绘制视觉

Kawaii 不包含全屏位图，也不在运行时解码角色图片。背景和角色由 LovyanGFX 基础图元直接绘制：

- `KawaiiTheme.h` 定义柔和薰衣草紫背景、浅色面板、粉色高光以及成功/警告/错误等 RGB565 颜色。
- `KawaiiAssets.h` 保存背景圆形图案的位置/半径及少量固定标签，不保存像素图。
- `drawPatternBackground()` 先清成主题背景色，再按位置表绘制柔和 blob 图案。
- `drawOuterShell()` 用圆角矩形、三角形和线条组成带耳朵的外框。
- `drawMascot()` / `drawMiniMascot()` 用圆、圆角矩形和三角形绘制角色。
- 爱心、爪印、Wi-Fi、电量和对焦框同样由代码图元绘制。

这种实现避免了运行时位图解码和大型全屏图片，但图元数量仍会增加每帧 Overlay 工作量，实际性能需要在 StickS3 上测量。

## 文件结构

| 文件 | 作用 |
| --- | --- |
| `KawaiiUiRenderer.h` | 六方法 Renderer 接口和绘图 helper 声明 |
| `KawaiiUiRenderer.cpp` | 文本、背景、外框、角色、Badge、状态行、设置 Tile、对焦框等公共绘制 helper |
| `KawaiiUiRenderer_Pages.cpp` | Boot、Status、Settings、Error、Shutdown 页面 |
| `KawaiiUiRenderer_LiveView.cpp` | LiveView Overlay |
| `KawaiiTheme.h` | RGB565 主题色 |
| `KawaiiLayout.h` | 240 × 135 页面坐标和尺寸 |
| `KawaiiAssets.h` | 代码背景图案位置与固定标签 |
| `KawaiiUiProfile.h` | 八个 Feature Flag 的 Kawaii 默认值 |

## 六个页面方法

所有 Variant 现在共享同一个六方法契约。Kawaii 的实现如下：

| 页面 | Renderer 方法 | 当前绘制内容 |
| --- | --- | --- |
| Boot | `renderBoot()` | 紫色图案背景、右侧单角色、标题、固定进度条、页脚、Wi-Fi/电量 Badge |
| Status | `renderStatus()` | `RICOH GR` 顶栏、左侧五行图标灯面板、右侧大角色和左下纵向操作提示 |
| LiveView | `renderLiveViewOverlay()` | 边缘 Live/快门状态、FPS、Wi-Fi、电量、对焦框、右下单角色和独立底部操作胶囊 |
| Settings | `renderSettings()` | Quick Controls 静态 Tile 和固定示例值 |
| Error | `renderError()` | 错误角色、错误/详情文本和 Retry/Reset 提示 |
| Shutdown | `renderShutdown()` | 双角色、Good Night 信息和关机详情 |

Boot 页的 `67%`、`FW 1.0.1`，LiveView 底部的 `+/-0.0`，Settings 页的 `1/125s`、`Standard`、`+/-0.0`、`Reset All`、`2 min`，以及部分页脚标签都是代码中的固定视觉样稿，不是实时相机属性或设置状态。Settings 的 Wi-Fi Tile 只根据 `model.wifiConnected` 显示 `On` / `Off`，仍不是可操作开关。

## 参考图对齐坐标

本轮只调整 Kawaii 绘制层，关键布局集中在 `KawaiiLayout.h`：

| 区域 | 坐标 / 尺寸 |
| --- | --- |
| 顶栏 | `(40, 3, 196, 19)` |
| 状态面板 | `(6, 27, 60, 74)`；首行 `y=30`，行高 `11`，行距 `3` |
| 状态按钮 | `(6, 105, 88, 12)` 与 `(6, 120, 88, 12)` |
| Boot 标题 / 进度条 | 标题 `(18, 42)`；进度条 `(42, 87, 132, 14)` |
| Boot 角色 | 中心 `(211, 72)`，`2x` |
| Status 角色 | 中心 `(180, 94)`，`3x`，允许被屏幕底边自然裁切 |
| LiveView 角色 | 中心 `(220, 99)`，`1x` |
| LiveView 底部提示 | A `(6, 120, 54, 12)`；曝光占位 `(76, 120, 46, 12)`；B `(126, 120, 78, 12)` |

状态页五行依次表达整体连接、BLE、Wi-Fi、电量和相机就绪状态。除电量百分比外不显示连接状态文字，所有连接/就绪结果只通过灰色或绿色圆灯表达；Renderer 只读取 `UiModel` 的结构化字段，不根据显示文字反推业务状态。

## Settings 的明确限制

`UiScreen::Settings` 已加入 Screen 枚举，`UiManager` 也能调用所有 Renderer 的 `renderSettings()`。不过当前运行链路没有可达入口：

- `UiPresenter::mapScreen()` 不会返回 `UiScreen::Settings`；
- 按钮处理没有派发 `UserCommand::OpenSettings`；
- Settings Tile 没有焦点、选择、Back/Confirm/Up/Down 导航；
- 页面不会写相机设置、NVS 或运行配置。

因此 Settings 只能描述为“静态渲染/视觉样稿”，不能描述为可操作菜单。未来若接入导航，仍应通过业务命令和强类型 Model 完成，Renderer 本身不得直接执行相机或存储操作。

## Feature Flags

Kawaii 继承六个通用开关，并增加两个仅由 Kawaii Profile 使用的绘制开关：

| 宏 | Kawaii 默认 | 作用 |
| --- | :---: | --- |
| `UI_FEATURE_FPS` | 开 | 快门状态为空闲时，Live Badge 中的 FPS |
| `UI_FEATURE_FRAME_STATS` | 关 | rendered/dropped 帧统计 |
| `UI_FEATURE_WIFI_RSSI` | 开 | Boot、Settings、LiveView 的 Wi-Fi Badge；不改变 Status 图标灯的结构化状态映射 |
| `UI_FEATURE_BATTERY` | 开 | Boot、Settings、LiveView 的电量 Badge 和 Status 电量行 |
| `UI_FEATURE_CAMERA_MODEL` | 开 | 保留 Profile 契约；参考图对齐后的 Status/LiveView 不再绘制型号长文本 |
| `UI_FEATURE_FOCUS_BRACKET` | 开 | LiveView 对焦框 |
| `UI_FEATURE_MASCOTS` | 开 | Boot、Status、LiveView 等页面的代码绘制角色；不移除独立的爱心/爪印装饰 |
| `UI_FEATURE_PATTERN_BACKGROUND` | 开 | 非 LiveView 页面的 blob 图案；背景底色和外框仍保留 |

所有宏都使用三态语义：`-1` 采用 Profile 默认、`0` 关闭、`1` 开启。它们只裁剪绘制分支，不得影响数据采集或相机功能。

例如，可新增一个轻量 Kawaii 环境：

```ini
[env:sticks3-ui-kawaii-lite]
extends = env:m5stack-sticks3-base
build_flags =
    ${env:m5stack-sticks3-base.build_flags}
    -DUI_VARIANT=4
    -DUI_FEATURE_MASCOTS=0
    -DUI_FEATURE_PATTERN_BACKGROUND=0
```

## LiveView Overlay 约束

Kawaii 与其他 Renderer 共用相同 Canvas 所有权：

```text
JPEG 成功解码到 displaySurface.canvas()
  -> KawaiiUiRenderer::renderLiveViewOverlay()
  -> M5DisplaySurface::present() 一次
```

`renderLiveViewOverlay()` 只绘制边缘 HUD 和可选元素：

- 不调用 `fillScreen()` 或 `clear()`；
- 不调用 `present()` 或 `pushSprite()`；
- 不创建新的全屏 Sprite；
- 不访问 BLE、Wi-Fi、HTTP、NVS 或相机控制；
- 不调用 `delay()`；
- 不改变 MJPEG 读取和 JPEG 解码策略。

Overlay 中的小面板可以覆盖局部像素，这是 HUD 的预期行为；不得扩展为清空或覆盖整个 JPEG 底图。

## 编译、烧录与验证

编译 Kawaii：

```bash
pio run -e sticks3-ui-kawaii
```

烧录 Kawaii：

```bash
pio run -e sticks3-ui-kawaii --target upload
```

可按实际串口启动监视：

```bash
pio device monitor --baud 115200 --filter time
```

编译成功只证明 Kawaii 能链接，Native Profile 契约测试只证明默认开关和 Screen 名称符合断言。发布前仍需在 StickS3 + RICOH GR 上验证：

1. Boot、Status、Error、Shutdown 页面方向、裁切和颜色。
2. LiveView 底图不被清空，Overlay 无整屏闪烁或明显残留。
3. FPS、decode/render 时间、dropped frames 与长期运行内存。
4. BLE 配对/重连、Wi-Fi 激活、LiveView、快门、配对重置和电源保护行为。
5. `UI_FEATURE_MASCOTS` 与 `UI_FEATURE_PATTERN_BACKGROUND` 关闭后仅改变视觉。

当前上述 Kawaii 实机项目均应记录为 `NOT RUN` 或“待验证”，除非提供对应提交的设备、相机固件、串口日志和视觉证据。完整矩阵见 [test_plan.md](./test_plan.md)。
