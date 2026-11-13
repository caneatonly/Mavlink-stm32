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

全部串口为 115200、8N1、无硬件流控。PB11 虽已由 CubeMX 配为 USART3 RX，本版没有使用接收功能，不需要连接。USB-TTL 从原先的 TELEM2 抓包用途改接 PB10；Mission Planner 继续通过飞控 USB 连接。

## 固件行为

- 启动后立即发云台 HEARTBEAT，随后每 1000 ms 发送一次。sysid=1、compid=154、type=GIMBAL(26)、autopilot=INVALID(8)。心跳和 ACK 共用递增发送序号。心跳表示适配板在线，不代表吊舱已经到位。
- 接收 source=1/1、target=1/154 或广播地址的 COMMAND_LONG（消息76），仅处理 command=205、param7=2。支持普通未签名 MAVLink2 和 MAVLink1；MAVLink2 末尾零截断由标准库补零，实测32字节载荷可正常解析。CRC 错误、未知消息、其他地址和其他命令不产生吊舱输出。未配置签名密钥，本版不执行带签名或扩展头标志的控制帧。
- param3 为方位角输入，param1 为俯仰角输入，单位均为度。默认两轴取反映射（2026-11-13方向校准配置）；param2 横滚仅显示，0x0072 没有横滚指令字段。
- 每条有效205立即发送11字节厂家帧：FB 2C AA 06 72 00 [方位INT16小端] [俯仰INT16小端] [XOR]。角度乘100四舍五入；XOR覆盖 LEN 到最后一个数据字节，共7字节。
- 仅检查浮点数有限性和厂家编码范围：方位 -179～179°，俯仰 -120～90°。超出范围拒绝整条指令，不截断或限幅成其他方向。
- 发回标准 COMMAND_ACK：0=UART2发送成功，2=角度不可编码，3=param7模式不支持，4=UART2发送失败。ACK不表示吊舱已执行完毕。
- USART3 输出控制目标 CTRL，以及原有 SYS、FRAME、ATT、TARGET 四类反馈。控制日志和状态日志分别最多5Hz，显示最新数据；控制命令发送不受日志周期调度。
- 本版移除旧 ROI 经纬高转换、ID86目标位置上报和自动激光控制。USART1 接收厂家原始命令时不会透传到吊舱。

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
