import pytest
from pytest_embedded import Dut

def test_app(dut: IdfDut):
    dut.expect_unity_test_output()

# def test_unit_test(dut: Dut, test_name: str) -> None:
#     if test_name == "TEST_ALL":
#         dut.run_all_single_board_cases()
#     elif test_name[:11] == "TEST_GROUP=":
#         dut.run_all_single_board_cases(test_name[12:])
#     else:
#         dut.run_single_board_case(test_name)
