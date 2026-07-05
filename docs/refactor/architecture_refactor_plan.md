# RICOH GR Live View Shooting 嵌入式架构重构计划

## 当前结论

本计划根据当前仓库源码、README 与外部重构说明整理。当前固件运行在 ESP32-S3 / M5Stack StickS3 / PlatformIO Arduino 环境，核心流程是：BLE 扫描/连接理光 GR IV 系列相机，读取相机状态，通过 BLE 打开相机 Wi-Fi，连接相机热点，通过 HTTP LiveView 读取 MJPEG/JPEG 帧并显示，同时支持按钮快门与连接恢复。

本轮重构遵循“阶段化、小步可回滚、每阶段可编译”的原则。阶段 0-9 已完成基础分层、服务门面、预览缓冲、AppController 状态机与 SystemSupervisor 健康事件接入；阶段 10 继续整理主运行时接线，不改变 BLE/Wi-Fi/Power/LiveView 协议行为。

## 当前代码结构

| 路径 | 当前职责 | 备注 |
| --- | --- | --- |
| `platformio.ini` | PlatformIO 环境、依赖、构建参数、native 测试过滤 | 已确认 |
| `README.md` / `README_EN.md` | 项目使用说明、验证机型与操作流程 | 已确认 |
| `src/config.h` | 固件常量、BLE handle/UUID、超时、缓冲区大小 | 已确认，后续可逐步迁移到 `core/AppConfig` |
| `src/main.cpp` | 初始化、状态机、BLE/Wi-Fi/HTTP/LiveView 编排、按钮触发、恢复策略 | 职责较重，是后续拆分重点 |
| `src/ricoh_ble_client.*` | NimBLE 扫描、连接、安全、读写 handle、读取 Wi-Fi 参数、电源状态、快门命令 | 已确认，高风险模块，后续只做包装式迁移 |
| `src/gr_wifi.*` | ESP32 Wi-Fi STA 连接、BSSID/channel 优化、状态文本 | 已确认 |
| `src/gr_api.*` | HTTP `/v1/props`、`/v1/liveview`、LiveView socket 读取 | 已确认 |
| `src/mjpeg_stream.*` | MJPEG 边界解析、帧回调、丢帧统计 | 已确认 |
| `src/jpeg_decoder.*` | JPEGDEC 解码并绘制到 LovyanGFX/M5Canvas | 已确认 |
| `src/display.*` | 启动画面、状态画面、LiveView overlay、UI 绘制 | 已确认 |
| `src/buttons.*` | M5Unified 按钮事件包装 | 已确认 |
| `src/camera_profile_store.*` | Preferences 持久化 BLE identity 与 Wi-Fi 缓存 | 已确认 |
| `src/camera_identity.*` | 相机名称/身份识别辅助 | 已确认 |
| `src/ble_reconnect_policy.*` | BLE 直连身份判断 | 已确认 |
| `test/` | native 逻辑测试 | TODO_UNVERIFIED：需后续补充覆盖新纯逻辑模块 |
| `docs/` / `logs/` | 项目说明、排查记录模板 | 当前有未跟踪文档，重构提交需避免混入无关文件 |

## 当前职责混合点

1. `src/main.cpp` 同时负责初始化、流程状态机、BLE 连接策略、Wi-Fi 连接策略、HTTP 探测、LiveView 恢复、按钮动作与 UI 状态输出。
2. 相机电源保护逻辑分散在 `main.cpp` 与 BLE 客户端能力之间，后续应抽出为只读策略层，避免任何误唤醒行为。
3. 预览性能统计分散在 LiveView 读取、MJPEG 处理、JPEG 解码、UI overlay 之间，后续应建立统一的 `PreviewFrameBuffer` / 诊断接口。
4. UI 状态文本由主流程直接拼接并推送，后续可以由 `UiManager` 接收上层状态，不让 BLE/Wi-Fi 细节直接耦合 UI。
5. 当前常量集中在 `config.h`，虽然可用，但和协议 handle、运行策略、UI 参数混在一起；后续可逐步拆为 `AppConfig`、`BoardConfig` 和协议常量。

## 目标分层

```text
src/core/        基础类型、配置、日志、周期任务工具
src/board/       StickS3 板级信息与硬件配置占位
src/drivers/     屏幕、按钮等底层驱动包装，占位期不接管 M5Unified
src/services/    相机、BLE、电源策略、Wi-Fi 预览、帧缓冲、快门服务
src/ui/          用户命令、按钮到命令映射、UI 管理
src/app/         AppState / AppController 主状态机
src/supervisor/  健康检查与恢复策略
```

