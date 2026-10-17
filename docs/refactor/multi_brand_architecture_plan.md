# 多品牌 / 多型号相机架构重构计划

> 状态：方案 A；阶段 0–6 已完成并在 GR III 系列 / GR IV 上实机回归（6.7 GR II 除外），阶段 7、8 未开始。进度与剩余任务见下方「当前进度与剩余任务」，逐步记录见「四、迁移路线」下的「执行记录」
> 日期：2026-10-13（进度更新：2026-10-17）
> 分析基线：分支 `feature/firmware-optimization`
> 关联文档：[architecture_refactor_plan.md](architecture_refactor_plan.md)、[../project_overview.md](../project_overview.md)、[../power_state_policy.md](../power_state_policy.md)

## 当前进度与剩余任务（2026-10-14）

以本节为准；每一步的细节、验证方式和依据见「执行记录」。

### 阶段进度

| 阶段 | 内容 | 状态 | 实机回归 |
| --- | --- | --- | --- |
| 0 | CI 运行 native 测试；死骨架清理；`main.cpp` 状态归组；测试按功能区拆分 | ✅ | — |
| 1–2 | `ICameraDriver` / `ICameraHost`；理光流程移入 `RicohDriver` | ✅ | GR III 系列、GR IV ✅；GR II ❌（无实机） |
| 3 | 机型注册表、`CameraPhase`、`AppRuntime` + `SimCameraDriver` | ✅ | GR III 系列 ✅ |
| 4 | 存储拆分（`cam.ricoh` 档案 / `app` 设置，升级整片擦除不迁移） | ✅ | GR III 系列 ✅ |
| 5 | 编译期选型号 `RVF_RICOH_GR2/GR3/GR4`、单型号固件环境、CI 构建矩阵、include 规则检查、依赖版本固定 | ✅ | GR III 系列、GR IV ✅（含单型号固件拒绝其他型号） |
| 6.1–6.6 | 理光 BLE 操作 / 凭据读取 / 协议识别 / 配对与状态通知 / 型号规则全部下沉到 `brands/ricoh/{gr3,gr4}`，共用代码只读参数 | ✅ | GR III 系列、GR IV ✅ |
| 6.7 | GR II Wi-Fi-only 路径：`gr2_*` 模块与 `shootGr2` 移入 `brands/ricoh/gr2`；`isGr2WifiOnlyProfile()` 分支改读配置参数；新增 `sticks3-ricoh-gr2` / `native-gr2` | ✅ 代码完成（2026-10-17） | ⏳ GR II 回归中 |
| 7 | 第二品牌样板驱动（富士 / 尼康） | ⏸ 需先抓包确认协议 | — |
| 8 | 运行时：驱动非阻塞、预览独立 FreeRTOS 任务、去 `String`、三段式 tick 合并为单个 `tick()` | ⏳ 仅完成接口层（`ICameraDriver` 无 Arduino 依赖、`showStatus` 收 `const char*`） | — |

### 剩余任务

**改造本身**

1. **6.7 GR II 实机回归**：代码已完成（分支 `feature/gr2-driver-split`），按 [known_issues.md](../known_issues.md)「支持与证据限制」第 4 条的清单回归全量固件与 `sticks3-ricoh-gr2`；GR II 自阶段 2 起首次实机验证。GR II 配网流程的界面 / 日志文案仍写 “GR II”（逐字保留），仅在配网分支中出现。
2. **阶段 8 运行时优化**：会改变时序，单独做、单独回归。可根治 K1（横屏快门写入阻塞主循环约 12 s）与 K7（内部 RAM 碎片）。
3. **阶段 7 第二品牌**：先抓取富士或尼康的 BLE / Wi-Fi / 预览协议，再据此检验 `ICameraDriver` 是否够用。
4. **理光驱动内部仍集中**：`RicohDriver.cpp`（约 2.9k 行）与 `ricoh_ble_client.cpp`（约 2.3k 行）虽已无型号分支，但仍是单个大文件、文件级全局状态；`services/BleCameraService`、`WifiPreviewService` 在目标分层（`transport/`、`media/`）中的归属未定。
5. **识别证据收集未拆分**：`collectProtocolEvidence()` 仍在共用代码中引用 GR III / GR IV 的 UUID 与 handle（其 GATT 访问顺序影响相机是否断开，刻意保留）；`config.h` 因此仍 include 两个型号的配置头。
6. **`CameraBinding`**：记录“相机归属哪个驱动”，第二个驱动出现时再加。

**工程与流程**

7. **发布流程**：`release.yml` 只发布全量固件；是否同时发布单型号固件待产品决定。
8. **`esp_new_jpeg` 未固定版本**：下载链接无版本号（目前均解析为 v1.0.2），上游更新时可能变化。
9. **`src/CLAUDE.md` 过时**：仍为 2026-06 平铺结构时期的内容，只加了过时提示，需按当前结构重写。

**已知问题（暂缓，详见 [known_issues.md](../known_issues.md)「已知且暂缓处理」）**

10. K1 横屏 BLE 快门写入阻塞约 12 s、K2 快门误判确认、K3 失败提示为空、K4 相机忙 / SD 卡满被拒、K5 Wi-Fi 全扫描首次失败后空等、K6 M5PM1 运行中 I2C 读失败、K7 内部 RAM 碎片、K8 GR IV 打开 LiveView 约 1.1 s、K9 GR IV 首次配对前几次连接失败、K10 日志文案误导。

## 目标

1. **可扩展不同品牌、型号的相机**（理光、富士、尼康……）。
2. **编译打包时可指定品牌 / 型号**，例如只打包理光固件，或只打包 GR IV 固件。
3. **每个型号的功能代码相互独立**，不互相耦合。

