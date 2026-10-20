"""Run packet codec Unity cases on the ESP32-C3 or in QEMU."""

import pytest
from pytest_embedded_idf import IdfDut


@pytest.mark.parametrize(
    "case_name",
    [
        pytest.param("Packet encode matches complete wire and header reference", id="packet-golden"),
        pytest.param("Packet encode supports an empty payload", id="packet-empty"),
        pytest.param("Packet encode rejects invalid arguments and oversized payloads", id="packet-arguments"),
        pytest.param("Packet encode reserves delimiter capacity and preserves guards", id="packet-capacity"),
        pytest.param("Packet encode handles maximum payload patterns without mutation", id="packet-max-payload"),
        pytest.param("Packet encode ignores undeclared payload bytes", id="packet-payload-length"),
        pytest.param("Packet decode matches independent wire references", id="decode-golden"),
        pytest.param("Packet decode rejects invalid arguments without mutation", id="decode-arguments"),
        pytest.param("Packet decode rejects malformed COBS and delimiters", id="decode-cobs"),
        pytest.param("Packet decode rejects inconsistent and oversized lengths", id="decode-lengths"),
        pytest.param("Packet decode validates every reserved header byte", id="decode-header"),
        pytest.param("Packet decode rejects corruption across the CRC coverage", id="decode-corruption"),
        pytest.param("Packet round trip covers all payload lengths and preserves input", id="packet-round-trip"),
    ],
)
def test_frame_packet(dut: IdfDut, case_name: str) -> None:
    assert case_name in {case.name for case in dut.test_menu}, f"Missing Unity case: {case_name}"
    dut.run_single_board_case(case_name)
