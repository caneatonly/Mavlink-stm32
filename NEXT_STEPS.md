# Next Steps for Development

## Immediate Actions (Day 1-2)

### 1. STM32CubeMX Configuration
Create a new STM32C8T6 project with:

**Peripherals to Configure:**
- **USART1** (Pixhawk MAVLink):
  - Mode: Asynchronous
  - Baud: 115200, 8N1
  - DMA: RX and TX (circular buffer for RX)
  - Interrupts: USART1 global interrupt

- **USART2** (Gimbal):
  - Mode: Asynchronous
  - Baud: 115200, 8N1
  - DMA: RX and TX (circular buffer for RX)
  - Interrupts: USART2 global interrupt

- **TIM2** (System timing):
  - For timeout tracking, timestamp generation
  - 1ms tick preferred

- **GPIO**:
  - LED indicator pins (PA5 for onboard LED if available)
  - Optional: Debug UART (USART3) for logging

**Save as:** `stm32_mavlink_bridge.ioc`

**Generate code:** Project Manager → Project Settings → Toolchain: Makefile or STM32CubeIDE

### 2. MAVLink Library Integration

```bash
# Download MAVLink C library
cd App/mavlink_bridge/
git clone https://github.com/mavlink/c_library_v2.git mavlink_c_lib
# Or download specific message set
```

Add to include paths in your build system:
```c
#include "mavlink_c_lib/common/mavlink.h"
```

### 3. Create Protocol Test Framework

**File:** `Tests/host_protocol_tests/test_vendor_protocol.c`

Write tests that run on your development PC (no STM32 hardware):
- Build geographic command frames
- Parse status frames
- Validate against captured packets from `Docs/captures/`
- Test XOR calculation
- Test LE encoding/decoding

**Compile:** `gcc -o test_protocol test_vendor_protocol.c vendor_protocol.c`

## Week 1: Protocol Implementation (P0)

Focus on pure software implementation and testing:

1. **Vendor Protocol Module** (`App/gimbal_protocol/`)
   - `gimbal_protocol.h` - Frame structures and function declarations
   - `gimbal_protocol.c` - Frame builder and parser
   - Test against known packets:
     - Geo pointing: `FB 2C AA 0E 1B 00 0B 97 33 44 BE E1 26 12 64 00 00 00 F1`
     - Geo tracking: `FB 2C AA 0E 1C 00 0B 97 33 44 BE E1 26 12 64 00 00 00 F6`

2. **Status Decoder** (`App/gimbal_status/`)
   - `gimbal_status.h` - GimbalStatus struct
   - `gimbal_status.c` - Parse 0x0014 frames, extract fields

3. **MAVLink Bridge** (`App/mavlink_bridge/`)
   - `mavlink_rx.c` - Parse COMMAND_INT(195)
   - `mavlink_tx.c` - Build SET_POSITION_TARGET_GLOBAL_INT(86)
   - **Keep ID86 transmission disabled by default**

4. **Laser FSM** (`App/laser_fsm/`)
   - `laser_fsm.h` - State enum, configuration
   - `laser_fsm.c` - State machine logic with simulated inputs

**Exit Criteria:** All unit tests pass on PC. Zero hardware dependencies.

## Week 2: Hardware Bench Testing (P1)

**Prerequisites:**
- [ ] Confirm gimbal COM port is in TTL mode (not RS232/RS422)
- [ ] Measure voltage level with multimeter (3.3V or 5V?)
- [ ] Purchase level shifter if needed

**Test Sequence:**
1. USB-TTL adapter → Gimbal COM → Serial monitor
   - Observe 50Hz status frames
   - Decode with your parser

2. Replace USB-TTL with programmed STM32
   - LED blinks when valid status received
   - UART3 debug output shows decoded fields

3. Send test commands from STM32:
   - 0x003D single ranging → expect distance in next status
   - 0x001B geo pointing → expect gimbal movement
   - 0x003E continuous ranging → expect sustained distance updates

**Capture and document** all observed behaviors in `Docs/captures/`

## Week 3: Integration and Advanced Testing (P2-P3)

See `Docs/roadmap.md` for detailed Phase 2 and Phase 3 steps.

**Critical Safety Checkpoints:**
- P2: Dual control (LAN + COM) coordination verified
- P3: Pixhawk routing confirmed in SITL
- P3: ID86 semantic safety validated
- P3: Flight control authority approval obtained

## Development Tools Checklist

- [ ] STM32CubeIDE installed
- [ ] ST-Link debugger/programmer available
- [ ] USB-TTL adapter (3.3V/5V compatible)
- [ ] Serial terminal (PuTTY, RealTerm, or VSCode Serial Monitor extension)
- [ ] Logic analyzer or oscilloscope (optional but helpful)
- [ ] Git configured and repository backed up
- [ ] Mission Planner installed (for P3 testing)

## Questions to Resolve First

Before starting hardware work, confirm:

1. **Gimbal COM voltage level**: 3.3V or 5V TTL?
   - Check with multimeter or vendor datasheet
   - STM32C8T6 GPIO is **NOT** 5V tolerant - needs level shifter if gimbal is 5V

2. **Pixhawk firmware**: ArduPilot or PX4? Which version?
   - Determines MAVLink routing configuration
   - Check `SERIALx_PROTOCOL` parameter

3. **MAVLink system IDs**: What are Pixhawk and STM32 expected to use?
   - Typically Pixhawk = 1, Gimbal = 100+
   - Configure filtering to avoid cross-talk

4. **Altitude datum**: Does gimbal use MSL, ellipsoid height, or something else?
   - Critical for coordinate accuracy
   - May need to test with known surveyed target

## Resources

- **CLAUDE.md**: Complete AI assistant guide - read this before asking Claude for help
- **Docs/protocol_reference.md**: Quick lookup for frame formats and field offsets
- **Docs/hardware_setup.md**: Wiring and configuration details
- **Original spec**: `TZ130TM_STM32_MAVLink2_协议转换开发说明.md` (Chinese, authoritative)

## Getting Help

When asking Claude Code for assistance:
1. Reference specific module names (e.g., "gimbal_protocol")
2. Mention test phase (P0/P1/P2/P3)
3. Provide error messages or unexpected behavior details
4. Claude has full context from CLAUDE.md - no need to re-explain architecture

Example: "Implement gimbal_protocol frame builder for 0x001B command, with unit test against the captured packet in CLAUDE.md"

---

**Current Status**: ✅ Project initialized, ready for STM32CubeMX configuration and P0 implementation.

**Next Developer Action**: Create `.ioc` file and generate HAL code, then begin protocol module implementation.
