"""Verify GET_STATUS through CH340 against the normal production application.

Use --app-path . --build-dir build, not the component-test or echo firmware.
These tests use no MCU wrappers or injected system statistics.
"""

import struct
import time
import zlib

import pytest
import serial
from pytest_embedded_idf import IdfDut

pytestmark = pytest.mark.hardware
HEADER = struct.Struct("<BBHQIHH")


def encode_request(session: int, sequence: int, *, kind=1, payload=b"", bad_crc=False) -> bytes:
    raw = HEADER.pack(1, kind, 0, session, sequence, len(payload), 0) + payload
    raw += struct.pack("<I", zlib.crc32(raw) ^ int(bad_crc))
    encoded = bytearray([0])
    code_index, code = 0, 1
    for byte in raw:
        if byte == 0:
            encoded[code_index] = code
            code_index = len(encoded)
            encoded.append(0)
            code = 1
        else:
            encoded.append(byte)
            code += 1
            if code == 255:
                encoded[code_index] = code
                code_index = len(encoded)
                encoded.append(0)
                code = 1
    encoded[code_index] = code
    return bytes(encoded) + b"\x00"


@pytest.fixture
def connection(dut: IdfDut, request):
    data_port = request.config.getoption("uart_data_port")
    console_port = request.config.getoption("port")
    assert not console_port or data_port.casefold() != console_port.casefold(), "Console and data ports must differ"
    dut.expect_exact("Application link ready", timeout=15)
    with serial.Serial(data_port, 115200, timeout=0.05, write_timeout=3, xonxoff=False, rtscts=False) as port:
        time.sleep(0.3)
        port.reset_input_buffer()
        yield port


def send(port, wire: bytes) -> None:
    offset = 0
    while offset < len(wire):
        count = port.write(wire[offset:])
        assert count is not None and count > 0, "UART write made no progress"
        offset += count
    port.flush()


def receive_status(port, session: int, sequence: int):
    encoded = bytearray()
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        byte = port.read(1)
        if byte == b"\x00":
            break
        encoded.extend(byte)
        assert len(encoded) <= 539, "Oversized response"
    else:
        pytest.fail(f"Response timeout; received {encoded.hex()}")
    assert encoded, "Empty response frame"
    raw = bytearray()
    offset = 0
    while offset < len(encoded):
        code = encoded[offset]
        offset += 1
        assert code != 0 and offset + code - 1 <= len(encoded), "Malformed COBS response"
        raw.extend(encoded[offset : offset + code - 1])
        offset += code - 1
        if code < 255 and offset < len(encoded):
            raw.append(0)
    assert len(raw) == 40, f"Expected 20-byte header, 16-byte payload and CRC; got {len(raw)}"
    assert struct.unpack_from("<I", raw, len(raw) - 4)[0] == zlib.crc32(raw[:-4]), "CRC mismatch"
    version, kind, flags, actual_session, actual_sequence, length, reserved = HEADER.unpack_from(raw)
    assert (version, kind, flags, length, reserved) == (1, 0x81, 0, 16, 0)
    assert actual_session == session, "Session was not preserved"
    assert actual_sequence == sequence, "Sequence was not preserved"
    uptime_ms, free_heap, minimum_heap = struct.unpack_from("<QII", raw, HEADER.size)
    assert 0 < minimum_heap <= free_heap < 4 * 1024 * 1024, "Invalid heap statistics"
    assert uptime_ms < 60000, "Unexpected uptime after fresh firmware boot"
    return uptime_ms, free_heap, minimum_heap


def expect_silence(port):
    deadline = time.monotonic() + 0.2
    while time.monotonic() < deadline:
        assert not port.read(256), "Unexpected or duplicate response"


@pytest.mark.parametrize("session,sequence", [(0, 0), (0x8877665544332211, 0xFEDCBA98), (2**64 - 1, 2**32 - 1)])
def test_status_identifiers(connection, session, sequence):
    send(connection, encode_request(session, sequence))
    receive_status(connection, session, sequence)
    expect_silence(connection)


def test_status_uptime_progresses(connection):
    send(connection, encode_request(123, 1))
    first = receive_status(connection, 123, 1)
    started = time.monotonic()
    time.sleep(0.2)
    send(connection, encode_request(123, 2))
    second = receive_status(connection, 123, 2)
    assert 100 <= second[0] - first[0] <= (time.monotonic() - started) * 1000 + 1000
    assert second[2] <= first[2], "Historical minimum heap increased without a reboot"
    expect_silence(connection)


@pytest.mark.parametrize("invalid", ["unknown-type", "nonempty-payload", "bad-crc", "malformed-cobs"])
def test_status_recovers_after_invalid_input(connection, invalid):
    bad = {
        "unknown-type": encode_request(123, 1, kind=0xFF),
        "nonempty-payload": encode_request(123, 1, payload=b"\x00"),
        "bad-crc": encode_request(123, 1, bad_crc=True),
        "malformed-cobs": b"\x03\x11\x00",
    }[invalid]
    send(connection, bad + encode_request(123, 2))
    receive_status(connection, 123, 2)
    expect_silence(connection)
