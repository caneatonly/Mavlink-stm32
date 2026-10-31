# Development Roadmap

## Phase 0: Project Setup ✅

- [x] Review specification documents
- [x] Create CLAUDE.md for AI assistant guidance
- [x] Initialize git repository
- [x] Define module structure
- [ ] STM32CubeMX peripheral configuration (.ioc file)
- [ ] Set up build system (Makefile/CubeIDE project)
- [ ] Integrate MAVLink C library

## Phase 1: Protocol Implementation (P0 Testing)

### 1.1 Vendor Protocol (No Hardware)
- [ ] Implement frame builder with LE/XOR encoding
- [ ] Validate 0x001B/0x001C against captured packets (byte-for-byte)
- [ ] Implement status parser (0x0014, 67-byte frame)
- [ ] Unit tests: valid frames, corrupted XOR, partial frames, stuck bytes
- [ ] Test coordinate order (lon/lat swap)

### 1.2 MAVLink Bridge (No Hardware)
- [ ] Integrate official MAVLink C library (v2.0)
- [ ] Implement COMMAND_INT parser for cmd=195
- [ ] Validate lat/lon field extraction (x=lat, y=lon)
- [ ] Implement frame filtering (system ID, component ID)
- [ ] Implement SET_POSITION_TARGET_GLOBAL_INT builder (ID 86, type_mask=0x0DF8)
- [ ] Unit tests: coordinate bounds, altitude reference validation
- [ ] PC-based MAVLink decoder validation

### 1.3 State Machine (Simulated Inputs)
- [ ] Implement laser FSM (IDLE → START → ACTIVE → STOP)
- [ ] Test tracking state debounce (0x01 for N frames)
- [ ] Test edge-triggered command sending (one 0x003E per transition)
- [ ] Test timeout and duration limit logic
- [ ] Test 0x02 (memory tracking) does NOT trigger ranging

**Gate**: All P0 tests pass before hardware connection.

## Phase 2: Gimbal Bench Testing (P1)

### 2.1 Receive Status
- [ ] Connect USB-TTL to gimbal COM (TTL mode, 115200 8N1)
- [ ] Verify voltage level (3.3V/5V)
- [ ] Capture and parse 50Hz status frames
- [ ] Verify XOR, frame sync, field decoding

### 2.2 Send Commands
- [ ] Send 0x003D (single laser ranging) - verify distance response
- [ ] Send 0x001B (geo pointing) - verify gimbal movement
- [ ] Send 0x001C (geo tracking) - compare behavior vs 0x001B
- [ ] Test 0x003E/0x003F (continuous laser on/off)

### 2.3 Coordinate Validation
- [ ] Inject known aircraft position via GPS+SBUS or direct MAVLink
- [ ] Send geographic command with known target
- [ ] Verify gimbal points correctly
- [ ] Verify altitude datum matches expectations

**Deliverable**: Document 0x001B vs 0x001C behavior differences.

## Phase 3: Dual Control Coordination (P2)

- [ ] ViewControlStudio (LAN) selects visual target
- [ ] Monitor tracking state via STM32 COM receiver
- [ ] Confirm tracking state transitions (0x00 → 0x05 → 0x01)
- [ ] Implement control window arbitration
- [ ] Send 0x003E from STM32 without disrupting LAN tracking
- [ ] Verify laser distance and target coordinates populate
- [ ] Test stop sequence (0x003F) and tracking termination

**Deliverable**: Control handoff protocol specification.

## Phase 4: Pixhawk Integration (P3)

### 4.1 MAVLink Route Discovery
- [ ] Identify Pixhawk firmware (ArduPilot/PX4) and version
- [ ] Configure MAVLink UART parameters
- [ ] Send ROI command from Mission Planner
- [ ] Capture raw UART output from Pixhawk TELEM port
- [ ] Confirm COMMAND_INT(195) routing to STM32

### 4.2 SITL Testing
- [ ] Set up software-in-the-loop simulator
- [ ] Feed COMMAND_INT(195) from SITL to STM32
- [ ] Verify geographic command generation (0x001B/0x001C)
- [ ] Simulate tracking state and laser measurements
- [ ] Send ID86 to SITL and verify decoding
- [ ] **Confirm ID86 does NOT trigger unintended flight behavior**

### 4.3 Safety Validation
- [ ] Confirm with flight control authority: ID86 semantic meaning
- [ ] Document custom software (if any) that intercepts ID86
- [ ] Consider switching to CAMERA_TRACKING_GEO_STATUS (ID 276) if safer
- [ ] Implement dual safety interlocks (compile-time + runtime enable)

### 4.4 Real Flight Controller (DISARMED ONLY)
- [ ] Connect to disarmed Pixhawk (propellers removed)
- [ ] Test full ROI → geo pointing flow
- [ ] Test tracking → ranging → target report flow
- [ ] Monitor for unexpected actuator commands
- [ ] Validate coordinate accuracy with known targets

**Gate**: Flight control authority sign-off required before armed testing.

## Phase 5: Integration and Validation

- [ ] Full system test: Mission Planner → Pixhawk → STM32 → Gimbal
- [ ] Measure end-to-end latency
- [ ] Test corner cases: tracking lost during ranging, altitude mismatch, timeout recovery
- [ ] Field test coordinate accuracy (GPS-surveyed ground targets)
- [ ] Stress test: sustained 10-minute laser ranging session
- [ ] Document operational procedures and safety checklist

## Phase 6: Production Hardening

- [ ] Implement health monitoring and diagnostics
- [ ] Add configuration interface (UART command shell or parameter storage)
- [ ] Optimize UART buffer sizes and DMA handling
- [ ] Add watchdog and fault recovery
- [ ] Create user manual and troubleshooting guide
- [ ] Establish firmware versioning and release process

## Unresolved Questions (Block Progress)

| Priority | Question | Blocking Phase |
|----------|----------|----------------|
| P0 | Is ID86 safe for this Pixhawk firmware? | P3.4 |
| P0 | Will Pixhawk route ROI to STM32 UART? | P3.1 |
| P0 | Do 0x001B/0x001C work on this gimbal firmware? | P1.2 |
| P1 | What is gimbal altitude datum (MSL/ellipsoid)? | P1.3 |
| P1 | How to detect new laser measurement vs. repeated status? | P2 |

See `CLAUDE.md` section "Unconfirmed Items" for full list.
