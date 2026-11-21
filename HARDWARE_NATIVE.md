# TZ130TM 原生 MAVLink 云台适配固件

当前实机工作树使用本固件。旧的厂家命令透传版本见 HARDWARE_BENCH.md，已验证的 tz130tm-bench.hex 保留在原构建目录中。本版文件名为 tz130tm-native.hex。

## 控制链路与接线

Mission Planner 点选“在此放置相机” → 飞控计算目标角度 → TELEM2 发 COMMAND_LONG / DO_MOUNT_CONTROL（205） → STM32 USART1 解析 → USART2 发厂家数引 0x0072 → 吊舱。吊舱反馈由 USART2 接收解析，连同控制目标从 USART3 输出到电脑。

| 接口 | STM32 引脚 | 对端接线与用途 |
| --- | --- | --- |
| USART1 | PA9 TX、PA10 RX | PA9 → TELEM2 RX；PA10 ← TELEM2 TX，双向 MAVLink |
| USART2 | PA2 TX、PA3 RX | PA2 → 吊舱 COM RX；PA3 ← 吊舱 COM TX |
| USART3 | PB10 TX | PB10 → USB-TTL RX，电脑接收调试文本 |
| GND | 共地 | STM32、飞控、吊舱、USB-TTL 共地 |

全部串口为 115200、8N1、无硬件流控。PB11 虽已由 CubeMX 配为 USART3 RX，本版没有使用接收功能，不需要连接。USB-TTL 从原先的 TELEM2 抓包用途改接 PB10。Mission Planner 继续通过飞控 USB 连接。

### 厂家 UI 接管与测距

控制权通过现有 TELEM2 更改，无需增加接线。STM32 接收 Mission Planner（255/190）发给云台组件（1/154）的 `MAV_CMD_USER_1`。参数 1 为 1 时暂停数引转发，为 0 时恢复。当前自动交接固件复位后默认暂停。心跳和吊舱状态在两种模式下均正常处理。

1. 如需做手动对照，在 Mission Planner 中运行 `tools/mission_planner/gimbal_control_owner.py` 并选择 No。等待 `[MODE] owner=flight`，再用 Mission Planner 地图点选。
2. 画面到位后，在已连接飞控的 Mission Planner 中运行 `tools/mission_planner/gimbal_control_owner.py` 并选择 Yes。等待 STM32 日志出现 `[MODE] owner=ui` 再操作厂家 UI。飞控仍在发送角度时，`[CTRL]` 应显示 `result=ui_control`。
3. 在厂家 UI 操作跟踪，并启动单次或连续测距。在串口助手观察 `[TARGET]` 中的距离和经纬高。该行是最新状态快照，重复显示同一个值不能确认又测量了一次。
4. 退出厂家 UI 跟踪，确认 Mission Planner 的当前指向，再运行脚本并选择 No。收到 `[MODE] owner=flight` 后恢复飞控指引。

脚本只提交控制权切换请求，STM32 的串口日志用于确认是否生效。切换依赖飞控将精确寻址的消息转发至已发现的云台组件，需在现有飞控台架上验收。

### 地图指向自动交接与测距回传

本工作树增加 Mission Planner 后台插件和飞控只读 Lua 脚本。沿用现有接线。插件安装一次后，继续使用原有「在此放置相机」并确认高度。插件监听原有 ROI 请求，收到飞控成功应答后，发给 STM32 一条 `MAV_CMD_USER_2 (31011)`，参数 1 为任务编号。

1. 将 `GimbalPointingPlugin.dll` 放到 Mission Planner 安装目录的 `plugins` 中并重启。烧录默认 Release 探测固件。STM32 启动后处于暂停转发状态。点两个不同位置，观察 `[POINT] ... event=probe_first_205`。在台架上核对通知确实到达，以及通知后的第一条有效角度确实属于新点。建议抓取飞控至 STM32 的串口数据，核对从 A 到 B 再回 A 的顺序。
2. 用厂家 UI 在稳定跟踪时启动单次或连续测距。STM32 从 `0x0014` 反馈读取坐标和距离，稳定跟踪且样本有效时生成 `SET_POSITION_TARGET_GLOBAL_INT`（ID 86），无需等待命令回显。首次有效样本立即处理，后续仅在距离或经纬高有变化时处理，最多每秒一次。探测版输出 `[RANGE] event=probe_86`。台架上需核对厂家坐标为 WGS84、目标高度为海拔。
3. 台架确认后，按需启用两个编译开关。可以各自设为 `ON`，分开验证指向和回传。

   ```powershell
   cmake --preset Release -B build/Release-enabled -DENABLE_POINTING_AUTO_RESUME=ON -DENABLE_RANGE_REPORT=ON
   cmake --build build/Release-enabled
   ```

   启用后，STM32 按任务编号开始一轮数引。新鲜 `FRAME` 两轴误差均小于 1°、目标稳定并持续 0.5 秒后，输出 `[POINT] ... event=arrived`，保持停发直到下一新编号。2 秒等不到角度，或 10 秒仍未到位，会保持停发并分别记录 `no_angle` 或 `not_settled`。用厂家 UI 跟踪并测距。`[RANGE] event=sent_86` 表示位置设定值已写入 USART1。
