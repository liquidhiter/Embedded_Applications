"""Exercise the health module's complete one-shot lifecycle on the target."""

from pytest_embedded_idf import IdfDut


def test_app_health_lifecycle(dut: IdfDut) -> None:
    case = "Health task validates its one-shot lifecycle and timer stop request"
    assert case in {entry.name for entry in dut.test_menu}, f"Missing Unity case: {case}"
    dut.run_single_board_case(case, timeout=15)
