# T04 · Host Action v1 八键配置覆盖

- **背景**：八个实体主按键应复用同一配置解析链路，不能复制八套实现。
- **目标**：补参数化宿主测试，逐项证明 KEY1—KEY8 都能保存完整配置值，并产生正确的按下／松开事件。
- **输入**：T02 固定合同、T03 纯逻辑实现、现有八键配置循环。
- **产出路径**：`host_test/` 中的最小覆盖测试；只有测试证明存在真实缺口时才修改生产逻辑。
- **验收标准**：八键逐项通过；每次 press 恰好一个事件，release 无事件；复制、粘贴、快捷键和固定文字回归；示例 UUID 只存在宿主测试。
- **状态**：已完成（2026-09-15 学员分支 `my-host-action-practice-v2`，基于 `aa33205`）；仅新增宿主测试，生产逻辑零改动。

## 学员执行记录（2026-09-15）

### 既有能力确认（不为改动而改动）

直接读码确认 KEY1—KEY8 早已共用一条通用解析链路，没有任何按键特化，因此 T04 不需要改生产逻辑：

- `components/keyboard/src/config_payload.cpp:894-903` 定义唯一的 8 元素映射表 `key_inputs{{"KEY1",Key1}…{"KEY8",Key8}}`；
- `:905-909` 单一 `for` 循环对八个键调用同一个 `parse_binding()`；
- `parse_binding → parse_action`（:579-591 / :544-…）是 T03 已加 `host_action:` 分支的唯一动作解析入口，八键天然等价覆盖。

缺口只在“证据”：T03 测试只演示了 KEY1。故本节点只补测试。

### 实际修改文件（2 个，全部在 host_test/）

- `host_test/config_payload_tests.cpp`：新增 3 组测试并注册（T04 固定示例 UUID 用 `host_action:123e4567-e89b-12d3-a456-426614174000`）：
  1. `each_of_the_eight_keys_parses_host_action_through_shared_path`：8 次循环，每次仅一个 KEYn 配置为 Host Action、其余 7 键 disabled；逐项断言解析 Ok、`ActionKind::HostAction`、Keymap 中保留完整 `host_action:` 前缀、press 恰好一个 HostAction 事件且值为完整配置串、release 为 None，其余 7 键为 Disabled。
  2. `all_eight_keys_coexist_as_host_actions_in_one_profile`：同一份 profile 八键全部配置为同一 Host Action，逐项断言解析与事件，证明无按键间串扰/特殊分支。
  3. `host_action_coexists_with_legacy_actions_without_regression`：一份混合 profile（KEY1 Host Action、KEY2 hotkey Return、KEY3 固定文字 "hi"、KEY4 disabled、KEY5–8 select_all/copy/paste/undo），断言 Host Action 语义不变且快捷键、固定文字、语义和弦（MacOS Meta+A/C/V/Z）回归。
- `host_test/firmware_source_contract_tests.cpp`：新增守卫 `host_action_example_uuids_never_ship_in_production_sources()` 并注册——断言 8 个生产源文件（app_main.cpp、nvs_store.cpp、keymap.cpp、config_payload.cpp、config_state.cpp、host_action_protocol.{h,cpp}、keymap.h）不含固定示例 UUID 片段 `123e4567-e89b`。

### 测试命令与结果（w64devkit GCC 13.2，CMake 3.30.2）

- 过程证据：首次构建因新测试使用未导入的 `Action` 类型名报 `'Action' does not name a type`（2 处），改为 `ai_keyboard::Action` 全局限定后通过。
- 定向：`cmake --build build-host --target config_payload_tests firmware_source_contract_tests` 链接成功；`ctest -R 'config_payload|firmware_source_contract'` → **2/2 通过（5.77 秒）**。
- 全量：`ctest --test-dir build-host` → **100% tests passed, 0 failed out of 58**（92.66 秒）；未新增测试可执行文件，覆盖全部在既有测试程序内。

### KEY1—KEY8 逐项证据（断言级，全部在通过的 config_payload_tests 内）

| 键 | 独立解析 | Keymap 保留完整前缀 | press 一个 HostAction | release None | 其余 7 键 Disabled |
|---|---|---|---|---|---|
| KEY1 | ✅ | ✅ | ✅ | ✅ | ✅ |
| KEY2 | ✅ | ✅ | ✅ | ✅ | ✅ |
| KEY3 | ✅ | ✅ | ✅ | ✅ | ✅ |
| KEY4 | ✅ | ✅ | ✅ | ✅ | ✅ |
| KEY5 | ✅ | ✅ | ✅ | ✅ | ✅ |
| KEY6 | ✅ | ✅ | ✅ | ✅ | ✅ |
| KEY7 | ✅ | ✅ | ✅ | ✅ | ✅ |
| KEY8 | ✅ | ✅ | ✅ | ✅ | ✅ |

另：八键共存 profile 8/8 全部 HostAction 且按下/松开语义一致。

### 旧动作兼容与示例 UUID 隔离证据

- 复制/粘贴/全选/撤销：混合 profile 测试断言 MacOS 下仍为 Meta+A/C/V/Z 的 HidKeyDown；全量套件中原 `keymap_tests`（双平台语义、默认八键 HID）与 config_payload 既有迁移/回归用例全部通过。
- 快捷键：混合 profile 中 KEY2 `{"hotkey":"Return"}` → HidKeyDown("Return")；固定文字：KEY3 `{"text":"hi"}` → FixedText("hi") 且松开 None。
- 示例 UUID 隔离：全仓 grep `123e4567-e89b` 仅命中 `host_test/`（15+ 行测试夹具）；生产源码守卫测试自动化锁定；默认 Keymap（`keymap.cpp:28-42` DefaultKeymap）与任何生产配置不含示例 UUID，也不含 `host_action` 默认绑定。
- `git status --short` 实测仅 `host_test/config_payload_tests.cpp`、`host_test/firmware_source_contract_tests.cpp` 两个文件 modified；`components/`、`main/` 零差异。

### 边界遵守

未修改 main/platform、USB/BLE 适配、路由、能力声明、GPIO、BOOT、电源、Flash 分区、设备身份；未烧录、未跑固件构建。

### 仍未验证项

- 宿主测试为电脑端逻辑证据；真机八键 GPIO→事件→Vendor HID 发送待 T06/T09 与实板验证。
- App 0.1.26 实际下发八键 Host Action 配置的兼容性未在本节点验证。
- 共享编码的真机字节、单通道不双发仍待 T06；能力声明与 512 字节预算待第 08 步。
