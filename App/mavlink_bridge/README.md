# MAVLink Bridge Module

Handles MAVLink2 communication with Pixhawk flight controller.

## Responsibilities

- Parse incoming MAVLink2 byte stream (UART-A)
- Decode `COMMAND_INT` messages (ID 75) for ROI commands (195)
- Encode `SET_POSITION_TARGET_GLOBAL_INT` (ID 86) for target reporting
- Filter messages by `target_system` and `target_component`
- Validate coordinate frames and altitude references

## Interface

### Input
- `void mavlink_rx_process(uint8_t byte)` - Feed UART bytes one at a time

### Output
- `GeoTarget` struct when valid ROI command received
- Callback: `void on_roi_command_received(const GeoTarget* target)`

### Configuration
- System ID and component ID filtering
- Accepted MAVLink frame types for altitude reference
- Enable/disable ID86 transmission (safety interlock)

## Safety

**ID86 transmission is DISABLED by default.** Must be explicitly enabled after:
1. Confirming receiving Pixhawk firmware behavior
2. Verifying semantic interpretation as "target report" not "flight command"
3. Completing SITL/simulation testing

See `CLAUDE.md` for P3 testing requirements.