## 总体结论

以单一理光品牌的需求来看，当前架构"可用且安全"：防误唤醒门控、Bond 自愈、纯逻辑抽出做 Native 测试都做得很好。但按照上面三个目标衡量，现有架构**承载不了**。根本原因是：系统里只有**"理光代际"**这一层抽象，没有**"相机"**这一层抽象；理光的工作流（BLE 唤醒相机 AP → HTTP MJPEG）被写死在主流程中。

| 维度 | 现状 | 目标 |
| --- | --- | --- |
| 品牌扩展 | ❌ 全局身份类型就是 `RicohProtocolGeneration` | 品牌无关的相机驱动接口 |
| 编译期选择 | ❌ 只有一个固件 env，所有代码无条件编译 | 一个 env 对应一个品牌 / 型号组合 |
| 型号隔离 | ⚠️ 有 `ricoh/gr3`、`ricoh/gr4` 目录，但只隔离了"描述符" | 每个型号一个独立驱动，互不 include |
| 传输 / 媒体复用 | ⚠️ 能力可复用，但命名和实现绑定理光 | BLE / Wi-Fi / HTTP / MJPEG / JPEG 作为公共层 |
| 并发与实时性 | ⚠️ 单超级循环 + 大量 `delay()` | 驱动非阻塞，预览独立任务 |

---

## 一、问题点

### P1：领域模型就是理光模型，没有"相机"抽象

`RicohProtocolGeneration`（GR2/3/4）实际被当作全系统的"相机身份"：

- 持久化：`CameraProfile.protocolGeneration`，NVS namespace 为 `ricoh2`。
- UI：`PairingGuide` 写死 GR II → GR III → GR IV 的轮转顺序。
- 主流程：`main.cpp` 中约 98 处代际相关判断。
- 能力描述：`CameraCapabilities` 的字段是理光语义（如 `supportsHttpFocusSetting` 注释为 "GR Remote write shape"）。

**影响**：新增一个富士型号，需要同时修改枚举、存储结构、UI 和 `main.cpp`。

### P2：`ricoh/gr3`、`ricoh/gr4` 是"假隔离"

- `IRicohBleProtocol` 只是**描述符**：仅返回 `FixedHandle` / `NetworkTypeUuid` 等传输方式枚举和若干开关量，本身不包含行为。
- 真正的行为（写 WLAN、读凭据、快门序列、探测证据）都在 2651 行的 `ricoh_ble_client.cpp` 中通过 `switch` 分支实现，`RICOH_BLE_GR3_*` 与 `RICOH_BLE_GR4_*` 常量混在同一文件。
- `ProtocolDetectionEvidence` 是公共结构体，但字段是 `hasGr3Xxx` / `hasGr4Xxx`，每新增一个型号都要修改公共结构。
- `RicohBleProtocolRouter` 把 GR3、GR4 对象作为成员写死并用 `switch` 选择，无法在编译期只保留其中一个。
- `Gr3PairingStrategy` / `Gr4LegacyPairingStrategy` 直接修改 NimBLE 全局 `ble_hs_cfg` 与 IO Capability。

**影响**：改 GR IV 的逻辑，实际改动的是 GR III 也依赖的大文件，回归范围扩大到所有型号。

### P3：GR II 完全绕开协议层，靠 if 散落实现

- `main.cpp` 中有 27 处 `isGr2WifiOnlyProfile()` 分支。
- `GrApi` 中有专用的 `shootGr2()`。
- `gr2_*` 系列文件直接平铺在 `src/` 下。

这说明当出现第三种接入方式（不走 BLE）时，现有架构**只能在主流程里插 if 分支**。富士、尼康接入会是同样的结局。

### P4：App 层状态机实际是"理光工作流"

`AppFlowActions` 有 57 个函数指针，名字即理光的步骤：`activateCameraWifiOverBle`、`finishCameraWifiOverHttp`、`reactivateCameraWifiForCachedResume`、`restoreReleasedBle`（BLE parking）等。

富士、尼康的遥控通常基于 BLE 配对 + Wi-Fi 上的 PTP/IP（TCP），实时取景多为按帧拉取，而不是 HTTP MJPEG 长连接（具体需抓包确认，见 TODO_UNVERIFIED）。因此"BLE 开 AP → HTTP → MJPEG"这一假设不能留在通用层。

另外，`AppController` 本身几乎不持有状态，主要是转调 `main.cpp`；真正的"上帝对象"仍是 3218 行的 `main.cpp`（约 150 个函数、约 60 个全局变量）。`AppState` 有 22 个值、两套命名混用，其中 `Idle`、`ConnectingBle`、`CheckingCameraPower`、`ActivatingWifi`、`HttpProbing` 没有被引用。

### P5：传输层与理光协议绑定

- `RicohBleClient` 同时负责 NimBLE 栈生命周期（`resetStack`、`deleteAllBonds`、安全配置）和理光 GATT 协议。多品牌同时编入时，"谁来配置协议栈"没有归属。
- `GrApi` 混合了通用 HTTP 客户端、理光 `/v1/*` 接口和 GR II 专用方法；`GrWifi` 本身是通用代码，但命名带 Gr。
- 结果是 MJPEG 切帧、JPEG 解码、Wi-Fi 连接这些品牌无关的能力，无法干净地给其他品牌复用。

### P6：配置、构建、存储都没有品牌维度

- `config.h` 混合了三类内容：理光 GATT UUID / Handle（"ricoh" 出现约 68 次）、通用超时与显示参数、GR II 配网热点密码。
- `platformio.ini` 只有一个固件 env；native 环境的 `build_src_filter` 手工逐个列文件。无法产出"只含理光"或"只含 GR IV"的固件。
- NVS schema 是理光 BLE 身份 + Wi-Fi 凭据，没有 `brand/model` 字段，也没有按驱动划分的私有存储区。

