# T06 · Host Action v1 Vendor HID 发送接入

- **背景**：T03 的共享编码需要接入现有 App Command 发送路径，不能在 USB 与 BLE 各写一套协议。
- **目标**：USB／BLE 复用同一编码结果，并沿用现有单通道选择和连接 owner／epoch 保护。
- **输入**：T02 固定 wire 合同、T03 共享编码、现有 Vendor HID App Command 路径。
- **产出路径**：必要的最小平台适配与宿主源码合同测试。
- **验收标准**：Report ID、kind、单 chunk 和 36 字节数据不变；状态 kind 继续为 `0x04`；USB 优先否则 BLE且不双发；不改 HID 描述符、BLE GATT 或设备身份。
- **状态**：已完成（2026-09-15 学员分支 `my-host-action-practice-v2`，基于 `aa33205` 后 T04 工作树）；平台最小接入完成，未烧录，真机验证留 T09。

## 学员执行记录（2026-09-15）

### 调用链（只读代码定位）

- 按下事件：`handle_input_event`（main/app_main.cpp:2925）→ `dispatch_firmware_event`（:1351）；
- 通道路由：`route_for_firmware_event`（components/keyboard/src/transport_routing.cpp:5-9，桩恒 `UsbFirst`）→ app_main.cpp:1413-1429：`usb_epoch!=0` 只走 USB，失败仅记日志 “BLE fallback suppressed” 并 return（不跨通道补发）；`usb_epoch==0` 才调 BLE；
- USB 编码/入队：`UsbHidTransport::send_firmware_event_for_epoch`（main/platform/usb_hid.cpp:1393）→ 新增 HostAction case → `send_app_command_report`（:1602）→ `queue_app_command_report`/`push_app_command_report_locked`（:1622/:1646，组 `[kind][chunk][total][len]+data` 63 字节帧并入 epoch 保护队列）→ Report ID 0x11（kReportIdAppCommand）；
- BLE 编码/入队：`BleHidTransport::send_firmware_event_for_owner`（main/platform/ble_hid.cpp:1913）→ 新增 HostAction case → `send_app_command_report`（:3550，同构组帧）→ `send_input_report`（:2009，owner 身份保护）→ Report ID 0x11 GATT notification。

### 实际修改文件（5 个，113 行纯新增、0 删除）

- `main/platform/usb_hid.cpp`（+30）：include 共享头；7 条 static_assert 锁定平台引用的即冻结值（0x11/0x05/0/1/36/63、36≤59）；switch 增 HostAction case——调共享 `host_action_uuid_body()` 校验去前缀，非规范 fail closed 返回 false；合法则以共享常量经既有 `send_app_command_report` 单 chunk 入队。
- `main/platform/ble_hid.cpp`（+28）：同一 include、同一组 static_assert、同构 HostAction case，经既有 `send_app_command_report`（默认 AppCommand report class、owner 保护）入队。
- `main/app_main.cpp`（+3）：仅在日志 kind 命名 switch 增 `HostAction → "host_action"`；通道分支零改动。
- `host_test/transport_routing_tests.cpp`（+13）：HostAction 事件在 BLE 已连/未连两种状态下均返回 `UsbFirst`。
- `host_test/firmware_source_contract_tests.cpp`（+39）：新增 `host_action_transport_reuses_shared_encoding_and_single_route` 源码守卫。

平台层没有复制任何 Host Action 常量：0x05/36/0/1 全部引用 `ai_keyboard::kHostAction*`，去前缀/校验统一走 `host_action_uuid_body()`；data length 保持 36（static_assert 锁定，从未出现 48）。

### 固定 payload 字段证据

| 字段 | 值 | 证据 |
|---|---|---|
| Report ID | 0x11 | 两平台 static_assert `kHostActionAppCommandReportId == kReportIdAppCommand`；发送原语固定用 kReportIdAppCommand |
| kind | 0x05 | static_assert `kHostActionAppCommandKind == 0x05`；入队第一字节由 `push_app_command_report_locked`/BLE 同函数写 `report[0]=kind` |
| chunk index / total | 0 / 1 | 传参 kHostActionChunkIndex/kHostActionTotalChunks，static_assert 锁定 |
| data length | 36 | body.size()=kHostActionUuidTextLen=36，static_assert `kHostActionDataLen == 36`；写 `report[3]` |
| 数据区 | `[4..39]` 裸 UUID ASCII | body 已由共享函数剥离 `host_action:` 前缀，copy 到 `report+4`；前缀仅存于配置/Keymap |
| `[40..62]` | 0 | 两平台均 `std::array<…,63> report{}` 零初始化，只 copy 36 字节 |
| 0x04 | 仍仅状态响应 | 共享层 static_assert `kHostActionAppCommandKind != kStatusResponseCommandKind`（T03）；status 发送路径未改 |
| 容器 | 63 | static_assert `kHostActionAppCommandPayloadLen == kAppCommandReportPayloadLen` |

### 单通道不双发证据

- 路由函数对 HostAction 恒返回 UsbFirst（宿主测试两状态断言）；
- dispatch 现有分支：USB epoch 有效→仅 USB，拒绝时 log “BLE fallback suppressed” 后 return；无 USB→才 BLE。HostAction 未增加任何第二发送分支（源码守卫：app_main.cpp 中 `FirmwareEventKind::HostAction` 仅出现 1 次，且不 include 共享头——调度层不编码）；
- USB epoch 与 BLE owner 身份保护沿用既有队列，未新增去重逻辑（不用 App 端去重掩盖固件）。

### 测试命令与结果（w64devkit GCC 13.2，CMake 3.30.2）

- 定向：`ctest -R 'transport_routing|firmware_source_contract|host_action_protocol|keymap|config_payload'` → **5/5 通过（1.37 秒）**。
- 全量：`ctest --test-dir build-host` → **100% tests passed, 0 failed out of 58**（44.05 秒）。
- 平台 main/ 代码不参与宿主编译，其行为由源码契约测试（逐行断言）+ 静态断言证明；未虚构真机结果。

### 旧 App Command／HID 行为回归

全量 58 项含 fixed_text 分块流、hotkey 0x02、config ack 0x03、status 0x04、键盘快照/Latch/epoch、队列仲裁等既有测试全部通过；git diff 为纯新增，既有发送函数体零删除。

### 禁止范围差异证据

- `git diff --stat`：5 文件 +113/-0；无 HID 描述符、BLE GATT 表、VID/PID（0x303A/0x1006）、设备名/序列号、GPIO/BOOT/电源/分区任何改动行；
- 三个生产文件 grep `host_action_v1` 零命中（能力声明仍留第 08 步）；
- 未改 components 共享协议（T03 文件本节点零差异）；未烧录、未跑 ESP-IDF 构建、未连设备。

### 仍未验证项

1. ESP-IDF 5.5.5 固件编译（含新增 -Wswitch 枚举覆盖）与真机抓包：实际线上 0x11 字节、时序、队列满拒绝行为——留 T09/实板；
2. BLE GATT notification 在真机双连接场景的不双发（代码路径单一，但未运行观察）；
3. App 0.1.26 收到 0x05 后的应用映射行为（App 侧职责，本仓不验）；
4. 能力声明 `"host_action_v1": true` 与 BLE 状态 ≤512 检查未实现（第 08 步）。
