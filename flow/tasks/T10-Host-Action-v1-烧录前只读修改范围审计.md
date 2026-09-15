# T10 · Host Action v1 烧录前只读修改范围审计

- **背景**：烧录前必须证明课程改动没有越过硬件、身份、分区、协议和隐私边界。
- **目标**：以 starter 固定标签到当前工作区的全部 staged、unstaged 和 untracked 内容为审计对象，逐文件读取差异并归类。
- **输入**：T02 冻结合同、T09 软件证据、EasyInput V2.0 硬件安全文档和完整 Git 状态。
- **产出路径**：`flow/` 中脱敏后的审计矩阵和结论。
- **验收标准**：GPIO、BOOT、GPIO8、分区、设备身份、HID／GATT 和隐私日志边界全部 PASS；每项变化可解释；`git diff --check` 通过。烧录仍需单独确认设备和授权。
- **状态**：已完成（2026-09-15，只读；**SAFE_TO_REQUEST_FLASH = NO**，阻塞项为 1 项文档卫生问题，见末尾）

## 审计边界与方法

- 全程只读：未修改代码、配置、测试；未访问设备、串口、App；未烧录；未执行删除／回退／覆盖。
- 审计对象＝课程起点提交 `743368a`（course: add Host Action v1 starter baseline）到当前工作区的全部版本控制差异。基线可靠性：`git merge-base --is-ancestor 743368a HEAD` exit 0；起点之前的 BLE 持久化修复（7619bd1）等不在差异内，不作为本课程改动重复计数。
- 仓库根单一 Git 仓库：`git rev-parse --show-toplevel` 指向本 Maker 仓；`managed_components/` 下无嵌套 `.git`。
- 分支 `my-host-action-practice-v2`，审计时 HEAD `0b69030`（第9步）。基线→HEAD 共 8 个提交：第02步 ogg 二进制读取修复、01-02 交接文档、第4步（T02 学员复核，纯文档）、T03/T04/T06/T08/T09。
- tracked 变化全部实际读取 diff；本节点无 untracked 文件；ignored 目录不递归抄生成文件，但顶层路径、命中规则、用途、是否参与构建/烧录、是否可能进入提交均逐项取证。

## 使用的命令（脱敏）

```text
git rev-parse --show-toplevel / git branch --show-current
git log --oneline -12 ; git merge-base --is-ancestor 743368a HEAD
git status --short --ignored                         # 不带任何路径限定
git status --porcelain=v1 -z ; git status --porcelain=v1 --ignored -z
git diff --name-status / --numstat 743368a HEAD
git diff 743368a HEAD -- components main > <忽略目录中的临时导出>   # 逐行读
git diff 743368a HEAD -- host_test / -- flow        # 逐行读
git diff --check 743368a HEAD ; git diff --check     # exit 0
git check-ignore -v <9 个顶层条目逐个>
git ls-files --error-unmatch dependencies.lock sdkconfig.defaults
git grep -n -E '123e4567-e89b-(12d3|42d3)' -- components main   # 无命中
python .trae/skills/easyinput-board-cy/scripts/check_board_baseline.py .
读取 build/project_description.json、build/sdkconfig、sdkconfig.defaults、partitions.csv、
     dependencies.lock、两个 .component_hash、.gitignore、.git/info/exclude
```

## 数量闭合（写入本卡前）

- staged 0、unstaged 0、untracked 0（porcelain 条目 0；工作树干净，T09 已随 `0b69030` 提交）。
- ignored 顶层条目 9：`.cache/`、`.eim/`、`.trae/`、`build-host/`、`build/`、`eim_build.log`、`eim_install.log`、`eim_list.log`、`managed_components/`。
- 基线→HEAD tracked 变化 28 个：新增 5、修改 23、删除 0。分类：Host Action 生产实现 11、宿主测试 9、project-flow-cy 记录 8。

## 全部 28 个 tracked 变化文件分类（diff 均已逐行读取）