### P7：UI 写死型号

`PairingGuide` 轮转顺序、`display.cpp` 中的 "GR II setup" 文案、`UiSnapshot` 中的 `provisioningSsid` 等字段都按理光型号写死。型号列表应由编译进固件的驱动提供。

### P8：测试与 CI 不支持按型号验证

- ~~所有测试集中在一个 2145 行的 `test/test_native/test_native_logic.cpp`。~~ 已按功能区拆为 `test/test_native_{app_flow,ble,camera_protocol,media,ui}/`（2026-10-13），按型号拆分留待阶段 6。
- ~~CI 只编译固件，不运行 `pio test -e native`。~~ 已加入 `native-test` job（2026-10-13）。
- 没有模拟相机驱动，离开真机无法测试 App 状态机。

### P9：嵌入式运行时

- **阻塞**：`main.cpp` 约 25 处 `delay()`，`ricoh_ble_client.cpp` 约 14 处。BLE 流程阻塞期间，按键依赖 `serviceButtonsDuringBleOperation()` 临时轮询补救。预览读取 / 解码 / 渲染与 BLE 流程在同一线程，ESP32-S3 双核未利用。
- **内存**：全工程约 279 处 Arduino `String`，长时间 LiveView 存在堆碎片风险；驱动接口层不应传递 `String`。
- BLE 回调与主循环之间用 `std::atomic` 交接数据，做法正确，应保留。

---

## 二、目标架构

### 2.1 分层与目录

```text
src/
├── platform/stickS3/     板级 HAL：显示、按键、IMU、电源、蜂鸣器（与相机无关）
├── core/                 Result、Logger、EventQueue、Clock、FSM 基础件
├── transport/            品牌无关传输层
│   ├── ble/              BleStack（NimBLE 生命周期、扫描、Bond 管理、安全配置下发）
│   ├── wifi/             WifiStation、SoftAP、WifiProvisioningPortal（从 GR II 抽出为通用件）
│   └── http/             HttpClient（keep-alive、超时、表单）
├── media/                MjpegFrameSource、JpegDecoder、FrameBuffer（品牌无关）
├── camera/               ★ 相机抽象层
│   ├── ICameraDriver.h          驱动接口
│   ├── CameraModelDescriptor.h  型号元数据 + 工厂函数
│   ├── CameraCapabilities.h     通用能力（预览、快门、配对方式…）
│   ├── CameraEvent.h            驱动 → App 事件
│   └── CameraRegistry.cpp       编译期生成 / 条件编译的型号表
├── app/                  品牌无关状态机：未配对 → 配对 → 连接 → 就绪 → 预览 → 拍摄
├── ui/                   型号列表、配对 UI 由 Registry 与驱动事件驱动
└── brands/
    ├── ricoh/
    │   ├── common/       理光协议原语：GATT 读写、/v1 REST 客户端、OperationMode 定义
    │   ├── gr2/          Gr2Driver（Wi-Fi only + 配网）+ gr2_config.h + 独立测试
    │   ├── gr3/          Gr3Driver（UUID、动态六位码、CAPTURE 门控）
    │   └── gr4/          Gr4Driver（固定 Handle、Legacy 安全配置）
    ├── fujifilm/         common/ + 各型号
    └── nikon/
```

### 2.2 核心接口：App 只表达意图，流程由驱动自己管

```cpp
// camera/ICameraDriver.h（示意，非最终代码）
class ICameraDriver {
 public:
  virtual ~ICameraDriver() = default;
  virtual const CameraModelDescriptor& descriptor() const = 0;

  virtual Result begin(DriverContext& ctx) = 0;   // ctx 提供 BleStack、Wifi、Storage、Clock
  virtual void   tick(uint32_t nowMs) = 0;        // 契约：非阻塞，必须在 N ms 内返回
  virtual bool   pollEvent(CameraEvent& ev) = 0;  // 状态变化、配对请求、拍摄确认、电量…

  // 意图接口（异步，结果通过事件回报）
  virtual Result requestPairing() = 0;
  virtual Result submitPasskey(uint32_t code) = 0;
  virtual Result setPreviewWanted(bool wanted) = 0;   // 横竖屏只翻译成这一个意图
  virtual Result requestShutter(const ShutterRequest&) = 0;
  virtual Result forgetPairing() = 0;

  virtual IFrameSource* frameSource() = 0;  // 统一输出 JPEG 帧；MJPEG 与 PTP 取景都能实现
};
```

关键设计取舍：

- **理光"BLE 开 AP → 连 Wi-Fi → HTTP 预览 → 竖持关 AP"整套流程下沉到理光驱动内部。** App 只表达"我要预览 / 我要拍"。BLE parking、Power / OperationMode 门控、Sleep Guard 都属于理光私有策略，App 不感知。
- **安全不变量由驱动自守**：Unknown 不写入、GR III 只有 fresh `CAPTURE` 才开 WLAN、GR II 不调用 `/v1/device/finish` 等，规则继续以 [power_state_policy.md](../power_state_policy.md) 与 [known_issues.md](../known_issues.md) 为准。App 层只提供通用约束：驱动未报告就绪时不发出拍摄意图。
- **能力用描述符 + 可选接口表达**：例如不支持预览的机型 `frameSource()` 返回 `nullptr`，而不是在公共结构体里不断增加 bool。
- **同一时间只实例化一个驱动**（只绑定一台相机），启动时按绑定信息懒创建，可放入静态内存区，节省 RAM。
- **NimBLE 是全局单例**：驱动以数据形式向 `BleStack` 提交安全配置（IO Capability、密钥分发、固定 Passkey），由 `BleStack` 统一下发，驱动不再直接修改 `ble_hs_cfg`。

