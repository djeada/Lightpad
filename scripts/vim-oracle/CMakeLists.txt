cmake_minimum_required(VERSION 3.16)
project(vim_oracle_driver CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
get_filename_component(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../App ABSOLUTE)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_executable(driver
    driver.cpp
    ${APP_DIR}/editor/vimmode.h
    ${APP_DIR}/editor/vimmode.cpp
    ${APP_DIR}/editor/vimmode_motions.cpp
    ${APP_DIR}/editor/vimmode_normal.cpp
    ${APP_DIR}/editor/vimmode_visual.cpp
    ${APP_DIR}/editor/vimmode_edit.cpp
    ${APP_DIR}/editor/vimmode_search.cpp
    ${APP_DIR}/editor/vimmode_ex.cpp
    ${APP_DIR}/core/logging/logger.cpp
)
target_include_directories(driver PRIVATE ${APP_DIR})
target_link_libraries(driver Qt6::Widgets)
