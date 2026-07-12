# Rabbit Pixel UI Theme

## 编译期入口

| 项目 | 值 |
| --- | --- |
| Variant 宏 | `UI_VARIANT_RABBIT=5` |
| PlatformIO 环境 | `sticks3-ui-rabbit` |
| Renderer | `rvf::ui::RabbitUiRenderer` |
| Profile | `rvf::ui::RabbitUiProfile` |

编译与烧录：

```bash
pio run -e sticks3-ui-rabbit
pio run -e sticks3-ui-rabbit --target upload
```

## 背景与安全区

`assets/rabbit_background.png` 是 240 × 135 原生 PNG，使用最近邻缩放保存像素边缘。两只兔子完整位于右侧，主体约从 `x=126` 开始；`RabbitLayout::kSafeX=5`、`kSafeW=108` 定义左侧文字安全区，Boot、Status、Settings、Error、Shutdown 的面板均不得越过 `x=113`。

背景图不包含文字、状态灯或按钮。Renderer 在图片之上绘制结构化 `UiModel` 状态，因此 BLE、Wi-Fi、电量、相机就绪和错误信息仍是动态数据。

## 页面设计

| 页面 | 绘制内容 |
| --- | --- |
| Boot | 左侧标题、阶段、进度和版本；右侧兔子背景无遮挡 |
| Status | 左侧 CAM/BLE/WIFI/BAT/READY 五行状态与底部 A/B 提示 |
| LiveView | 只绘制顶部 Live/FPS、RICOH GR、Wi-Fi/电量和中间对焦框，不清空 JPEG 底图 |
| Settings | 左侧静态视觉样稿，不修改相机设置 |
| Error | 左侧错误信息和重试/重置提示 |
| Shutdown | 左侧 Good Night 与关机详情 |

Rabbit Renderer 只负责显示，不访问 BLE、Wi-Fi、HTTP、NVS 或相机控制，也不调用 `present()`。`UI_FEATURE_MASCOTS=0` 会关闭 PNG 背景并回退为纯色背景。

## 图片处理

源图作为构图参考，生成 16:9 米白色像素背景并把两只兔子等比例放入右侧安全区；随后以最近邻方式缩放至 240 × 135。固件通过 `RabbitBackground.h` 中的内嵌 PNG 数据绘制，无需文件系统分区。
