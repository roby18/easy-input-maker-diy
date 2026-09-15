# T09 · Host Action v1 软件侧总验收

- **背景**：局部测试通过不等于完整固件可构建。
- **目标**：重新配置、发现并运行全部宿主测试，再用项目锁定的 ESP-IDF 5.5.5 完成 `v2`／`esp32s3` 默认构建。
- **输入**：T03、T04、T06、T08 的当前工作区和测试清单。
- **产出路径**：被忽略的 `build-host/`、`build/` 以及公开的脱敏验证记录。
- **验收标准**：发现数等于执行数等于通过数，0 失败；默认构建成功并记录镜像与分区余量；清楚区分逻辑通过、构建通过和尚未烧录／真机验证。
- **状态**：已完成（2026-09-15，学员+Trae，仅软件侧证据）

## 本节点范围与证据口径

- 重新配置宿主测试并先读取 CTest 实际清单，再执行清单中的全部测试。
- 全部通过后激活项目锁定的 ESP-IDF v5.5.5，对默认 `v2`／`esp32s3` 目标执行默认构建。
- 本节点不识别设备、不读串口、不烧录、不运行 EasyInput App，不声称真机功能已通过。
- 未修改 Report ID `0x11`、kind `0x04`／`0x05`、63 字节容器、HID 描述符、BLE GATT、设备身份、GPIO、BOOT、GPIO8 电源或 Flash 分区。
- 上一轮 `58/58` 与能力状态最大 `512/512` 只作为交接读取，**不计作本节点结果**；本卡所有数字均为本轮重新运行的实测值。

## 起点核对：清单差异与本节点最小修复（先说明数量，再动代码）

重新配置后 CTest 实际发现 **58** 个测试。与课程起点和 flow/ 记录核对：

- 练习仓课程起点提交 `743368a` 的 `host_test/CMakeLists.txt` 注册 **57** 个测试（源码清点）：课程公开基线为 56 个，本练习仓起点多 1 个 `ble_persistence_policy_tests`（该文件在 `743368a` 中已存在，BLE 持久化硬化随仓库基线带入，不是本课程新增，也不是本节点新增/缺失项）。
- T03 新增 `host_action_protocol_tests` → 58；T04、T08 当时把测试**扩充进既有目标**（`config_payload_tests.cpp`、`config_status_tests.cpp`），未新增 CTest 目标，因此 T04/T06/T08 各轮记录均为 58。
- 本节点按 T09 验收要求核对三个具名目标：`host_action_protocol_tests` 在册；`host_action_key_bindings_tests`（T04 八键覆盖）与 `host_action_capability_status_tests`（T08 能力声明/512 变体）**未注册**。

最小修复（纯搬迁，0 条断言增删改）：

1. T04 的 3 个函数（`each_of_the_eight_keys_parses_host_action_through_shared_path`、`all_eight_keys_coexist_as_host_actions_in_one_profile`、`host_action_coexists_with_legacy_actions_without_regression`）及其 3 个文件内助手从 `config_payload_tests.cpp` 原样剪切到新目标 `host_action_key_bindings_tests.cpp`。
2. T08 的变体枚举测试 `host_action_v1_is_boolean_once_in_every_published_variant` 及其专用助手（count/overlay/worst-case/speaker 构造、`is_valid_utf8` 复制一份）从 `config_status_tests.cpp` 原样剪切到新目标 `host_action_capability_status_tests.cpp`；原文件保留其余 17 个状态测试与它们对能力串的既有断言。
3. `host_test/CMakeLists.txt` 注册两个新目标。搬迁后 `config_payload_tests` 与 `config_status_tests` 仍各自编译通过、断言全绿，证明覆盖只搬家、不缩水。

数量关系：**57（本仓起点）+ 3（Host Action 具名目标）= 60**。重新配置后实际发现 60，无新增（搬迁外）、无缺失、无未被发现的测试。课程公开答案为 59（其起点 56），差异 1 即前述本仓起点既有的 `ble_persistence_policy_tests`。

