# STM32 MAVLink ↔ TZ130TM 原生云台适配

本项目用 STM32 转接 ArduPilot 和 TZ130TM 吊舱。日常查看源码、编译与交付均从检出 `master` 的工作树进行。本机对应 `master-integration`。用 `git worktree list` 可核对每个目录检出的分支和提交。

## 当前实现

- **地图指向**。Mission Planner 点选位置，插件通知 STM32 开始新任务。飞控经 TELEM2 发角度命令 205，STM32 转为厂家数引 `0x0072`，到位后暂停转发，由厂家 UI 跟踪。
- **坐标回传**。厂家 UI 触发单次或连续测距。稳定跟踪时，STM32 将有效经纬高封装为 MAVLink2 `SET_POSITION_TARGET_GLOBAL_INT`（ID 86），经 USART1 发给飞控 TELEM2。
- **调试输出**。USART3 显示吊舱状态、距离、经纬高及 USART1 发出的原始帧。接线、参数和操作步骤见 [HARDWARE_NATIVE.md](HARDWARE_NATIVE.md)。

## 代码结构

- stm32-mavlink/App/common：配置、环形缓冲、数据结构、定点文本格式。
- stm32-mavlink/App/mavlink_handler：心跳、205接收与角度转换、ACK、控制日志。
- stm32-mavlink/App/vendor_protocol：数引帧编码和吊舱状态解析。
- stm32-mavlink/App/status_reporter：吊舱反馈文本输出。
- stm32-mavlink/Core：CubeMX/HAL、USART1/2接收中断和主循环。
- stm32-mavlink/ThirdParty/mavlink：本地包含的标准 MAVLink 头文件子集。
- stm32-mavlink/tools/run_host_tests.py：主机回归测试入口。

## 固件配置

当前源码的两种 Release 配置都使用 ID 86。切换时选择对应的 HEX，或用同一份源码按下表重新构建。

| 配置 | 自动地图指向 | USART1 坐标回传 | 用途 |
| --- | --- | --- | --- |
| 探测版 | OFF | OFF，USART3 预览 ID 86 | 检查点选通知、角度和封包 |
| 启用版 | ON | ON，向 TELEM2 发送 ID 86 | 未解锁台架端到端测试 |

在 `stm32-mavlink` 目录执行所需配置的命令。

```powershell
# 探测版，输出到 build/Release
cmake --preset Release -DENABLE_POINTING_AUTO_RESUME=OFF -DENABLE_RANGE_REPORT=OFF
cmake --build --preset Release

# 启用版，输出到 build/Release-enabled
cmake --preset Release -B build/Release-enabled -DENABLE_POINTING_AUTO_RESUME=ON -DENABLE_RANGE_REPORT=ON
cmake --build build/Release-enabled

# 主机回归
python tools/run_host_tests.py
```

两种构建的原始文件均名为 `tz130tm-native.hex`。对外交付时，文件名应标明探测或启用配置及 Git 提交。主机回归需要 GCC，ARM 构建需要 CMake、Ninja 和 arm-none-eabi 工具链。所有串口为 115200 8N1。

## 验证状态

地图指向、到位停发、厂家 UI 跟踪和测距已完成台架测试。此前的 ID 276 开发阶段已验证 STM32 发包。当前 ID 86 已通过 Release 编译和主机回归，最新固件的实机发包和飞控收包仍待确认。ID 86 可作为飞行位置设定值，先在未解锁台架或 SITL 验证。

早期厂家命令透传版见 [HARDWARE_BENCH.md](HARDWARE_BENCH.md)，提交 `9d45e11` 是原生 MAVLink 指向的历史交付版本。

本地提交、合并和版本发布流程见 [docs/RELEASING.md](docs/RELEASING.md)，历史记录见 [CHANGELOG.md](CHANGELOG.md)。
