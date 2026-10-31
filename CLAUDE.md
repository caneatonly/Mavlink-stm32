# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

STM32C8T6-based MAVLink2 ↔ TZ130TM gimbal protocol converter for Pixhawk-controlled UAV systems. The STM32 bridges between:
- **UART-A**: Pixhawk MAVLink2 communication
- **UART-B**: TZ130TM gimbal proprietary protocol (T-series)

### Core Functions

1. **Geographic ROI Targeting**: Converts MAVLink `COMMAND_INT(195)` ROI commands to gimbal geographic pointing commands (`0x001B`/`0x001C`)
2. **Visual Target Geo-Location**: Monitors gimbal tracking state, triggers laser ranging, and reports target coordinates back via MAVLink

### Critical Safety Constraints

⚠️ **BEFORE ANY IMPLEMENTATION**:
- `SET_POSITION_TARGET_GLOBAL_INT` (Message ID 86) is a **flight control command**, not a benign telemetry report
- **NEVER send ID86 to a real Pixhawk with propellers** until confirmed safe by flight control authority
- Default: ID86 transmission to real flight controller is **DISABLED** by both compile-time switch and runtime flag
- Test only with PC simulation or SITL until semantic safety is verified

## Build and Development

### Prerequisites
- STM32CubeIDE or ARM GCC toolchain
- STM32CubeMX for peripheral configuration
- Official MAVLink C library (v2.0)

### Build Commands
```bash
# Using Makefile (once generated)
make clean
make all

# Using STM32CubeIDE
# Project → Build All (Ctrl+B)
```

### Flashing
```bash
# ST-Link
st-flash write build/stm32_mavlink_bridge.bin 0x8000000

# Or via STM32CubeIDE: Run → Debug (F11)
```

### Testing Phases

**P0 - Protocol Unit Tests (No Hardware)**
- Test vendor frame builder/parser with known byte sequences
- Validate MAVLink COMMAND_INT parsing
- Test state machine with simulated inputs
- **All P0 tests MUST pass before connecting hardware**

**P1 - Gimbal COM Bench Test**
- Connect USB-TTL to gimbal COM port (TTL mode, 115200 8N1)
- Verify `FC 2C 55` status feedback reception
- Test `0x001B`/`0x001C` geographic commands
- Test `0x003E`/`0x003F` laser ranging control

**P2 - Dual Control Coordination**
- ViewControlStudio via LAN for visual target selection
- STM32 via COM for laser ranging control
- Verify no command collision during handoff windows

**P3 - Pixhawk Integration**
- Identify actual Pixhawk firmware (ArduPilot/PX4) and MAVLink routing
- Test with SITL/PC simulator FIRST
- Enable real flight controller only after safety confirmation

## Architecture

### Module Boundaries

```
App/
├── mavlink_bridge/      # MAVLink2 RX/TX, COMMAND_INT decoder, target reporter
├── gimbal_protocol/     # Vendor frame builder/parser, LE/XOR encoding
├── gimbal_status/       # 0x0014 status field decoder → GimbalStatus struct
├── laser_fsm/           # Tracking state machine → laser on/off edge triggers
├── control_arbitration/ # LAN/COM control window coordination
└── system_health/       # Timeout, error counters, debug logging

Core/
├── Inc/                 # STM32 HAL headers
└── Src/                 # main.c, system init

Drivers/                 # STM32 HAL drivers

Tests/
├── host_protocol_tests/ # Pure software encoding/decoding tests
└── uart_replay/         # Captured packet replay validation

Docs/
└── captures/            # Vendor packet captures with firmware version notes
```

### Data Flow

```
Mission Planner
      ↓ (MAVLink ROI)
  Pixhawk
      ↓ UART-A: MAVLink2
  STM32 Protocol Converter
      ↓ UART-B: Vendor protocol
  TZ130TM Gimbal ← LAN/UDP ← ViewControlStudio (visual target selection)
```

