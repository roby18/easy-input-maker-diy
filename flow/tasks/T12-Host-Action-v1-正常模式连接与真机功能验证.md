# T12 · Host Action v1：正常模式连接、App 入口预检、配置同步确认与真机功能验证

- 日期：2026-09-15；分支 `my-host-action-practice-v2`。
- 边界：本节点**未再次烧录、未复位、未运行 esptool/identify-device、未修改固件/配置源码**；只做只读枚举、只读进程/元数据核对、静态源码取证与真机人工观察。设备 MAC 在本记录仅保留尾号 **501C**，不保存完整序列号/USB 拓扑。

## 1. 前置门（读 T11 写入会话）

- DEVICE_CONFIRMED=YES、FLASH_WRITTEN=YES、NORMAL_POWER_CYCLE_CONFIRMED=YES（T11 卡第 8 节）→ 三门满足，继续。
- 起始 POST_FLASH_READY=YES（T11 一次 OS 枚举见 0x303A/0x1006 HID 三集合）；本节点 fresh 枚举复核仍为 PID1006 HID（键盘/鼠标/厂商定义三集合）、串口 0（Maker 正常应用只有 HID 无 CDC，PORT_COUNT=0 中立）。

## 2. 正常模式连接预检

- USB HID：操作系统枚举到预期 VID 0x303A/PID 0x1006 的 3 个 HID 集合，无 PID1001 下载节点。
- App：底部状态栏「USB 连接 · 键盘输入已连接 · 设置可用」→ App 已识别设备，设置通道就绪（与「键盘输入已连接」是两个独立标识，后者不替代前者）。
- BLE：后续矩阵中实测可发现并连接 EasyInput AI（见项 2）。
- 任一路已有正常模式连接证据 → POST_FLASH_READY 维持 **YES**。

## 3. EasyInput App 只读预检（版本/单实例/非开发）

- 安装包元数据：注册表卸载项 DisplayName=EasyInput、DisplayVersion=**0.1.43**，InstallLocation=`D:\Program Files\EasyInput`。
- 进程：仅 1 个 EasyInput 主进程（该目录下 EasyInput.exe）；6 个 WebView2 子进程全部挂同一用户数据目录 `com.chenyuecy.easy-input`，属同一 Tauri 应用，非多实例。
- exe 元数据：ProductName=EasyInput，ProductVersion=FileVersion=0.1.43；路径不在 target/debug 等开发目录；桌面/下载/D:\AI 常见根未发现开发副本；无 0.1.25/旧版安装项。
- 使用者在设置/关于页肉眼确认用户可见版本号 = 0.1.43。
- 结论：版本、单实例、非开发三项成立，无冲突需要退出/更新。

## 4. App 入口与设置通道（使用者肉眼确认）

- 键盘页实体键「按下动作」列表的自定义分组下存在「**打开应用**」入口（与快捷键、固定文字并列）。
- 设置通道显示「设置可用」，不是「尚未连接」。
- 初始映射：KEY8→打开应用→豆包，配置空间 1210/2048 字节。

## 5. 同步前静态源码取证（只读，Report ID 0x11 三 kind 不得互换）

| 核对项 | 结论 | 证据 |
|---|---|---|
| 保存成功回 kind 0x03 | PASS | main/app_main.cpp:3086-3094 保存后 `send_hid_config_ack(1,true,json,saved,…)`，phase 硬编码 1；解析失败 3049-3057 回 (1,false,false) |
| phase/ok/bytes/crc16/saved 编码 | PASS | main/platform/usb_hid.cpp:1616-1629 与 ble_hid.cpp:3562-3575 一致：7 字节 `[0]phase [1]ok [2..3]bytes LE [4..5]crc16 LE [6]saved`，单 chunk(0/1)；bytes=本次接收长度、crc16=`config_json_crc16(接收原始 payload)`（app_main.cpp:2984-2986、3085 注释） |
| 只回属主通道、不双发 | PASS | app_main.cpp:2987-3016：USB 来源→`send_config_ack_for_epoch`（epoch 过期拒绝），BLE→`send_config_ack_for_owner`，Wi-Fi 不回 HID |
| kind 0x04 saved/bytes/crc16 未被破坏 | PASS | components/keyboard/src/status_hid_protocol.cpp:97-111（[9..10]bytes、[11..12]crc16）该文件不在课程 diff；config_status.cpp 仅插入三处 `"host_action_v1":true`，saved 序列化上下文未改 |
| 0x03/0x04/0x05 隔离 | PASS | 常量分立：ConfigAck=0x03（usb:37/ble:61）、StatusResponse=0x04、HostAction=0x05（host_action_protocol.h:24，static_assert 钉死）；课程 diff 在 USB/BLE 仅加 include、static_assert、send_firmware_event，未触碰任何 ack 函数 |

