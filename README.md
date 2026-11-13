# STM32 MAVLink ↔ TZ130TM 原生云台适配

本实机工作树采用 ArduPilot 原生 SToRM32 MAVLink 控制路线：

Mission Planner 点选目标 → 飞控计算角度 → TELEM2 / STM32 USART1 → 解析 COMMAND_LONG / DO_MOUNT_CONTROL（205）→ USART2 发送厂家数引 0x0072 → 吊舱。

USART3 向电脑输出控制目标和吊舱反馈。接线、参数、方向调整和操作步骤见 **[HARDWARE_NATIVE.md](HARDWARE_NATIVE.md)**。

## 当前实现

- 1Hz 主动云台心跳，身份 sysid=1 / compid=154。
- 标准 MAVLink 流解析、CRC校验、32字节截断载荷补零、地址过滤和命令 ACK。
- 方位/俯仰角转换为厂家11字节数引帧，两轴方向系数可独立配置。
- USART3 最多5Hz显示 CTRL、SYS、FRAME、ATT、TARGET。
- 已移除旧 ROI 经纬高转换、ID86上报及自动激光控制。

## 代码结构

- stm32-mavlink/App/common：配置、环形缓冲、数据结构、定点文本格式。
- stm32-mavlink/App/mavlink_handler：心跳、205接收与角度转换、ACK、控制日志。
- stm32-mavlink/App/vendor_protocol：数引帧编码和吊舱状态解析。
- stm32-mavlink/App/status_reporter：吊舱反馈文本输出。
- stm32-mavlink/Core：CubeMX/HAL、USART1/2接收中断和主循环。
- stm32-mavlink/ThirdParty/mavlink：本地包含的标准 MAVLink 头文件子集。
- stm32-mavlink/tools/run_host_tests.py：主机回归测试入口。

## 构建

在 stm32-mavlink 目录执行：

```powershell
cmake --preset Release
cmake --build --preset Release
python tools/run_host_tests.py
```

固件：build/Release/tz130tm-native.hex。主机测试需要GCC；ARM构建需要CMake、Ninja和arm-none-eabi工具链。所有串口115200 8N1。

## 验证状态

Debug/Release构建、主机协议测试、70帧真实抓包回放和四种方向系数组合已通过。用户于2026-11-13报告：双轴取反版本已实机运行，Mission Planner地图点选到吊舱运动的控制链路正常跑通。零位、参考系、目标指向精度及机动飞行效果尚未量化验证。

此前的厂家原始命令透传固件说明保留在 [HARDWARE_BENCH.md](HARDWARE_BENCH.md)，旧 tz130tm-bench.hex 未被新固件覆盖。

本地提交、合并和版本发布流程见 [docs/RELEASING.md](docs/RELEASING.md)，当前功能与验证范围见 [CHANGELOG.md](CHANGELOG.md)。
