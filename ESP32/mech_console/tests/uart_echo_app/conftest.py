"""Options and opt-in guard for the physical UART echo test."""

import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--uart-data-port", default=None, help="CH340 data port, for example COM10; enables physical UART tests"
    )


@pytest.fixture(autouse=True)
def require_uart_hardware(request):
    if not request.config.getoption("uart_data_port"):
        pytest.skip("Physical UART test requires --uart-data-port (for example COM10)")