### 2.3 型号独立规则

| 规则 | 说明 |
| --- | --- |
| include 白名单 | `brands/<brand>/<model>/` 只能 include `camera/`、`transport/`、`media/`、`core/` 与本品牌 `common/`；**禁止** include 其他型号、`app/`、`ui/`、`main` |
| 品牌 common 不认识型号 | `ricoh/common` 只放协议原语（GATT 读写、REST 调用），**禁止出现** `if (model == GR4)`；型号差异通过参数 / 策略对象由型号目录注入 |
| 配置独立 | 每个型号有自己的 `xxx_config.h`；原 `config.h` 只保留平台级与通用参数 |
| 存储独立 | 每个驱动使用独立 NVS namespace（如 `cam.ricoh.gr4`，≤ 15 字符）；App 只保存"当前绑定的 modelId" |
| 测试独立 | `test/test_ricoh_gr4/` 等按型号划分的测试目录，可单独运行 |
| 自动校验 | CI 对**每个单型号 env** 单独编译（`sticks3-ricoh-gr4` 能编过即证明 GR4 不依赖 GR3），并用小脚本检查 include 规则 |

### 2.4 编译期选择品牌 / 型号

`platformio.ini` 示意：

```ini
[common]
platform = espressif32@6.12.0
board = esp32-s3-devkitc-1
framework = arduino
extra_scripts = pre:scripts/link_esp_new_jpeg.py, pre:scripts/gen_camera_registry.py
base_src = +<*> -<brands/>

[ricoh_all]
src  = +<brands/ricoh/common/> +<brands/ricoh/gr2/> +<brands/ricoh/gr3/> +<brands/ricoh/gr4/>
defs = -DCAM_RICOH_GR2=1 -DCAM_RICOH_GR3=1 -DCAM_RICOH_GR4=1

[env:sticks3-ricoh]            ; 理光全系
extends = common
build_src_filter = ${common.base_src} ${ricoh_all.src}
build_flags = ${common.build_flags} ${ricoh_all.defs}

[env:sticks3-ricoh-gr4]        ; 只含 GR IV
extends = common
build_src_filter = ${common.base_src} +<brands/ricoh/common/> +<brands/ricoh/gr4/>
build_flags = ${common.build_flags} -DCAM_RICOH_GR4=1

[env:sticks3-fujifilm]  ...
[env:sticks3-all]       ...    ; 全品牌，开发调试用
[env:native-sim]        ...    ; 模拟驱动 + App 状态机测试
```

使用方式：

```bash
pio run -e sticks3-ricoh -t upload
pio run -e sticks3-ricoh-gr4
# 任意组合：预构建脚本读取环境变量，用 env.Replace(SRC_FILTER=...) 调整编译范围并生成注册表
CAMERA_MODELS=ricoh.gr3,ricoh.gr4 pio run -e sticks3-custom
```

- **注册表采用显式生成，不使用静态构造自注册**：预构建脚本根据所选型号生成 `camera_registry_gen.cpp`（或使用 X-macro + `#if CAM_*`）。启动顺序确定；将来驱动移入 `lib/` 成为静态库时，也不会被链接器当作未引用代码丢弃。
- **固件只含一个型号时，UI 自动跳过机型选择页。**
- **发布流程**：`release.yml` 按 env 矩阵构建，产物名带品牌 / 型号，如 `...-ricoh-gr4-v1.1.0-merged.bin`。

### 2.5 存储迁移（不得让现有用户重新配对）

> 2026-10-14 更新：用户升级固件总是整片擦除重烧，**不需要迁移**。阶段 4 已按此简化，见「执行记录」。本节以下内容为原始设想，仅作参考。

- 新结构：`CameraBinding { modelId, driverSchemaVersion }` + 每个驱动的私有 namespace。
- 理光驱动首次启动时读取旧 `ricoh2` v4 数据，按 `protocolGeneration` 映射为 `ricoh.gr2/gr3/gr4`，迁移完成后写入新位置。
- NimBLE 的 Bond 由协议栈自行存储，与本次迁移无关，可保留。迁移逻辑沿用现有 schema v3 → v4 的做法，并补充 Native 测试。

### 2.6 运行时优化（建议在品牌拆分之后进行）

- **驱动非阻塞**：理光驱动先整体包装，阶段性允许阻塞；之后逐步把 `delay()` 改为驱动内部由 `tick()` 推进的子状态机。
- **预览独立 FreeRTOS 任务**：读取 + 解码放在 core1，双缓冲交给 UI，避免 BLE 长操作卡住画面和按键。
- **驱动边界不传 `String`**：统一使用定长 `char[]` 或 `const char*`，降低堆碎片风险。

---

## 三、待决策事项

### 品牌内共享程度（方案 A / B）

GR III 与 GR IV 约 60–70% 的流程相同（电源门控、HTTP 预览、恢复）。

| 方案 | 做法 | 优点 | 缺点 |
| --- | --- | --- | --- |
| **A（推荐）** | 相同部分放入 `ricoh/common`，作为"不认识型号的流程组件" | 重复少，修 bug 只改一处 | common 一改会影响多个型号，需要多型号回归 |
| B | 每个型号自带完整流程，允许代码重复 | 隔离最彻底 | 修一个 bug 需要改多处 |

"完全不耦合"更接近方案 B。建议采用方案 A，并以"common 禁止出现型号分支"的规则加 CI 单型号编译来约束。

**决定：采用方案 A（2026-10-13 作者确认）。** `ricoh/common` 只放不认识型号的协议原语与流程组件，禁止出现型号分支；型号差异由 `ricoh/<model>/` 以参数 / 策略对象注入，并以 CI 单型号编译约束。

