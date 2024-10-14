# This pytest test is executed on the host.
# The script checks the result of the unit test execution on the development board.
# For more information about pytest-embedded, see
#  https://docs.espressif.com/projects/pytest-embedded/en/latest/

from pytest_embedded_idf.dut import IdfDut

def test_app(dut: IdfDut):
    dut.expect_unity_test_output()