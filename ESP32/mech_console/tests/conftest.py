"""Shared physical-UART opt-in option; QEMU tests never require this option."""

import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--uart-data-port", default=None, help="CH340 data port, for example COM10; enables physical UART tests"
    )
    parser.addoption("--uart-stress", action="store_true", help="enable physical UART stress tests")
    parser.addoption("--uart-stress-frames", type=int, default=1000, help="frames per stress profile (1..10000)")


@pytest.fixture(autouse=True)
def require_uart_hardware(request):
    if request.node.get_closest_marker("stress") and not request.config.getoption("uart_stress"):
        pytest.skip("UART stress tests require --uart-stress")
    if request.node.get_closest_marker("hardware") and not request.config.getoption("uart_data_port"):
        pytest.skip("Physical UART test requires --uart-data-port (for example COM10)")
