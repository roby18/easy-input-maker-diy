# T08 · Host Action v1 能力声明与 BLE 状态预算

- **背景**：App 需要从状态 JSON 发现能力，而 BLE 最终发布状态有 512 字节硬上限。
- **目标**：把唯一布尔能力值加入实际状态构造路径，并验证每个发布变体的最终 UTF-8 字节数。
- **输入**：T02 合同、T06 发送接入、现有完整／紧凑／诊断／battery／fallback 状态路径。
- **产出路径**：最小状态实现与宿主预算测试。
- **验收标准**：能力是布尔 `true`、只在 capabilities 中出现一次；`0x04`／`0x05` 职责不变；所有实际发布变体不超过 512 字节；不得提高上限或静默删除无关状态字段。
- **状态**：待办

---

# 学员执行记录（2026-09-15，分支 my-host-action-practice-v2）

基线原文保留在上方，本节为学员追加。状态：**已完成（宿主层证据）**。未运行 ESP-IDF 构建、未烧录、未做 App／真机验证。

## 起点核实

- 开始时工作区干净，HEAD `a8cc568`（T06）；无候选实现、无未提交红测。生产代码 grep `host_action_v1` 零命中（仅 flow/ 文档）。
- 两项已批准预算规则在课程基线 `743368a` 中即已存在，本节点**保留并以测试锁定**，未重写：
  - `kSpeakerProbeFirmwareStringMaxLen = 16`（config_status.cpp:17），普通状态 `kFirmwareStringMaxLen = 40`（config_status.cpp:16）不变。
  - normal／diagnostics／battery 三条回退链都有"先只省略 cycle_seq／cycle_inactive_ms／cycle_deep_sleep／cycle_wake 四字段、保留 current power（mode／inactive_ms／deep_sleep_block／last_wake）"的中间级（config_status.cpp:664-679、748-763、804-819），再往后才是更激进的删减。

## 能力声明所在路径（布尔字面量，3 处构造点 + 1 个 fallback 文档）

| # | 路径 | 位置 | 覆盖的实际发布变体 |
|---|---|---|---|
| 1 | `append_sync_core()` 无条件在 config_max_bytes 后输出 `,"host_action_v1":true` | components/keyboard/src/config_status.cpp:238-248 | 所有 compact 变体（normal compact 及全部回退级、confirmation、battery 五级、diagnostics 四级、boot 指纹兜底） |
| 2 | `build_speaker_probe_status_json()` 独立 capabilities | config_status.cpp:510-512 | speaker_probe |
| 3 | `build_full_status_json()` capabilities 末尾 | config_status.cpp:563-567 | full normal |
| 4 | `kConfigStatusFallbackJson` 共享 fallback 常量 | components/keyboard/include/keyboard/config_status.h:16 | publish_status_json 的两次 fallback |

BLE 最终发布（main/platform/ble_hid.cpp:1883-1917 `publish_status_json` 与 :3695-3731 `status_json_for_publish`）不构造能力字段：只做 512 兜底替换和 ble overlay 附加；全 main/ 目录 grep `host_action_v1` **零命中**。

## TDD 证据

- RED（生产未改时）：`config_status_tests` 断言失败于 config_status_tests.cpp:68（能力串不含新布尔）；`firmware_source_contract_tests` 失败于 :3640（构造点计数）。
- GREEN：四处最小修改（生产 diff 仅 +9 行含注释、-1 行）后两测试转绿。

## 每个实际发布变体的最终 UTF-8 字节数（测试运行时实测打印，非引用常量）

| 发布变体 | 状态 JSON raw | BLE 最终发布字节 | 说明 |
|---|---|---|---|
| full normal | 399 | 399 | 非 battery 走 detailed overlay，最坏计数器片段放不下时共享编码器跳过附加 |
| compact normal（含 cycle，放得下） | 504 | 504 | 四 cycle 字段保留 |
| compact normal（超限，仅省略 cycle） | 462 | 462 | current power 全保留 |
| compact minimal audio（最深运行时回退） | 426 | 426 | control_state 保留，旧静态能力与 power 已省，host_action_v1 仍在 |
| compact diagnostics（long-press 分支） | 509 | 509 |  |
| speaker_probe（19 项全指标最坏情况） | **512** | **512** | **全变体最大值**；+19 字节能力位由 firmware 36→16（省 20 字节）抵消 |
| confirmation（push 响应） | 410 | 410 | 无 audio/power/diag |
| battery compact + BLE overlay | 402 | **479** | 最坏简单 overlay（connected/valid=1，三参数 65535，77 字节）实际附加成功 |
| battery bounded base（计数／字符串极限） | 492 | 492 | current power 全保留，四 cycle 字段省略；最坏 overlay 放不下时不附加 |
| fallback 常量 | 241 | 241 | status_too_large 兜底文档 |