## 实际命令与退出结果（脱敏：工具根目录以变量表示）

- `$MAKER_CMAKE_BIN`＝ESP-IDF 工具目录中的 CMake/CTest **3.30.2**；工具链为 w64devkit **GCC 13.2.0**（MinGW Makefiles）；IDF 激活用 EIM 为本机生成的 v5.5.5 PowerShell profile（等价桌面 "IDF_v5.5.5 Powershell" 快捷方式）。

```powershell
# 干净重新配置（先删除被忽略的 build-host/）
& "$MAKER_CMAKE_BIN" -S host_test -B build-host -G 'MinGW Makefiles' `
  '-DCMAKE_BUILD_TYPE=Debug' '-DCMAKE_CXX_COMPILER_FORCED=TRUE' `
  '-DCMAKE_CXX_COMPILER_ID=GNU' '-DCMAKE_CXX_COMPILER_VERSION=13.2.0'
# exit 0；The CXX compiler identification is GNU 13.2.0

& "$MAKER_CTEST_BIN" --test-dir build-host -N
# exit 0；Total Tests: 60（搬迁前同机实测为 58）

& "$MAKER_CMAKE_BIN" --build build-host -j4
# exit 0；warning/error 行 = 0

& "$MAKER_CTEST_BIN" --test-dir build-host --output-on-failure
# exit 0；100% tests passed, 0 tests failed out of 60；61.62 秒
# 同日第二轮（结果落盘）：exit 0；60 行结果全部 Passed；39.08 秒

. <EIM v5.5.5 PowerShell profile>
idf.py --version          # ESP-IDF v5.5.5，exit 0
git -C $env:IDF_PATH describe --tags --always   # v5.5.5
idf.py --list-targets     # 含 esp32s3，exit 0
idf.py -C . build         # Project build complete，exit 0
```

### 本轮两个环境问题的定性与处理（非代码失败）

1. **干净配置初次失败**：CMake 编译器 ABI 探测生成的 `CompilerIdCXX/a.exe` 被 Windows Defender 实时保护以机器学习泛化规则拦截（`Trojan:Win32/Wacatac.B!ml` / `Phonzy.A!ml` / `Kepavll!rfn`），CMake `file(READ ... a.exe)` 得到 `Invalid argument`。2×2 对照实验证明是**该探测二进制内容**触发（TEMP 目录同样被拦；hello-world 在 build-host 下可读可跑），与路径/源码无关。非提权会话无法添加 Defender 排除项；采用 CMake 官方支持的 `-DCMAKE_CXX_COMPILER_FORCED=TRUE` 并显式提供编译器 ID/版本以跳过该探测步骤（仅本机配置参数，不写入仓库）。后续 60 个真实测试可执行文件全部正常链接、读取、运行。
2. **ninja 后置 Python 步骤**在沙箱内尝试写系统 Python 自身的 `__pycache__` 目录被宿主沙箱拒绝（非构建错误）；以 `PYTHONDONTWRITEBYTECODE=1` 重跑后 ninja 全步骤 exit 0。

## CTest 数量核对

- 本仓课程起点（`743368a` 源码清点）：57。
- Host Action 新增具名目标：`host_action_protocol_tests`（T03）、`host_action_key_bindings_tests`（T04，本节点注册）、`host_action_capability_status_tests`（T08，本节点注册），共 3 个。
- 本轮重新配置后实际发现：**60**；执行 60；通过 60；失败 0；跳过 0。发现数＝执行数＝通过数。
- 两轮完整执行（61.62 秒 / 39.08 秒）结果一致。

## 本轮完整 CTest 清单与结果（ctest -N 编号，全部 Passed）

