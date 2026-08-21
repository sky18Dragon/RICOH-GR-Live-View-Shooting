# GR III / GR IV 配对引导设计

## 用户流程

1. NVS 中没有已绑定相机身份时，停留在机型选择页，不自动扫描。
2. 按 B 在 `GR III` 与 `GR IV` 之间切换，按 A 确认。
3. 确认后应用对应安全 profile，再扫描 RICOH GR 设备。
4. 对候选设备先执行只读 GATT 发现；检测代际与选择不一致时，在正式安全配对前拒绝。
5. 配对成功后才持久化相机代际、身份地址、bond 状态和安全 profile。
6. 已绑定状态长按 B：关闭传输、清除应用 profile 与 NimBLE bond、重建 Host，然后返回机型选择页。

## 输入隔离

机型选择页优先消费 A/B：

- A：确认机型，不触发快门。
- B 单击或双击：只切换一次机型，不触发镜像或 LiveView Lock。
- B 长按：在引导页不执行清除操作。
- 电源键仍可正常关机。

`ButtonInput` 负责生成原始 B 单击/双击事件；`PairingGuide` 在场景上下文中映射为选择动作；退出引导后才由全局命令层映射为镜像、LiveView Lock 和清除绑定。

## 安全 profile

| 配置 | GR III family | GR IV family |
|---|---|---|
| IO capability | KeyboardDisplay | DisplayYesNo |
| Passkey | 相机显示的动态 6 位码 | 固定 `123456` |
| Own address | Public | RPA Public Default |
| Key distribution | ENC + ID + SIGN | ENC + ID |
| Authenticated bond | 必须 | 保持既有 legacy 行为 |
| Security 后服务发现 | 重新发现 | 不需要 |

安全参数属于 NimBLE Host 配置。不同 profile 间切换时必须先结束活动连接，确认 Host 停止后再清理对象和初始化新 profile；不能在连接中直接改 IO capability、地址类型或 key distribution。

## 状态不变量

- 未确认机型时禁止扫描和任何 BLE side effect。
- 只读代际识别完成前禁止 Wi-Fi、电源和快门写入。
- 选择代际与检测代际不同，禁止配对。
- bond 与应用 profile 采用成功后提交，不在扫描开始时提前持久化。
- 清除绑定后不自动扫描，必须重新选择并确认机型。

## 参考资料

- [Espressif ESP-IDF NimBLE Security 示例](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/ble_get_started/nimble/NimBLE_Security)
- [Espressif ESP-BLE-UART NimBLE 实现与 bond 管理](https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/common/ble_uart/ble_uart_nimble.c)
- [NimBLE-Arduino Secure Client 示例](https://github.com/h2zero/NimBLE-Arduino/blob/master/examples/NimBLE_Secure_Client/NimBLE_Secure_Client.ino)
- [NimBLE-Arduino Device/Host 生命周期实现](https://github.com/h2zero/NimBLE-Arduino/blob/master/src/NimBLEDevice.cpp)
