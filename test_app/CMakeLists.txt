# This is the project CMakeLists.txt file for the test subproject
cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "../components/")
list(APPEND EXTRA_COMPONENT_DIRS "mocks")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_app)