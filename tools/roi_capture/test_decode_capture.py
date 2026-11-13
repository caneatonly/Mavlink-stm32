import struct
import unittest

from decode_capture import FrameDecoder, describe_frame, read_hex


HEARTBEAT = bytes.fromhex(
    "FD 09 00 00 49 01 01 00 00 00 05 00 00 00 04 03 59 03 03 AD BB"
)


def make_command_frame(payload, magic=0xFD, signed=False):
    """Independent packet fixture: COMMAND_INT CRC_EXTRA is 158."""
    if magic == 0xFD:
        header = bytes([len(payload), int(signed), 0, 10, 255, 190, 75, 0, 0])
    else:
        header = bytes([len(payload), 10, 255, 190, 75])
    crc = 0xFFFF
    for byte in header + payload + bytes([158]):
        tmp = byte ^ (crc & 0xFF)
        tmp ^= (tmp << 4) & 0xFF
        crc = ((crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF
    return bytes([magic]) + header + payload + struct.pack("<H", crc) + (bytes(13) if signed else b"")


class CaptureTests(unittest.TestCase):
    def test_real_heartbeat_byte_by_byte(self):
        decoder = FrameDecoder()
        frames = []
        for byte in HEARTBEAT:
            frames.extend(decoder.feed(bytes([byte])))
        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0]["msgid"], 0)
        self.assertEqual(frames[0]["seq"], 0x49)
        self.assertIn("HEARTBEAT", describe_frame(frames[0]))
        self.assertEqual(decoder.crc_errors, 0)

    def test_truncated_mavlink2_roi_preserves_frame_and_coordinates(self):
        payload = struct.pack("<4fiifH5B", 0, 0, 0, 0,
                              310255427, 1214418189, 0, 201, 1, 1, 3, 0, 0)
        frame = FrameDecoder().feed(make_command_frame(payload.rstrip(b"\0")))[0]
        line = describe_frame(frame)
        for expected in ["command=201", "frame=3", "lat=31.0255427",
                         "lon=121.4418189", "alt_m=0", "target=1/1"]:
            self.assertIn(expected, line)
        self.assertEqual(len(frame["payload"]), 33)

    def test_corrupt_packet_then_valid_packet_recovers(self):
        bad = bytearray(HEARTBEAT)
        bad[-1] ^= 1
        decoder = FrameDecoder()
        frames = decoder.feed(bytes(bad) + HEARTBEAT)
        self.assertEqual(len(frames), 1)
        self.assertEqual(decoder.crc_errors, 1)

    def test_mavlink1_negative_altitude(self):
        payload = struct.pack("<4fiifH5B", 0, 0, 0, 0,
                              -310255427, -1214418189, -3.5, 195, 1, 1, 0, 0, 0)
        frame = FrameDecoder().feed(make_command_frame(payload, 0xFE))[0]
        line = describe_frame(frame)
        self.assertIn("lat=-31.0255427", line)
        self.assertIn("alt_m=-3.5", line)

    def test_signed_packet_is_framed_without_claiming_signature_verification(self):
        payload = struct.pack("<4fiifH5B", 0, 0, 0, 0,
                              310255427, 1214418189, 1, 201, 1, 1, 3, 0, 0)
        packet = make_command_frame(payload, signed=True)
        decoder = FrameDecoder()
        self.assertEqual(decoder.feed(packet[:-1]), [])
        frame = decoder.feed(packet[-1:])[0]
        self.assertIn("signature=NOT_VERIFIED", describe_frame(frame))

    def test_hex_input_requires_only_hex_bytes(self):
        self.assertEqual(read_hex("\ufeff« FD 09 »\n"), b"\xfd\x09")
        with self.assertRaises(ValueError):
            read_hex("[09:00] FD 09")


if __name__ == "__main__":
    unittest.main()
