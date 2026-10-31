# STM32 MAVLink2 ↔ TZ130TM Protocol Converter

STM32C8T6-based bidirectional protocol bridge for Pixhawk UAV and TZ130TM gimbal systems.

## Features

- **Geographic ROI**: Converts MAVLink Region-of-Interest commands to gimbal geographic pointing
- **Target Geo-location**: Monitors visual tracking, triggers laser ranging, reports target coordinates
- **Dual Interface**: MAVLink2 (Pixhawk) + T-series proprietary protocol (gimbal)

## Safety Notice

⚠️ **This system sends MAVLink flight control messages. Do NOT connect to armed aircraft until safety validation is complete.**

Default configuration disables all potentially hazardous outputs. See `CLAUDE.md` for development and testing phases.

## Hardware Requirements

- STM32C8T6 development board
- Pixhawk flight controller (ArduPilot/PX4)
- TZ130TM three-axis gimbal
- 2x UART connections (TTL, 115200 baud)

## Quick Start

### 1. Initial Setup

```bash
# Clone and initialize
git clone <repository-url>
cd stm32-mavlink2

# Open with STM32CubeIDE
# File → Open Projects from File System → Select project directory
```

### 2. Configure Hardware

See `Docs/hardware_setup.md` for:
- STM32 pinout assignments
- UART wiring (Pixhawk ↔ STM32 ↔ Gimbal)
- Voltage level verification

### 3. Build and Test

Follow **P0-P3 testing phases** in `CLAUDE.md`:
- P0: Protocol unit tests (no hardware)
- P1: Gimbal bench test
- P2: Dual control coordination
- P3: Pixhawk integration (simulation first)

## Project Structure

```
├── App/                    # Application logic
│   ├── mavlink_bridge/     # MAVLink2 interface
│   ├── gimbal_protocol/    # Vendor protocol codec
│   ├── gimbal_status/      # Status parser
│   ├── laser_fsm/          # Ranging state machine
│   └── system_health/      # Diagnostics
├── Core/                   # STM32 HAL code
├── Drivers/                # STM32 drivers
├── Tests/                  # Unit and integration tests
└── Docs/                   # Documentation and captures

See CLAUDE.md for detailed architecture.
```

## Documentation

- **[CLAUDE.md](CLAUDE.md)**: Complete development guide for AI assistants
- **[TZ130TM_STM32_MAVLink2_协议转换开发说明.md](TZ130TM_STM32_MAVLink2_协议转换开发说明.md)**: Original specification (Chinese)
- **T系列吊舱通讯协议V1.4**: Vendor protocol manual

## Status

**Phase**: Initial project setup
- ✅ Documentation reviewed
- ⬜ STM32CubeMX peripheral configuration
- ⬜ Protocol unit tests (P0)
- ⬜ Hardware bench tests (P1-P3)

## License

[To be determined]

## Contact

[Project maintainer information]
