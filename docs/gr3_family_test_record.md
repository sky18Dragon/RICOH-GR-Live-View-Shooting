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
| GR III/GR IIIx 设备 | 未提供，未烧录、未执行实机测试 |
| 结论 | 实现完成，等待实机验证；编译通过不等于相机实机验证 |

参考仓库中的 GR III/GR IIIx 真机资料是协议证据，不是本分支固件的测试结果。

## 待执行矩阵

| # | 测试 | GR III | GR IIIx | 关键证据 |
| ---: | --- | --- | --- | --- |
| 1 | 菜单配对、识别 `GR3_FAMILY` | 未执行 | 未执行 | Profile source / GATT |
| 2 | 相机显示随机六位码 | 未执行 | 未执行 | 相机画面，不记录数字 |
| 3 | 只用 Button A 输入并 Bond | 未执行 | 未执行 | authenticated/bonded，不记录 Passkey |
| 4 | StickS3 重启免重复输入 | 未执行 | 未执行 | saved bond reconnect |
| 5 | 读取 SSID/Passphrase/Channel | 未执行 | 未执行 | present flags / channel |
| 6 | `CAPTURE` 下 UUID 开 AP、连接 Wi-Fi | 未执行 | 未执行 | Network Type UUID result |
| 7 | `/v1/props` | 未执行 | 未执行 | HTTP status |
| 8 | `/v1/liveview` 持续画面 | 未执行 | 未执行 | 时长/FPS/stall |
| 9 | BLE AF 快门且 LiveView 保持 | 未执行 | 未执行 | shutter result / preview |
| 10 | 休眠不写 WLAN、不伸镜头 | 未执行 | 未执行 | mode + absence of write |
| 11 | 开机后 Button A 安全重试 | 未执行 | 未执行 | fresh link + CAPTURE |
| 12 | 自动只读重试间隔 ≥8s | 未执行 | 未执行 | timestamps |
| 13 | 相机端删配对后的 Bond 自愈 | 未执行 | 未执行 | explicit security failures |
| 14 | Button B 清除后重新配对 | 未执行 | 未执行 | NVS/Bond/Profile reset |
| 15 | 相机/StickS3 交替重启恢复 | 未执行 | 未执行 | reconnect history |
| 16 | Wi-Fi/LiveView 断线恢复 | 未执行 | 未执行 | recovery timeline |

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