**Critical Protocol Details**:
- Vendor control: `FB 2C AA <LEN> <CMD_LE> <DATA> <XOR>`
- Vendor status: `FC 2C 55 <LEN> <CMD_LE> <DATA> <XOR>` @ 50Hz
- XOR checksum: from LEN byte through end of data (NOT including frame header)
- Multi-byte fields: **Little-Endian**
- Coordinate order: Vendor uses `lon,lat`; MAVLink uses `lat,lon` (x=lat, y=lon)

### Key Coordinate Transformations

**MAVLink → Vendor Geographic Command**:
```
COMMAND_INT.x (lat_e7) → vendor payload[4..7] (lat × 10^7, LE)
COMMAND_INT.y (lon_e7) → vendor payload[0..3] (lon × 10^7, LE)
COMMAND_INT.z (alt_m)  → vendor payload[8..11] (alt × 10, LE, in decimeters)
```

**Vendor Status → MAVLink Target Report**:
```
status.payload[31..34] (lat × 10^7, LE) → SET_POSITION_TARGET_GLOBAL_INT.lat_int
status.payload[27..30] (lon × 10^7, LE) → SET_POSITION_TARGET_GLOBAL_INT.lon_int
status.payload[35..36] (alt_m, LE)      → SET_POSITION_TARGET_GLOBAL_INT.alt (float)
```

## Implementation Rules

### Protocol Parsing

1. **Stream-based state machines**: Support partial frames, sticky frames, and error recovery
2. **Never assume** one UART callback = one complete frame
3. **Validate everything**: frame header, length range, XOR/CRC, address filters, coordinate bounds
4. **Explicit endianness**: Use LE read/write helpers, never cast unaligned memory to structs

### Vendor Protocol Specifics

- **Communication mode**: Single-request (单次请求式) - send commands only when needed, no idle frames
- **Status feedback**: 50Hz default; gimbal ready when `payload[1] bit7 == 1`
- **Tracking states** (`payload[57]`):
  - `0x01` = stable tracking → **ONLY** state eligible for laser ranging
  - `0x02` = memory tracking → NOT considered valid for ranging (target may be lost)
  - `0x00/0x03` = idle/lost → laser OFF
- **Laser safety**: Max 15 min continuous ranging (vendor recommendation); implement shorter software timeout (e.g., 10 min)

### Laser State Machine

```
IDLE → (stable tracking confirmed for N frames) → START_REQUEST
    → send 0x003E once
    → RANGING_ACTIVE (report valid coordinates per laser update rate)
    → (tracking lost/timeout/limit reached) → STOP_REQUEST
    → send 0x003F once
    → IDLE
```

**Critical**: Only send start/stop commands on **state edges**, NOT repeatedly at 50Hz status rate

### Coordinate Validity

Only report target coordinates when ALL conditions met:
1. Gimbal ready (`payload[1] bit7 == 1`)
2. Status frame XOR valid
3. Tracking state == `0x01` (stable)
4. Laser distance in valid range (not blind zone)
5. Coordinates within Earth bounds
6. Data is fresh (not replaying old measurements)

**Freshness challenge**: Vendor protocol has no explicit "new data" flag. At 1Hz laser + 50Hz status, same measurement repeats across multiple frames. Do NOT treat every 50Hz frame as new laser sample.

### MAVLink Handling

- Parse with official MAVLink C library (v2.0)
- Filter by `target_system` and `target_component` (do not blindly respond to all commands)
- For `COMMAND_INT(195)`:
  - Check `frame` field (altitude reference): accept only when confirmed to match vendor altitude datum
  - Validate coordinate ranges: lat ∈ [-90°, +90°], lon ∈ [-180°, +180°]
  - Remember: `COMMAND_INT.x` = latitude, `.y` = longitude (reversed vs. vendor order)

### Control Arbitration

