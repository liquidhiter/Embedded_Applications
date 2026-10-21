"""Test the physical CH340-to-UART1 packet path using independent Python frames.

The ESP32 runs the real app_link receive task and a test-only packet responder.
COM8 is used for firmware/console; COM10 carries binary protocol traffic.
"""

import struct
import threading
import time
import zlib

import pytest
import serial
from pytest_embedded_idf import IdfDut

pytestmark = pytest.mark.hardware


def cobs_encode(raw: bytes) -> bytes:
    encoded = bytearray([0])
    code_index, code = 0, 1
    for value in raw:
        if value == 0:
            encoded[code_index] = code
            code_index = len(encoded)
            encoded.append(0)
            code = 1
        else:
            encoded.append(value)
            code += 1
            if code == 255:
                encoded[code_index] = code
                code_index = len(encoded)
                encoded.append(0)
                code = 1
    encoded[code_index] = code
    return bytes(encoded)


def make_frame(payload: bytes, sequence: int, *, bad_crc: bool = False) -> bytes:
    header = struct.pack("<BBHQIHH", 1, 0x20, 0, 0x8877665544332211, sequence, len(payload), 0)
    raw = header + payload
    crc = zlib.crc32(raw) ^ int(bad_crc)
    return cobs_encode(raw + struct.pack("<I", crc)) + b"\x00"


@pytest.fixture
def data_serial(dut: IdfDut, request):
    data_port = request.config.getoption("uart_data_port")
    console_port = request.config.getoption("port")
    if console_port and data_port.casefold() == console_port.casefold():
        pytest.fail("The UART data port must differ from the USB console port")
    dut.expect_exact("UART_LINK_READY", timeout=15)
    with serial.Serial(
        data_port, baudrate=115200, timeout=0.05, write_timeout=3, xonxoff=False, rtscts=False, dsrdtr=False
    ) as connection:
        time.sleep(0.3)
        connection.reset_input_buffer()
        yield connection


def send(connection, data: bytes) -> None:
    offset = 0
    while offset < len(data):
        written = connection.write(data[offset:])
        assert written is not None and written > 0, "UART write made no progress"
        offset += written
    connection.flush()


def expect_reply(connection, expected: bytes) -> None:
    received = bytearray()
    deadline = time.monotonic() + 5
    while len(received) < len(expected) and time.monotonic() < deadline:
        received.extend(connection.read(len(expected) - len(received)))
    assert bytes(received) == expected, f"Reply mismatch: expected={expected.hex()}, received={received.hex()}"
    # Detect unintended replies to rejected frames, duplicates, and extra bytes.
    unexpected = bytearray()
    deadline = time.monotonic() + 0.2
    while time.monotonic() < deadline:
        unexpected.extend(connection.read(1024))
    assert not unexpected, f"Unexpected trailing data: {unexpected.hex()}"


@pytest.mark.parametrize(
    "scenario", ["empty", "binary", "maximum", "fragmented", "multiple", "bad-crc", "malformed", "oversized"]
)
def test_uart_link(data_serial, scenario: str) -> None:
    payload = bytes([0x11, 0, 0x22, 0xFF])
    good = make_frame(payload, 0xFEDCBA98)
    if scenario == "empty":
        expected = make_frame(b"", 0)
        send(data_serial, expected)
    elif scenario == "binary":
        expected = make_frame(bytes(range(256)), 1)
        send(data_serial, expected)
    elif scenario == "maximum":
        expected = make_frame(bytes(range(256)) * 2, 0xFFFFFFFF)
        send(data_serial, expected)
    elif scenario == "fragmented":
        expected = good
        send(data_serial, good[:2])
        time.sleep(0.25)
        assert not data_serial.read(1), "An incomplete packet must not trigger a reply"
        for value in good[2:-1]:
            send(data_serial, bytes([value]))
            time.sleep(0.002)
        assert not data_serial.read(1), "A packet without its delimiter must not trigger a reply"
        send(data_serial, good[-1:])
    elif scenario == "multiple":
        expected = make_frame(b"", 2) + good + make_frame(b"\x00\xff", 3)
        send(data_serial, expected)
    else:
        expected = good
        prefix = {
            "bad-crc": make_frame(payload, 4, bad_crc=True),
            "malformed": b"\x00\x00\x03\x11\x00",
            "oversized": b"\x01" * 600 + b"\x00",
        }[scenario]
        send(data_serial, prefix + good)
    expect_reply(data_serial, expected)