## 6. 配置同步确认（CONFIG_SYNC_CONFIRMED）

- 第 1 轮（KEY8→豆包）：点一次「同步到键盘」后，界面明确显示「**设置已同步到键盘。USB连接**」，配置空间 1210/2048——属键盘回 kind 0x03（phase=1/ok=true/saved=true，bytes+crc16 与发送匹配）后的确认态，非「已发送」中间态。
- 第 2 轮（八键重配，见下）：同步后 App 同样显示「设置已同步到键盘」确认文字。
- 未出现「正在确认/等待键盘确认/尚未确认」；未发生重连重试，未连续点击。
- **CONFIG_SYNC_CONFIRMED = YES**。

## 7. 真机功能矩阵（七项，逐项由使用者肉眼报告）

最终八键配置：K1 语音输入、K2 语音编辑、K3 固定文字、K4 打开应用→豆包浏览器、K5 全选、K6 复制、K7 粘贴、K8 回车；另在一个键上定义「快捷键」=Ctrl+A。

| # | 项目 | 通道/做法 | 使用者观察 | 结论 |
|---|---|---|---|---|
| 1 | USB-only 触发目标动作 | 仅 USB，KEY8→豆包（首轮配置）短按 | 豆包前台拉起、单实例、无报错 | PASS |
| 2 | BLE-only 触发目标动作 | 拔 USB、App BLE 连接 EasyInput AI 后短按 | 蓝牙连接·键盘输入已连接；豆包拉起、单实例、无报错；拔线后配置仍生效（持久化旁证） | PASS |
| 3 | USB/BLE 双连接按一次只执行一次 | BLE 保持在线并插回 USB，双连接下短按 | 两通道均在；恰好 1 个豆包窗口，无报错 | PASS |
| 4 | 一次按下只触发一次 | 断开 BLE 回 USB-only，短按后观察 10s | 仅 1 个窗口，无延迟二次弹出 | PASS |
| 5 | 松开不重复 | USB-only，按住 3s 再松开，继续观察 5s | 按住期间 1 个窗口不增加，松开零新增 | PASS |
| 6 | 复制/粘贴/快捷键/固定文字不回归 | 记事本实测 | K3 固定文字上屏、K5 全选、K6 复制、K7 粘贴、K8 回车正常；快捷键 Ctrl+A 正常全选 | PASS |
| 7 | App 实际打开所选本机目标应用 | 最终配置 K4→豆包浏览器，短按 | 豆包浏览器被拉到前台、单窗口、无报错 | PASS |

- 映射由 EasyInput App 在本机建立（UUID→本机应用），同步到键盘的是含真实 UUID 的按键配置；未手写/编造/复用宿主测试 UUID。
- 语音输入在 BLE-only 下由使用者顺带试用，正常。

## 8. 观察项（不计入 Host Action 成败）

- 使用者报告：BLE/语音使用时**指示灯未亮**。静态核对课程 28 个变化文件中 LED/WS2812/GPIO8 电源相关文件 **0 个**，main/platform/led_strip_status.cpp、components/keyboard/src/boot_led_sequence.cpp 相对基线零差异 → 非本功能引入；属基线既有指示灯行为/期望问题，留待独立节点核实（本节点不改代码、不下结论）。

## 9. 最终状态

| 状态 | 值 |
|---|---|
| DEVICE_CONFIRMED | YES（承 T11） |
| FLASH_WRITTEN | YES（承 T11） |
| NORMAL_POWER_CYCLE_CONFIRMED | YES（承 T11） |
| POST_FLASH_READY | **YES**（USB HID/App/BLE 正常模式连接证据齐备） |
| CONFIG_SYNC_CONFIRMED | **YES**（两轮同步均见键盘确认） |
| FUNCTION_VERIFIED | **YES**（七项矩阵全部 PASS，均由使用者肉眼报告） |

- 仍未覆盖/后续：指示灯基线行为（第 8 节）；本节点未做音频内容正确性、网络/词库等非 Host Action 路径的专项验收。