### A. Host Action 生产实现（11，T03/T06/T08 节点）

| 文件 | 增/删 | 归属与证据摘要 |
|---|---|---|
| `components/keyboard/include/keyboard/host_action_protocol.h` | +55（新） | 冻结合同共享编码：0x11/0x05/0/1/36/63 常量、严格 canonical 校验声明；纯逻辑无 USB/BLE/NVS |
| `components/keyboard/src/host_action_protocol.cpp` | +112（新） | 校验 fail-closed、63 字节编码（[4..39] 裸 UUID，[40..62] 零）；static_assert 锁定 0x05≠0x04、4+36=40 |
| `components/keyboard/include/keyboard/keymap.h` | +5/0 | 新增 ActionKind/Event 枚举值与 Action.host_action 字段 |
| `components/keyboard/src/keymap.cpp` | +7/0 | HostAction 按下发一次、松开发 None |
| `components/keyboard/src/config_payload.cpp` | +16/0 | 解析 `host_action:` 全前缀值，非 canonical 整份拒绝，不转小写 |
| `components/keyboard/include/keyboard/config_status.h` | +1/−1 | fallback 常量加一次布尔能力声明 |
| `components/keyboard/src/config_status.cpp` | +7/−1 | compact 核心/speaker_probe/full 三处各加一次 `"host_action_v1":true` |
| `components/keyboard/CMakeLists.txt` | +1/0 | 注册新源文件 |
| `main/platform/usb_hid.cpp` | +30/0 | include+7 条 static_assert+switch 内 1 个 HostAction case，经既有单 chunk 0x11 通道；无其他改动行 |
| `main/platform/ble_hid.cpp` | +28/0 | 同上（BLE 侧），经既有 send_app_command_report；无其他改动行 |
| `main/app_main.cpp` | +3/0 | 仅日志 kind 名 `host_action`；不含共享头、不编码、无发送分支 |

### B. 宿主测试（9）

| 文件 | 增/删 | 归属 |
|---|---|---|
| `host_test/host_action_protocol_tests.cpp` | +192（新） | T03 合同测试 |
| `host_test/host_action_key_bindings_tests.cpp` | +195（新） | T04 八键覆盖，T09 原样搬迁为具名目标 |
| `host_test/host_action_capability_status_tests.cpp` | +531（新） | T08 能力声明/512 变体，T09 原样搬迁为具名目标 |
| `host_test/config_payload_tests.cpp` | +58/0 | T03 解析用例；T04 用例 T09 迁出（净增 58） |
| `host_test/config_status_tests.cpp` | +7/−3 | 能力串期望同步 + T08 用例 T09 迁出登记 |
| `host_test/keymap_tests.cpp` | +27/0 | T03 按下/松开事件 |
| `host_test/transport_routing_tests.cpp` | +13/0 | T06 UsbFirst 单通道路由 |
| `host_test/firmware_source_contract_tests.cpp` | +110/−12 | 含两类内容：① 第02步非 Host Action 的 Windows 可移植性修复（ogg fixture 改二进制读取，+13/−1 量级，提交 44bd2ba，flow/进展已记录）；② T03/T06/T08 源码契约守卫（starter 守卫按设计替换为示例 UUID 禁入生产、共享编码单路由、能力声明不进 0x11 payload） |
| `host_test/CMakeLists.txt` | +4/0 | core 源列表 +1、注册三个具名测试目标 |

### C. project-flow-cy 记录（8，均为流程文档）

`flow/plan.md`、`flow/进展.md`、T02/T03/T04/T06/T08/T09 六张任务卡。其中 T02 卡仅追加学员分支复核记录；T03 卡与进展含本卡结论指出的本机路径问题（见末尾阻塞项）。

## ignored 顶层条目逐项分类（9 个，全覆盖）

