# T02 · Host Action v1 固定兼容协议

> 本任务卡记录已经冻结的兼容合同，不是方案建议。后续实现只能落实该合同，不能自行设计、改写或扩展 v1。本节点只改 `flow/`，不代表 Host Action 已在固件中实现。

- **背景**：EasyInput App 侧使用已经准备好的 0.1.26；App 同步给 Maker 固件的按键配置会包含真机 UUID。当前 Maker 公开基线尚未实现 Host Action，也尚未出现 App Command kind `0x05` 的实现，这是后续节点要增加的能力，不是冲突。
- **目标**：在改代码前核对现有保留编号、消息容器和项目约定，并完整记录 Host Action v1 的固定配置、传输、事件、映射、验证与演进边界。
- **允许范围**：只读当前 Maker 仓库；只新增或修改根级 `flow/` 项目记录。
- **禁止越过的边界**：不修改固件、宿主测试、构建配置或其他非 `flow/` 文件；不运行测试、构建或烧录；不读取或修改其他电脑端工程；不访问设备；不自行设计或改写协议。
- **产出路径**：本任务卡；`flow/decisions.md`；`flow/plan.md`；`flow/进展.md`。
- **状态**：合同核对与记录完成；纯逻辑实现见 `flow/tasks/T03-Host-Action-v1-纯逻辑实现.md`，USB／BLE 传输适配仍未开始。

## 操作前文件状态

- 分支与提交：`main`，`34087cd40d24d23579da0357973ebc1a37e7ce7c`。
- `git status --short` 在本节点写入前只有：`M flow/plan.md`、`M flow/进展.md`、`?? flow/tasks/T01-工程结构与板型只读核对.md`。
- `git diff --name-status` 在本节点写入前只有：`M flow/plan.md`、`M flow/进展.md`。
- 结论：操作前没有已跟踪或未跟踪的非 `flow/` 变更；上述既有 `flow/` 改动属于先前工作，本节点在其上增量记录，不覆盖或冒充原有内容。

## 现有工程冲突核对

| 核对项 | 当前仓库证据 | 结论 |
|---|---|---|
| Report ID | `components/keyboard/include/keyboard/fixed_text_protocol.h` 固定 App Command Report ID 为 `0x11`；`main/platform/usb_hid.cpp` 与 `main/platform/ble_hid.cpp` 使用相同编号 | v1 继续使用既有 `0x11`，无冲突 |
| kind 编号 | 现有 App Command kind 为 `0x01` 固定文字、`0x02` 热键、`0x03` 配置回执；`components/keyboard/include/keyboard/status_hid_protocol.h` 明确状态响应为 `0x04` | App Command kind `0x05` 未被实现或占用；源码中其他 HID Usage、键码和测试数据的字面量 `0x05` 不属于此编号空间 |
| 消息容器 | USB／BLE HID 描述符都把 Report ID `0x11` 的 payload 定为 63 字节；现有编码函数使用 `[0]` kind、`[1]` chunk index、`[2]` total chunks、`[3]` data length，数据从 `[4]` 开始，最大 59 字节 | 4 字节头加 36 字节 UUID 共占 `[0..39]`；`[40..62]` 共有 23 字节余量，固定字段可以完整承载 |
| 运输选择 | `components/keyboard/src/transport_routing.cpp` 当前返回 `UsbFirst`；`main/app_main.cpp` 选定 USB endpoint epoch 后直接返回，无 USB owner 时才尝试 BLE，也不会因发送失败迁移到另一通道 | 可以沿用现有单通道选择；合同禁止 USB／BLE 双发，与当前规则无冲突 |
| 配置与事件基线 | `components/keyboard/src/config_payload.cpp`、`components/keyboard/include/keyboard/keymap.h` 和 `components/keyboard/src/keymap.cpp` 当前没有 Host Action 动作或事件 | 属于待实现缺口，不是合同冲突 |
| BLE 状态预算 | `components/keyboard/include/keyboard/config_status.h` 固定状态安全上限为 512 字节，状态协议和宿主测试也以 512 字节为上限 | 第 08 步加入能力声明时必须继续验证不超过 512 字节 |

**冲突门结论**：没有发现现有项目明确占用 App Command kind `0x05`，没有发现 63 字节容器无法承载固定字段，也没有发现与项目约定直接冲突的事实；因此按授权立即写入本合同，不停止等待第二次确认。

