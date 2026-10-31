# Hardware Setup Guide

## STM32C8T6 Pinout

### UART-A (Pixhawk ↔ STM32)
- **Protocol**: MAVLink2
- **Baud**: 115200, 8N1 (verify with Pixhawk config)
- **Connections**:
  - Pixhawk TX → STM32 RX (e.g., PA10 / USART1_RX)
  - Pixhawk RX ← STM32 TX (e.g., PA9 / USART1_TX)
  - GND common

### UART-B (STM32 ↔ Gimbal)
- **Protocol**: T-series vendor protocol
- **Baud**: 115200, 8N1 (fixed per vendor spec)
- **Connections**:
  - Gimbal TTL_TX1 (pin 1) → STM32 RX (e.g., PA3 / USART2_RX)
  - Gimbal TTL_RX1 (pin 2) ← STM32 TX (e.g., PA2 / USART2_TX)
  - Gimbal GND (pin 3) → STM32 GND

⚠️ **Voltage Level**: Verify gimbal COM TTL voltage (3.3V vs 5V) before connecting. Use level shifter if needed.

## Gimbal Configuration

### COM Port Mode
1. Press **SET** button on gimbal
2. Select TTL mode (NOT RS232/RS422)
3. Confirm baud rate: 115200, 8N1

### Control Mode
- Single-request mode (单次请求式) - default, recommended
- Status feedback: 50Hz default

### Simultaneous LAN + COM
- ViewControlStudio connected via LAN (UDP port 10000)
- STM32 connected via COM
- **Critical**: Coordinate control windows to avoid command collision

## Pixhawk Configuration

⚠️ **To be confirmed during P3 testing**:
- Actual firmware: ArduPilot or PX4?
- MAVLink2 UART parameters (SERIALx_PROTOCOL, SERIALx_BAUD)
- System ID and component ID
- MAVLink routing for ROI commands
- Gimbal/mount backend configuration

## Power Supply

- STM32C8T6: 3.3V logic, can be powered via USB or external 5V → 3.3V regulator
- Gimbal: Self-powered (check user manual for specifications)
- Pixhawk: Self-powered

**Do NOT attempt to power gimbal or Pixhawk from STM32 GPIO.**

## Testing Setup

### Phase P1 (Gimbal Only)
```
PC (ViewControlStudio) ──LAN──► Gimbal ◄──COM── USB-TTL ──► PC (Serial monitor)
```

### Phase P2 (Add STM32)
```
PC (ViewControlStudio) ──LAN──► Gimbal ◄──COM── STM32
                                           └──USB── PC (Debug/flash)
```

### Phase P3 (Full System)
```
Mission Planner
     │
  Pixhawk ◄──UART-A── STM32 ──UART-B──► Gimbal ◄──LAN── ViewControlStudio
```

## Troubleshooting

### No Status Feedback from Gimbal
- Check TTL mode selection (not RS232/RS422)
- Verify RX/TX not swapped
- Confirm 115200 baud, 8N1
- Check GND connection

### Pixhawk Not Sending ROI Commands
- Verify MAVLink routing configuration
- Check if gimbal/mount backend is enabled
- Use MAVLink inspector to confirm command transmission
- May need custom parameter configuration (see CLAUDE.md unconfirmed items)

### Command Collision (LAN + COM)
- Implement control window arbitration
- Use manual coordination during testing
- Consider single-control-point architecture for production
