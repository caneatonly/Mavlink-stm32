# Gimbal Protocol Module

Vendor T-series protocol frame builder and parser.

## Frame Formats

### Control Frame (STM32 → Gimbal)
```
FB 2C AA <LEN> <CMD_LE> <DATA...> <XOR>
```

### Status Frame (Gimbal → STM32)
```
FC 2C 55 <LEN> <CMD_LE> <DATA...> <XOR>
```

## Key Functions

### Frame Building
- `build_geo_pointing_frame(lon_e7, lat_e7, alt_dm, cmd)` → 0x001B/0x001C
- `build_laser_start_frame()` → 0x003E
- `build_laser_stop_frame()` → 0x003F

### Frame Parsing
- `parse_status_frame(uint8_t* buf, size_t len, StatusFrame* out)` → validate and decode

## Critical Details

- **Checksum**: XOR from LEN byte through end of data (NOT including `FB 2C AA` header)
- **Endianness**: All multi-byte fields are Little-Endian
- **Coordinate order**: Vendor expects `[lon, lat]` (opposite of MAVLink)
- **Altitude units**: Decimeters (1/10 meter), not meters

## Validation

All frame builders MUST be validated byte-for-byte against captured packets in `Docs/captures/`.

Reference packets (ViewControlStudio capture):
- Geo pointing 0x001B: `FB 2C AA 0E 1B 00 0B 97 33 44 BE E1 26 12 64 00 00 00 F1`
- Geo tracking 0x001C: `FB 2C AA 0E 1C 00 0B 97 33 44 BE E1 26 12 64 00 00 00 F6`
