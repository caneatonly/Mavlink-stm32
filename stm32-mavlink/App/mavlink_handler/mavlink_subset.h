#ifndef MAVLINK_SUBSET_H
#define MAVLINK_SUBSET_H

/* Unmodified upstream codecs, with the CRC table limited to our three messages.
 * Leave MAVLINK_CHECK_MESSAGE_LENGTH undefined: MAVLink2 trims trailing zeros,
 * including COMMAND_LONG.confirmation in the user's 32-byte payload captures. */
#define MAVLINK_COMM_NUM_BUFFERS 1
#define MAVLINK_MESSAGE_CRCS { \
    {0, 50, 9, 9, 0, 0, 0}, \
    {76, 152, 33, 33, 3, 30, 31}, \
    {77, 143, 3, 10, 3, 8, 9} \
}
#include <minimal/mavlink.h>
#include <common/mavlink_msg_command_long.h>
#include <common/mavlink_msg_command_ack.h>

#endif