## Host Action v1 冻结合同

### 1. 配置保存与 UUID 校验

- 配置层保存完整字符串：`host_action:<canonical-lowercase-uuid>`。
- `host_action:` 后必须恰好是 36 个 ASCII 字符的规范小写 UUID；连字符位于从 1 开始计数的第 9、14、19、24 位，其余位置只允许 `0-9` 或 `a-f`。
- 大写、长度错误、连字符位置错误或包含非法字符的非规范 UUID 必须直接拒绝并 fail closed；不得自动转成小写或以其他方式修复后接受。
- 不增加 UUID version 限制，也不增加 nil UUID 限制；只按上述规范格式校验。
- 配置解析成功后，Keymap／运行状态与持久化内容都保留完整的 `host_action:` 前缀和规范 UUID；只有运行传输编码时去掉前缀。

### 2. 运行传输字节合同

| 字段 | 固定值或内容 |
|---|---|
| Report ID | `0x11` |
| payload `[0]` kind | `0x05` |
| payload `[1]` chunk index | `0` |
| payload `[2]` total chunks | `1` |
| payload `[3]` data length | `36` |
| payload `[4..39]` data | 不带 `host_action:` 前缀的 36 字节 UUID ASCII |
| payload `[40..62]` | 现有 63 字节 App Command 容器余量，不是 Host Action 数据 |

- 63 字节只表示现有 App Command 消息容器大小，不是 Host Action 数据长度；Host Action v1 的 data length 固定为 36。
- App Command kind `0x04` 已用于状态响应，Host Action v1 不得复用或改写 `0x04`。
- USB 与 BLE 必须发送完全相同的 v1 内容，并沿用现有单通道选择；同一次按键动作不得向 USB 与 BLE 双发。

### 3. 按键事件语义

- 按下时发送一次 Host Action v1。
- 松开时不发送 Host Action，也不补发释放消息。
- 非规范配置必须 fail closed，不得生成或发送 Host Action 事件。

### 4. 固件与 App 的职责边界

- 固件只保存配置中的完整 `host_action:<canonical-lowercase-uuid>`，不保存应用路径、应用名称或 Bundle ID。
- 真实的“UUID → 本机应用”映射只保存在 EasyInput App 本地。
- EasyInput App 同步给固件的是包含真机 UUID 的按键配置；固件按下后只把 UUID 按本合同发回当前单一宿主通道。
- 固定示例 UUID 只用于宿主测试，不得作为最终应用映射、默认按键映射或固件内置应用身份。

### 5. 版本演进

- Host Action v1 的以上字段已经冻结。
- 未来如需任何不兼容变化，必须使用新的协议版本和新的能力声明；不得在仍宣称 v1 时静默改写字段、校验、事件或发送语义。

## 后续完整改动层（不得缺项）

> 下列内容是完整实现必须覆盖的范围。本节点不自行拆分或实现这些代码；能力声明的时间点已经固定。

- [ ] 配置解析与持久化：识别并严格校验完整 `host_action:<canonical-lowercase-uuid>`，候选无效时 fail closed，成功后由现有配置状态和 NVS 路径持久化完整字符串。
- [ ] Keymap 与事件：增加对应动作和固件事件；按下产生一次事件，松开产生 `None`／不发送。
- [ ] 共享协议编码：在可宿主测试的共享层生成 `0x11`／`0x05` App Command 报告，严格固定 `0/1/36` 头字段和 36 字节 UUID 数据，并确保 `[40..62]` 只作为现有容器余量，避免 USB／BLE 各自复制一套易漂移编码。
- [ ] USB／BLE 适配：两侧调用同一共享编码结果并保留各自现有 owner／epoch 与队列保护；沿用 USB 优先的单通道选择，禁止失败迁移造成双发。
- [ ] 宿主测试：覆盖规范小写 UUID、按下一次／松开不发、精确字节布局、USB／BLE 内容一致、单通道路由，以及大写、错误长度、错误连字符位置、非法字符的 fail-closed；同时证明没有额外加入 UUID version 或 nil UUID 限制。固定示例 UUID 只能存在于宿主测试语境。
- [ ] 第 08 步能力声明：只在第 08 步加入 `"host_action_v1": true`，并为所有相关 BLE 状态输出增加或更新“不超过 512 字节”的检查；第 08 步之前不得提前声明能力。

