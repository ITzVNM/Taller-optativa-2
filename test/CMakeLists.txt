cmake_minimum_required(VERSION 3.22)

# Build the soil_sensor component and its test/ subcomponent in a dedicated
# application, keeping tests out of the production firmware.
set(TEST_COMPONENTS soil_sensor)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(soil_rs485_tests)
