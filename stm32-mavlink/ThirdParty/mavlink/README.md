# MAVLink reference headers

These 13 headers were copied without modification from the existing project's
App/mavlink_handler/mavlink_c_lib directory. Upstream reference:
https://github.com/mavlink/c_library_v2 (MAVLink generated C reference library).

source-sha256.json records each copied header's SHA-256. No runtime network
fetch or dependency on another worktree is required. Include them through
App/mavlink_handler/mavlink_subset.h; that application wrapper selects the
HEARTBEAT, COMMAND_LONG and COMMAND_ACK CRC entries. Do not edit generated
headers to change the application protocol.

Reference documentation: https://mavlink.io/en/mavgen_c/
