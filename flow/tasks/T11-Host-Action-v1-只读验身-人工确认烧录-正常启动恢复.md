# T11 · Host Action v1：只读验身 → 人工烧录确认 → 写入固件 → 恢复正常启动

- 日期：2026-09-15；分支 `my-host-action-practice-v2`；开始时 HEAD `5af4162`（第10步补充，移除绝对路径）。
- 依据：T10 烧录前审计处置后结论 **SAFE_TO_REQUEST_FLASH = YES（工作区口径，①–⑧ 全 PASS）**。
- 性质：一次不可逆设备写入；授权前全程只读；未执行 erase_flash，未改偏移、分区、身份或任何工程配置；本节点不跑 Host Action 功能矩阵。
- 隐私：完整 MAC 与完整 USB 序列号/拓扑只在当次对话授权卡中临时出现，本记录仅保留 MAC 尾号 **501C**。

## 1. 审计连续性核对（授权前只读）

- `git status --porcelain`：工作区干净，无未跟踪文件。
- 审计 HEAD `0b69030`→当前 HEAD 新增 2 个提交（`69e153a 第10步`、`5af4162`），路径级比对**仅含 flow/ 5 个文档**（plan、T03、T09、T10、进展）；`components/ main/ host_test/ CMakeLists.txt sdkconfig.defaults partitions.csv dependencies.lock` 在该范围 **零差异**。
- 结论：无新增固件相关源码、测试、构建配置、硬件配置或设备身份变化 → 不需要 REAUDIT。

## 2. 环境、目标与产物核对

- 实际激活环境：ESP-IDF **v5.5.5**（`idf.py --version`；IDF git describe `v5.5.5`），esptool **v4.12.0**；未升级、未 set-target。
- `build/project_description.json`：target **esp32s3**，config = `build/sdkconfig`（由受版本控制的 `sdkconfig.defaults` 派生）；`build/CMakeCache.txt`：**EASY_INPUT_BOARD=v2**。
- 复跑 `idf.py build` → `Project build complete`（4 个目标均为最新），产物与已验收源码匹配 → 不需要 REBUILD。
- 经人确认的写入范围（取自 `build/flasher_args.json`，dio / 16MB / 80m）：

| 偏移 | 镜像（build/ 下） | 大小（字节） |
|---|---|---|
| 0x0 | bootloader/bootloader.bin | 20,832 |
| 0x8000 | partition_table/partition-table.bin | 3,072 |
| 0x10000 | easy_input_keyboard.bin（应用） | 1,640,688 |

nvs（0x9000）、phy_init（0xf000）、sound_a（0x310000）、sound_b（0x3A0000）不在写入列表；不执行整片擦除。

## 3. 板级规则（easyinput-board-cy）

- EasyInput V2.0 / 固件板型 v2 / PCB 丝印 AI Keyboard V2.1 为同一硬件；进入下载模式的唯一正确动作＝**开机状态短按并松开一次 BOOT**；不按住、不配合上电；板上无用户 RESET/EN 键；恢复＝电源开关关机再正常开机，恢复时不按 BOOT。

## 4. 只读验身与人工确认

- 烧前现场：板子正常运行，操作系统仅见 VID_303A&PID_1006 的 HID 设备（3 个集合：键盘/鼠标/厂商定义），**串口数 0**（Maker 正常应用 USB 只有 HID、无 CDC，符合预期）。
- 自动连接只读尝试 1 次：`python -m esptool chip_id` → `Found 0 serial ports`（rc=2），证实尚未进入下载状态，未要求任何按键以外的操作。
- 进入下载模式历史：**manual**——使用者保持开机、短按并松开 BOOT 一次；随后出现唯一候选口 **COM4**（VID_303A&PID_1001 原生 USB-Serial/JTAG 复合设备）。
- 只读验身：`python -m esptool -p COM4 chip_id` → **ESP32-S3 QFN56 rev v0.2**，WiFi/BLE，嵌入式 PSRAM 8MB（AP_3v3），40MHz，USB-Serial/JTAG；MAC 尾号 **501C**（与 USB 复合设备实例号互相印证）。芯片型号与工程 target 一致，单候选无歧义。
- 向使用者发出写入授权卡（端口、芯片、完整 MAC 仅临时显示、工程目标、IDF 版本、完整写入范围、镜像路径/大小、显式端口命令、不擦整片声明）。
- 授权：使用者原样输入确认句「**确认烧录到 501C**」，逐字匹配；其他回复不视为授权。
- 写入前最后比对：重扫仍为单口 COM4；再次 chip_id，芯片与 MAC 尾号 501C 一致 → 续烧。

