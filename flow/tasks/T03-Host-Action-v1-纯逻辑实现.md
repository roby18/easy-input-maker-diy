# T03 · Host Action v1 纯逻辑实现

- **背景**：T02 已冻结协议；当前 starter 尚未实现 Host Action。
- **目标**：先用宿主测试固定规范小写 UUID 校验、完整配置值解析、Keymap 按下／松开语义和共享报告编码，再做最小纯逻辑实现。
- **输入**：`flow/tasks/T02-Host-Action-v1-固定兼容协议.md`、现有配置／Keymap／App Command 容器。
- **产出路径**：由学员根据现有分层选择 `components/keyboard/` 与 `host_test/` 中的最小文件范围。
- **验收标准**：合法 UUID 接受；大写、长度、连字符或字符错误 fail closed；不增加 version／nil 限制；按下一个事件、松开无事件；固定 wire 字段通过测试；本节点不接 USB／BLE。
- **状态**：已完成（2026-09-14 学员分支 `my-host-action-practice-v2`，基于 `0212918`）；纯逻辑层与宿主测试已落地，USB／BLE 未接入，待 T06。

## 学员执行记录（2026-09-14）

### 合同核对

开工前重读 `flow/tasks/T02-Host-Action-v1-固定兼容协议.md`：配置完整前缀、canonical lowercase UUID fail closed、Report ID `0x11`／kind `0x05`／chunk `0`／total `1`／data length `36`、裸 UUID 占据 `[4..39]`、`[40..62]` 仅容器余量、不复用 `0x04`、按下一次松开不发、能力声明留到第 08 步——全部一致，无缺口，继续实现。

### TDD 失败 → 通过证据

1. **先写测试（RED）**：新建 `host_test/host_action_protocol_tests.cpp`，扩展 `config_payload_tests.cpp`、`keymap_tests.cpp`，并在 `host_test/CMakeLists.txt` 登记新目标。此时尚无实现，定向构建实测失败：
   `fatal error: keyboard/host_action_protocol.h: No such file or directory`（`host_action_protocol_tests.cpp:9`，make Error 1/2）。
2. **最小实现（GREEN）**：新增共享纯逻辑头/源与 Keymap、配置解析改动后，定向 3/3 通过，全量 58/58 通过（见下）。

### 实际修改文件（共 11 个，全部在纯逻辑层与 host_test）

新增：
- `components/keyboard/include/keyboard/host_action_protocol.h`：冻结合同常量（前缀、36、0x11、0x05、0、1、63、4）、严格校验、共享编码声明。
- `components/keyboard/src/host_action_protocol.cpp`：canonical lowercase 逐位校验（连字符 0 基位 8/13/18/23，其余仅 `0-9a-f`；无 version/nil 限制）、前缀校验、去前缀取 body、63 字节编码（`[0]=0x05 [1]=0 [2]=1 [3]=36`，`[4..39]` 裸 UUID，余量零填充），非法值返回 false 且不部分写入；`static_assert` 锁定与 fixed_text 同一 0x11/63/4 容器且 `0x05 != 0x04`。
- `host_test/host_action_protocol_tests.cpp`：接受/拒绝矩阵、nil 与任意 version 接受、大写不转小写、长度/连字符/非法字符拒绝、精确字节布局、余量为零、复用既有容器、不占 0x04、编码 fail closed。

修改：
- `components/keyboard/include/keyboard/keymap.h`：`ActionKind::HostAction`、`FirmwareEventKind::HostAction`、`Action.host_action`（保存完整 `host_action:<uuid>`）。
- `components/keyboard/src/keymap.cpp`：HostAction 按下产生一次事件（值为完整配置串），松开返回 `None`。
- `components/keyboard/src/config_payload.cpp`：裸字符串动作识别 `host_action:` 前缀；canonical 才接受并原样保存，任何非规范值返回 `UnknownAction` 拒绝整份配置（不转小写、不修复）。
- `components/keyboard/CMakeLists.txt`、`host_test/CMakeLists.txt`：登记 `host_action_protocol.cpp` 与新测试目标。
- `host_test/config_payload_tests.cpp`：合法绑定解析（全前缀保留、按下 HostAction/松开 None）与 7 种非规范值整份拒绝。
- `host_test/keymap_tests.cpp`：按下一次 HostAction、松开 None。
- `host_test/firmware_source_contract_tests.cpp`：移除 `course_starter_does_not_ship_host_action_implementation()` 及其注册——该防护断言六处源文件不含 Host Action，T03 按课程合同实现后其使命结束；未改动同文件其他契约断言。

### 测试命令与结果（Windows，w64devkit GCC 13.2 + CMake 3.30.2 MinGW Makefiles）

- 配置：`cmake -S host_test -B build-host`（PATH 前置 `D:\AI\hardware\easyinput\tools\w64devkit\bin`）。
- 定向：`cmake --build build-host --target host_action_protocol_tests config_payload_tests keymap_tests` 全部链接成功；
  `ctest --test-dir build-host -R 'host_action_protocol|config_payload|keymap'` → **100% tests passed, 0 failed out of 3**。
- 全量：`cmake --build build-host --parallel 6` 全部目标成功；
  `ctest --test-dir build-host` → **100% tests passed, 0 failed out of 58**（基线 57 个，新增 1 个测试可执行文件；总耗时 38.83 秒）。

### 边界遵守与差异证据

- `git status --short` 实测改动仅 `components/keyboard/` 与 `host_test/` 下 11 个文件；**没有** `main/app_main.cpp`、`main/platform/usb_hid.cpp`、`main/platform/ble_hid.cpp`，未接 USB／BLE、未改路由/队列/描述符、未双发。
- 未加入 `"host_action_v1"` 能力声明（第 08 步才允许）；未改 BLE 状态 JSON 与 512 预算逻辑。
- 未改 GPIO、BOOT、GPIO8、分区、设备身份；未运行 ESP-IDF 构建、未烧录、未连设备、未离开本仓。

### 仍未验证项

- 共享编码尚未被 USB／BLE 调用（T06 范围）；本次只验证纯函数输出，未验证真机字节、单通道实际选择与不双发。
- 配置经 App→Vendor HID 落盘→NVS 的整链路未运行验证（解析器与持久化路径分别存在，但未端到端跑固件）。
- ESP-IDF 5.5.5 固件构建未在本节点运行（留待 T09）；新枚举 `FirmwareEventKind::HostAction` 在 `app_main.cpp` 派发 switch 中暂走既有 unknown/default 路径，T06 接入。
- 宿主测试为电脑端逻辑证据，不等同烧录或实板功能证据。
