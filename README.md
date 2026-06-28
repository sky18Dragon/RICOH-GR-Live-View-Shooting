# RICOH GR StickS3 Remote Viewfinder

[![PlatformIO](https://img.shields.io/badge/PlatformIO-v6.12.0-blue.svg)](https://platformio.org/)
[![Board](https://img.shields.io/badge/Board-ESP32--S3-orange.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/index.html)
[![Hardware](https://img.shields.io/badge/Hardware-M5Stack--StickS3-red.svg)](https://docs.m5stack.com/en/core/m5stamp_s3)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

[English Version](README_EN.md)

一个运行在 **M5Stack StickS3** 上的理光 RICOH GR 智能无线电子取景器 (EVF) 与遥控快门固件。

---

## 📸 现场演示 / Showcase

> 💡 **提示**：可在此处替换或放入您的硬件组装与实际工作照片。建议存放于项目 `docs/images/` 目录下。

| 📸 手持遥控 (Remote Monitor & Shutter) | 🎬 腰平取景工作实拍 (In Action) |
| :---: | :---: |
| ![RICOH GR StickS3 Remote Monitor & Shutter](docs/images/hardware_setup.jpg) | ![Waist Level Viewfinder Action](docs/images/liveview_action.jpg) |

---

## 🎯 制作目的与主要场景

本固件专为理光 RICOH GR 系列相机的街头摄影与创意拍摄而设计，旨在打造一个**迷你无线监视器**：

1. **腰平取景器 (Waist-Level Viewfinder)**：将 StickS3 固定在相机热靴上或手持，无需低头或趴在地上，即可通过手里的屏幕完成低角度、街头暗访等腰平视角的精准构图。
2. **远程监视遥控器 (Remote Monitor & Shutter)**：相机架设在三脚架上时，可在最远 10 米外手持 StickS3 实时预览画面并遥控快门，非常适合合影、延时摄影或自拍。

---

## 🚀 核心工作流

固件实现了**完全免人工配置**的自动化连接：

```text
[开机上电] 
   │
   ▼
[自动 BLE 扫描] ────► 发现首选理光相机
   │
   ▼
[自动 BLE 建立加密连接] ────► 注入理光验证 Passkey 123456
   │
   ▼
[自动激活相机 Wi-Fi] ────► 通过 BLE 下发 WLAN 开启指令并读取临时 SSID/密码
   │
   ▼
[自动连接相机 Wi-Fi] ────► ESP32 切换至 STA 模式接入相机热点
   │
   ▼
[进入实时预览 (LiveView)] ──► HTTP /v1/liveview 视频流解码 + 硬件快门触发 (GPIO11)
```

---

## ✨ 核心特性

- **动态凭据免配置**：理光相机每次开启 Wi-Fi 生成的密码不同。本固件通过 BLE 实时读取相机参数区（SSID / 密码 / BSSID），实现真正的**即开即连**。
- **边缘中断快门 (GPIO11)**：外接瞬时快门按键使用硬件 GPIO 中断（`FALLING` 沿）进行事件锁存，保证即使在 CPU 高负载进行 JPEG 解码时也绝不漏按。
- **低延迟双缓冲渲染**：采用 `JPEGDEC` 解码库在 PSRAM 中直接输出大端序 RGB565 像素，LovyanGFX 双缓冲合并透明 HUD 信息（FPS、电量、RSSI、帧率统计）后一次性推送至 LCD，彻底消除屏幕闪烁。
- **RF 共存与防锁死保护**：
  - 显式启用 Wi-Fi 节能睡眠模式 (`WiFi.setSleep(true)`)，确保 ESP32-S3 单天线在 BLE 与 Wi-Fi 共存时连接稳定；
  - 连接连续失败时，自动重构 BLE 协议栈 (`resetStack`)，解决底层 NimBLE 偶发性死锁问题。
- **本地配置文件持久化**：使用 ESP32 NVS 存储首选相机的 BLE 地址和名称。开机自动寻机重连，未发现首选设备时自动进入配对扫描模式。

---

## 🛠️ 硬件需求与组装

1. **控制板**：[M5Stack StickS3](https://docs.m5stack.com/en/core/m5stamp_s3)
2. **相机**：RICOH GR III / GR IIIx / GR IV 或兼容 BLE 控制的理光机型。
3. **外接快门按键**：接在 **GPIO11 (G11)** 与 **GND** 之间（固件使用 `INPUT_PULLUP`），可组装在手柄或快门线上。

---

## 💻 固件烧录

基于 PlatformIO 环境开发，配置位于 [platformio.ini](platformio.ini)。

```bash
# 1. 编译固件
platformio run

# 2. 烧录到 StickS3 模块
platformio run -t upload

# 3. 串口日志监控（波特率 115200）
platformio device monitor
```

---

## ⚙️ 开发环境与依赖

本固件基于 **PlatformIO IDE** (VS Code 插件版或 CLI 版) 进行开发和构建，配置位于 [platformio.ini](platformio.ini)。

### 1. 硬件配置
* **芯片**：ESP32-S3-PICO-1-N8R8 (双核 Xtensa LX7，主频 240MHz)
* **存储**：8MB QSPI Flash + 8MB OPI PSRAM (`qio_opi` 内存模式)
* **分区表**：8MB Default 分区 (`default_8MB.csv`)

### 2. 工具链与框架
* **编译平台**：PlatformIO `espressif32@6.12.0` (ESP-IDF 依赖的底层工具链)
* **核心框架**：Arduino Core for ESP32

### 3. 核心库依赖
* **[M5Unified](https://github.com/m5stack/M5Unified)**：M5Stack 统一驱动库，用于管理 LCD 显示屏、背光、电量及按键输入。
* **[M5PM1](https://github.com/m5stack/M5PM1)**：用于 StickS3 内部电源管理。
* **[NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) (v2.5.0)**：轻量级 BLE 协议栈，提供高连接可靠性与低内存消耗，解决原版 BLE 协议栈易锁死的问题。
* **[JPEGDEC](https://github.com/bitbank2/JPEGDEC) (v1.8.2+)**：超轻量级 JPEG 高性能解码器，支持在 PSRAM 中进行流式分块解码。
* **[ArduinoJson](https://github.com/bblanchon/ArduinoJson) (v7.0.0+)**：用于快速解析理光相机的 HTTP JSON 数据。

---

## 🕹️ 按键与交互控制

- **G11 快门按键 (外接)**：按下即执行 BLE 快门序列（半按对焦 $\rightarrow$ 全按曝光 $\rightarrow$ 释放快门）。如果当前处于断开状态，按下 G11 会自动拉起相机连接恢复流程。
- **Button B (StickS3 侧边键)**：暂停 / 恢复 LiveView。暂停图传后，Wi-Fi 连接和 HTTP 请求会被断开，以最大化省电；再次按下时重新通过 BLE 唤醒 Wi-Fi 并连接。
- **Button A (StickS3 正面键)**：保留，作为扩展交互接口。

---

## 🔗 理光 BLE 特征值规范

固件通过对以下 BLE 服务和特征句柄（适配理光 GR4/GR3 协议）进行读写，完成控制与参数获取：

| 功能 (Feature) | 句柄 (GATT Handle) | 操作 (Operation) | 载荷规范 (Payload) |
| :--- | :---: | :---: | :--- |
| **Wi-Fi 开启开关** | `0x0135` | Write | 写入 `0x01` 激活热点 |
| **Wi-Fi SSID** | `0x0138` | Read | 动态获取相机 SSID |
| **Wi-Fi 密码** | `0x013A` | Read | 动态获取 Wi-Fi 密码 |
| **Wi-Fi BSSID** | `0x0140` | Read | 动态获取 AP MAC 地址 |
| **快门控制** | `0x0099` | Write | 序列：对焦 `0x01` $\rightarrow$ 拍摄 `0x02` $\rightarrow$ 释放 `0x00` |

---

## 🛡️ 异常恢复保护机制

1. **画面卡死监测 (Watchdog)**：若图传解析线程在持续 `LIVEVIEW_STALL_TIMEOUT_MS` (默认 5 秒) 内未能收到并解码有效帧，将判定为相机丢包卡死，自动切断连接并尝试热启动。
2. **快速热启动 (Warm Reconnection)**：当 Wi-Fi 因信号差断开时，若 BLE 链路依然保持连接，固件不会进行重新扫频。它会在 `BleReady` 状态下，重新发送 Wi-Fi ON 指令并快速尝试 Wi-Fi 重连。
3. **冷启动 (Cold Reset)**：若 BLE 链路同样断开，固件将关闭所有网络传输，重置 BLE 协议栈，并退回 `BleScan` 状态进行全局重新搜寻与配对连接。
4. **配对与首次使用**：若 NVS 中无绑定记录，固件在启动时会自动进行 `FIRST_BOOT_BLE_PAIRING_ATTEMPTS = 12` 轮的全局配对扫描，并注入 Passkey `123456` 进行配对。
