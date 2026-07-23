# GR III Family Hardware Test Record

## 本次实现验证快照

| 字段 | 记录 |
| --- | --- |
| 日期 | 2026-07-18 |
| 分支 | `codex/feat-gr3-family-support` |
| 已构建代码提交 | `da9ed54`（文档提交不改变固件代码） |
| 目标板 | M5Stack StickS3 / `m5stack-sticks3` |
| Native | 27/27 PASSED |
| 发布固件构建 | SUCCESS；RAM 75,860 / 327,680 bytes（23.2%）；Flash 1,278,761 / 3,342,336 bytes（38.3%） |
| GATT diagnostics 构建 | `RICOH_BLE_GATT_DIAGNOSTICS=1` SUCCESS；Flash 1,279,309 bytes（38.3%） |
| GR III/GR IIIx 设备 | GR IIIx 已完成实机验证（2026-07-24 用户确认）；GR III 未执行 |
| 结论 | GR IIIx 已通过本分支固件实机验证；GR III 与 HDF 版本仍需独立验证 |

参考仓库中的 GR III/GR IIIx 真机资料仍仅作为协议证据；本分支新增 GR IIIx 实机结论来自 2026-07-24 用户确认，详细矩阵、相机固件版本与去密日志待补充。

## 待执行矩阵

| # | 测试 | GR III | GR IIIx | 关键证据 |
| ---: | --- | --- | --- | --- |
| 1 | 菜单配对、识别 `GR3_FAMILY` | 未执行 | 已验证（待日志） | Profile source / GATT |
| 2 | 相机显示随机六位码 | 未执行 | 已验证（待日志） | 相机画面，不记录数字 |
| 3 | 只用 Button A 输入并 Bond | 未执行 | 已验证（待日志） | authenticated/bonded，不记录 Passkey |
| 4 | StickS3 重启免重复输入 | 未执行 | 已验证（待日志） | saved bond reconnect |
| 5 | 读取 SSID/Passphrase/Channel | 未执行 | 已验证（待日志） | present flags / channel |
| 6 | `CAPTURE` 下 UUID 开 AP、连接 Wi-Fi | 未执行 | 已验证（待日志） | Network Type UUID result |
| 7 | `/v1/props` | 未执行 | 已验证（待日志） | HTTP status |
| 8 | `/v1/liveview` 持续画面 | 未执行 | 已验证（待日志） | 时长/FPS/stall |
| 9 | BLE AF 快门且 LiveView 保持 | 未执行 | 已验证（待日志） | shutter result / preview |
| 10 | 休眠不写 WLAN、不伸镜头 | 未执行 | 已验证（待日志） | mode + absence of write |
| 11 | 开机后 Button A 安全重试 | 未执行 | 已验证（待日志） | fresh link + CAPTURE |
| 12 | 自动只读重试间隔 ≥8s | 未执行 | 已验证（待日志） | timestamps |
| 13 | 相机端删配对后的 Bond 自愈 | 未执行 | 已验证（待日志） | explicit security failures |
| 14 | Button B 清除后重新配对 | 未执行 | 已验证（待日志） | NVS/Bond/Profile reset |
| 15 | 相机/StickS3 交替重启恢复 | 未执行 | 已验证（待日志） | reconnect history |
| 16 | Wi-Fi/LiveView 断线恢复 | 未执行 | 已验证（待日志） | recovery timeline |

## 2026-07-24 GR IIIx 实机确认

| 字段 | 值 |
| --- | --- |
| 日期/测试者 | 2026-07-24 / 用户确认 |
| 相机型号 | RICOH GR IIIx |
| 相机固件版本 | 待补充 |
| StickS3 commit | 待补充 |
| 是否清空 NVS | 待补充 |
| 测试编号与结果 | 1-16 已验证（待补充详细证据） |
| 关键耗时/FPS | 待补充 |
| 去密日志路径 | 待补充 |
| 备注 | 当前只证明 GR IIIx；不自动覆盖 GR III 或 HDF 版本 |

## 实机填写模板

| 字段 | 值 |
| --- | --- |
| 日期/测试者 | |
| 相机型号 | |
| 相机固件版本 | |
| StickS3 commit | |
| 是否清空 NVS | |
| 测试编号与结果 | |
| 关键耗时/FPS | |
| 去密日志路径 | |
| 备注 | |

日志提交前必须删除完整 Passphrase、六位 Passkey 和其他凭据。