## 5. 写入固件

- 命令：`idf.py -p COM4 flash`（底层 `esptool --chip esp32s3 -p COM4 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 easy_input_keyboard.bin`）。
- 结果：**退出码 0**；连接时报告 Chip is ESP32-S3、MAC 尾号 501C；三个镜像分别 `Wrote 20832 / 1640688 / 3072 bytes` 且均 **Hash of data verified**；末尾 `Hard resetting via RTS pin`。
- 自动连接一次成功，无需第二次 BOOT 提示；未擦除、未改偏移/分区/身份。
- 写入后端口观察：COM4（PID 1001）仍存在，设备停在 ROM 下载模式。

## 6. 烧录后检查会话（session 未签发）

- 按 esp-idf-cy 尝试 `post-flash-check.sh prepare -p COM4 -m <MAC…501C> --download-entry manual` 共 2 次：
  1. 沙箱内：环境探测访问本机 AppInstaller 诊断日志被宿主沙箱拒绝，只读 identify helper rc=49，prepare rc=4（identity_failed）；
  2. 沙箱外重试：脚本自身在本机非交互 Git Bash 下出现多处 `echo: write error: Bad file descriptor`，仍 rc=4。
- 按 skill「同一脚本失败两次即停止」停止；两次均为本机脚本/环境兼容问题，非设备问题。
- 身份闭合由烧录会话本身保证（连接与写入过程中 esptool 报告同一芯片与 MAC 尾号、三镜像 Hash 校验通过、退出码 0）。
- **记录「session 未签发」**；未为签发 session 而重新烧录、重新复位或再次运行 esptool。

## 7. 人工正常启动恢复与唯一一次只读 USB/HID 枚举

- 使用者按板上电源开关完全关机，不按 BOOT 正常开机（已提示：有电池时只拔 USB 不等于完全断电）；恢复动作由使用者本人完成并回报。
- 恢复后只做一次操作系统级枚举（无 esptool/identify，不保存完整序列号与 USB 拓扑）：
  - 串口数：**0**（正常应用仅 HID，不构成失败或成功的独立证据）；
  - 预期正常模式设备在：**VID_303A&PID_1006**，3 个 HID 集合（键盘 COL01 / 鼠标 COL02 / 厂商定义 COL03）+ 父 USB 节点；
  - **VID_303A&PID_1001 下载模式节点：0**，无 ROM 下载残留证据。
- 观察到预期正常模式 HID 设备且无下载模式证据 → **POST_FLASH_READY = YES**。

## 8. 五项状态（本节点最终交付）

| 状态 | 值 | 依据 |
|---|---|---|
| DEVICE_CONFIRMED | **YES** | COM4 单候选；ESP32-S3 QFN56 v0.2/8MB PSRAM；MAC 尾号 501C 授权前与写入前两次只读一致 |
| FLASH_WRITTEN | **YES** | `idf.py -p COM4 flash` 退出 0；3 镜像写入并 Hash verified；无 erase |
| NORMAL_POWER_CYCLE_CONFIRMED | **YES** | 使用者电源开关完全关机→不按 BOOT 正常开机；枚举 PID1001=0、PID1006 HID 在 |
| POST_FLASH_READY | **YES** | 唯一一次 OS 枚举见预期 0x303A/0x1006 HID 三集合，无下载模式证据 |
| FUNCTION_VERIFIED | **NO** | 未做 Host Action 功能矩阵、未建立 App/USB HID/BLE 正常模式业务连接证据 |

- 写入与恢复完成门（前三项 YES）满足，可进入正常模式与真实功能验证节点；该节点需先建立 EasyInput App、USB HID 或 BLE 的正常模式连接证据。
- 本节点未修改源码、测试、GPIO、BOOT、GPIO8、Flash 分区、HID/GATT、USB/BLE 设备身份或 App 映射。
