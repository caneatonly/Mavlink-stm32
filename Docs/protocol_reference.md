# Protocol Quick Reference

## Vendor T-Series Protocol

### Control Frame Structure
```
Offset  Length  Field
0-2     3       Header: FB 2C AA
3       1       LEN = 2 (cmd) + N (data)
4-5     2       Command (Little-Endian)
6..     N       Data
6+N     1       XOR checksum (from byte 3 to end of data)
```

### Status Frame Structure
```
Offset  Length  Field
0-2     3       Header: FC 2C 55
3       1       LEN = 2 (cmd) + N (data)
4-5     2       Command (Little-Endian), typically 0x0014
6..     N       Data (N=60 for 0x0014)
6+N     1       XOR checksum (from byte 3 to end of data)
```

### Key Commands

| Command | Direction | Purpose | Data Length |
|---------|-----------|---------|-------------|
| 0x001B | → Gimbal | Geographic pointing | 12 bytes |
| 0x001C | → Gimbal | Geographic tracking | 12 bytes |
| 0x003D | → Gimbal | Single laser ranging | 4 bytes |
| 0x003E | → Gimbal | Start continuous laser | 4 bytes |
| 0x003F | → Gimbal | Stop continuous laser | 4 bytes |
| 0x0014 | ← Gimbal | Status feedback @ 50Hz | 60 bytes |

### Geographic Command Data (0x001B/0x001C)
```c
struct GeoCommandData {
    int32_t lon_e7;     // Longitude × 10^7 (degrees), LE
    int32_t lat_e7;     // Latitude × 10^7 (degrees), LE
    int32_t alt_dm;     // Altitude × 10 (decimeters), LE
};
// Order: [lon, lat, alt] - NOTE: lon before lat
```

### Status Data (0x0014) - Selected Fields
```c
// Offsets are relative to data area (byte 6 of full frame)
struct StatusData {
    uint8_t  ready_status;           // [1], bit7 = gimbal ready
    int16_t  azimuth_deg100;         // [7..8], LE
    int16_t  pitch_deg100;           // [9..10], LE
    uint16_t laser_dist_dm;          // [25..26], LE, 0.1m units
    int32_t  target_lon_e7;          // [27..30], LE
    int32_t  target_lat_e7;          // [31..34], LE
    int16_t  target_alt_m;           // [35..36], LE, meters
    uint8_t  cmd_echo;               // [46], last received command
    uint8_t  tracking_state;         // [57], see table below
};
// Full frame absolute positions: add 6 to each offset
```

### Tracking States
| Value | Name | Laser Eligible | Notes |
|-------|------|----------------|-------|
| 0x00 | Idle | ❌ | No target |
| 0x01 | Stable | ✅ | Only state for valid ranging |
| 0x02 | Memory | ❌ | Target may be lost, not reliable |
| 0x03 | Lost | ❌ | Target definitely lost |
| 0x05 | Intercept | ❌ | Acquiring target |

## MAVLink2 Protocol

### COMMAND_INT (Message ID 75)
```c
struct command_int_t {
    uint8_t  target_system;       // Pixhawk system ID
    uint8_t  target_component;    // Component ID
    uint8_t  frame;               // MAV_FRAME enum (altitude reference)
    uint16_t command;             // MAV_CMD enum, use 195 for ROI
    uint8_t  current;             // 0 for new command
    uint8_t  autocontinue;        // Usually 0
    float    param1;              // Gimbal device ID (or NaN)
    float    param2;              // Usually 0
    float    param3;              // Usually 0
    float    param4;              // Usually 0
    int32_t  x;                   // LATITUDE × 10^7 (degrees)
    int32_t  y;                   // LONGITUDE × 10^7 (degrees)
    float    z;                   // Altitude (meters), reference per 'frame'
};
// Command: MAV_CMD_DO_SET_ROI_LOCATION = 195
// NOTE: x = lat, y = lon (opposite of vendor order!)
```