4. 若需要在飞控日志里确认收包，将 `tools/ardupilot/record_gimbal_measurement.lua` 放入 SD 卡的 `APM/scripts`，开启 `SCR_ENABLE` 后重启。脚本校验并记录 ID 86，飞控日志条目为 `T086`。Lat/Lon 为度 × 10⁷，Alt 为米。台架如需查看未解锁日志，需启用未解锁时的日志记录。若已运行其他初始化 MAVLink 接收队列的脚本，应检查是否需要合并订阅。

ID 86 寻址飞控 `1/1`，使用 `MAV_FRAME_GLOBAL=0` 和位置掩码 `0x0DF8`。经纬度为度 × 10⁷，厂家反馈高度按海拔米数填入。飞控在支持位置设定值的模式（如 Guided）下可能执行该指令，先在未解锁台架或 SITL 验证。

默认 Release 的两个自动功能开关均为 OFF，用于先确认 TELEM2 实际时序和厂家单测反馈。自动指向只适用于已验证通知和角度先后关系的系统配置。当前 205 角度帧没有任务编号。心跳、手动控制权脚本和 `[TARGET]` 日志可用于台架。

### 在 USART3 核对 USART1 发包

串口助手以 115200、8N1 和文本模式打开 USART3。稳定跟踪且有有效坐标与距离时，`[UART1_TX] msgid=86 len=64 ... hex=` 后显示完整 MAVLink2 字节，包含帧头和 CRC。对照 `[RANGE]` 中的经纬高，测距距离也保留在该行。探测版显示 `result=probe`，仅预览封包。启用回传后，`result=sent` 表示 STM32 已完成 USART1 发送，`result=uart1_error` 表示发送失败。核对 STM32 封包可直接看 USART3。确认飞控收到时再查看飞控日志。

USART3 同时显示心跳（ID 0）和 ACK（ID 77）。ACK 日志每 200 ms 最多显示一条，取最近一帧。

## 固件行为

- 启动后立即发云台 HEARTBEAT，随后每 1000 ms 发送一次。sysid=1、compid=154、type=GIMBAL(26)、autopilot=INVALID(8)。心跳和 ACK 共用递增发送序号。心跳表示适配板在线，不代表吊舱已经到位。
- 飞控数引输入为 source=1/1、target=1/154 或广播地址的 COMMAND_LONG（消息76），command=205、param7=2。任务通知和手动切换来自地面站 255/190，要求精确寻址 1/154。支持普通未签名 MAVLink2 和 MAVLink1。MAVLink2 末尾零截断由标准库补零，实测32字节载荷可正常解析。CRC 错误、未知消息和其他地址不产生吊舱输出。
- param3 为方位角输入，param1 为俯仰角输入，单位均为度。默认两轴取反映射（2026-11-13方向校准配置）；param2 横滚仅显示，0x0072 没有横滚指令字段。
- 每条有效205立即发送11字节厂家帧：FB 2C AA 06 72 00 [方位INT16小端] [俯仰INT16小端] [XOR]。角度乘100四舍五入；XOR覆盖 LEN 到最后一个数据字节，共7字节。
- 仅检查浮点数有限性和厂家编码范围：方位 -179～179°，俯仰 -120～90°。超出范围拒绝整条指令，不截断或限幅成其他方向。
- 发回标准 COMMAND_ACK：0=UART2发送成功，1=厂家 UI 接管期间暂不执行，2=角度不可编码，3=param7模式不支持，4=UART2发送失败。ACK不表示吊舱已执行完毕。
- USART3 输出控制目标 CTRL，以及原有 SYS、FRAME、ATT、TARGET 四类反馈。控制日志和状态日志分别最多5Hz，显示最新数据；控制命令发送不受日志周期调度。
- 地图指向使用飞控计算的目标角度，测距仍由厂家 UI 控制。USART1 接收厂家原始命令时不会透传到吊舱。

## 方向调整

修改 stm32-mavlink/App/common/config.h 后重新编译：

```c
#define GIMBAL_YAW_SIGN    -1
#define GIMBAL_PITCH_SIGN  -1
```

方向系数只允许 +1 或 -1，需要翻转某一轴时在两者之间切换。它们直接作用于线上 param3 和 param1，没有额外隐藏的取反、零偏或姿态补偿。ArduPilot 4.6.3 的 SToRM32 后端已经对内部俯仰和机体相对方位取反后发送；日志保留收到的原始角度，当前按用户要求将两轴厂家输出同时取反，后续依据台架实测确认映射。该设置尚未证明两端参考系在机体倾斜时一致，不应把一次静态指向成功当成机动飞行指向验证。

