"""Verify RGB lifecycle on target/QEMU and real RMT transmission on the board."""

import pytest
from pytest_embedded_idf import IdfDut


@pytest.mark.parametrize(
    "case_name",
    [
        pytest.param("RGB preserves creation errors and configures the board LED", id="rgb-create-config"),
        pytest.param("RGB preserves clear errors and retries after successful cleanup", id="rgb-clear-retry"),
        pytest.param("RGB blocks reinitialization after cleanup failure", id="rgb-cleanup-failure"),
        pytest.param("RGB sends channel values and propagates update failures", id="rgb-color-errors"),
        pytest.param("RGB hardware transmits primary colors and turns off", id="rgb-hardware"),
    ],
)
def test_app_rgb(dut: IdfDut, request, case_name: str) -> None:
    if case_name.startswith("RGB hardware") and "qemu" in request.config.getoption("embedded_services"):
        pytest.skip("Physical RGB transmission requires a board with RMT")
    assert case_name in {case.name for case in dut.test_menu}, f"Missing Unity case: {case_name}"
    dut.run_single_board_case(case_name)
