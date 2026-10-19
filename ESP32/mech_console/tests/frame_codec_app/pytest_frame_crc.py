"""Expose CRC32 Unity cases as independent target-side pytest items."""

import pytest
from pytest_embedded_idf import IdfDut


@pytest.mark.parametrize(
    "case_name",
    [
        pytest.param("CRC32 matches the ISO-HDLC check vector", id="crc-standard-vector"),
        pytest.param("CRC32 accepts empty input and overwrites previous output", id="crc-empty"),
        pytest.param("CRC32 rejects invalid pointers and clears failed output", id="crc-arguments"),
        pytest.param("CRC32 matches independent binary reference vectors", id="crc-binary-vectors"),
        pytest.param("CRC32 handles maximum-sized deterministic data without mutation", id="crc-max-frame"),
        pytest.param("CRC32 detects every single-bit change in the check vector", id="crc-bit-errors"),
    ],
)
def test_frame_crc(dut: IdfDut, case_name: str) -> None:
    assert case_name in {case.name for case in dut.test_menu}, f"Missing Unity case: {case_name}"
    dut.run_single_board_case(case_name)