**LAN + COM simultaneous connection**: Vendor confirmed this is safe IF commands don't collide in time
- ViewControlStudio (LAN) sends visual tracking commands
- STM32 (COM) sends laser ranging commands
- **No built-in mutual exclusion** - implement explicit control window coordination or manual operational discipline
- For production: consider single arbitration point (proxy/central controller) to serialize all commands

## Testing and Validation

### Byte-Level Validation

**Confirmed vendor packets** (from ViewControlStudio capture):
- Geographic pointing (lon=114.4231691°, lat=30.4538046°, alt=10.0m):
  - `0x001B`: `FB 2C AA 0E 1B 00 0B 97 33 44 BE E1 26 12 64 00 00 00 F1`
  - `0x001C`: `FB 2C AA 0E 1C 00 0B 97 33 44 BE E1 26 12 64 00 00 00 F6`

When implementing frame builders, **compare byte-for-byte** with these known-good packets.

### Required Test Coverage

- Coordinate order swapping (lat/lon vs. lon/lat)
- Altitude reference mismatch rejection
- Tracking state 0x02 NOT triggering laser
- Old coordinate data not reported as new measurements
- XOR calculation on correct byte range
- Little-endian multi-byte field encoding/decoding
- Partial/split/stuck frames in UART stream

## Unconfirmed Items (Do NOT Assume)

❌ **Cannot proceed with real flight until confirmed**:
- Whether Pixhawk firmware will route ROI commands to STM32 UART
- Semantic meaning of ID86 to the specific receiving Pixhawk firmware
- Whether custom software exists to interpret ID86 as "target report" rather than "flight command"

❌ **Vendor protocol behavior requires bench testing**:
- Whether `0x001B` vs `0x001C` have different tracking behaviors
- Actual altitude datum used by vendor (MSL/ellipsoid/other)
- How gimbal signals "new laser measurement available" in status stream

❌ **Hardware details pending**:
- Actual Pixhawk firmware (ArduPilot vs PX4) and version
- TTL voltage level (3.3V vs 5V) for COM interface
- UART port assignments on both Pixhawk and STM32

## Code Style and Conventions

- **Modular boundaries**: MAVLink layer doesn't touch UART-B; vendor protocol layer outputs `GimbalStatus` struct, not raw bytes
- **Typed interfaces**: Use explicit structs like `GeoTarget{lon_e7, lat_e7, alt_m, alt_reference}` at module boundaries
- **No magic numbers**: Define protocol constants with clear names
- **Logging**: Track error counters, frame drops, state transitions for field debugging
- **Configuration**: Expose tunable parameters (debounce frames, timeouts, laser max duration) rather than hardcoding

### Critical Constants

```c
// Vendor protocol
#define VENDOR_CTRL_HEADER     {0xFB, 0x2C, 0xAA}
#define VENDOR_STATUS_HEADER   {0xFC, 0x2C, 0x55}
#define VENDOR_BAUD            115200

// MAVLink
#define MAV_CMD_DO_SET_ROI_LOCATION  195
#define MAVLINK_MSG_ID_COMMAND_INT   75
#define MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT  86

// Tracking states
#define TRACKING_IDLE          0x00
#define TRACKING_STABLE        0x01
#define TRACKING_MEMORY        0x02
#define TRACKING_LOST          0x03
#define TRACKING_INTERCEPT     0x05

// Safety
#define LASER_MAX_DURATION_S   600  // Below vendor 15min recommendation
#define STABLE_CONFIRM_FRAMES  3    // Debounce at 50Hz (~60ms)
```

## Documentation References

All protocol details, byte offsets, and field definitions are derived from:
1. TZ130TM User Manual V1.0
2. T-Series Gimbal Communication Protocol V1.4 (2025-04-01)
3. Vendor-supplied geographic command table (0x001B/0x001C) - supplementary screenshots
4. ViewControlStudio + SSCOM capture validation
5. MAVLink common message set (https://mavlink.io/en/messages/common)

When vendor documentation conflicts with test captures, **captures take precedence**. Mark any assumptions clearly as "TBD pending bench test."
