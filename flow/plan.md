# 计划（plan）——Host Action v1 课程起点

## 基线

- [x] 课程起点与完整 `main` 共用当前公共固件底座，保留 BLE 配置持久化修复、编码器、音频、电源和硬件安全边界。
- [x] 课程起点不含 Host Action 生产实现、能力声明或答案型测试，也不含产品专属 OTA、签名和部署工具。
- [x] 起点重新发现并通过 57/57 项宿主测试，ESP-IDF 5.5.5 对 `v2`／`esp32s3` 默认构建通过。

## 学员推进节点

- [x] T03 · 用宿主测试建立 UUID 校验、配置解析、Keymap 按下／松开和共享编码纯逻辑。
- [x] T04 · 用参数化测试证明 KEY1—KEY8 都经过同一配置链路，旧动作不回归。
- [x] T06 · 把共享编码接入现有 Vendor HID App Command，并保持 USB／BLE 单通道选择。
- [x] T08 · 增加布尔能力声明并证明所有实际 BLE 状态不超过 512 字节。
- [x] T09 · 重新运行全部宿主测试和 ESP-IDF 5.5.5 默认构建，明确软件证据边界。
- [x] T10 · 烧录前只读审计全部变化；烧录与真机功能验证仍需独立授权和观察。（审计 SAFE_TO_REQUEST_FLASH 初判 NO；同日经授权脱敏 3 处 flow 本机路径后⑧转 PASS，工作区口径 YES，历史提交残留待使用者决定，见 T10 卡处置记录）
- [x] T11 · 只读验身→人工确认→显式端口烧录→电源开关恢复正常启动；五项状态前三 YES、POST_FLASH_READY=YES、FUNCTION_VERIFIED=NO（见 T11 卡；功能验证待下一节点授权）。
- [x] T12 · 正常模式连接/App 0.1.43 预检/配置同步确认/七项真机功能矩阵：POST_FLASH_READY=YES、CONFIG_SYNC_CONFIRMED=YES、FUNCTION_VERIFIED=YES（指示灯观察项另立，见 T12 卡）。

## 合同入口

- 固定协议与不承担范围：`flow/tasks/T02-Host-Action-v1-固定兼容协议.md`
- 课程节点任务卡：`flow/tasks/T03-*`、`T04-*`、`T06-*`、`T08-*`、`T09-*`、`T10-*`
- 当前交接棒：`flow/进展.md` 顶部
