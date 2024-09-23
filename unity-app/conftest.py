# def pytest_addoption(parser):
#     parser.addoption("--test-name", action="store", default="test-name")


def pytest_generate_tests(metafunc):
    # This is called for every test. Only get/set command line arguments
    # if the argument is specified in the list of test "fixturenames".
    option_value = metafunc.config.option.test_name
    if 'test_name' in metafunc.fixturenames and option_value is not None:
        metafunc.parametrize("test_name", [option_value])
