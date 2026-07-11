# Pin Map

## 从代码确认的引脚使用方式

| 功能 | 代码位置 | 当前确认 |
| --- | --- | --- |
| I2C SDA/SCL for M5PM1 | `src/main.cpp::beginStickPower()` | 使用 `M5.getPin(m5::pin_name_t::in_i2c_sda)` 和 `M5.getPin(m5::pin_name_t::in_i2c_scl)` 获取，不在代码中硬编码 GPIO |
| M5PM1 I2C 地址 | `M5PM1_DEFAULT_ADDR` | 来自 M5PM1 库，代码未展开具体值 |
| M5PM1 I2C 频率 | `M5PM1_I2C_FREQ_100K` | 100kHz |
| Button A | `src/buttons.cpp` | `M5.BtnA.wasPressed()` |
| Power button | `src/buttons.cpp` / `src/main.cpp` | `M5.BtnPWR.wasHold()` + `M5PM1.btnGetState()` |
| LCD | `src/display/M5DisplaySurface.cpp` / `src/jpeg_decoder.cpp` | M5Unified / M5.Display / M5Canvas，未在代码中硬编码 SPI GPIO |
| USB CDC serial | `platformio.ini` | `ARDUINO_USB_CDC_ON_BOOT=1` |

## TODO_UNVERIFIED

- Button A 的实际 GPIO 编号。
- Power button 的实际 GPIO / PMIC event 连接方式。
- LCD SPI pins、backlight pin、reset pin。
- I2C SDA/SCL 对应 ESP32-S3 GPIO 编号。

## GPIO 修改规则

1. 新增外设前必须查 M5Stack StickS3 官方 pin map，并在本文件记录来源。
2. 不允许根据网上其他 ESP32-S3 板卡猜测 StickS3 引脚。
3. 不允许占用 M5Unified/M5PM1/LCD/USB 已使用引脚。

## 后续 Codex 修改代码时必须注意

- GPIO 信息缺失时只能写 `TODO_UNVERIFIED`，不能编造。
- 外设初始化失败必须有日志和 fallback，不要阻塞主循环。