### SET_POSITION_TARGET_GLOBAL_INT (Message ID 86)
```c
struct set_position_target_global_int_t {
    uint32_t time_boot_ms;        // Timestamp since boot
    uint8_t  target_system;       // Pixhawk system ID
    uint8_t  target_component;    // Component ID
    uint8_t  coordinate_frame;    // MAV_FRAME enum, e.g., MAV_FRAME_GLOBAL=0
    uint16_t type_mask;           // Bitmask, use 0x0DF8 (3576) for position-only
    int32_t  lat_int;             // LATITUDE × 10^7
    int32_t  lon_int;             // LONGITUDE × 10^7
    float    alt;                 // Altitude (meters)
    float    vx, vy, vz;          // Velocity (ignored, set to 0)
    float    afx, afy, afz;       // Acceleration (ignored, set to 0)
    float    yaw;                 // Yaw (ignored, set to 0)
    float    yaw_rate;            // Yaw rate (ignored, set to 0)
};
// type_mask = 0x0DF8: position valid, velocity/accel/yaw ignored
// NOTE: lat_int before lon_int (same as COMMAND_INT)
```

### MAV_FRAME Altitude References
| Frame | Value | Altitude Meaning |
|-------|-------|------------------|
| MAV_FRAME_GLOBAL | 0 | MSL (mean sea level) |
| MAV_FRAME_GLOBAL_RELATIVE_ALT | 3 | Relative to home |
| MAV_FRAME_GLOBAL_INT | 5 | MSL (integer coords) |
| MAV_FRAME_GLOBAL_RELATIVE_ALT_INT | 6 | Relative to home |
| MAV_FRAME_GLOBAL_TERRAIN_ALT | 10 | Relative to terrain |
| MAV_FRAME_GLOBAL_TERRAIN_ALT_INT | 11 | Relative to terrain |

⚠️ **Only accept frames that match confirmed vendor altitude datum.**

## Coordinate Transformations

### MAVLink ROI → Vendor Geographic Command
```c
// Input: COMMAND_INT with command=195
int32_t vendor_lon_e7 = mavlink.y;  // MAVLink y = longitude
int32_t vendor_lat_e7 = mavlink.x;  // MAVLink x = latitude
int32_t vendor_alt_dm = (int32_t)(mavlink.z * 10.0f);  // meters → decimeters

// Output: Build 0x001B or 0x001C frame
// Data order: [lon, lat, alt] in Little-Endian
```

### Vendor Status → MAVLink Target Report
```c
// Input: Status 0x0014 data payload
int32_t mavlink_lat_int = status.target_lat_e7;  // LE bytes [31..34]
int32_t mavlink_lon_int = status.target_lon_e7;  // LE bytes [27..30]
float   mavlink_alt = (float)status.target_alt_m;  // LE bytes [35..36]

// Output: Build SET_POSITION_TARGET_GLOBAL_INT (ID 86)
// Field order: lat_int, lon_int, alt
```

## Validation Checks

### Coordinate Bounds
```c
// Latitude: -90° to +90°
-900000000 <= lat_e7 <= 900000000

// Longitude: -180° to +180°
-1800000000 <= lon_e7 <= 1800000000

// Altitude: vendor-specific range, confirm with datasheet
// Typically: -1000m to +10000m
```

### XOR Checksum
```c
uint8_t calculate_xor(const uint8_t* frame, size_t start, size_t end) {
    uint8_t xor = 0;
    for (size_t i = start; i <= end; i++) {
        xor ^= frame[i];
    }
    return xor;
}

// Vendor frames: XOR from byte 3 (LEN) through end of data
// Example: 19-byte geo command
uint8_t xor = calculate_xor(frame, 3, 17);  // frame[18] should match xor
```

## Hardware Configuration

### UART Settings (Both Interfaces)
- Baud: 115200
- Data: 8 bits
- Parity: None
- Stop: 1 bit
- Flow control: None

### Gimbal COM Connector (GH125-10)
| Pin | Signal | STM32 Connection |
|-----|--------|------------------|
| 1 | TTL_TX1 | STM32 UART RX |
| 2 | TTL_RX1 | STM32 UART TX |
| 3 | GND | STM32 GND |

⚠️ **Confirm voltage level (3.3V/5V) before connecting.**
