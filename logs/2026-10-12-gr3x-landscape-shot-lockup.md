# 2026-10-12 GR IIIx 横屏预览拍摄后相机死机

## 环境

- 相机：RICOH GR IIIx，固件 V1.50（BLE 配对 `GR_D2FEAA`，Wi-Fi `GR_CDFD63`）
- 控制器：StickS3，分支 `feature/firmware-optimization` 提交 `9bc1fa7`
- 与上一轮成功会话的差异：LiveView 请求带 `reso=720x480`；新订阅 BatteryLevel / ShotCount / CaptureStatus 通知

## 现象

横屏 LiveView 中第一次 BtnA 快门成功，约 1 秒后相机死机：电源指示灯熄灭，需拔电池才能重新开机（用户确认非低电量关机）。StickS3 侧表现为第二次快门写入挂起，5.12 秒后 `SUPERVISION_TIMEOUT`。

## 关键日志

```
08:01:15.429 HTTP: resoList=[720x480 1080x720] selected reso=720x480
08:01:15.435 HTTP: opening LiveView path=/v1/liveview?maxfps=7&reso=720x480
08:01:18.558 BLE: Ricoh shutter OperationRequest START param=1 autofocus=1 prep=DIRECT
08:01:19.008 BLE: capture status notify raw=[01 00 00 00]
08:01:19.009 BLE: shot confirmed by camera notify after 451ms
08:01:19.308 BLE: capture status notify raw=[00 00 00 00]
08:01:20.108 BLE: battery notify value=0
08:01:21.458 BLE: Ricoh shutter OperationRequest START param=1 autofocus=1 prep=DIRECT
08:01:26.558 BLE: disconnected reason=520 (0x208) meaning=SUPERVISION_TIMEOUT
```

## 结论

- 没有电源通知 `0x00`，也没有 `0x213`/`0x215` 主动断连：相机不是正常关机。
- BatteryLevel 通知 `0` 出现在死机前约 1.3 秒，按死机前兆处理，不当作真实电量显示。
- 可疑触发因素（尚未区分）：显式 `reso=720x480`；新增的三个状态通知订阅。`reso` 在 GR IIIx 上无收益（无更小尺寸），已默认关闭。

## 已验证的正面结果

- 竖屏和横屏下 CaptureStatus 都按 `01 00 00 00` → `00 00 00 00` 通知拍摄开始/结束，快门确认延迟 350–470 ms。
- ShotCount 在该相机上不发通知。

## 后续对照测试

1. 当前默认：`LIVEVIEW_REQUEST_RESO=false`，状态通知开启。横屏连续拍摄多次。
2. 若仍死机：`RICOH_BLE_SUBSCRIBE_CAMERA_STATUS 0`，恢复到上一轮已验证的行为再测。