---

## 四、迁移路线

每一步都必须可编译、可实机回归、可回滚；沿用 [architecture_refactor_plan.md](architecture_refactor_plan.md) 的小步迁移原则。

| 阶段 | 内容 | 风险 | 必须验证 |
| --- | --- | --- | --- |
| 0 | CI 加入 `pio test -e native`；整理各型号实机回归清单作为行为基线 | 无 | CI 绿；基线清单覆盖 GR II / III / IIIx / IV |
| 1 | 新建 `camera/` 接口、Registry、`transport/ble/BleStack` 骨架，只增不改 | 低 | `pio run` 与 Native 测试通过，行为无变化 |
| 2 | **Strangler 包装**：把 `main.cpp` 的理光流程与 `AppFlowActions` 整体搬入 `brands/ricoh/`，包装成 `RicohDriver` 适配器；`main` / `AppController` 只经 `ICameraDriver` 交互。理光内部暂时仍耦合，但对外已隔离 | **高** | GR II / III / IV 全量实机回归：配对、重连、横竖切换、快门、Sleep Guard |
| 3 | App 状态机通用化；UI 型号列表改由 Registry 驱动；新增 `SimCameraDriver` 做 Native 测试 | 中 | 状态机测试覆盖主要转移；配对引导行为不变 |
| 4 | 存储拆分：驱动私有 namespace + 应用设置独立（升级整片擦除，不做迁移；`CameraBinding` 移到阶段 5） | 低 | 擦除后首次配对、重启重连、长按 B 重置、显示镜像保存 |
| 5 | 构建系统：多 env、生成注册表、CI 按型号矩阵编译（兼作耦合检测器） | 低 | 每个单型号 env 都能独立编译 |
| 6 | 理光内部拆分：**先 GR II**（本就独立，最容易）→ GR IV → GR III；`ricoh_ble_client.cpp` 拆为 `ricoh/common` 原语 + 各型号流程；`config.h` 按型号拆分 | 中高 | 每拆一个型号即做该型号实机回归 |
| 7 | 第二品牌样板驱动（如富士），先实现配对 + 快门，用于验证抽象是否足够 | 取决于协议逆向 | 不需要修改 `app/`、`ui/` 即可接入 |
| 8 | 运行时优化：预览任务、驱动非阻塞、去 `String` | 中 | 长时间 LiveView 内存稳定，FPS 不下降 |

### 执行记录

**2026-10-13 阶段 0–2**

- 阶段 0：CI 新增 `native-test` job；迁移前未建立实机行为基线清单，改为迁移后对照串口日志回归。
- 阶段 1：只新增了 [`camera/ICameraDriver.h`](../../src/camera/ICameraDriver.h)（`ICameraDriver` + `ICameraHost` + `CameraStatus`）。Registry 与 `transport/ble/BleStack` 推迟到第一次真正使用它们的阶段（Registry → 阶段 3/5，BleStack → 阶段 6），避免重新引入空骨架。
- 阶段 2：理光流程从 `main.cpp` 原样搬入 [`brands/ricoh/RicohDriver.cpp`](../../src/brands/ricoh/RicohDriver.cpp)。`main.cpp`（约 800 行）只剩板级、UI、按键与 Supervisor，经 `ICameraDriver` 驱动相机，驱动经 `ICameraHost` 回调显示 / 输入。`AppController` 现在只在驱动内部使用。
  - 校验方式：原 155 个函数中 129 个逐字未变，其余 24 个只改了跨边界调用；全部日志 / UI 字符串的出现次数与迁移前一致；固件编译通过，Native 121 个用例通过。
  - 主动保留到后续阶段的耦合：`main.cpp` 直接实例化 `RicohDriver`（阶段 5 由注册表替代）；配对引导 `PairingGuide` 仍使用 `RicohProtocolGeneration`，经 `CameraModelId` 透传（阶段 3）；`CameraStatus.appState` 仍是理光工作流状态（阶段 3 通用化）；显示镜像设置与相机档案共用 `ricoh2` 的 `CameraProfileStore`，由 `main` 注入驱动（阶段 4 拆分）；驱动接口仍传递 `String`（阶段 8）。
  - 实机回归（2026-10-13/14）：GR III 系列与 GR IV 通过首次配对、竖屏 BLE 快门、横屏 LiveView + BLE 快门、横竖反复切换、关机保护与手动唤醒；GR IV 另验证了重启后快速重连与长按 B 重置配对。GR II 暂无实机，回归清单见 [known_issues.md](../known_issues.md)「支持与证据限制」第 4 条。
  - 回归中发现 GR IV 的 AP 每次 WLAN ON 可能换信道（6 / 11 交替），按缓存信道连接常需约 14 s；已加 `wifiChannelChangesOnRestart` 能力位缩短信道猜测，并让全信道扫描不再被重连打断，GR IV 进入 LiveView 降到约 4–7 s。
  - 驱动接口暂时包含 `beginTick / serviceFlowTick / serviceBackgroundTick` 三段式 tick，用来保持原主循环的调用顺序（Supervisor 夹在中间）。驱动改为非阻塞（阶段 8）后合并为单个 `tick()`。

**2026-10-14 阶段 3**

