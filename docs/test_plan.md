# Test Plan

## 验证原则

UI Variant 重构需要分别验证构建、Native 逻辑和真实设备行为：

- 固件构建证明指定 Variant 能编译和链接。
- Native 测试证明主机侧逻辑和 UI 数据契约满足断言。
- StickS3 + 相机实测才能证明屏幕输出、Overlay、帧率和完整 BLE/Wi-Fi/快门链路。

记录结果时必须附命令输出或设备证据。不要把“编译成功”写成“实机通过”，也不要把历史测试结论当作本次改动的回归结果。

## 1. 工作区与变更范围

```bash
git status --short
git diff --stat
git diff -- README.md docs/ui_architecture.md docs/ui_kawaii_theme.md docs/wifi_preview_flow.md docs/project_overview.md docs/test_plan.md
```

检查：

- UI Variant 修改没有意外改变 GATT 句柄、BLE 安全配对、Wi-Fi 缓存、MJPEG 帧解析或 JPEG scale 策略。
- Ricoh、Minimal、Debug、Kawaii、Rabbit 没有各自复制业务流程。
- 新旧环境名都仍存在。

## 2. 固件编译矩阵

逐个执行，不使用单一默认环境替代矩阵：

```bash
pio run -e sticks3-ui-ricoh
pio run -e sticks3-ui-minimal
pio run -e sticks3-ui-debug
pio run -e sticks3-ui-kawaii
pio run -e sticks3-ui-rabbit
pio run -e m5stack-sticks3
```

通过条件：

- 五个环境均为 `SUCCESS`。
- Ricoh、Minimal、Debug、Kawaii、Rabbit 分别使用 `UI_VARIANT=1/2/3/4/5`。
- 旧兼容环境 `m5stack-sticks3` 仍能构建，并选择 Ricoh Renderer。
- 没有新增编译 error；新增 warning 需要说明原因和影响。

如修改八个 `UI_FEATURE_*` 开关，还应至少补充一个非默认组合的编译检查。Kawaii 可重点检查 `UI_FEATURE_MASCOTS=0` 与 `UI_FEATURE_PATTERN_BACKGROUND=0`，并确认 `-1`、`0`、`1` 语义没有破坏构建。

## 3. Native 三套测试

`env:native` 的 `test_filter` 包含三套测试，一条命令可全部运行：

```bash
pio test -e native
```

需要单独定位失败时可执行：

```bash
pio test -e native --filter test_native
pio test -e native --filter test_ui_presenter
pio test -e native --filter test_ui_variant_contract
```

覆盖范围：

| 测试套件 | 当前重点 |
| --- | --- |
| `test_native` | MJPEG 组帧/丢帧、相机身份、BLE 重连身份和 Supervisor 健康判断 |
| `test_ui_presenter` | `AppState` 到 `UiScreen` / `UiPhase` 的映射、待机与错误优先级、空文本安全、展示字符串不影响状态 |
| `test_ui_variant_contract` | Ricoh、Minimal、Debug、Kawaii、Rabbit Profile 默认值，以及 `UiScreen::Settings` 名称 |

通过条件：三套测试均完成且无失败。Native 不包含 M5Unified 屏幕、真实 BLE、Wi-Fi 或 HTTP 实测。

## 4. UI 静态契约检查

人工 Review Renderer 的 include 和调用点，确认：

- 业务文件只实例化 `ActiveUiRenderer`，不直接依赖具体 Ricoh/Minimal/Debug/Kawaii 类型。
- Renderer 只依赖 `UiModel`、自身 Theme/Layout/Profile 和绘图 API。
- 四个 Renderer 都实现 `renderBoot()`、`renderStatus()`、`renderSettings()`、`renderLiveViewOverlay()`、`renderError()`、`renderShutdown()` 六方法契约。
- Renderer 不调用 BLE、Wi-Fi、HTTP、NVS 或 `AppController` 业务操作。
- Renderer 不调用 `M5.begin()`、`pushSprite()`、`M5DisplaySurface::present()` 或 `delay()`。
- 四个 `renderLiveViewOverlay()` 都不调用 `fillScreen()` / `clear()`，也不自行 `present()`。
- `M5.begin()` 和 `pushSprite()` 仅由 `M5DisplaySurface` 管理。
- UI 页面/阶段不通过 `detail`、`errorMessage` 等展示字符串推断。
- `UiManager` 支持 `UiScreen::Settings`，但 `UiPresenter::mapScreen()` 和按钮流程没有进入 Settings 的路径；不得把静态画面描述成可操作设置菜单。

## 5. StickS3 UI 实机冒烟

每个 Variant 单独烧录；串口按实际端口替换：

```bash
pio run -e sticks3-ui-ricoh --target upload
pio device monitor --baud 115200 --filter time
```

Kawaii 烧录命令：

```bash
pio run -e sticks3-ui-kawaii --target upload
pio run -e sticks3-ui-rabbit --target upload
```

对 `sticks3-ui-minimal`、`sticks3-ui-debug`、`sticks3-ui-kawaii` 和 `sticks3-ui-rabbit` 重复同样步骤。每个 Variant 检查：