## 编译与烧录

在此工作树的 stm32-mavlink 目录执行：

```powershell
cmake --preset Release
cmake --build --preset Release
```

输出 build/Release/tz130tm-native.hex，使用 ST-Link Utility 或 CubeProgrammer 按原方式烧录。BIN 起始地址为 0x08000000。将 Release 换成 Debug 可生成调试版本。原有 bench HEX 不被新命名覆盖。

## 实机操作

1. 按上表接线，电脑串口助手选择 USB-TTL 的实际 COM 口，设置115200 8N1，接收用文本显示；停止之前模拟云台心跳的定时发送。
2. 烧录本版后复位 STM32，PB10 应输出 [BOOT] TZ130TM NATIVE，包含两轴当前方向系数。正常吊舱反馈将持续显示 SYS、FRAME、ATT、TARGET。
3. 先让 STM32 工作，再重启飞控。保留已确认的 MNT1_TYPE=4、SERIAL2_PROTOCOL=2、SERIAL2_BAUD=115；本次固件实现没有修改飞控参数。ArduPilot 4.6.3 的 SToRM32 后端仅在启动前60秒发现云台，错过时需重启飞控。
4. 在 Mission Planner 中查看是否出现 Mount: SToRM32。先用静态台架、小角度目标验证，再点选“在此放置相机”。有205到达时出现 CTRL；若只有吊舱状态而没有CTRL，先确认飞控发现云台及TELEM2输出。
5. CTRL 中 result=sent 表示厂家控制帧已成功写入 USART2。结合实际运动、FRAME 三轴框架角及 SYS 伺服状态判断动作；cmd_echo 单独不能证明动作成功。方向相反时调整配置并重编译。

控制日志示例（仅示例，不是本版实机记录）：

```text
[CTRL] p1_pitch_deg=0.188 p2_roll_deg=0.000 p3_yaw_deg=70.355 vendor_yaw_deg=-70.36 vendor_pitch_deg=-0.19 result=sent ack=sent
```

result=rejected 表示角度无效或超出范围；mode_unsupported 表示 param7 不是2；uart2_error 表示控制帧发送失败；ack=uart1_error 表示返回飞控的 ACK 发送失败。TARGET 经纬高仍是吊舱计算的目标坐标，不是飞机自身位置。

## 软件验证与证据范围

2026-11-13：Debug、Release 构建通过；Release FLASH 14664字节，静态RAM 3528字节，不含运行栈峰值。主机测试使用 -Wall -Wextra -Werror：基础环形缓冲、厂家数引编码、吊舱状态字段、状态文本输出，以及原生MAVLink转换均通过。

原生转换测试覆盖心跳及ACK序号、1Hz周期和tick回绕、32字节截断载荷、逐字节半包、连续帧、MAVLink1、错误CRC、未知消息、地址和命令过滤、广播、签名帧隔离、模式/有限数/编码范围、UART发送失败、日志合并与失败重试。方位和俯仰的四种正负系数组合均通过。

用户提供的 RX 抓包3153字节作为 tests/data/telem2_mount_rx.hex，含70条完整205、2条飞控心跳和末尾31字节不完整帧；回放产生70条厂家控制帧及70条ACK，不完整尾帧不执行。此项是软件回放，不是本版固件的真实串口或指向效果验证。

主机测试命令：

```powershell
python tools/run_host_tests.py
```

需本机 GCC / MinGW，无额外 Python 依赖。工具只生成并运行本机测试程序，不打开COM口。

用户于2026-11-13报告当前双轴取反版本已实机正常跑通，地图点选可驱动吊舱；该记录为用户实机反馈。待进一步验证：零位、参考系、量化指向误差及机动飞行效果。未增加通信统计、链路超时或指令发送调度。

依据：[ArduPilot Copter-4.6.3 SToRM32后端](https://github.com/ArduPilot/ardupilot/blob/Copter-4.6.3/libraries/AP_Mount/AP_Mount_SToRM32.cpp)、[MAVLink COMMAND_LONG](https://mavlink.io/en/messages/common.html#COMMAND_LONG)、厂家《T系列吊舱通讯协议V1.4-20250401》0x0072数引命令表。

2026-11-13方向调整：用户反馈原同号版本方位反向，且地面测试输入较高目标Alt时镜头向下；按用户要求将yaw/pitch默认系数均改为-1。随后用户报告当前版本已正常跑通；量化指向精度仍待验证。

双轴取反版重新验证：Debug FLASH 24688字节，Release FLASH 14668字节，静态RAM均3528字节；未通过命令行覆盖方向系数的默认配置协议测试及70帧回放通过。旧+1/+1镜像另存为tz130tm-native-yaw1-pitch1.hex/bin。
