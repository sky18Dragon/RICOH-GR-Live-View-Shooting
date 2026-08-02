# Hardware Notes

## 已确认硬件/软件环境

从 `platformio.ini` 和代码确认：

- MCU/板级目标：ESP32-S3 / M5Stack StickS3。
- PlatformIO board：`esp32-s3-devkitc-1`。
- Flash/PSRAM 注释：8MB Flash、8MB PSRAM。
- Arduino USB CDC：`ARDUINO_USB_CDC_ON_BOOT=1`。
- USB mode：`ARDUINO_USB_MODE=1`。
- PSRAM：代码使用 `psramFound()` 检查，并优先用 `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` 分配 256KB JPEG frame buffer。
- PMIC/电源：`M5PM1`，代码通过 `M5PM1_DEFAULT_ADDR` 和 `M5PM1_I2C_FREQ_100K` 初始化。
- 显示：M5Unified/LovyanGFX，横屏 240x135，`M5Canvas` 双缓冲。
- 按键：`M5.BtnA`、`M5.BtnB`、`M5.BtnPWR`；另有 M5PM1 power button state 轮询。GR III 首次配对时，A 修改数字，短按 B 确认数字。

## 外设与库

- BLE：NimBLE-Arduino 2.5.0。
- Wi-Fi：ESP32 Arduino `WiFi.h`。
- HTTP：裸 `WiFiClient`。
- JPEG：`bitbank2/JPEGDEC`。
- JSON：ArduinoJson 7.x 依赖已配置，但 `gr_api.cpp` 当前以手写解析提取 props 字段。
- NVS：`Preferences`。

## 相机侧

- 默认 camera IP：`192.168.0.1`。
- LiveView：HTTP `/v1/liveview`。
- Props：HTTP `/v1/props`。
- GR IV 固定 handle 路径已在 GR IV / GR IV HDF 上实测。GR III UUID 路径已在 GR III HDF 上实测。

## TODO_UNVERIFIED

- StickS3 LCD、Button A、Power Button、I2C 的实际 GPIO 编号未在代码中直接写死；M5Unified/M5PM1 动态封装。
- 是否所有 StickS3 硬件批次都使用相同 PMIC/引脚映射，需要查官方板级资料或实机确认。

## 维护注意事项

- 新增外设前必须先更新 `pin_map.md`，确认 GPIO 冲突。
- 不要绕过 M5Unified 的板级 pin API 直接猜测 GPIO。
- 大内存需求优先使用 PSRAM，并考虑 PSRAM 不存在时的 fallback。
