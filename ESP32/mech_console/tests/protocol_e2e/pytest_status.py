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


def encode_request(
    session: int, request_id: int, *, kind=0x0A, args=b"", opcode=1, payload=None, sequence=0, bad_crc=False
) -> bytes:
    if payload is None:
        payload = struct.pack("<IHH", request_id, opcode, len(args)) + args
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


def receive_response(port, session: int, request_id: int, status=0):
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
    result_length = 16 if status == 0 else 0
    assert len(raw) == 32 + result_length, f"Unexpected response length: {len(raw)}"
    assert struct.unpack_from("<I", raw, len(raw) - 4)[0] == zlib.crc32(raw[:-4]), "CRC mismatch"
    version, kind, flags, actual_session, sequence, length, reserved = HEADER.unpack_from(raw)
    assert (version, kind, flags, sequence, length, reserved) == (1, 0x0B, 0, 0, 8 + result_length, 0)
    assert actual_session == session, "Session was not preserved"
    assert struct.unpack_from("<IHH", raw, HEADER.size) == (request_id, status, result_length)
    if status != 0:
        return None
    uptime_ms, free_heap, minimum_heap = struct.unpack_from("<QII", raw, HEADER.size + 8)
    assert 0 < minimum_heap <= free_heap < 4 * 1024 * 1024, "Invalid heap statistics"
    assert uptime_ms < 60000, "Unexpected uptime after fresh firmware boot"
    return uptime_ms, free_heap, minimum_heap


def expect_silence(port):
    deadline = time.monotonic() + 0.2
    while time.monotonic() < deadline:
        assert not port.read(256), "Unexpected or duplicate response"


@pytest.mark.parametrize("session,request_id", [(0, 0), (0x8877665544332211, 0xFEDCBA98), (2**64 - 1, 2**32 - 1)])
def test_status_identifiers(connection, session, request_id):
    send(connection, encode_request(session, request_id))
    receive_response(connection, session, request_id)
    expect_silence(connection)


def test_status_uptime_progresses(connection):
    send(connection, encode_request(123, 1))
    first = receive_response(connection, 123, 1)
    started = time.monotonic()
    time.sleep(0.2)
    send(connection, encode_request(123, 2))
    second = receive_response(connection, 123, 2)
    assert 100 <= second[0] - first[0] <= (time.monotonic() - started) * 1000 + 1000
    assert second[2] <= first[2], "Historical minimum heap increased without a reboot"
    expect_silence(connection)


@pytest.mark.parametrize(
    "invalid",
    [
        "unknown-type",
        "old-status-type",
        "short-envelope",
        "length-mismatch",
        "oversized-envelope",
        "nonzero-sequence",
        "bad-crc",
        "malformed-cobs",
    ],
)
def test_status_recovers_after_invalid_input(connection, invalid):
    bad = {
        "unknown-type": encode_request(123, 1, kind=0xFF),
        "old-status-type": encode_request(123, 1, kind=1),
        "short-envelope": encode_request(123, 1, payload=b"\x00"),
        "length-mismatch": encode_request(123, 1, payload=struct.pack("<IHH", 1, 1, 1)),
        "oversized-envelope": encode_request(123, 1, args=bytes(57)),
        "nonzero-sequence": encode_request(123, 1, sequence=1),
        "bad-crc": encode_request(123, 1, bad_crc=True),
        "malformed-cobs": b"\x03\x11\x00",
    }[invalid]
    send(connection, bad + encode_request(123, 2))
    receive_response(connection, 123, 2)
    expect_silence(connection)


@pytest.mark.parametrize(
    "opcode,args,status", [(0, b"", 4), (2, bytes(56), 4), (65535, b"", 4), (1, b"x", 1), (1, bytes(56), 1)]
)
def test_command_error_response(connection, opcode, args, status):
    send(connection, encode_request(123, 1, opcode=opcode, args=args))
    receive_response(connection, 123, 1, status)
    send(connection, encode_request(123, 2))
    receive_response(connection, 123, 2)
    expect_silence(connection)
