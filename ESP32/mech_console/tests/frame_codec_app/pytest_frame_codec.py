"""Run the registered C codec tests on the ESP32-C3 through its USB console."""

import pytest
from pytest_embedded_idf import IdfDut


@pytest.mark.parametrize(
    "case_name",
    [
        pytest.param("COBS encode matches known vectors", id="encode-known-vectors"),
        pytest.param("COBS encode validates pointers and empty input", id="encode-arguments"),
        pytest.param("COBS encode handles 254-byte block boundaries", id="encode-block-boundaries"),
        pytest.param("COBS encode respects capacity and clears failed length", id="encode-capacity"),
        pytest.param("COBS decode matches known vectors", id="decode-known-vectors"),
        pytest.param("COBS decode rejects invalid arguments", id="decode-arguments"),
        pytest.param("COBS decode rejects zero codes and truncated blocks", id="decode-malformed-blocks"),
        pytest.param("COBS decode rejects zero bytes inside data blocks", id="decode-zero-data"),
        pytest.param("COBS decode handles FF blocks without inserting zero", id="decode-full-blocks"),
        pytest.param("COBS decode respects capacity and preserves guards", id="decode-capacity"),
        pytest.param("COBS encode decode round trips deterministic payloads", id="round-trip"),
    ],
)
def test_frame_codec(dut: IdfDut, case_name: str) -> None:
    """Expose each target-side Unity case as an independently selectable pytest item."""
    registered = {case.name for case in dut.test_menu}
    assert case_name in registered, f"Missing Unity case: {case_name}"
    dut.run_single_board_case(case_name)