| # | 测试名称 | 结果 |
|---:|---|---|
| 1 | `board_pins_tests` | Passed |
| 2 | `board_pins_v2_tests` | Passed |
| 3 | `audio_io_arbiter_tests` | Passed |
| 4 | `audio_control_wire_tests` | Passed |
| 5 | `audio_packet_wire_tests` | Passed |
| 6 | `audio_session_tests` | Passed |
| 7 | `agent_status_tests` | Passed |
| 8 | `awake_wait_planner_tests` | Passed |
| 9 | `boot_led_sequence_tests` | Passed |
| 10 | `cold_boot_feedback_tests` | Passed |
| 11 | `battery_estimator_tests` | Passed |
| 12 | `ble_advertising_state_tests` | Passed |
| 13 | `ble_connection_profile_tests` | Passed |
| 14 | `ble_management_gate_tests` | Passed |
| 15 | `ble_input_scheduler_tests` | Passed |
| 16 | `ble_fixed_text_stream_tests` | Passed |
| 17 | `ble_owner_recovery_tests` | Passed |
| 18 | `ble_persistence_policy_tests` | Passed（本仓起点相对课程基线多 1 个的来源） |
| 19 | `ble_status_wire_tests` | Passed |
| 20 | `config_payload_tests` | Passed（T03 解析用例保留；T04 八键用例已迁出） |
| 21 | `config_receiver_tests` | Passed |
| 22 | `config_state_tests` | Passed |
| 23 | `config_status_tests` | Passed（17 个状态预算/结构用例；T08 变体枚举已迁出） |
| 24 | `debounce_tests` | Passed |
| 25 | `diagnostic_command_tests` | Passed |
| 26 | `encoder_tests` | Passed |
| 27 | `encoder_press_gesture_tests` | Passed |
| 28 | `firmware_source_contract_tests` | Passed |
| 29 | `held_keyboard_state_tests` | Passed |
| 30 | `hid_report_queue_tests` | Passed |
| 31 | `hid_keycode_tests` | Passed |
| 32 | `host_action_protocol_tests` | Passed |
| 33 | `host_action_capability_status_tests` | Passed（本节点注册，T08 迁入） |
| 34 | `input_feedback_tests` | Passed |
| 35 | `input_test_tests` | Passed |
| 36 | `speaker_assets_wifi_policy_tests` | Passed |
| 37 | `ima_adpcm_decoder_tests` | Passed |
| 38 | `sound_asset_store_tests` | Passed |
| 39 | `sound_asset_reader_tests` | Passed |
| 40 | `speaker_assets_store_executor_tests` | Passed |
| 41 | `speaker_assets_flash_runner_tests` | Passed |
| 42 | `speaker_assets_protocol_tests` | Passed |
| 43 | `speaker_assets_wifi_wire_tests` | Passed |
| 44 | `speaker_assets_session_tests` | Passed |
| 45 | `speaker_assets_runtime_tests` | Passed |
| 46 | `gatt_status_snapshot_tests` | Passed |
| 47 | `host_action_key_bindings_tests` | Passed（本节点注册，T04 迁入） |
| 48 | `keyboard_snapshot_delivery_tests` | Passed |
| 49 | `keymap_tests` | Passed |
| 50 | `platform_selection_tests` | Passed |
| 51 | `peripheral_power_lease_tests` | Passed |
| 52 | `power_cycle_tests` | Passed |
| 53 | `power_policy_tests` | Passed |
| 54 | `speaker_audio_contract_tests` | Passed |
| 55 | `speaker_probe_status_tests` | Passed |
| 56 | `speaker_playback_tests` | Passed |
| 57 | `speaker_service_startup_tests` | Passed |
| 58 | `status_hid_protocol_tests` | Passed |
| 59 | `transport_routing_tests` | Passed |
| 60 | `usb_hid_endpoint_arbiter_tests` | Passed |

### 能力声明变体字节预算（搬迁后目标独立运行实测，UTF-8 序列化字节）

`./build-host/host_action_capability_status_tests.exe` 退出码 0，实测表：full 399/399；compact 含 cycle 504/504；compact 省 cycle 462/462；minimal audio 426/426；diagnostics 509/509；**speaker probe 512/512（最大，剩余 0）**；confirmation 410/410；battery compact 402→479；battery bounded base 492/492；fallback 241/241。max=512，limit=512，headroom=0——与 T08 交接一致，且为本次重跑结果。