## 本合同不承担的范围

- 不修改、读取或设计其他电脑端工程；App 侧版本固定使用已经准备好的 EasyInput 0.1.26。
- 不在固件中保存或推断应用路径、名称、Bundle ID，也不复制 App 本地的 UUID 映射。
- 不把测试示例 UUID 变成真实或默认应用映射。
- 不增加 UUID version 或 nil UUID 限制，不自动规范化非规范输入。
- 不新增 Report ID、第二种 Host Action 消息、释放消息、分片方式、USB／BLE 双发或其他 v1 字段。
- 不修改 GPIO、BOOT、GPIO8 共享供电、分区、USB／BLE 设备身份或硬件生命周期。
- 本节点不修改代码、不运行测试或构建、不烧录；合同记录完成不等于功能实现、测试通过、构建通过或实板通过。

## 本节点验收自检

- [x] 操作前分支、提交和工作区文件状态已先记录。
- [x] 固定合同的配置字符串、规范小写 UUID 校验和 fail-closed 已逐项记录。
- [x] 未增加自动小写、UUID version 或 nil UUID 限制。
- [x] `0x11`、`0x05`、`0`、`1`、`36` 与数据区 `[4..39]` 已逐项记录。
- [x] 已明确 `[40..62]` 只是 63 字节容器余量，且 `0x04` 已用于状态响应。
- [x] 已记录固件身份边界、App 本地映射、测试 UUID 边界及按下／松开语义。
- [x] 已记录 USB／BLE 相同内容、现有单通道选择和禁止双发。
- [x] 已记录全部后续改动层，以及第 08 步能力声明和 BLE 状态 512 字节检查。
- [x] 已记录 v1 冻结与不兼容变化必须升级版本和能力声明。
- [x] 当前缺少 Host Action／kind `0x05` 实现按预期记为待实现，不误报为冲突或已完成功能。

---

## 学员分支复核记录（2026-09-14 · my-host-action-practice-v2）

> 学员在本机练习分支按本卡重新执行核对。上方基线内容（含作者侧 `main@34087cd...` 操作前状态）保持原样，本节只增量记录本分支证据，不覆盖、不冒充基线记录。

### 操作前文件状态（本分支）

- 当前分支：`my-host-action-practice-v2`（`.git/HEAD`）；HEAD：`8806d8d6e2a751cb08a5173316b2bdf300e064d2`（`docs(flow): log step 01-02 practice handoff cards`），父提交 `44bd2bad...`（Windows 宿主测试 ogg 二进制读取修复，已提交）；仓库克隆自 `https://github.com/CY-CHENYUE/easy-input-maker.git`（起点 `7d5f1574...`，见 `.git/logs/HEAD`）。
- 本节点写入前未执行 `git status`；依据 `.git/logs/HEAD` 与工作区文件核对，疑似唯一未提交变更是 `flow/进展.md` 顶部 2026-09-10 第 03 步记录（HEAD 提交信息只覆盖第 01–02 步），学员可用 `git status --short` 复核；未发现其他 `flow/` 之外变更的证据。
- 写入前全仓 grep：没有任何 Host Action 源码文件，`host_action` 字样仅命中 `flow/` 文档与 starter 防护测试本身。

### 独立源码复核（当前树 file:line）

