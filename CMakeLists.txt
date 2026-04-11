cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(Lightpad VERSION 0.1 LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to build tests
option(BUILD_TESTS "Build the test suite" ON)

# Add the App subdirectory
add_subdirectory(App)

# Add tests subdirectory if enabled
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