- 3a 机型注册表：[`camera/CameraRegistry`](../../src/camera/CameraRegistry.h) 提供配对引导的机型列表（`CameraModelDescriptor`：id、显示名、配对前提示），理光的列表在 [`brands/ricoh/RicohModels.h`](../../src/brands/ricoh/RicohModels.h)。`PairingGuide`、`UiSnapshot`、`display.cpp` 改为按注册表索引工作，不再写死 GR II / III / IV；列表顺序、默认项（GR III）、B 键循环顺序与提示文案均与之前一致。
- 3b 相机阶段：新增品牌无关的 [`CameraPhase`](../../src/camera/CameraPhase.h)。`RicohDriver` 经 [`ricohCameraPhase()`](../../src/brands/ricoh/RicohCameraPhase.h) 把工作流状态映射过去，`CameraStatus` 只暴露 `phase` 和仅用于日志的 `stateName`。`UiCoordinator::selectScene()` 与 `SystemSupervisor` 改用 `CameraPhase`；未使用的 `connectionPhaseFloor()` 与 `_stateChangedAtMs` 删除。原 UI / Supervisor 测试保持原断言，改为经映射函数构造输入，另加覆盖全部 22 个状态的映射表测试。
- 结果：`main.cpp`、`ui/`、`supervisor/`、`camera/` 不再引用 `AppState` 或 `RicohProtocolGeneration`。`app/` 里的 `AppController` 实际只被 `RicohDriver` 使用，已移到 [`brands/ricoh/flow/`](../../src/brands/ricoh/flow/RicohFlowController.h) 并改名：`AppController` → `RicohFlowController`、`AppState` → `RicohFlowState`、`AppFlowActions` → `RicohFlowActions`、`AppTickPlan` → `RicohTickPlan`（纯改名，固件 Flash 字节数不变、日志字符串不变）。`app/` 改放品牌无关的应用逻辑（见 3c）。
- 3c 应用运行时：`main.cpp` 中的应用决策搬入品牌无关、可 native 编译的 [`app/AppRuntime`](../../src/app/AppRuntime.h)：每轮主循环（`tick()`）、按键分发、配对引导、LiveView 锁、姿态 → 预览意图、配网期输入、启动收尾。设备能力经 `IAppPlatform` 由 `main.cpp` 提供（按键、长按电源、关机、电量、UI 刷新、Supervisor、镜像、日志）。搬移后的函数体与原版逐语句比对一致（仅 `printf` 改为 `snprintf` + `log`，输出相同）。`ICameraDriver.h` 去掉 Arduino 依赖：`ICameraHost::showStatus` 改收 `const char*`，`PreviewFrameBufferStorage` 拆到独立头文件。
- [`SimCameraDriver`](../../test/test_native_app_runtime/SimCameraDriver.h) + 记录型 `FakeAppPlatform`：新套件 `test_native_app_runtime` 的 12 个用例按调用日志断言主循环顺序（含 Supervisor 位于 flow 与 background tick 之间）、关机 / 自动关机短路、快门 / 重置 / 镜像 / 锁的路由、配对引导吞掉输入并确认选中机型、配网循环。Native 共 136 例。
- 实机冒烟（2026-10-14，GR III 系列）：选机型界面显示与 B 循环、确认配对、重启快速重连、竖屏 / 横屏快门、横竖切换、LiveView 锁定与解锁、长按电源关机均正常。

**2026-10-14 阶段 4（存储拆分）**

- 前提变更：用户升级固件总是整片擦除重烧，NVS 不会残留旧数据，因此不做迁移，旧格式读取路径全部删除。
- 理光档案：[`CameraProfileStore`](../../src/camera_profile_store.h) 改为 NVS namespace `cam.ricoh` 中单个 `profile` JSON 快照（schema v4）。删除约 25 个“兼容镜像”逐字段 Key 的写入、v3 → v4 读取与迁移（`StoredCameraProfileMetadata`、`migratedLegacyGr4`、`camera_profile_schema.cpp`）、只写旧 Key 的 `saveBleIdentity()`、未使用的 `clear()` / `clearWifiCredentials()`；`clearBlePairing()` 改名 `clearProfile()`。文件 407 → 139 行，固件 Flash 减少约 4.9 KB。
- 应用设置：显示镜像移到 [`platform/AppSettingsStore`](../../src/platform/AppSettingsStore.h)（namespace `app`），长按 B 重置配对不影响它，与之前一致。
- `RicohDriver` 自己持有档案存储，不再由 `main.cpp` 注入，消除阶段 2 记录的“共用 `ricoh2`”耦合。
- `CameraBinding`（记录相机归属哪个驱动）推迟到阶段 5：目前只有一个驱动，没有可选项，提前加只是无人读取的代码。
- 测试：删除两个只覆盖旧格式迁移的用例；Native 134 例。实机验证（2026-10-14，GR III 系列，整片擦除后）：首次配对与 Wi-Fi 缓存写入、重启后档案读回并快速重连、缓存 Wi-Fi 参数命中（开机直接横屏 0.36 s 连上）、镜像设置重启后保持、长按 B 重置后档案清空而镜像保留、重新配对均通过。

**2026-10-14 阶段 6（进行中）**

- 顺序调整：GR II 暂无实机，最后拆；BLE 层每个操作的 `switch` 同时含 GR III / GR IV 两个分支，搬迁必然两者同步，因此 BLE 层改动需两台相机都回归，驱动流程层再按型号分步。
- 6.0 纯移动：`src/ricoh/` → `brands/ricoh/{common,gr3,gr4}`（固件字节数不变）。`config.h` 中理光 GATT 常量按型号拆到 `common/RicohBleUuids.h`、`gr3/Gr3BleConfig.h`、`gr4/Gr4BleConfig.h`，由 `config.h` 继续 include（固件字节数不变）。
- 6.1 BLE 操作下沉到型号：新增 [`RicohGattSession`](../../src/brands/ricoh/common/RicohGattSession.h)（handle / UUID 读写、受保护读取、订阅、通知路由、链路安全、日志；无 NimBLE / Arduino 类型），`RicohBleClient` 内的 `NimBleGattSession` 在 NimBLE 上实现。`openWifi`、`closeWifi`、`readPowerState`（读取部分）、`enablePowerStateNotify`、`requestCaptureOperationMode` 的按传输方式 `switch` 改为调用 `Gr3BleProtocol` / `Gr4LegacyBleProtocol` 的同名行为，错误文本与日志逐字保留（仅删去 4 个不可达的 `transport unavailable` 默认分支）；`IRicohBleProtocol` 去掉三个传输方式枚举。新套件 `test_native_ricoh_gatt` 用记录型 `FakeGattSession` 断言各型号写入的 handle / UUID / 字节、日志与失败文本（13 例，Native 共 147 例）。
  - 实机回归（2026-10-14）：GR III 系列与 GR IV 均通过（用户确认）。