## ESP-IDF 默认构建证据

- ESP-IDF：`v5.5.5`（`idf.py --version` 与 `git describe` 一致）；芯片目标：`esp32s3`（sdkconfig 默认，未运行 set-target/menuconfig）；板型：默认 `v2`。
- 首次默认增量构建：`Project build complete`，exit 0，无环境/依赖/编译/链接失败；当时全部对象为最新，仅执行尺寸校验步骤。
- 为取得当前源码的真实编译证据，删除 6 个 Host Action 相关对象后直接 ninja（15 步，exit 0），本轮**实际重新编译**：
  `components/keyboard/src/host_action_protocol.cpp`、`components/keyboard/src/status_hid_protocol.cpp`、`components/keyboard/src/config_status.cpp`；
  `main/app_main.cpp`、`main/platform/usb_hid.cpp`、`main/platform/ble_hid.cpp`；
  随后重新归档 `libkeyboard.a`、`libmain.a` 并重新链接 ELF/镜像。`warning:` / `error:` / `deprecated` 计数均为 **0**。
- 应用镜像：`build/easy_input_keyboard.bin`，**0x1908f0／1,640,688 字节**（check_sizes 口径；idf size 未填充镜像 1,640,569 字节）。
- ELF：`build/easy_input_keyboard.elf`，21,404,576 字节；ELF 中实测包含 `host_action_v1`、`host_action:`、`send_firmware_event` 字符串，证明当前固件镜像含 Host Action 能力声明与发送路径代码。
- 最小 App 分区（partitions.csv 的 factory，未改动）：`0x300000`／3,145,728 字节；剩余 **`0x16f710`／1,505,040 字节，48% free**。
- Bootloader：`0x5160`／20,832 字节；其区域剩余 `0x2ea0` 字节，36% free。

## 本节点修改文件与禁止范围核对

- 代码改动仅 5 个宿主测试文件，生产代码零改动：
  - `host_test/CMakeLists.txt`（+2 注册）
  - `host_test/host_action_key_bindings_tests.cpp`（新增，T04 测试原样迁入）
  - `host_test/host_action_capability_status_tests.cpp`（新增，T08 测试原样迁入）
  - `host_test/config_payload_tests.cpp`（仅剪切迁出与 main 登记，-170 行）
  - `host_test/config_status_tests.cpp`（仅剪切迁出、include 与 main 登记，-466/+3）
- 构建前后对 10 个受保护文件做 SHA-1 对照（sdkconfig.defaults、partitions.csv、host_action_protocol.h/.cpp、status_hid_protocol.h、config_status.h、app_main.cpp、usb_hid.cpp、ble_hid.cpp、host_test/CMakeLists.txt）：**全部一致**。
- 未运行 `set-target`、`menuconfig`、`flash`、`monitor`、端口枚举、设备识别或 App；未改 Report ID、kind、63 容器、HID 描述符、GATT、设备身份、GPIO、BOOT、GPIO8 电源或 Flash 分区；未升级 ESP-IDF、未改构建目标。
- `build-host/`、`build/` 均为被忽略产物目录（本节点日志落于其中，不进版本控制）。

## 结论：三类证据严格分开

1. **60/60 宿主测试通过**只证明当前代码的宿主可执行逻辑与源码合同通过。
2. **ESP-IDF v5.5.5 / esp32s3 默认构建通过**只证明当前 `v2` 源码可以生成固件镜像（含尺寸与分区余量证据）。
3. **仍未验证**：烧录、启动、串口日志、USB／BLE 真机发送、双连接单通道路由、512 字节状态的真机 GATT 发布、EasyInput App 0.1.26 能力识别与 UUID→本机应用映射——留 T10（烧录前只读审计）及之后经独立授权的真机验证。
