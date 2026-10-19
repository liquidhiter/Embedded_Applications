"""Validate the real UART data path independently of the USB console."""

import random
import time

import pytest
import serial
from pytest_embedded_idf import IdfDut

from tools.uart_echo_test import EchoFailure, exchange

pytestmark = pytest.mark.hardware


@pytest.fixture
def data_serial(dut: IdfDut, request):
    data_port = request.config.getoption("uart_data_port")
    console_port = request.config.getoption("port")
    if console_port and data_port.casefold() == console_port.casefold():
        pytest.fail("The UART data port must differ from the ESP32 USB console port")
    dut.expect_exact("UART_ECHO_READY", timeout=15)
    with serial.Serial(
        data_port, baudrate=115200, timeout=0.1, write_timeout=5, xonxoff=False, rtscts=False, dsrdtr=False
    ) as connection:
        time.sleep(0.3)
        connection.reset_input_buffer()
        yield connection


@pytest.mark.parametrize("length", [1, 255, 256, 257, 1024, 4096], ids=lambda length: f"echo-{length}-bytes")
def test_uart_echo(data_serial, length, record_property):
    rng = random.Random(length)
    durations = []
    for repeat in range(10):
        payload = bytearray(rng.getrandbits(8) for _ in range(length))
        if length >= 2:
            payload[0], payload[-1] = 0, 0xFF
        try:
            durations.append(exchange(data_serial, bytes(payload), timeout=5, quiet=0.02))
        except EchoFailure as error:
            pytest.fail(
                f"Round {repeat + 1}: {error}; expected={length}, "
                f"written={error.written}, received={len(error.received)}, "
                f"elapsed={error.elapsed:.3f}s"
            )
    record_property("payload_bytes", length)
    record_property("rounds", 10)
    record_property("mean_echo_rtt_ms", sum(durations) / len(durations) * 1000)
