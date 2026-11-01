"""Run command worker cases with real FreeRTOS queues and controlled handlers.

Each item boots a fresh image because the production worker has no stop API.
Allocation faults and handler gating exist only in the component-test firmware.
"""

import pytest
from pytest_embedded_idf import IdfDut


@pytest.mark.parametrize(
    "case_name",
    [
        pytest.param("Command rejects invalid submissions and duplicate startup", id="command-arguments"),
        pytest.param("Command copies queued data and retains FIFO order", id="command-copy-order"),
        pytest.param("Command rejects full queues then accepts requests after draining", id="command-full"),
        pytest.param("Command continues processing after handler errors", id="command-errors"),
        pytest.param("Command startup retries after queue allocation failure", id="command-queue-failure"),
        pytest.param("Command startup frees its queue when task allocation fails", id="command-task-failure"),
    ],
)
def test_app_command(dut: IdfDut, case_name: str) -> None:
    assert case_name in {case.name for case in dut.test_menu}, f"Missing Unity case: {case_name}"
    dut.run_single_board_case(case_name)
