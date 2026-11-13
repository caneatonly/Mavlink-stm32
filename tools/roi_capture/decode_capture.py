"""Read raw MAVLink captures saved by a serial assistant; Python standard library only."""

import argparse
from collections import Counter
from pathlib import Path
import re
import struct


CRC_EXTRA = {0: 50, 75: 158, 76: 152, 77: 143}


def x25_crc(data):
    value = 0xFFFF
    for byte in data:
        tmp = byte ^ (value & 0xFF)
        tmp = (tmp ^ (tmp << 4)) & 0xFF
        value = ((value >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF
    return value


def read_hex(text):
    text = text.replace("\ufeff", "").replace("«", "").replace("»", "")
    if not re.fullmatch(r"(?:\s*[0-9a-fA-F]{2})*\s*", text):
        raise ValueError("HEX input must contain only byte pairs and whitespace; remove timestamps and labels")
    return bytes.fromhex(text)


class FrameDecoder:
    """Streaming diagnostic decoder for IDs 0, 75, 76 and 77.

    Other message IDs are skipped using the declared frame length. Their CRCs
    are not checked. Signed frames are delimited, but signatures are NOT verified.
    """

    def __init__(self):
        self.buffer = bytearray()
        self.crc_errors = 0
        self.unknown_frames = 0

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while self.buffer:
            if self.buffer[0] not in (0xFD, 0xFE):
                del self.buffer[0]
                continue
            version = 2 if self.buffer[0] == 0xFD else 1
            header_length = 10 if version == 2 else 6
            if len(self.buffer) < header_length:
                break
            payload_length = self.buffer[1]
            if version == 2 and self.buffer[2] & ~1:
                del self.buffer[0]
                continue
            signed = version == 2 and bool(self.buffer[2] & 1)
            packet_length = header_length + payload_length + 2 + (13 if signed else 0)
            if len(self.buffer) < packet_length:
                break
            msgid = int.from_bytes(self.buffer[7:10], "little") if version == 2 else self.buffer[5]
            if msgid not in CRC_EXTRA:
                self.unknown_frames += 1
                del self.buffer[:packet_length]
                continue
            crc_offset = header_length + payload_length
            expected = x25_crc(self.buffer[1:crc_offset] + bytes([CRC_EXTRA[msgid]]))
            actual = int.from_bytes(self.buffer[crc_offset:crc_offset + 2], "little")
            if expected != actual:
                self.crc_errors += 1
                del self.buffer[0]
                continue
            seq_offset = 4 if version == 2 else 2
            frames.append({
                "version": version, "msgid": msgid,
                "seq": self.buffer[seq_offset],
                "sysid": self.buffer[seq_offset + 1],
                "compid": self.buffer[seq_offset + 2],
                "payload": bytes(self.buffer[header_length:crc_offset]),
                "signed": signed,
            })
            del self.buffer[:packet_length]
        return frames


def describe_frame(frame):
    prefix = (f"MAV{frame['version']} src={frame['sysid']}/{frame['compid']} "
              f"seq={frame['seq']} crc=OK")
    if frame["signed"]:
        prefix += " signature=NOT_VERIFIED"
    payload = frame["payload"]
    msgid = frame["msgid"]
    if msgid == 0:
        custom, kind, autopilot, base, state, _ = struct.unpack("<IBBBBB", payload.ljust(9, b"\0")[:9])
        return (f"{prefix} HEARTBEAT custom_mode={custom} type={kind} "
                f"autopilot={autopilot} base_mode=0x{base:02X} state={state}")
    if msgid == 75:
        fields = struct.unpack("<4fiifH5B", payload.ljust(35, b"\0")[:35])
        p1, _, _, _, lat, lon, alt, command, target_sys, target_comp, reference, _, _ = fields
        return (f"{prefix} COMMAND_INT command={command} target={target_sys}/{target_comp} "
                f"frame={reference} lat={lat / 1e7:.7f} lon={lon / 1e7:.7f} "
                f"alt_m={alt:g} param1={p1:g} lat_e7={lat} lon_e7={lon}")
    if msgid == 76:
        fields = struct.unpack("<7fH3B", payload.ljust(33, b"\0")[:33])
        return (f"{prefix} COMMAND_LONG command={fields[7]} target={fields[8]}/{fields[9]} "
                f"params={fields[:7]}")
    command, result = struct.unpack_from("<HB", payload.ljust(10, b"\0"))
    return f"{prefix} COMMAND_ACK command={command} result={result}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--format", choices=("hex", "bin"), default="hex")
    args = parser.parse_args()
    try:
        data = (read_hex(args.capture.read_text(encoding="utf-8-sig"))
                if args.format == "hex" else args.capture.read_bytes())
    except (OSError, ValueError) as error:
        parser.error(str(error))
    decoder = FrameDecoder()
    frames = decoder.feed(data)
    for frame in frames:
        print(describe_frame(frame))
    print(f"SUMMARY verified_ids={dict(Counter(f['msgid'] for f in frames))} "
          f"bad_crc={decoder.crc_errors} unknown_crc_not_checked={decoder.unknown_frames} "
          f"unfinished_bytes={len(decoder.buffer)}")


if __name__ == "__main__":
    main()