## 分阶段迁移大纲

1. 阶段 0：生成本计划文档，不改功能代码。
2. 阶段 1：新增空架构骨架文件，确保 `pio run` 通过，不接管任何业务逻辑。
3. 阶段 2：引入 `AppConfig`、`Logger`、`Result`，只迁移已确认常量和日志接口，保持值与行为不变。
4. 阶段 3：抽象 `UserCommand` / `ButtonInput`，先镜像现有 Button A 行为，不改变短按/长按语义。
5. 阶段 4：抽出 `CameraPowerPolicy`，集中表达“只有确认开机才允许打开 Wi-Fi/预览”的判断，不能改变现有保护行为。
6. 阶段 5：包装 `RicohBleClient` 为 `BleCameraService` 门面，优先转发，不重写 NimBLE 流程。
7. 阶段 6：包装 `GrWifi`、`GrApi`、`MjpegStream`、`JpegDecoder` 为 `WifiPreviewService`，新增性能日志但不优化算法。
8. 阶段 7：引入 `PreviewFrameBuffer` 与长时间运行统计；若风险高，先只记录诊断，不替换内存分配。
9. 阶段 8：`AppController` 逐步接管主状态机；禁止一次性重写 `main.cpp`。
10. 阶段 9：`SystemSupervisor` 添加健康检查事件；不在监督器内执行耗时恢复。
11. 阶段 10：整理 App runtime 接线，收拢 Arduino `loop()` 每轮调度入口；只提升主循环可读性，不改变状态机顺序与业务判断。

## 风险与验证要点

| 阶段 | 主要风险 | 必须验证 |
| --- | --- | --- |
| 0-1 | 理论上无业务风险 | `git diff` 只包含文档/新增骨架，`pio run` 通过 |
| 2 | 日志/常量替换造成行为漂移 | BLE 扫描、Wi-Fi 连接、LiveView、快门与重构前一致 |
| 3 | 按钮语义变化 | Button A 快门、保护态手动唤醒流程不变 |
| 4 | 相机关机状态被误唤醒 | 关机相机不进入 Wi-Fi/LiveView；开机相机正常进入预览 |
| 5 | BLE pairing/direct reconnect 退化 | 首次配对、已配对直连、断连重连、快门 BLE 写入正常 |
| 6 | Wi-Fi/LiveView 卡顿或失败 | Wi-Fi 连接、HTTP props、LiveView 帧显示、掉线恢复正常 |
| 7 | 内存占用上升、watchdog | 5 分钟以上预览，free heap 稳定，无崩溃重启 |
| 8 | 状态机条件遗漏 | 全流程状态转移日志清晰，异常恢复不退化 |
| 9 | 误触发恢复 | 正常预览不误恢复，断连后恢复路径正确 |
| 10 | 主循环调度顺序漂移 | native 测试与固件构建通过，确认按钮、相机流程、Wi-Fi/LiveView 监控、Supervisor、UI 刷新顺序保持一致 |

## 后续 Codex 提示词骨架

### 阶段 1

新增架构骨架文件，只定义空类、枚举和注释；禁止修改现有 BLE/Wi-Fi/预览/快门/按钮/电源状态代码；运行 `pio run`。

### 阶段 2

只迁移已确认常量和日志接口；保持数值、超时、调用顺序完全一致；运行 `pio run` 并输出行为风险。

### 阶段 3

抽象按钮命令，但先不改变 `handleButtons()` 行为；Button A 快门和保护态唤醒语义必须一致。

### 阶段 4

实现相机电源策略单元，所有判断从现有代码搬运，不新增唤醒动作；关机保护必须优先。

### 阶段 5+

只做包装式迁移，先让旧对象作为服务内部依赖；每次只接管一个窄接口，实机日志验证通过后再继续。

### 阶段 10

整理 `main.cpp` 的运行时接线；允许抽出只包装现有调用顺序的小函数，禁止改变 BLE 扫描/连接、Wi-Fi ON、电源保护、HTTP props、LiveView 和按钮语义。

## 本轮实施范围

阶段 10 只整理 App runtime 接线：`loop()` 保留为 Arduino 入口，实际每轮调度由内部 `runAppTick()` 承载。不会迁移、删除、重命名或改写 BLE/Wi-Fi/Power/LiveView 功能代码。
