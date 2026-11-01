"""Run protocol dispatch and response serialization Unity cases on target/QEMU.

Test-only wrappers capture sends and supply deterministic status values. Wrappers
delegate to real APIs outside these calls so existing link cases remain valid.
"""

import pytest
from pytest_embedded_idf import IdfDut


@pytest.mark.parametrize(
    "case_name",
    [
        pytest.param("Protocol rejects null requests without sending", id="protocol-null"),
        pytest.param("Protocol rejects unknown types and dispatch table holes", id="protocol-types"),
        pytest.param("Protocol rejects malformed command envelopes without sending", id="protocol-payload"),
        pytest.param("Protocol status uses the exact little endian payload layout", id="protocol-layout"),
        pytest.param("Protocol status preserves zero and maximum request identifiers", id="protocol-identifiers"),
        pytest.param("Protocol propagates send failures without retrying", id="protocol-send-errors"),
        pytest.param("Protocol returns unsupported for unknown opcodes", id="protocol-unsupported"),
        pytest.param("Protocol returns invalid for GET_STATUS arguments", id="protocol-invalid-args"),
    ],
)
def test_app_protocol(dut: IdfDut, case_name: str) -> None:
    assert case_name in {case.name for case in dut.test_menu}, f"Missing Unity case: {case_name}"
    dut.run_single_board_case(case_name)
