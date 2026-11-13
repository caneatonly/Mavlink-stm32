# TELEM2 capture fixture

telem2_mount_rx.hex is the RX-only stream from the user's 2026-11-13 capture.
TX and RX display fragments were separated before concatenation. The stream
has 3153 bytes: 70 complete COMMAND_LONG / DO_MOUNT_CONTROL (205) frames,
2 flight-controller heartbeats and 31 bytes of an incomplete final frame.
All complete frames have valid MAVLink2 CRCs. Source is 1/1, mount target 1/154,
mode 2. No timestamps were supplied, so the fixture proves decoding, not rate
or latency. Tests require exactly 70 vendor frames and 70 ACKs.
