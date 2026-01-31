cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(LightpadTests LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Find Qt6 Test module
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Test)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include directories for application sources
include_directories(${CMAKE_SOURCE_DIR}/App)
include_directories(${CMAKE_SOURCE_DIR}/App/accessibility)
include_directories(${CMAKE_SOURCE_DIR}/App/completion)
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
include_directories(${CMAKE_SOURCE_DIR}/App/ui/panels)

# Logger test executable
add_executable(test_logger
    unit/test_logger.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_link_libraries(test_logger
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_logger PRIVATE QT_DEPRECATED_WARNINGS)

# Theme test executable
add_executable(test_theme
    unit/test_theme.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
)

target_link_libraries(test_theme
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_accessibility PRIVATE QT_DEPRECATED_WARNINGS)

# RunTemplateManager test executable
# Note: We need to include the qrc resource file so templates are available in tests
qt_add_resources(RUN_TEMPLATE_RESOURCES ${CMAKE_SOURCE_DIR}/App/run_templates/run_templates.qrc)

add_executable(test_runtemplatemanager
    unit/test_runtemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/run_templates/runtemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    ${RUN_TEMPLATE_RESOURCES}
)

target_include_directories(test_runtemplatemanager PRIVATE ${CMAKE_SOURCE_DIR}/App)

target_link_libraries(test_runtemplatemanager
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_runtemplatemanager PRIVATE QT_DEPRECATED_WARNINGS)

# FormatTemplateManager test executable
qt_add_resources(FORMAT_TEMPLATE_RESOURCES ${CMAKE_SOURCE_DIR}/App/format_templates/format_templates.qrc)

add_executable(test_formattemplatemanager
    unit/test_formattemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/format_templates/formattemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    ${FORMAT_TEMPLATE_RESOURCES}
)

target_include_directories(test_formattemplatemanager PRIVATE ${CMAKE_SOURCE_DIR}/App)

target_link_libraries(test_formattemplatemanager
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_formattemplatemanager PRIVATE QT_DEPRECATED_WARNINGS)

# Terminal test executable
add_executable(test_terminal
    unit/test_terminal.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/terminal.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/shellprofile.cpp
    ${CMAKE_SOURCE_DIR}/App/run_templates/runtemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_terminal PRIVATE
    ${CMAKE_SOURCE_DIR}/App/ui/panels
)

target_link_libraries(test_terminal
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_terminal PRIVATE QT_DEPRECATED_WARNINGS)

# TerminalTabWidget test executable
add_executable(test_terminaltabwidget
    unit/test_terminaltabwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/terminaltabwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/terminal.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/shellprofile.cpp
    ${CMAKE_SOURCE_DIR}/App/run_templates/runtemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_terminaltabwidget PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/ui/panels
)

target_link_libraries(test_terminaltabwidget
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

# SyntaxPluginRegistry test executable
add_executable(test_syntaxpluginregistry
    unit/test_syntaxpluginregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/syntaxpluginregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/cppsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/javascriptsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/pythonsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_syntaxpluginregistry PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/syntax
)

target_link_libraries(test_syntaxpluginregistry
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_terminaltabwidget PRIVATE QT_DEPRECATED_WARNINGS)
target_compile_definitions(test_syntaxpluginregistry PRIVATE QT_DEPRECATED_WARNINGS)

# CompletionProviderRegistry test executable
add_executable(test_completionproviderregistry
    unit/test_completionproviderregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/completion/completionproviderregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_completionproviderregistry PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/completion
)

target_link_libraries(test_completionproviderregistry
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_completionproviderregistry PRIVATE QT_DEPRECATED_WARNINGS)

# DAP (Debug Adapter Protocol) test executable
add_executable(test_dap
    unit/test_dap.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/dapclient.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/debugadapterregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/breakpointmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/debugconfiguration.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/watchmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/debugsession.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/debugsettings.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_dap PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/dap
)

target_link_libraries(test_dap
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_dap PRIVATE QT_DEPRECATED_WARNINGS)

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
add_test(NAME FormatTemplateManagerTests COMMAND test_formattemplatemanager)
add_test(NAME TerminalTests COMMAND test_terminal)
add_test(NAME TerminalTabWidgetTests COMMAND test_terminaltabwidget)
add_test(NAME SyntaxPluginRegistryTests COMMAND test_syntaxpluginregistry)
add_test(NAME CompletionProviderRegistryTests COMMAND test_completionproviderregistry)
add_test(NAME DapTests COMMAND test_dap)

# GitIntegration test executable
add_executable(test_gitintegration
    unit/test_gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitintegration PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitintegration
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitintegration PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitIntegrationTests COMMAND test_gitintegration)

# GoToLineDialog test executable
add_executable(test_gotolinedialog
    unit/test_gotolinedialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/gotolinedialog.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gotolinedialog PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs
)

target_link_libraries(test_gotolinedialog
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gotolinedialog PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GoToLineDialogTests COMMAND test_gotolinedialog)

# LspClient test executable
add_executable(test_lspclient
    unit/test_lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_lspclient PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/lsp
)

target_link_libraries(test_lspclient
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_lspclient PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME LspClientTests COMMAND test_lspclient)

# Set environment variable for headless testing
set_tests_properties(
    LoggerTests 
    ThemeTests 
    FileManagerTests 
    DocumentTests 
    SettingsManagerTests 
    AsyncWorkerTests 
    PluginManagerTests 
    VimModeTests 
    I18nTests 
    AccessibilityTests 
    RunTemplateManagerTests 
    FormatTemplateManagerTests 
    TerminalTests 
    TerminalTabWidgetTests
    SyntaxPluginRegistryTests 
    CompletionProviderRegistryTests 
    DapTests 
    GitIntegrationTests
    GoToLineDialogTests
    LspClientTests
    PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Set timeout for terminal tests that may take longer due to shell process management
set_tests_properties(TerminalTests TerminalTabWidgetTests PROPERTIES TIMEOUT 30)

# Set timeout for git tests that may take longer due to git operations
set_tests_properties(GitIntegrationTests PROPERTIES TIMEOUT 30)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_logger test_theme test_filemanager test_document test_settingsmanager 
            test_asyncworker test_pluginmanager test_vimmode test_i18n test_accessibility 
            test_runtemplatemanager test_formattemplatemanager test_terminal test_terminaltabwidget
            test_syntaxpluginregistry test_completionproviderregistry test_dap
            test_gitintegration test_gotolinedialog test_lspclient
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
