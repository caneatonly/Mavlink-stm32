#ifndef MAVLINK_SUBSET_H
#define MAVLINK_SUBSET_H

/* Upstream codecs and the CRC entries needed by the bridge.
 * Leave MAVLINK_CHECK_MESSAGE_LENGTH undefined: MAVLink2 trims trailing zeros,
 * including COMMAND_LONG.confirmation in the user's 32-byte payload captures. */
#define MAVLINK_COMM_NUM_BUFFERS 1
#define MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT 86
#define MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT_LEN 53
#define MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT_CRC 5
#define MAVLINK_MESSAGE_CRCS { \
    {0, 50, 9, 9, 0, 0, 0}, \
    {76, 152, 33, 33, 3, 30, 31}, \
    {77, 143, 3, 10, 3, 8, 9}, \
    {MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT, \
     MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT_CRC, \
     MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT_LEN, \
     MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT_LEN, 3, 50, 51} \
}
#include <minimal/mavlink.h>
#include <common/mavlink_msg_command_long.h>
#include <common/mavlink_msg_command_ack.h>

#endif
