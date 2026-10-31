# Laser FSM Module

State machine for laser ranging control based on tracking status.

## State Diagram

```
      IDLE
       │
       │ Stable tracking confirmed (N frames @ 0x01)
       ▼
  START_REQUEST ──► Send 0x003E once
       │
       ▼
  RANGING_ACTIVE ──► Monitor & report valid coordinates
       │
       │ Lost/timeout/limit reached
       ▼
  STOP_REQUEST ──► Send 0x003F once
       │
       ▼
      IDLE
```

## Trigger Conditions

### Start Ranging
- Tracking state = `0x01` for `STABLE_CONFIRM_FRAMES` consecutive frames (default: 3)
- Gimbal ready bit set
- No active ranging session

### Stop Ranging
- Tracking state != `0x01`
- Status timeout (no valid frames received)
- Laser duration limit exceeded (default: 10 minutes)
- Manual abort command

## Safety

- **One command per edge**: Do NOT send 0x003E/0x003F repeatedly at 50Hz status rate
- **Duration limit**: Vendor recommends max 15min continuous ranging; implement configurable limit below this
- **Freshness**: Only report coordinates when laser measurement is genuinely new (not 50Hz repetition of same 1Hz sample)

## Configuration

```c
#define STABLE_CONFIRM_FRAMES  3      // ~60ms debounce at 50Hz
#define LASER_MAX_DURATION_S   600    // 10 minutes
#define STATUS_TIMEOUT_MS      200    // 4x 50Hz period
#define LASER_FREQUENCY_HZ     1      // Start conservatively
```
