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
include_directories(${CMAKE_SOURCE_DIR}/App/accessibility)
include_directories(${CMAKE_SOURCE_DIR}/App/core)
include_directories(${CMAKE_SOURCE_DIR}/App/core/async)
include_directories(${CMAKE_SOURCE_DIR}/App/core/io)
include_directories(${CMAKE_SOURCE_DIR}/App/core/logging)
include_directories(${CMAKE_SOURCE_DIR}/App/editor)
include_directories(${CMAKE_SOURCE_DIR}/App/i18n)
include_directories(${CMAKE_SOURCE_DIR}/App/lsp)
include_directories(${CMAKE_SOURCE_DIR}/App/plugins)
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

# FileManager test executable
add_executable(test_filemanager
    unit/test_filemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/io/filemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_link_libraries(test_filemanager
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_filemanager PRIVATE QT_DEPRECATED_WARNINGS)

# Document test executable
add_executable(test_document
    unit/test_document.cpp
    ${CMAKE_SOURCE_DIR}/App/core/document.cpp
    ${CMAKE_SOURCE_DIR}/App/core/io/filemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_link_libraries(test_document
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_document PRIVATE QT_DEPRECATED_WARNINGS)

# SettingsManager test executable
add_executable(test_settingsmanager
    unit/test_settingsmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/settingsmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_link_libraries(test_settingsmanager
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_settingsmanager PRIVATE QT_DEPRECATED_WARNINGS)

# AsyncWorker test executable
add_executable(test_asyncworker
    unit/test_asyncworker.cpp
    ${CMAKE_SOURCE_DIR}/App/core/async/asyncworker.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_link_libraries(test_asyncworker
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_asyncworker PRIVATE QT_DEPRECATED_WARNINGS)

# PluginManager test executable
add_executable(test_pluginmanager
    unit/test_pluginmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/plugins/pluginmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_link_libraries(test_pluginmanager
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_pluginmanager PRIVATE QT_DEPRECATED_WARNINGS)

# VimMode test executable
add_executable(test_vimmode
    unit/test_vimmode.cpp
    ${CMAKE_SOURCE_DIR}/App/editor/vimmode.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_link_libraries(test_vimmode
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_vimmode PRIVATE QT_DEPRECATED_WARNINGS)

# I18n test executable
add_executable(test_i18n
    unit/test_i18n.cpp
    ${CMAKE_SOURCE_DIR}/App/i18n/i18n.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_link_libraries(test_i18n
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_i18n PRIVATE QT_DEPRECATED_WARNINGS)

# Accessibility test executable
add_executable(test_accessibility
    unit/test_accessibility.cpp
    ${CMAKE_SOURCE_DIR}/App/accessibility/accessibilitymanager.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/settingsmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_link_libraries(test_accessibility
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_accessibility PRIVATE QT_DEPRECATED_WARNINGS)

# RunTemplateManager test executable
# Note: We need to include the qrc resource file so templates are available in tests
qt5_add_resources(RUN_TEMPLATE_RESOURCES ${CMAKE_SOURCE_DIR}/App/run_templates/run_templates.qrc)

add_executable(test_runtemplatemanager
    unit/test_runtemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/run_templates/runtemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    ${RUN_TEMPLATE_RESOURCES}
)

target_include_directories(test_runtemplatemanager PRIVATE ${CMAKE_SOURCE_DIR}/App)

target_link_libraries(test_runtemplatemanager
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
)

target_compile_definitions(test_runtemplatemanager PRIVATE QT_DEPRECATED_WARNINGS)

# Register tests with CTest
add_test(NAME LoggerTests COMMAND test_logger)
add_test(NAME ThemeTests COMMAND test_theme)
add_test(NAME FileManagerTests COMMAND test_filemanager)
add_test(NAME DocumentTests COMMAND test_document)
add_test(NAME SettingsManagerTests COMMAND test_settingsmanager)
add_test(NAME AsyncWorkerTests COMMAND test_asyncworker)
add_test(NAME PluginManagerTests COMMAND test_pluginmanager)
add_test(NAME VimModeTests COMMAND test_vimmode)
add_test(NAME I18nTests COMMAND test_i18n)
add_test(NAME AccessibilityTests COMMAND test_accessibility)
add_test(NAME RunTemplateManagerTests COMMAND test_runtemplatemanager)

# Set environment variable for headless testing
set_tests_properties(LoggerTests ThemeTests FileManagerTests DocumentTests SettingsManagerTests 
    AsyncWorkerTests PluginManagerTests VimModeTests I18nTests AccessibilityTests RunTemplateManagerTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_logger test_theme test_filemanager test_document test_settingsmanager 
            test_asyncworker test_pluginmanager test_vimmode test_i18n test_accessibility test_runtemplatemanager
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
