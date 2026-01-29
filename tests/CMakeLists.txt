cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

project(LightpadTests LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Find Qt5 Test module
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui Test)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include directories for application sources
include_directories(${CMAKE_SOURCE_DIR}/App)
include_directories(${CMAKE_SOURCE_DIR}/App/core)
include_directories(${CMAKE_SOURCE_DIR}/App/core/logging)
include_directories(${CMAKE_SOURCE_DIR}/App/settings)
include_directories(${CMAKE_SOURCE_DIR}/App/syntax)
include_directories(${CMAKE_SOURCE_DIR}/App/ui)

# Logger test executable
add_executable(test_logger
    unit/test_logger.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_link_libraries(test_logger
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_logger PRIVATE QT_DEPRECATED_WARNINGS)

# Theme test executable
add_executable(test_theme
    unit/test_theme.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
)

target_link_libraries(test_theme
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_theme PRIVATE QT_DEPRECATED_WARNINGS)

# Register tests with CTest
add_test(NAME LoggerTests COMMAND test_logger)
add_test(NAME ThemeTests COMMAND test_theme)

# Set environment variable for headless testing
set_tests_properties(LoggerTests ThemeTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_logger test_theme
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