| 顶层条目 | 命中规则 | 来源/用途 | 参与构建/烧录 | 可能进入提交 | 依据 |
|---|---|---|---|---|---|
| `build/` | `.gitignore:10 build/` | ESP-IDF v5.5.5/esp32s3 默认构建输出（elf/bin/bootloader/分区表、生成的 S 汇编、project_description.json、build/sdkconfig） | 产物本身是烧录物料来源，但烧录动作本节点未执行 | 否（被忽略；无强制添加） | `git check-ignore -v`；目录内全部为生成物 |
| `build-host/` | `.gitignore:12 build-host/` | 60 个宿主测试的 MinGW 构建树与测试 exe；本卡审计导出 diff 也临时落在此处 | 仅宿主测试，不参与固件/烧录 | 否 | 同上 |
| `managed_components/` | `.gitignore:13 managed_components/` | 组件管理器下载的托管依赖（2 个，见下节） | 参与固件编译（tinyusb），来源由 lock 锁定 | 否（策略明确只提交 dependencies.lock） | `check-ignore -v`；无嵌套 `.git` |
| `.cache/` | `.gitignore:22 .cache/` | clangd 索引缓存（编辑器/语言服务器） | 不参与 | 否 | 目录仅含 clangd/index |
| `.trae/` | `.git/info/exclude:8 .trae/`（本地排除，非版本化规则） | 本机 4 个 cy 技能目录（含本审计使用的板级技能） | 不参与构建（纯文档/脚本） | 否（仅本机 exclude；除非显式强制添加） | check-ignore；技能文件为 Markdown/Python/图片 |
| `.eim/` | `.git/info/exclude:7 .eim/` | 本机 EIM（ESP-IDF 安装管理器）环境登记 JSON，含本机路径 | 不参与（构建时另行激活环境） | 否 | 单文件 eim_idf.json |
| `eim_build.log` | `.gitignore:26 *.log` | EIM 安装/构建日志（2026-08-27，课程前环境搭建），内容含本机路径 | 不参与 | 否 | 头部时间戳与 EIM 字样 |
| `eim_install.log` | 同上 | EIM 安装日志（23 KB，含本机安装设置） | 不参与 | 否 | 同上 |
| `eim_list.log` | 同上 | EIM 版本列表日志（161 B） | 不参与 | 否 | 同上 |

预览/缓存类（`.cache`、`.trae`、`.eim`、三个 `*.log`）均为开发机工具产物：不被 CMake/IDF 构建系统引用，不生成进固件镜像，且被忽略规则覆盖，故不影响构建、烧录或提交。`.trae/`、`.eim/` 由本地 `.git/info/exclude` 排除（该文件本身不版本化，换机不携带），其余条目由版本化的 `.gitignore` 覆盖。

## 托管依赖核对（managed_components ↔ dependencies.lock）

- 实际组件目录 2 个，各自 `.component_hash` 与受版本控制的 `dependencies.lock` 逐项一致：
  - `espressif__esp_tinyusb`：`6a50305b…71bf`＝lock `espressif/esp_tinyusb`（版本 1.7.6~2）。
  - `espressif__tinyusb`：`a72b7d67…4752`＝lock `espressif/tinyusb`（版本 0.21.0~1）。
- lock 的构建目标 **`target: esp32s3`**，IDF 依赖 **`idf: 5.5.5`**（main/idf_component.yml 声明 `idf: "==5.5.5"`、`espressif/esp_tinyusb: "^1.1"`）；tinyusb 声明目标含 esp32s3。与当前构建目标/IDF 版本一致。
- `dependencies.lock` 受版本控制且相对 743368a **零改动**；组件目录无嵌套 Git 仓库。结论：PASS，无缺失/漂移。

## 有效 sdkconfig 链路（不把"源码根没有 sdkconfig"误写成"无有效配置"）