- 6.2 Wi-Fi 凭据读取下沉：`IRicohBleProtocol::readWifiCredentials()` 负责一次轮询（GR IV 读 5 个固定 handle，GR III 读 3 个 UUID），超时轮询与日志仍在 `RicohBleClient`。GR IV 的约 270 行 `String` 文本解析逐语句移植为标准 C++ 的 [`gr4/Gr4WifiCredentialParser`](../../src/brands/ricoh/gr4/Gr4WifiCredentialParser.h)（`trim` / 大小写 / `indexOf` / `substring` 按 Arduino `String` 语义实现）；新增与 Arduino 无关的 `RicohWifiCredentials`，返回前转换为驱动使用的 `RicohBleWifiCredentials`。`RicohGattSession` 增加 `pause()` 以保留原读取间隔（GR IV 每个 handle 后、GR III 仅成功读取后）。字符串字面量逐条核对一致。`ricoh_ble_client.cpp` 2732 → 2337 行；`test_native_ricoh_gatt` 增至 22 例（解析器各输入形态 + 两型号轮询顺序 / 停顿 / 中止），Native 共 156 例。解析器测试依据对原 `String` 语义的理解编写、未与原实现直接比对，GR IV 实机凭据读取为最终验证。
  - 实机回归（2026-10-14）：GR III 系列与 GR IV 重新配对后凭据读取均正常（用户确认）。
- 6.3 协议识别下沉：证据收集 `collectProtocolEvidence()` 不拆（其 GATT 访问顺序与刷新次数影响相机是否中途断开），只把判定移入型号：`IRicohBleProtocol::matchEvidence()` 返回 None / Partial / Full，`detectionPriority()` 表示特异度（GR IV 2 > GR III 1）。`RicohBleProtocolRouter::detect()` 用通用规则：取特异度最高的 Full；若有更高特异度的型号出现 Partial 则判为未知。原 `detectRicohProtocol()` 从 `camera_protocol_profile` 移除，同名函数改由路由器提供。新测试把原判定函数逐字保留为参照，对 15 个布尔证据 × 计数 0–6 共 229,376 种组合逐一比较，全部一致；人为把 GR IV 特异度改低会立即失败。阶段 5 只编入部分型号时，路由器只询问已编入型号，其他相机会被判为未知。
- 6.4 / 6.5 状态通知与连接 / 配对中的型号分支：`statusNotifyAllowed()`（GR III 要求认证加密，订阅过程本身两型号相同、留在共用代码）；GR III 的“异步读 SSID 触发配对”移为 `armPairing()`（会话新增 `startRead()`），`triggerPairingWithProtectedRead()` 保留为门控，因为它还决定配对前是否做完整服务发现；认证失败后的探测读移为 `probeAuthenticationError()`（会话新增 `probeRead()`）；GAP 事件处理器不再使用 GR IV 的开 / 关常量，改用路由该 handle 的型号的 `decodePowerState()`（路由时一并记录型号，断开时随 handle 清零），对 GR IV 的 256 个取值逐一验证与旧常量一致。`ricoh_ble_client.cpp` 中除证据收集与快门日志外不再出现型号常量。Native 共 162 例。
- 6.6 型号规则改为配置参数：`camera_protocol_profile.cpp` 中按代际分支的策略改为读取 `CameraProtocolProfile` 的新参数——`securityProfile`、`wifiRequiresOperationModeRead`、`waitsInBleStartupMode`、`wifiRequiresAuthenticatedLink`、`discoveryConnectionPromotable`；`protocolAllowsBleSideEffect` 改看 `capabilities.hasBle`。各型号配置表、安全配置与 GR III 凭据校验移到 `brands/ricoh/{gr2,gr3,gr4}/Gr*Profile.cpp`（函数内静态对象，避免跨文件静态初始化顺序问题）。驱动的 GR III 安全门控改看 `wifiRequiresAuthenticatedLink`（日志改用代际名，输出仍为 `GR3_FAMILY`），配对引导默认机型改取注册表默认项；`RicohDriver.cpp` 不再出现 GR III / GR IV 代际常量。旧配置表与策略函数逐字保存在测试参考文件中：配置表逐字段比较一致；策略对 4 个代际 × 256 个模式字节 × 读取成败 × 全部副作用 / 安全配置穷举比较一致；人为修改 GR IV 参数会立即失败。Native 共 165 例。GR III 启动模式等待与快门 `BLE_STARTUP` 重试本身已由 `waitsInBleStartupMode` 驱动，流程代码留在驱动内（与驱动状态紧耦合），日志文字保留 “GR3”。
- 6.7a GR II 模块下沉（2026-10-17）：`gr2_provisioning_portal` / `gr2_provisioning_logic` / `gr2_shutter_policy` 移为 `gr2/Gr2ProvisioningPortal`、`Gr2ProvisioningLogic`、`Gr2ShutterPolicy`；`GrApi::shootGr2` 改为 `gr2/Gr2HttpShutter` 中的自由函数 `shootGr2(GrApi&)`，经新公开的 `GrApi::postCommand`（原 `postGr2Command`）/ `setError`。`RicohDriver` 经 `RicohGr2Support.h` 使用 GR II 组件，`RVF_RICOH_GR2=0` 时为空实现，单型号固件不再链接配网 Web 服务器（`sticks3-ricoh-gr3` Flash −53 KB）。
- 6.7b GR II 分支改为配置参数：`isGr2WifiOnlyProfile()` 改为 `storedCameraIsWifiOnly()`，读已存相机的配置（`cameraIsWifiOnly()`：Wi-Fi 启动方式为 ManualOnly）。GR II 的 HTTP 差异成为 `Gr2Profile` 参数：`liveViewWithoutMaxFps`、`fixedModelLabel`（"RICOH GR II"，无 `/v1/props`）、`httpShutterUsesGrRemoteSequence`、`httpWlanFinishPowersOff`；选机型改用 `cameraNeedsManualWifiProvisioning()`。`RicohDriver.cpp` 不再出现 `Gr2Family`。新测试对 0–255 每个代际字节检查各参数恰好只选中 GR II；人为改掉 GR II 参数会立即失败。字符串字面量除改由配置提供的型号名外逐条一致。Native 共 170 例。
- 6.7c 单 GR II 固件：新增 `sticks3-ricoh-gr2`（Flash 1,474,485 字节，BLE 栈仍编入，GR III / IV 相机按识别规则拒绝）与 `native-gr2`，CI 同步。

