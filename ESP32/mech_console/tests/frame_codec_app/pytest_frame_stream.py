"""Run each stream-framer Unity case independently on the target or in QEMU."""

import pytest
from pytest_embedded_idf import IdfDut


@pytest.mark.parametrize(
    "case_name",
    [
        pytest.param("Stream delivers a complete frame without delimiter", id="stream-complete"),
        pytest.param("Stream preserves partial frames across every split", id="stream-splits"),
        pytest.param("Stream handles bytewise input and multiple frames", id="stream-multiple"),
        pytest.param("Stream ignores empty frames and retains trailing partial data", id="stream-empty"),
        pytest.param("Stream accepts exactly the maximum frame length", id="stream-max-length"),
        pytest.param("Stream discards an oversized frame and recovers in the same chunk", id="stream-overflow"),
        pytest.param("Stream preserves discard state across calls", id="stream-discard-state"),
        pytest.param("Stream validates arguments and can be reinitialized", id="stream-arguments"),
    ],
)
def test_frame_stream(dut: IdfDut, case_name: str) -> None:
    assert case_name in {case.name for case in dut.test_menu}, f"Missing Unity case: {case_name}"
    dut.run_single_board_case(case_name)
