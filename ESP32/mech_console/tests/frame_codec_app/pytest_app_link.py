"""Exercise the real link task with queued UART1 reads on target or QEMU.

The test image wraps uart_read_bytes for UART1 only; this is not a physical
UART test. A fresh boot per pytest item isolates the link's one-start lifecycle.
"""

import pytest
from pytest_embedded_idf import IdfDut


@pytest.mark.parametrize(
    "case_name",
    [
        pytest.param("Link rejects null and duplicate starts", id="link-start"),
        pytest.param("Link preserves partial frames across reads and timeouts", id="link-fragments"),
        pytest.param("Link delivers multiple frames from one read in order", id="link-multiple"),
        pytest.param("Link drops invalid packets and resumes at the next frame", id="link-malformed"),
        pytest.param("Link discards oversized frames then delivers a maximum packet", id="link-overflow"),
        pytest.param("Link retries after a UART read failure", id="link-read-error"),
    ],
)
def test_app_link(dut: IdfDut, case_name: str) -> None:
    assert case_name in {case.name for case in dut.test_menu}, f"Missing Unity case: {case_name}"
    dut.run_single_board_case(case_name)