**2026-10-14 阶段 5（编译期选择型号）**

- 开关：[`brands/ricoh/RicohModelSelection.h`](../../src/brands/ricoh/RicohModelSelection.h) 定义 `RVF_RICOH_GR2/GR3/GR4`，默认全开（全量固件），全关时编译报错。注册表、协议路由、配置与安全配置查表、配对策略均按开关编入；未编入型号的配置查表返回 Unknown 配置，所有 BLE 副作用被阻断。
- 安全：GR IV 暴露全部 GR III 标记，若单 GR III 固件丢掉 GR IV 的识别规则，会把 GR IV 当 GR III 驱动。因此识别规则拆为纯函数 `gr3/Gr3Detection`、`gr4/Gr4Detection`，**所有固件都编入**；路由器对所有已知型号做识别，胜出者未编入时判为未知。全量构建下与 6.3 结果一致（6.3 的 229,376 组合穷举测试仍通过）。
- 未识别相机前使用的安全基线（即 GR IV legacy 配置，逐字不变）移到 `common/RicohDiscoverySecurityBaseline`，GR IV 的配对策略调用它，单 GR III 固件也保有该基线。
- 构建：`platformio.ini` 新增 `sticks3-ricoh-gr3`、`sticks3-ricoh-gr4`（排除其他型号源码，仅保留其识别规则），能独立链接即证明型号间无链接级耦合；native 拆为可组合的源码清单，新增 `native-gr3` / `native-gr4` 运行 `test_native_model_selection`（注册表、选择、未知配置、未编入型号的相机被拒绝而非误认）。新增 `scripts/check_model_includes.py` 检查型号目录不 include 其他型号、`app/`、`ui/`、`main`（人为违规可检出）。CI 构建矩阵与 native 任务同步加入。发布流程仍只发布全量固件，是否发布单型号固件待产品决定。
- 依赖固定：新环境暴露出 `lib_deps` 未固定版本的问题——M5Unified 跟随 GitHub 默认分支、M5GFX 只有 `>=0.2.25` 约束，每次全新安装（含 CI）都会取最新版（新环境拿到 M5Unified 0.2.26 / M5GFX 0.2.32，而实机验证用的是 0.2.18 / 0.2.25）。已固定为实机验证版本：M5GFX `#0.2.25`、M5Unified `#0.2.18`、M5PM1 `#1.0.7`（均为 Git 标签；PlatformIO 注册表已无 M5GFX 0.2.25）。全量固件字节数与固定前完全一致。`esp_new_jpeg` 下载链接无版本，但两次安装均为 v1.0.2，暂不处理。
- 共用代码中剩余的按代际 `switch` 均为注册表式查表并已接入开关。

**2026-10-14 统一实机回归（6.3–6.6、依赖固定、阶段 5）**

- GR III 系列与 GR IV 均通过（用户确认），覆盖：全量固件下两台相机的配对与协议识别、状态通知、横屏预览与快门、横竖切换、关机保护与唤醒、重启快速重连；单 GR IV / 单 GR III 固件的选机型列表与正常使用；单 GR III 固件拒绝 GR IV 相机（识别为 UNKNOWN，不配对、不开相机 Wi-Fi）。
- 阶段 6 剩余：6.7 GR II Wi-Fi-only 路径（2026-10-17 代码完成，待 GR II 实机回归）。

### 执行注意事项

- 阶段 2 是关键且风险最高的一步，必须先完成阶段 0 的行为基线。
- 在定稿接口前，先抓取一个富士或尼康的真实交互流程对照，避免接口只是从理光"抽象"出来、第二个品牌接入时又要重写。
- 品牌拆分与运行时并发优化不要放在同一个提交中，两者的回归方式完全不同。
- 所有涉及相机唤醒的改动仍需同时阅读 [power_state_policy.md](../power_state_policy.md) 与 [ricoh_ble_protocol.md](../ricoh_ble_protocol.md)。

---

## TODO_UNVERIFIED

- 富士、尼康的 BLE 配对、Wi-Fi 启动方式与实时取景协议（PTP/IP 等）需抓包或查阅逆向资料确认，再据此定稿 `ICameraDriver` 与 `IFrameSource`。
- 各单型号 env 的 Flash / RAM 占用（当前全量固件 Flash 约 43.6%）。
