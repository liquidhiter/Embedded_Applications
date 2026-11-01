"""Run HELLO/ACK state, wire-layout and failure-atomicity cases on target/QEMU."""

import pytest
from pytest_embedded_idf import IdfDut


@pytest.mark.parametrize(
    "case_name",
    [
        pytest.param("Session init validates arguments and preserves failed output", id="session-init"),
        pytest.param("Session rejects null HELLO arguments without side effects", id="session-null"),
        pytest.param("Session HELLO produces the exact ACK layout", id="session-ack-layout"),
        pytest.param("Session accepts nonce and negotiation boundaries", id="session-boundaries"),
        pytest.param("Session rejects malformed HELLO in DOWN and HANDSHAKE", id="session-invalid"),
        pytest.param("Session repeats ACK without generating another ID", id="session-retry"),
        pytest.param("Session rejects conflicting HELLO and disallowed states", id="session-state-conflicts"),
        pytest.param("Session preserves outputs when generation fails then retries", id="session-id-failure"),
        pytest.param("Session instances keep independent IDs and callback contexts", id="session-instances"),
    ],
)
def test_app_session(dut: IdfDut, case_name: str) -> None:
    assert case_name in {case.name for case in dut.test_menu}, f"Missing Unity case: {case_name}"
    dut.run_single_board_case(case_name)