**最大值 512 字节 = 512 上限，剩余空间 0**（speaker_probe 最坏情况刚好顶满；512 常量未改动）。非 battery 变体的 final_ble=raw 是因为共享编码器在最坏 detailed overlay（约 300 字节）附加后会超 512 时整体跳过附加——生产真实计数器通常远小；无论附加与否最终发布恒 ≤512，由 `append_ble_*_status_wire_json` 内部门限保证。

## 规则逐项测试证据

- 规则一（speaker_probe）：`speaker_probe_worst_case_is_versioned_complete_and_gatt_safe` 与新枚举用例共同断言 40 字符 firmware 被裁为 **16**（无第 17 个 f）、`v/g/st/r/e/x` + `mg/fu/du/mu/n/p/sw/h0/h1/hl/hm/pk/rm` 共 **19 个指标键全部存在**、字段名与 JSON 结构未变；普通路径用例仍断言 40 字符 firmware 完整保留（config_status_tests.cpp recent_cycle_overflow 用例，`assert_sync_core(json, std::string(40,'f'), …)`）。常量行 git diff 未触碰。
- 规则二（power-cycle 中间回退）：
  - normal 放得下：`carries_truthful_recent_deep_sleep_cycle_when_budget_allows`（既有）+ 新枚举"compact normal with cycle"（504 字节，四 cycle 字段逐项在）。
  - normal 超限：`recent_cycle_overflow_keeps_current_power_and_omits_only_cycle`（既有）+ 新枚举"compact normal no cycle"（462 字节，mode/inactive_ms/deep_sleep_block/last_wake 逐项在，四 cycle 字段逐项不在）。
  - battery 超限：`battery_status_is_bounded_at_counter_limits_and_keeps_power`（既有）+ 新枚举"battery bounded base"（492，current power 四字段在、四 cycle 字段不在）；`includes_fresh_battery_metadata`、`battery_status_keeps_current_power_before_optional_speaker_evidence` 继续证明 battery 每一级都先保 current power。
  - 能力存在性：新 `assert_host_action_capability_boolean_once` 对全部 10 个枚举变体断言 `host_action_v1` 恰好出现 1 次、形态为 JSON 布尔 `true`（非字符串 `"true"`、非 false、无重复顶层字段）且 UTF-8 合法。

## 0x04／0x05 职责与禁止范围证据

- 新源码守卫 `host_action_capability_stays_in_status_json_outside_app_command_payload`（firmware_source_contract_tests.cpp:3611-3648）：host_action_protocol.{h,cpp} 不含 `host_action_v1`/`capabilities`；config_status.{h,cpp} 不 include host_action_protocol、不引用 kHostActionAppCommandKind；cpp 中能力名恰好 3 个构造点、h 中恰好 1 个 fallback；任何字符串化写法被禁。
- `git status --short` 仅 4 文件：config_status.h（+1/-1）、config_status.cpp（+8/-2）、host_test/config_status_tests.cpp、host_test/firmware_source_contract_tests.cpp；**main/ 零差异**（USB/BLE 发送、HID 描述符、GATT、身份、GPIO、BOOT、电源、Flash 分区均未触碰）；host_action_protocol.*/transport_routing/keymap/config_payload 零差异；0x11/0x05/0x04/63 容器与 512 常量均未改；无未跟踪文件。
- 未运行 ESP-IDF 构建、未烧录。

## 测试命令与结果

- 定向（RED→GREEN）：`cmake --build build-host --target config_status_tests firmware_source_contract_tests` + `ctest --test-dir build-host -R 'config_status_tests|firmware_source_contract_tests'`。
- 全量：`ctest --test-dir build-host` → **58/58 通过（51.87 秒）**。
- 回归：T03 纯逻辑三件套（host_action_protocol/keymap/config_payload）与 T06 transport_routing、platform 契约守卫均在全量中通过；status_hid_protocol（0x04）、ble_status_wire、gatt_status_snapshot 通过。

## 仍未验证项

① 19 个 speaker 指标是按冻结合同与基线代码计数的宿主断言，未经真机 GATT 读取；② battery worst-case overlay 门限（77 字节预留）下 detail/cycle 同时保留不可达——这是冻结常量的性质，真实连接 overlay 通常更小，实际保留级别需真机观察；③ ESP-IDF 编译、烧录、App 0.1.26 对新能力字段的解析留 T09/T10；④ 未做 App／真机验证（本任务授权范围外）。