- 源码根 `sdkconfig` 与 `sdkconfig.old`：**物理不存在**（.gitignore 第 15-16 行列有两者，但无文件可忽略）。
- 实际生效配置：`build/sdkconfig`（93,839 B，头部明示 "Automatically generated … ESP-IDF 5.5.5 Project Configuration"）；`build/project_description.json` 的 `config_file` 指向 build/sdkconfig，`config_defaults` 指向受版本控制的 `sdkconfig.defaults`——有效配置由 defaults 派生。
- 关键生效项：`CONFIG_IDF_TARGET="esp32s3"`、chip id 0x0009、Flash **16MB**（`CONFIG_ESPTOOLPY_FLASHSIZE_16MB`）、`CONFIG_PARTITION_TABLE_CUSTOM=y`、自定义文件名 `partitions.csv`、偏移 0x8000、MD5 开启；板型由根 CMakeLists 缓存变量锁定 `EASY_INPUT_BOARD=v2`（非 v2 直接 FATAL_ERROR，唯一发布目标），build/CMakeCache.txt 实测为 v2。
- `sdkconfig.defaults` 与 `partitions.csv` 受版本控制且相对基线**零改动**。partitions.csv：nvs 0x9000/0x6000、phy 0xf000/0x1000、factory 0x10000/**0x300000**、sound_a 0x310000/0x90000、sound_b 0x3A0000/0x90000。

## 八项复核矩阵

| # | 项目 | 结论 | 命令/文件/diff 证据 |
|---|---|---|---|
| ① | GPIO、board_pins、BOOT、GPIO8 共享电源未被本功能改变 | **PASS** | `board_pins.h` 不在 28 个变化文件中（基线→HEAD 零 diff）；v2 分支实测 S1–S8=2,47,38,41,1,6,7,48、编码器 17/16/18、WS2812 GPIO12×5、MIC 9/10/11、SPK 14/13/15、KEY_WAKE 21、`kBoot0Pin=0`、`kPeripheralPowerEnablePin=8` 高有效，与板级技能合同逐项一致；板级只读扫描 1 PASS+2 WARN（WARN 为扫描器看不到编译分支，按技能说明不构成 FAIL，v2 分支由 CMakeCache 与 CMake FATAL_ERROR 闸门佐证） |
| ② | 分区表/Flash 容量/有效 sdkconfig/目标/板型仍为 v2·esp32s3 合同 | **PASS** | partitions.csv、sdkconfig.defaults 零 diff；build/sdkconfig 派生链、16MB、custom partitions、esp32s3、EASY_INPUT_BOARD=v2 见上节 |
| ③ | USB/BLE VID/PID、制造商、产品名、序列号、设备名、HID 描述符、GATT 标识与地址策略未改 | **PASS** | 身份常量所在文件仅 usb_hid.cpp/ble_hid.cpp 被改，逐行 diff 中新增行只有 include、static_assert 与 1 个 HostAction case；对生产 diff 新增行做 VID/PID/manufacturer/product/serial/descriptor/GATT/UUID16/adv/name/addr/GPIO 关键字扫描无身份改动；当前值仍为 VID 0x303A、PID 0x1006、bcdDevice 0x010A、USB 序列号 "easy-input-v2"、BLE 设备名 "EasyInput AI"、标准 HID 服务 0x1812；描述符数组（usb_hid.cpp:72 起）不在 diff hunk |
| ④ | Host Action 符合冻结合同 | **PASS** | Report ID 0x11、状态 kind 0x04、Action kind 0x05、chunk 0/1（单 chunk）、data len 36、线上 [4..39] 仅无前缀 UUID、[40..62] 零；共享编码单一来源，两侧 static_assert 锁值；路由测试断言 BLE 连/不连均 UsbFirst，失败不跨通道补发（dispatch 无 HostAction 发送分支）；fail-closed（非 canonical 拒绝且不部分填充） |
| ⑤ | 不保存应用路径/Bundle ID/应用名/图标/启动参数；示例 UUID 仅在宿主测试；默认 Keymap/生产配置/生产源码无固定应用映射 | **PASS** | `git grep` 两个示例 UUID 在 components/ 与 main/ **零命中**；keymap.cpp 中 HostAction 仅出现在 event_for_action switch，无默认绑定；host_action_protocol.{h,cpp} 无 bundle/icon/launch/path/nvs/save 关键字（仅注释声明不触碰 NVS）；固件只保存配置中由用户下发的 UUID 动作文本（ opaque 动作标识），不含应用元数据；契约测试 `host_action_example_uuids_never_ship_in_production_sources` 锁定 8 个生产文件 |
| ⑥ | 无新增泄露用户内容/UUID/应用信息/设备唯一标识的日志 | **PASS** | 生产 diff 新增 ESP_LOG 仅 4 行：两条固定 "ACTION … host_action_(usb|ble)"（source 沿用既有固定来源标签）、两条固定 "rejected non-canonical value" 警告——均不打印 event.value/UUID；既有日志守卫测试 `runtime_logs_do_not_emit_user_or_device_identifiers` 未被削弱且 T09 全量 60/60 通过 |
| ⑦ | 复制/粘贴/快捷键/固定文字/音频/电源等无不可由 Host Action 与 512 状态兼容解释的改动 | **PASS** | 28 文件清单中无音频、电源、剪贴板、HID 键码实现文件；共享层唯一邻近改动 config_status.* 三处均为能力声明（512 预算在 T08/T09 已实测解释）；第02步 ogg 读取修复单列为本机可移植性改动（测试文件内，不触生产代码） |
| ⑧ | ignored 全分类、依赖与 lock 一致、二进制/本机路径未混入提交范围 | **FAIL** | ignored 九项全部分类（上表）、依赖 hash/目标/版本一致 PASS、二进制产物均在忽略目录 PASS；但受版本控制的 flow 文档中有 **3 处本机绝对路径**（违反 AGENTS.md "不提交本机绝对路径"）：T03 任务卡第 40 行（仓库外备用工具链目录）、T09 任务卡第 65 行（系统 Python 目录）、进展日志第 60 行（同一备用工具链目录）。本卡不复制路径原文；这些路径是历史节点（第02步/T09）写入的公开流程文档，本节点为只读审计，未擅自修改 |

## 其他检查

- `git diff --check 743368a HEAD` exit 0；工作区 `git diff --check` exit 0（无空白错误/冲突标记）。
- 课程起点相对课程公开基线多 1 个 `ble_persistence_policy_tests`（起点即存在，743368a 之前随基线带入），已在 T09 卡说明，本审计不重复计为课程改动。
- 仓库内不存在源码根 `sdkconfig`/`sdkconfig.old`（不存在已记录，不存在"有效配置缺失"）；无 `output/`、`tmp/` 等其他被忽略目录实际出现。
- 仍 UNKNOWN：无。ignored 条目用途均有可验证依据；板级扫描的 2 个 WARN 性质由技能文档明确（启发式看不到编译分支），且有人工对照与构建缓存证据补足，不标 UNKNOWN。

## 结论

**SAFE_TO_REQUEST_FLASH = NO。**

- 固件、硬件与协议面：①–⑦ 全部 PASS，依赖/构建配置/产物链证据闭合，未发现越界或无法解释的代码差异；T09 已证 60/60 宿主逻辑与 v5.5.5/esp32s3 默认构建通过（该结论仍只代表逻辑与可生成固件）。
- 唯一阻塞项（⑧，文档卫生，非固件风险）：受版本控制 flow 文档的 3 处本机绝对路径（T03 卡:40、T09 卡:65、进展.md:60，行号以审计时工作区为准）。按本节点规则不得在只读审计中擅自删改；处置需由使用者决定——在后续被授权的节点脱敏（改为工具链占位名/通用环境说明）或书面接受其公开，之后复跑本审计第 ⑧ 项即可重新评估。
- 即使结论翻为 YES，烧录仍需单独确认目标设备与授权；烧录、启动、真机 USB/BLE 发送、GATT 512 发布与 App 识别在本节点均未验证。