| 合同项 | 当前证据 | 结论 |
|---|---|---|
| Report ID 0x11／容器 63 | `components/keyboard/include/keyboard/fixed_text_protocol.h:9-14`（0x11、payload 63、header 4、chunk data 59）；`main/platform/usb_hid.cpp:29,39`、`main/platform/ble_hid.cpp:58,63`；描述符 `usb_hid.cpp:106-113`、`ble_hid.cpp:270-277`（ID 0x11，Count 63） | 与合同一致 |
| 头部字节布局 | `components/keyboard/src/ble_fixed_text_stream.cpp:61-69`：`[0]`kind、`[1]`chunk、`[2]`total、`[3]`len，数据自 `[4]`；USB 同签名 `main/platform/usb_hid.cpp:1602-1629` | 4+36=40 占 `[0..39]`，`[40..62]` 为 23 字节余量，固定字段可完整承载 |
| kind 编号占用 | `0x01` fixed_text_protocol.h:10／usb_hid.cpp:34；`0x02`、`0x03` usb_hid.cpp:35-36、ble_hid.cpp:59-60；`0x04` status_hid_protocol.h:20 | App Command kind `0x05` 未被占用；其余字面量 0x05（HID Usage Page、键码、测试数据）不属此编号空间 |
| 单通道选择 | `components/keyboard/src/transport_routing.cpp:5-9` 恒返回 `UsbFirst`；同文件 147-211 行 `KeyboardTransportLatch` 单 owner、断线不迁移；调用点 `main/app_main.cpp:1376` | 可沿用现有单通道；禁止双发与现状无冲突 |
| Host Action 缺口 | `components/keyboard/include/keyboard/keymap.h:77-82` `FirmwareEventKind` 无 HostAction；防护测试 `host_test/firmware_source_contract_tests.cpp:3530-3543` 断言六处源文件不含 `FirmwareEventKind::HostAction`、`host_action_protocol.h`、`host_action_v1` | starter 起点合同，非冲突 |
| 512 字节状态预算 | `components/keyboard/include/keyboard/config_status.h:11` `kConfigStatusGattSafeLen = 512`；`status_hid_protocol.h:27` 最大 JSON 等同该值 | 第 08 步必须继续满足 |
| 持久化路径 | `main/platform/nvs_store.cpp` 已以字符串保存整份配置 JSON（`nvs_get_str` 79/92、`nvs_set_str` 123/287） | 完整 `host_action:` 字符串后续走既有 NVS 路径 |

**本分支冲突门结论**：未发现 0x05 被占用、63 字节容器无法承载固定字段或其他直接冲突；按授权写入记录，不停止、不等待二次确认。

### 合同逐项复述与自检

1. 配置层保存完整 `host_action:<canonical-lowercase-uuid>`；Keymap／运行态／持久化都保留前缀，仅运行传输编码去掉前缀。
2. 前缀后恰好 36 个 ASCII；连字符在从 1 起第 9、14、19、24 位；其余位置仅 `0-9`、`a-f`。
3. 大写、长度错误、连字符位置错误、非法字符一律直接拒绝并 fail closed；不自动转小写、不修复后接受。
4. 不增加 UUID version 限制，不增加 nil UUID 限制。
5. 传输固定：Report ID `0x11`、kind `0x05`、chunk index `0`、total chunks `1`、data length `36`；数据为 payload `[4..39]` 的 36 字节裸 UUID ASCII。
6. `[40..62]` 只是既有 63 字节容器余量；63 是 App Command 容器大小，不是 Host Action 数据长度。
7. kind `0x04` 已用于状态响应，v1 不复用、不改写。
8. 按下发送一次；松开不发送、不补发释放消息；非规范配置不产生也不发送事件。
9. 固件不保存应用路径、名称或 Bundle ID；“UUID → 本机应用”映射只保存在 EasyInput App 本地；App 同步给固件的是含真机 UUID 的按键配置。
10. 固定示例 UUID 只用于宿主测试，不作为最终／默认应用映射或固件内置应用身份。
11. USB 与 BLE 发送完全相同内容，沿用现有单通道选择；禁止双发，禁止发送失败迁移到另一通道。
12. 完整改动层六项不缺：配置解析与持久化、Keymap 与事件、共享协议编码、USB／BLE 适配、宿主测试、第 08 步 `"host_action_v1": true` 能力声明与所有相关 BLE 状态 ≤512 字节检查；能力声明在第 08 步前不得出现。
13. v1 全部字段已冻结；未来不兼容变化必须使用新版本与新能力声明，不得在仍宣称 v1 时静默改写。
14. 本节点不改代码、不运行测试／构建／烧录、不离开本仓；缺少 Host Action／kind `0x05` 实现记为后续待实现，不误报为冲突，也不冒充已完成功能。

### 操作后状态

- 本节点仅新增本节，并在 `flow/进展.md` 顶部追加 2026-09-14 交接条。
- `flow/decisions.md` 第 16-21 行已有 2026-08-22 冻结合同决策、`flow/plan.md` 第 20 行已有合同入口，内容与本合同一致，未重复新增决策或计划条目。
- 零代码变更：未触碰 `main/`、`components/`、`host_test/`、`CMakeLists.txt` 等任何 `flow/` 之外文件；未运行测试、构建或烧录；未访问其他电脑端工程或设备。