@pytest.mark.stress
@pytest.mark.parametrize("profile", ["full-duplex", "recovery"])
def test_uart_link_stress(data_serial, request, record_property, profile: str) -> None:
    count = request.config.getoption("uart_stress_frames")
    assert 1 <= count <= 10000, "--uart-stress-frames must be in 1..10000"
    inputs, replies = [], []
    payload_bytes = 0
    for sequence in range(count):
        length = 64 if profile == "recovery" and sequence % 5 == 0 else 512
        # Alternate zero, nonzero and mixed payloads; every valid frame has a unique sequence.
        payload = [b"\x00" * length, b"\xff" * length, (bytes(range(256)) * 2)[:length]][sequence % 3]
        if profile == "recovery":
            if sequence % 17 == 0:
                inputs.append(make_frame(b"bad", sequence, bad_crc=True))
            if sequence % 50 == 0:
                inputs.append(b"\x01" * 600 + b"\x00")
        frame = make_frame(payload, sequence)
        inputs.append(frame)
        replies.append(frame)
        payload_bytes += length
    outgoing, expected = b"".join(inputs), b"".join(replies)
    received = bytearray()
    writer_errors = []
    stop = threading.Event()

    def write_stream():
        try:
            offset = 0
            # No deliberate pacing: feed the adapter while the main thread drains replies.
            while offset < len(outgoing) and not stop.is_set():
                written = data_serial.write(outgoing[offset : offset + 4096])
                if written is None or written <= 0:
                    raise RuntimeError("UART writer made no progress")
                offset += written
            if not stop.is_set():
                data_serial.flush()
        except Exception as error:
            writer_errors.append(error)

    started = time.monotonic()
    # 8N1 at 115200 baud: 10 bits per byte, with headroom for USB and OS scheduling.
    deadline = started + max(len(outgoing), len(expected)) * 10 / 115200 * 3 + 10
    writer = threading.Thread(target=write_stream, name="uart-stress-writer", daemon=True)
    writer.start()
    try:
        while len(received) < len(expected) and time.monotonic() < deadline:
            received.extend(data_serial.read(min(4096, len(expected) - len(received))))
            if writer_errors:
                break
    finally:
        stop.set()
        writer.join(timeout=4)
        if writer.is_alive():
            data_serial.cancel_write()
            writer.join(timeout=4)
        assert not writer.is_alive(), "UART writer did not stop"
    elapsed = time.monotonic() - started
    assert not writer_errors, f"UART writer failed: {writer_errors}"
    assert len(received) == len(expected), (
        f"Missing replies: expected={len(expected)} bytes, received={len(received)}, elapsed={elapsed:.3f}s"
    )
    mismatch = next((i for i, (actual, wanted) in enumerate(zip(received, expected)) if actual != wanted), None)
    assert mismatch is None, (
        f"Reply stream differs at byte {mismatch}: possible corruption, loss, duplication or reordering"
    )
    expect_reply(data_serial, b"")
    record_property("frames", count)
    record_property("tx_wire_bytes", len(outgoing))
    record_property("rx_wire_bytes", len(received))
    record_property("elapsed_s", elapsed)
    record_property("payload_bytes_per_s", payload_bytes / elapsed)
    record_property("rx_wire_bytes_per_s", len(received) / elapsed)
    print(
        f"\nUART stress {profile}: frames={count}, elapsed={elapsed:.3f}s, "
        f"RX wire={len(received) / elapsed:.1f} B/s, payload={payload_bytes / elapsed:.1f} B/s"
    )
