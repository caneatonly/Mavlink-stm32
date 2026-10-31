# Gimbal Status Module

Decodes 0x0014 status feedback frames into structured data.

## Status Frame Structure

50Hz feedback, expected 67 bytes total:
- Header: `FC 2C 55`
- LEN: `0x3E` (62 = 2-byte cmd + 60-byte data)
- CMD: `0x0014` (LE)
- DATA: 60 bytes per vendor Table 14
- XOR: checksum

## Key Fields (Data Area Offsets)

| Offset | Type | Meaning |
|--------|------|---------|
| [1] | uint8_t | Self-check, bit7=ready |
| [7..8] | int16_t LE | Azimuth × 100 |
| [9..10] | int16_t LE | Pitch × 100 |
| [25..26] | uint16_t LE | Laser distance × 0.1m |
| [27..30] | int32_t LE | Target longitude × 10^7 |
| [31..34] | int32_t LE | Target latitude × 10^7 |
| [35..36] | int16_t LE | Target altitude (m) |
| [57] | uint8_t | **Tracking state** |

## Tracking States

- `0x00`: Idle → laser OFF
- `0x01`: **Stable tracking** → eligible for ranging
- `0x02`: Memory tracking → NOT considered valid (target may be lost)
- `0x03`: Target lost → laser OFF
- `0x05`: Intercept → wait for stable

## Output

`GimbalStatus` struct with:
- Parsed fields above
- Validity flags (XOR pass, ready bit, coordinate bounds)
- Timestamp for freshness checking
