# 多品牌 / 多型号相机架构重构计划

> 状态：已定稿方案 A；阶段 0 的 CI 部分、阶段 1（仅接口）、阶段 2 代码迁移已完成（2026-10-13），阶段 2 已在 GR III 系列与 GR IV 上实机回归，GR II 尚未回归。详见「四、迁移路线」下的「执行记录」
> 日期：2026-10-13
> 分析基线：分支 `feature/firmware-optimization`
> 关联文档：[architecture_refactor_plan.md](architecture_refactor_plan.md)、[../project_overview.md](../project_overview.md)、[../power_state_policy.md](../power_state_policy.md)

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

### 执行注意事项

- 阶段 2 是关键且风险最高的一步，必须先完成阶段 0 的行为基线。
- 在定稿接口前，先抓取一个富士或尼康的真实交互流程对照，避免接口只是从理光"抽象"出来、第二个品牌接入时又要重写。
- 品牌拆分与运行时并发优化不要放在同一个提交中，两者的回归方式完全不同。
- 所有涉及相机唤醒的改动仍需同时阅读 [power_state_policy.md](../power_state_policy.md) 与 [ricoh_ble_protocol.md](../ricoh_ble_protocol.md)。

---

## TODO_UNVERIFIED

- 富士、尼康的 BLE 配对、Wi-Fi 启动方式与实时取景协议（PTP/IP 等）需抓包或查阅逆向资料确认，再据此定稿 `ICameraDriver` 与 `IFrameSource`。
- 各单型号 env 的 Flash / RAM 占用（当前全量固件 Flash 约 43.6%）。
