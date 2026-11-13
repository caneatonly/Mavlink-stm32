> 历史版本说明：本文对应已验证的厂家命令透传固件 tz130tm-bench.hex。当前原生MAVLink适配固件请使用 [HARDWARE_NATIVE.md](HARDWARE_NATIVE.md)，对应 tz130tm-native.hex。

# TZ130TM 厂家命令透传测试固件

分支：`bench/tz130tm-hardware`，基于 `master` 的 `bec76cb`。请在本独立工作目录打开工程；原目录仍是 `test` 分支。

## 功能

- UART1接收电脑发送的原始二进制字节，经UART2原样发给吊舱，不检查或修改命令、参数、长度及校验。
- UART2解析 `FC 2C 55 3E 14 00` 的完整67字节反馈，XOR正确后提取状态。
- UART1以ASCII文本输出：准备状态、自检字节、伺服状态、三轴框架角、三轴姿态角、命令反馈、姿态指示完成反馈、跟踪状态、激光距离、目标经纬高。
- 有新有效反馈时最多每200ms输出一组。无新反馈时不重复输出旧数据；未实现链路超时判断。
- 本轮构建不包含MAVLink、自动激光启停和ID86回传；无通信统计、自定义发送调度或目标参数检查。厂家命令由操作者提供。

## 接线

两路均为115200、8数据位、无校验、1停止位。吊舱快拆选择TTL模式。

| 来源 | 连接位置 |
|---|---|
| USB-TTL TXD | STM32 PA10 / UART1 RX |
| USB-TTL RXD | STM32 PA9 / UART1 TX |
| USB-TTL GND | STM32 GND |
| STM32 PA2 / UART2 TX | 吊舱COM 2脚 TTL_RX1 |
| STM32 PA3 / UART2 RX | 吊舱COM 1脚 TTL_TX1 |
| STM32 GND | 吊舱COM 3脚信号地 |

按实际插头编号确认接线方向和逻辑电平。吊舱独立供电，USB-TTL的VCC不接吊舱。GPS+SBUS上的飞控位姿输入保留原接线。

## 编译与烧录

在本目录的 `stm32-mavlink` 下执行：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

输出位于 `stm32-mavlink/build/Debug/`：

- `tz130tm-bench.hex`：推荐用STM32CubeProgrammer经ST-Link/SWD下载，HEX包含地址。
- `tz130tm-bench.bin`：下载地址为 `0x08000000`。
- `stm32-mavlink.elf`：供调试使用。

也可将两条命令中的Debug改成Release；该构建同样生成HEX和BIN。本轮无需MAVLink头文件依赖。

## 串口助手操作

1. 连接USB-TTL对应COM口，设置115200 8N1。
2. **勾选十六进制发送，取消十六进制显示（接收采用文本显示）**。
3. 关闭发送脚本、自动追加换行和定时发送；命令自带校验，不要再次追加ADD8等校验。
4. 复位STM32，应看到 `[BOOT] TZ130TM RAW BENCH ...`。
5. 收到有效吊舱反馈后，看到下面四类文本。先人工单次发送已验证命令，观察机械动作及框架角变化。

```text
[SYS] ready=1 selftest=0xFF servo=0x0C
[FRAME] yaw_deg=0.08 pitch_deg=1.23 roll_deg=0.35
[ATT] yaw_deg=-12.26 pitch_deg=0.00 roll_deg=0.00 cmd_echo=0x00 attitude_feedback=0x00
[TARGET] tracking=0x00 distance_m=0.0 lon_deg=0.0000000 lat_deg=0.0000000 alt_m=0
```

以上数值是已提供反馈样本的解码示例，不代表新固件已通过实机验证。FRAME为框架角，ATT为协议中的吊舱姿态角；两组角度不能混用。TARGET为目标定位结果，不是飞机自身经纬高。

方位+5°、俯仰+5°：

```text
FB 2C AA 06 72 00 F4 01 F4 01 74
```

归中：

```text
FB 2C AA 06 71 00 00 00 00 00 77
```

之后可同样发送厂家地理指引/地理跟踪命令。固件对所有输入原样透传，因此控制范围由发送的命令决定。命令反馈和姿态完成字段保留原始十六进制值；不要仅凭其数值判断动作成功。

本固件面向低频、人工单次命令测试，不作为持续满速串口压测或正式飞控部署固件。

## 本次软件验证结果

2026-11-07：四组主机检查通过（基础模块、厂家协议、选定状态字段、台架桥接5项用例）。覆盖原始字节透传、真实捕获帧、分包、坏校验、短帧、负数及极值，主机编译使用 `-Wall -Wextra -Werror`。

Debug和Release均构建成功；Release镜像9216字节，静态RAM占用3064字节（不含运行时栈峰值）。已核验HEX校验、HEX/BIN一致性及向量表地址；CubeMX生成区未改动。构建时尚未烧录，随后实机进度见下文。

## 实机进度与待办

- [x] 用户已烧录运行：收到SYS、FRAME、ATT、TARGET格式化反馈；归中和方位/俯仰+5度命令均成功控制吊舱。当前台架已验证PC→STM32 UART1→UART2→吊舱，以及吊舱反馈经STM32解析返回PC的路径。
- [x] 用户用USB-TTL监听X7 Pro飞控TELEM2，收到9帧连续MAVLink2 HEARTBEAT（sysid=1、compid=1、序号232～240）；逐帧CRC均正确。此项只证明飞控TELEM2→电脑接收，不等于飞控→STM32已经验证。
- [ ] 室外GPS有效后验证飞控自身位姿确实进入吊舱，再分别测试0x001B地理指引、0x001C地理跟踪和实际指向。当前室内无GPS，暂缓。
- [ ] 确认飞控完整固件版本及地面站版本，抓取地面站设置ROI时TELEM2实际输出的消息ID、command、target_system、target_component、frame及目标经纬高。
- [ ] 确定ROI输入契约和路由方案后再设计下一轮STM32 MAVLink接收测试固件。当前原始厂家命令透传固件不解析MAVLink，不应用于自动转换飞控遥测。

接口核对：ArduPilot的CUAV X7系列文档将TELEM2映射为SERIAL2。Mission Planner当前master中“Point Camera Here”调用COMMAND_INT/DO_SET_ROI（201），使用相对Home高度；不能预设它就是DO_SET_ROI_LOCATION（195）或海拔高度。该结论仍需与用户安装版本及现场抓包对照。

参考：[X7串口映射](https://ardupilot.org/copter/docs/common-cuav-x7-family-overview.html)、[Mission Planner操作](https://ardupilot.org/copter/docs/common-mount-targeting.html)、[Mission Planner源码](https://github.com/ArduPilot/MissionPlanner/blob/master/GCSViews/FlightData.cs)、[ArduPilot路由](https://ardupilot.org/dev/docs/mavlink-routing-in-ardupilot.html)。