1. Boot 页面方向、尺寸和文字正常，无异常重启。
2. BLE 扫描/连接、Wi-Fi 连接、错误、恢复和 Shutdown 页面能随强类型状态切换。
3. 进入 LiveView 后底图持续存在，Overlay 不清屏、不整屏闪烁、不明显残留。
4. 每个成功 JPEG 帧仅发生一次硬件上屏；如需确认，使用临时低频计数或逻辑分析方式，避免每帧打印干扰性能。
5. Ricoh、Minimal、Debug、Kawaii、Rabbit 显示各自风格，但相同按键和业务状态产生相同行为。
6. 六个通用 Feature Flag 的默认元素与 Profile 一致；Kawaii 的角色和代码图案背景默认开启，关闭对应专用 Flag 后只改变绘制。
7. Kawaii 的 Boot、Status、LiveView、Error、Shutdown 画面方向、裁切与文本可读性正常；角色、爱心、爪印和背景图案均来自代码绘制。
8. 当前正常按键流程不应进入 Settings。若使用专门测试入口预览该页面，只验证静态布局；Shutter、Filter、Exposure、Wi-Fi、Pair、Sleep 等固定文本不应被当作真实设置值或交互能力。

## 6. BLE / Wi-Fi / Power 功能回归

至少执行以下场景，并保存串口日志：

1. **首次配对**
   - 擦除 NVS/flash 后启动。
   - 期望：扫描、配对、安全连接、保存 BLE 身份、激活 Wi-Fi、HTTP 探测并进入 LiveView。

2. **已配对直连**
   - StickS3 重启，相机处于工作状态。
   - 期望：优先使用已保存 BLE 身份；缓存 Wi-Fi 参数短超时尝试，失败时回退到新参数。

3. **相机关机/待机保护**
   - 让相机处于关机、`BLE_STARTUP` 或 `POWER_OFF_TRANSFER` 语义。
   - 期望：不为继续预览发送 Wi-Fi ON，进入 `CAMERA_SLEEP_GUARD`。

4. **手动唤醒**
   - Guard 冷却结束后按 Button A。
   - 期望：启动既有手动唤醒/重连流程；UI 只反映状态，不改变流程。

5. **Button A AF 快门**
   - LiveView 正常运行时按 Button A。
   - 期望：执行既有 BLE AF 快门流程，预览按原策略保持或恢复；Overlay 以强类型 `UiShutterStatus` 短时显示成功或失败反馈。

6. **Button B 配对重置**
   - 长按 Button B。
   - 期望：清除本地配对/Profile，断开连接并重新进入扫描；显示 `PairingReset` 语义。

7. **电源键关机**
   - 长按电源键。
   - 期望：关闭 LiveView、Wi-Fi 和 BLE，显示 Shutdown 页面后关机。

8. **断线与 Stall 恢复**
   - 分别制造 Wi-Fi 断开、BLE 断开和 Preview 帧超时。
   - 期望：Supervisor/AppController 按既有策略恢复；Renderer 不阻塞恢复。

至少在 Ricoh 与 Minimal 上完成完整功能回归；Debug 与 Kawaii 至少完成启动、连接、LiveView、Overlay 和关机冒烟。若只测试了部分 Variant，报告中必须明确范围。当前 Kawaii 实机结果仍为待验证，不能由编译或 Native 测试代替。

## 7. Preview 性能与长期运行

对重构前基线和四个待发布 Variant 使用同一相机、距离和供电条件，记录：

- FPS。
- `droppedFrames`。
- `JpegDecoder::lastDecodeMs()`。
- 完整 render 时间。
- Wi-Fi RSSI。
- LiveView stall 和重连次数。
- 内部 heap、最大连续块和 PSRAM 可用量。
- 运行时长及是否出现闪屏、残影、重启或 watchdog。

通过条件需由项目维护者根据基线确定；没有同条件数据时只报告测量值，不使用“无明显下降”等结论。

## 8. 结果记录模板

```text
Commit / branch:
Board package / PlatformIO:
StickS3 revision:
Camera model / firmware:

Build sticks3-ui-ricoh: PASS / FAIL / NOT RUN
Build sticks3-ui-minimal: PASS / FAIL / NOT RUN
Build sticks3-ui-debug: PASS / FAIL / NOT RUN
Build sticks3-ui-kawaii: PASS / FAIL / NOT RUN
Build sticks3-ui-rabbit: PASS / FAIL / NOT RUN
Build m5stack-sticks3: PASS / FAIL / NOT RUN

Native test_native: PASS / FAIL / NOT RUN
Native test_ui_presenter: PASS / FAIL / NOT RUN
Native test_ui_variant_contract: PASS / FAIL / NOT RUN

Hardware Ricoh UI: PASS / FAIL / NOT RUN
Hardware Minimal UI: PASS / FAIL / NOT RUN
Hardware Debug UI: PASS / FAIL / NOT RUN
Hardware Kawaii UI: PASS / FAIL / NOT RUN
Kawaii Settings static contract: PASS / FAIL / NOT RUN

FPS / dropped / decode ms / render ms:
BLE-Wi-Fi-LiveView regression evidence:
Overlay visual evidence:
Known failures / unverified items:
```

最终报告必须保留所有 `NOT RUN`，不得省略或改写为通过。
