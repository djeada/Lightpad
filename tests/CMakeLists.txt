cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(LightpadTests LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Find Qt6 Test module
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Test Network)
find_package(Qt6 QUIET COMPONENTS Pdf PdfWidgets)
find_package(Python3 QUIET COMPONENTS Interpreter)

# Check if PDF support is available (inherit from parent build or detect)
if(DEFINED HAVE_PDF_SUPPORT)
    # Use parent's setting
elseif(Qt6Pdf_FOUND AND Qt6PdfWidgets_FOUND)
    set(HAVE_PDF_SUPPORT TRUE)
else()
    set(HAVE_PDF_SUPPORT FALSE)
endif()

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
include_directories(${CMAKE_SOURCE_DIR}/App/definition)
include_directories(${CMAKE_SOURCE_DIR}/App/test_templates)
include_directories(${CMAKE_SOURCE_DIR}/App/diagnostics)
include_directories(${CMAKE_SOURCE_DIR}/App/language)
include_directories(${CMAKE_SOURCE_DIR}/App/core/editor)

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
    ${CMAKE_SOURCE_DIR}/App/theme/themedefinition.cpp
    ${CMAKE_SOURCE_DIR}/App/theme/themeengine.cpp
    ${CMAKE_SOURCE_DIR}/App/theme/themepresets.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
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
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
    ${CMAKE_SOURCE_DIR}/App/run_templates/runtemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/runtemplateselector.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styleddialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/widgets/pythonenvironmentwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/themedmessagebox.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
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
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
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
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/terminal.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/terminalpty.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/terminalview.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/shellprofile.cpp
    ${CMAKE_SOURCE_DIR}/App/run_templates/runtemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/theme/themeengine.cpp
    ${CMAKE_SOURCE_DIR}/App/theme/themedefinition.cpp
    ${CMAKE_SOURCE_DIR}/App/theme/themepresets.cpp
)

target_include_directories(test_terminal PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/ui/panels
)

target_link_libraries(test_terminal
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

if(UNIX AND NOT APPLE)
    target_link_libraries(test_terminal PRIVATE util)
endif()

target_compile_definitions(test_terminal PRIVATE QT_DEPRECATED_WARNINGS)

# TerminalTabWidget test executable
add_executable(test_terminaltabwidget
    unit/test_terminaltabwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/terminaltabwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/terminal.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/terminalpty.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/terminalview.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/shellprofile.cpp
    ${CMAKE_SOURCE_DIR}/App/run_templates/runtemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    ${CMAKE_SOURCE_DIR}/App/theme/themeengine.cpp
    ${CMAKE_SOURCE_DIR}/App/theme/themedefinition.cpp
    ${CMAKE_SOURCE_DIR}/App/theme/themepresets.cpp
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

if(UNIX AND NOT APPLE)
    target_link_libraries(test_terminaltabwidget PRIVATE util)
endif()

# SyntaxPluginRegistry test executable
add_executable(test_syntaxpluginregistry
    unit/test_syntaxpluginregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/syntaxpluginregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/cppsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/csssyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/dockerfilesyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/glslsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/gosyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/hlslsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/htmlsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/javascriptsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/javasyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/jsonsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/latexsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/markdownsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/metalsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/pythonsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/rustsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/shellsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/typescriptsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/wgslsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/yamlsyntaxplugin.cpp
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

# LatexSyntaxPlugin test executable
add_executable(test_latexsyntaxplugin
    unit/test_latexsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/syntaxpluginregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/latexsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_latexsyntaxplugin PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/syntax
)

target_link_libraries(test_latexsyntaxplugin
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_latexsyntaxplugin PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME LatexSyntaxPluginTests COMMAND test_latexsyntaxplugin)

# DockerfileSyntaxPlugin test executable
add_executable(test_dockerfilesyntaxplugin
    unit/test_dockerfilesyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/syntaxpluginregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/dockerfilesyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_dockerfilesyntaxplugin PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/syntax
)

target_link_libraries(test_dockerfilesyntaxplugin
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_dockerfilesyntaxplugin PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME DockerfileSyntaxPluginTests COMMAND test_dockerfilesyntaxplugin)

# PluginBasedSyntaxHighlighter test executable
add_executable(test_pluginbasedsyntaxhighlighter
    unit/test_pluginbasedsyntaxhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/pluginbasedsyntaxhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/shellsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/pythonsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_pluginbasedsyntaxhighlighter PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/syntax
)

target_link_libraries(test_pluginbasedsyntaxhighlighter
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_pluginbasedsyntaxhighlighter PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME PluginBasedSyntaxHighlighterTests COMMAND test_pluginbasedsyntaxhighlighter)

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

# CompletionEngine test executable
add_executable(test_completionengine
    unit/test_completionengine.cpp
    ${CMAKE_SOURCE_DIR}/App/completion/completionengine.cpp
    ${CMAKE_SOURCE_DIR}/App/completion/completionproviderregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_completionengine PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/completion
)

target_link_libraries(test_completionengine
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_completionengine PRIVATE QT_DEPRECATED_WARNINGS)

# DAP (Debug Adapter Protocol) test executable
add_executable(test_dap
    unit/test_dap.cpp
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/dapclient.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/expressiontranslator.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/debugadapterregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/adapterutils.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/pythonruntime.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/pythondebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/nodedebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/gdbdebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/lldbdebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/godebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/rustdebugadapter.cpp
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
    Qt6::Network
    Qt6::Test
)

target_compile_definitions(test_dap PRIVATE QT_DEPRECATED_WARNINGS)

# Fake DAP adapter used by protocol round-trip tests. Built next to test_dap so
# the tests can locate it via QCoreApplication::applicationDirPath().
add_executable(fakedap_adapter
    unit/fakedapadapter_main.cpp
)

target_link_libraries(fakedap_adapter
    PRIVATE
    Qt6::Core
    Qt6::Network
)

set_target_properties(test_dap fakedap_adapter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin"
)

# Real-adapter integration tests (GDB DAP + debugpy). Skips gracefully when
# the toolchain is unavailable.
add_executable(test_dap_integration
    unit/test_dap_integration.cpp
    ${CMAKE_SOURCE_DIR}/App/build/cmakeproject.cpp
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/dapclient.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/expressiontranslator.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/debugadapterregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/adapterutils.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/pythonruntime.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/pythondebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/nodedebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/gdbdebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/lldbdebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/godebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/adapters/rustdebugadapter.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/breakpointmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/debugconfiguration.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/watchmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/debugsession.cpp
    ${CMAKE_SOURCE_DIR}/App/dap/debugsettings.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_dap_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/dap
)

target_link_libraries(test_dap_integration
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Test
)

target_compile_definitions(test_dap_integration PRIVATE QT_DEPRECATED_WARNINGS)

set_target_properties(test_dap_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin"
)

# Compiler output -> clickable diagnostics.
add_executable(test_compilerdiagnosticparser
    unit/test_compilerdiagnosticparser.cpp
    ${CMAKE_SOURCE_DIR}/App/diagnostics/compilerdiagnosticparser.cpp
)

target_include_directories(test_compilerdiagnosticparser PRIVATE
    ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(test_compilerdiagnosticparser
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
)

target_compile_definitions(test_compilerdiagnosticparser PRIVATE QT_DEPRECATED_WARNINGS)

set_target_properties(test_compilerdiagnosticparser PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin"
)

add_test(NAME CompilerDiagnosticParserTests COMMAND test_compilerdiagnosticparser)

# File explorer filesystem operations: the cases that silently lose data.
add_executable(test_filedirtreemodel
    unit/test_filedirtreemodel.cpp
    ${CMAKE_SOURCE_DIR}/App/filetree/filedirtreemodel.cpp
)

target_include_directories(test_filedirtreemodel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(test_filedirtreemodel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

target_compile_definitions(test_filedirtreemodel PRIVATE QT_DEPRECATED_WARNINGS)

set_target_properties(test_filedirtreemodel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin"
)

add_test(NAME FileDirTreeModelTests COMMAND test_filedirtreemodel)

# Run target resolution: what the Run button promises must be what launches.
qt_add_resources(RUN_TARGET_RESOLVER_RESOURCES ${CMAKE_SOURCE_DIR}/App/run_templates/run_templates.qrc)

add_executable(test_runtargetresolver
    unit/test_runtargetresolver.cpp
    ${CMAKE_SOURCE_DIR}/App/run_templates/runtargetresolver.cpp
    ${CMAKE_SOURCE_DIR}/App/run_templates/runtemplatemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/build/cmakeproject.cpp
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    ${RUN_TARGET_RESOLVER_RESOURCES}
)

target_include_directories(test_runtargetresolver PRIVATE
    ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(test_runtargetresolver
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_runtargetresolver PRIVATE QT_DEPRECATED_WARNINGS)

set_target_properties(test_runtargetresolver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin"
)

add_test(NAME RunTargetResolverTests COMMAND test_runtargetresolver)

# CMake project support (detection, parsing, configure/build round-trip).
add_executable(test_cmakeproject
    unit/test_cmakeproject.cpp
    ${CMAKE_SOURCE_DIR}/App/build/cmakeproject.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_cmakeproject PRIVATE
    ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(test_cmakeproject
    PRIVATE
    Qt6::Core
    Qt6::Test
)

set_target_properties(test_cmakeproject PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin"
)

add_test(NAME CMakeProjectTests COMMAND test_cmakeproject)

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
add_test(NAME TerminalTabWidgetTests COMMAND test_terminaltabwidget)
add_test(NAME SyntaxPluginRegistryTests COMMAND test_syntaxpluginregistry)
add_test(NAME CompletionProviderRegistryTests COMMAND test_completionproviderregistry)
add_test(NAME CompletionEngineTests COMMAND test_completionengine)
add_test(NAME DapTests COMMAND test_dap)
add_test(NAME DapIntegrationTests COMMAND test_dap_integration)
set_tests_properties(DapIntegrationTests PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME DapDebugpySmoke
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/debugpy_smoke_test.py
    )
    # Spawns a real debug adapter and asserts against wall-clock deadlines, so
    # it has to run on its own: sharing the machine with a parallel ctest run
    # makes it fail on timing rather than on behaviour.
    set_tests_properties(DapDebugpySmoke PROPERTIES
        SKIP_RETURN_CODE 2
        LABELS "dap;integration"
        RUN_SERIAL TRUE
        TIMEOUT 300
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    add_test(
        NAME DapGdbSmoke
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/gdb_dap_smoke_test.py
    )
    # Same again: this one also compiles a sample program with gcc before it
    # can start, which is the first thing to slow down under load.
    set_tests_properties(DapGdbSmoke PROPERTIES
        SKIP_RETURN_CODE 2
        LABELS "dap;integration"
        RUN_SERIAL TRUE
        TIMEOUT 300
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

# GitIntegration test executable
add_executable(test_gitintegration
    unit/test_gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
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

# GitRepositoryState test executable
add_executable(test_gitrepositorystate
    unit/test_gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitrepositorystate PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitrepositorystate
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitrepositorystate PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitRepositoryStateTests COMMAND test_gitrepositorystate)

# Repository State Map widget test executable
add_executable(test_repositorystatemapwidget
    unit/test_repositorystatemapwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/widgets/repositorystatemapwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/widgets/flowlayout.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
)

target_include_directories(test_repositorystatemapwidget PRIVATE
    ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(test_repositorystatemapwidget
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_repositorystatemapwidget PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME RepositoryStateMapWidgetTests COMMAND test_repositorystatemapwidget)

# Diff/hunk model: partial staging is only safe if the patches are exact.
add_executable(test_gitdiffmodel
    unit/test_gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitdiffmodel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitdiffmodel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitdiffmodel PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitDiffModelTests COMMAND test_gitdiffmodel)

# GitFileSystemModel test executable
add_executable(test_gitfilesystemmodel
    unit/test_gitfilesystemmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/filetree/gitfilesystemmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitfilesystemmodel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
    ${CMAKE_SOURCE_DIR}/App/filetree
)

target_link_libraries(test_gitfilesystemmodel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitfilesystemmodel PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitFileSystemModelTests COMMAND test_gitfilesystemmodel)

# GitWorkbenchDialog test executable
add_executable(test_gitworkbenchdialog
    unit/test_gitworkbenchdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/gitworkbenchdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/themedmessagebox.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styleddialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitworkbenchdialog PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs
    ${CMAKE_SOURCE_DIR}/App/ui
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_gitworkbenchdialog
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitworkbenchdialog PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitWorkbenchDialogTests COMMAND test_gitworkbenchdialog)

# Staging canvas: the three-state view and its hunk/line staging.
add_executable(test_stagingcanvasdialog
    unit/test_stagingcanvasdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/stagingcanvasdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/themedmessagebox.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styleddialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_stagingcanvasdialog PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs
    ${CMAKE_SOURCE_DIR}/App/ui
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_stagingcanvasdialog
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_stagingcanvasdialog PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME StagingCanvasDialogTests COMMAND test_stagingcanvasdialog)

# Commit graph: topology, WIP node, scope controls and comparison anchors.
add_executable(test_gitgraphwidget
    unit/test_gitgraphwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/widgets/gitgraphwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitgraphwidget PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
    ${CMAKE_SOURCE_DIR}/App/ui
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_gitgraphwidget
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitgraphwidget PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitGraphWidgetTests COMMAND test_gitgraphwidget)

# Commit graph surface: scope controls, inspector and commit actions.
add_executable(test_gitlogdialog
    unit/test_gitlogdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/gitlogdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/themedmessagebox.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styleddialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/operationpreviewdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/widgets/gitgraphwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitlogdialog PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
    ${CMAKE_SOURCE_DIR}/App/ui
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_gitlogdialog
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitlogdialog PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitLogDialogTests COMMAND test_gitlogdialog)

# Compare Anything: the direction logic is what makes a diff mean something.
add_executable(test_gitcomparison
    unit/test_gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitcomparison PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitcomparison
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitcomparison PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitComparisonTests COMMAND test_gitcomparison)

# Compare Anything surface: endpoints, presets, recents and the result view.
add_executable(test_compareanythingdialog
    unit/test_compareanythingdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/compareanythingdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styleddialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_compareanythingdialog PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
    ${CMAKE_SOURCE_DIR}/App/ui
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_compareanythingdialog
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_compareanythingdialog PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME CompareAnythingDialogTests COMMAND test_compareanythingdialog)

# Branch sync: lanes, strategies and the difference between lease and force.
add_executable(test_gitsyncmodel
    unit/test_gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitsyncmodel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitsyncmodel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitsyncmodel PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitSyncModelTests COMMAND test_gitsyncmodel)

# Sync radar surface: lanes, strategy previews and push-mode safety.
add_executable(test_branchsyncradardialog
    unit/test_branchsyncradardialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/branchsyncradardialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/themedmessagebox.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styleddialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/operationpreviewdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_branchsyncradardialog PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
    ${CMAKE_SOURCE_DIR}/App/ui
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_branchsyncradardialog
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_branchsyncradardialog PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME BranchSyncRadarDialogTests COMMAND test_branchsyncradardialog)

# Commit crafting model: buckets, stale assignments and review prompts.
add_executable(test_commitplan
    unit/test_commitplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/commitplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
)

target_include_directories(test_commitplan PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_commitplan
    PRIVATE
    Qt6::Core
    Qt6::Test
)

target_compile_definitions(test_commitplan PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME CommitPlanTests COMMAND test_commitplan)

# Commit crafting workspace: bucket assignment and exact per-bucket commits.
add_executable(test_commitcraftingdialog
    unit/test_commitcraftingdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/commitcraftingdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/themedmessagebox.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styleddialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/commitplan.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_commitcraftingdialog PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
    ${CMAKE_SOURCE_DIR}/App/ui
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_commitcraftingdialog
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_commitcraftingdialog PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME CommitCraftingDialogTests COMMAND test_commitcraftingdialog)

# File evolution timeline: rename following, filters and paging.
add_executable(test_gitfiletimeline
    unit/test_gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitfiletimeline PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitfiletimeline
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitfiletimeline PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitFileTimelineTests COMMAND test_gitfiletimeline)

# Operation preview: consequences worked out without touching the repository.
add_executable(test_gitoperationpreview
    unit/test_gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitoperationpreview PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitoperationpreview
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitoperationpreview PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitOperationPreviewTests COMMAND test_gitoperationpreview)

# Integration advisor: intent to command, with the trade-off spelled out.
add_executable(test_gitintegrationadvice
    unit/test_gitintegrationadvice.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegrationadvice.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitintegrationadvice PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitintegrationadvice
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitintegrationadvice PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitIntegrationAdviceTests COMMAND test_gitintegrationadvice)

# Undo wizard model: seven goals, four layers, and which one is safe.
add_executable(test_gitundoplan
    unit/test_gitundoplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitundoplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitundoplan PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitundoplan
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitundoplan PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitUndoPlanTests COMMAND test_gitundoplan)

# Safe time travel: three intents, and what a detached HEAD really means.
add_executable(test_gittimetravel
    unit/test_gittimetravel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gittimetravel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gittimetravel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gittimetravel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gittimetravel PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitTimeTravelTests COMMAND test_gittimetravel)

# Interactive rebase plan: the todo, the messages and what the result becomes.
add_executable(test_gitrebaseplan
    unit/test_gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitrebaseplan PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitrebaseplan
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitrebaseplan PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitRebasePlanTests COMMAND test_gitrebaseplan)

# Conflict storyboard model: classification and who the two sides really are.
add_executable(test_gitconflictmodel
    unit/test_gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitconflictmodel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitconflictmodel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitconflictmodel PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitConflictModelTests COMMAND test_gitconflictmodel)

# Recovery timeline: grouping reflog noise into events you can act on.
add_executable(test_gitrecovery
    unit/test_gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitrecovery PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitrecovery
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitrecovery PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitRecoveryTests COMMAND test_gitrecovery)

# Stash shelf: what a stash holds, and the apply/pop difference.
add_executable(test_gitstashshelf
    unit/test_gitstashshelf.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitstashshelf.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitstashshelf PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitstashshelf
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitstashshelf PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitStashShelfTests COMMAND test_gitstashshelf)

# Command mirror: what the UI actually runs, with credentials redacted.
add_executable(test_gitcommandlog
    unit/test_gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitstashshelf.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitcommandlog PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitcommandlog
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitcommandlog PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitCommandLogTests COMMAND test_gitcommandlog)

# Branch hygiene: classification relative to a chosen base, and delete safety.
add_executable(test_gitbranchhygiene
    unit/test_gitbranchhygiene.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitbranchhygiene.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitstashshelf.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitbranchhygiene PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitbranchhygiene
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitbranchhygiene PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitBranchHygieneTests COMMAND test_gitbranchhygiene)

# Worktree map: each attached directory with its own branch and its own state.
add_executable(test_gitworktreemap
    unit/test_gitworktreemap.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitworktreemap.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitbranchhygiene.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitstashshelf.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitworktreemap PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitworktreemap
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitworktreemap PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitWorktreeMapTests COMMAND test_gitworktreemap)

# Guided bisect: progress parsing, verdicts and the automated run.
add_executable(test_gitbisect
    unit/test_gitbisect.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitbisect.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitworktreemap.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitbranchhygiene.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitstashshelf.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitbisect PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitbisect
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitbisect PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitBisectTests COMMAND test_gitbisect)

# Provenance lens: what blame says, and what it does not.
add_executable(test_gitprovenance
    unit/test_gitprovenance.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitprovenance.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitbisect.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitworktreemap.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitbranchhygiene.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitstashshelf.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitprovenance PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitprovenance
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitprovenance PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitProvenanceTests COMMAND test_gitprovenance)

# What-if sandbox: graph simulation that never touches the repository.
add_executable(test_gitsandbox
    unit/test_gitsandbox.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitintegration.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrepositorystate.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsandbox.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitprovenance.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitbisect.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitworktreemap.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitbranchhygiene.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcommandlog.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitstashshelf.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitcomparison.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrecovery.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitconflictmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitrebaseplan.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitoperationpreview.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitfiletimeline.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitsyncmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/git/gitdiffmodel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gitsandbox PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/git
)

target_link_libraries(test_gitsandbox
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gitsandbox PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GitSandboxTests COMMAND test_gitsandbox)

# GoToLineDialog test executable
add_executable(test_gotolinedialog
    unit/test_gotolinedialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/gotolinedialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styledpopupdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styleddialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
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

# GoToSymbolDialog test executable
add_executable(test_gotosymboldialog
    unit/test_gotosymboldialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/gotosymboldialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styledpopupdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styleddialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_gotosymboldialog PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs
    ${CMAKE_SOURCE_DIR}/App/lsp
)

target_link_libraries(test_gotosymboldialog
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_gotosymboldialog PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME GoToSymbolDialogTests COMMAND test_gotosymboldialog)

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

# DiagnosticsManager test executable
add_executable(test_diagnosticsmanager
    unit/test_diagnosticsmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/diagnostics/diagnosticsmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_diagnosticsmanager PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/diagnostics
    ${CMAKE_SOURCE_DIR}/App/lsp
)

target_link_libraries(test_diagnosticsmanager
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_diagnosticsmanager PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME DiagnosticsManagerTests COMMAND test_diagnosticsmanager)

# LanguageFeatureManager test executable
add_executable(test_languagefeaturemanager
    unit/test_languagefeaturemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/language/languagefeaturemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/diagnostics/diagnosticsmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/settingsmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/syntaxpluginregistry.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/cppsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/csssyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/dockerfilesyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/glslsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/gosyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/hlslsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/htmlsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/javascriptsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/javasyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/jsonsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/latexsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/markdownsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/metalsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/pythonsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/rustsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/shellsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/typescriptsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/wgslsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/syntax/yamlsyntaxplugin.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_languagefeaturemanager PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/language
    ${CMAKE_SOURCE_DIR}/App/diagnostics
    ${CMAKE_SOURCE_DIR}/App/lsp
    ${CMAKE_SOURCE_DIR}/App/syntax
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_languagefeaturemanager
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_languagefeaturemanager PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME LanguageFeatureManagerTests COMMAND test_languagefeaturemanager)

# RecentFilesManager test executable
add_executable(test_recentfilesmanager
    unit/test_recentfilesmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/recentfilesmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_recentfilesmanager PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/core
)

target_link_libraries(test_recentfilesmanager
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_recentfilesmanager PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME RecentFilesManagerTests COMMAND test_recentfilesmanager)

# NavigationHistory test executable
add_executable(test_navigationhistory
    unit/test_navigationhistory.cpp
    ${CMAKE_SOURCE_DIR}/App/core/navigationhistory.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_navigationhistory PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/core
)

target_link_libraries(test_navigationhistory
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_navigationhistory PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME NavigationHistoryTests COMMAND test_navigationhistory)

# Minimap test executable
add_executable(test_minimap
    unit/test_minimap.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/minimap.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_minimap PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/ui/panels
)

target_link_libraries(test_minimap
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_minimap PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME MinimapTests COMMAND test_minimap)

# SplitEditorContainer test executable
add_executable(test_spliteditorcontainer
    unit/test_spliteditorcontainer.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/spliteditorcontainer.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/spliteditorcontainer.h
    ${CMAKE_SOURCE_DIR}/App/core/lightpadtabwidget.cpp
    ${CMAKE_SOURCE_DIR}/App/core/lightpadtabwidget.h
    ${CMAKE_SOURCE_DIR}/App/core/lightpadpage.h
    ${CMAKE_SOURCE_DIR}/App/ui/mainwindow.h
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_spliteditorcontainer PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/core
    ${CMAKE_SOURCE_DIR}/App/ui
    ${CMAKE_SOURCE_DIR}/App/ui/panels
)

target_link_libraries(test_spliteditorcontainer
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_spliteditorcontainer PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME SplitEditorContainerTests COMMAND test_spliteditorcontainer)

# DockUtils test executable
add_executable(test_dockutils
    unit/test_dockutils.cpp
)

target_include_directories(test_dockutils PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/ui
)

target_link_libraries(test_dockutils
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_dockutils PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME DockUtilsTests COMMAND test_dockutils)

# FindReplacePanel test executable
add_executable(test_findreplacepanel
    unit/test_findreplacepanel.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_findreplacepanel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/ui/panels
)

target_link_libraries(test_findreplacepanel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_findreplacepanel PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME FindReplacePanelTests COMMAND test_findreplacepanel)

# ImageViewer test executable
add_executable(test_imageviewer
    unit/test_imageviewer.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/viewers/imageviewer.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/themedmessagebox.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_imageviewer PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/ui/viewers
)

target_link_libraries(test_imageviewer
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_imageviewer PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME ImageViewerTests COMMAND test_imageviewer)

# MarkdownTools test executable
add_executable(test_markdowntools
    unit/test_markdowntools.cpp
    ${CMAKE_SOURCE_DIR}/App/markdown/markdowntools.cpp
    ${CMAKE_SOURCE_DIR}/App/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_markdowntools PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/markdown
    ${CMAKE_SOURCE_DIR}/App/lsp
)

target_link_libraries(test_markdowntools
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_markdowntools PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME MarkdownToolsTests COMMAND test_markdowntools)

# MarkdownPreviewPanel test executable
add_executable(test_markdownpreviewpanel
    unit/test_markdownpreviewpanel.cpp
    ${CMAKE_SOURCE_DIR}/App/markdown/markdownpreviewpanel.cpp
    ${CMAKE_SOURCE_DIR}/App/markdown/markdowntools.cpp
    ${CMAKE_SOURCE_DIR}/App/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_markdownpreviewpanel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/markdown
    ${CMAKE_SOURCE_DIR}/App/lsp
)

target_link_libraries(test_markdownpreviewpanel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

if(HAVE_WEBENGINE)
    target_link_libraries(test_markdownpreviewpanel PRIVATE Qt6::WebEngineWidgets)
    target_compile_definitions(test_markdownpreviewpanel PRIVATE HAVE_WEBENGINE)
endif()

target_compile_definitions(test_markdownpreviewpanel PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME MarkdownPreviewPanelTests COMMAND test_markdownpreviewpanel)

# LatexTools test executable
add_executable(test_latextools
    unit/test_latextools.cpp
    ${CMAKE_SOURCE_DIR}/App/latex/latextools.cpp
    ${CMAKE_SOURCE_DIR}/App/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_latextools PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/latex
    ${CMAKE_SOURCE_DIR}/App/lsp
)

target_link_libraries(test_latextools
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_latextools PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME LatexToolsTests COMMAND test_latextools)

# LatexPreviewPanel test executable
add_executable(test_latexpreviewpanel
    unit/test_latexpreviewpanel.cpp
    ${CMAKE_SOURCE_DIR}/App/latex/latexpreviewpanel.cpp
    ${CMAKE_SOURCE_DIR}/App/latex/latextools.cpp
    ${CMAKE_SOURCE_DIR}/App/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_latexpreviewpanel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/latex
    ${CMAKE_SOURCE_DIR}/App/lsp
)

target_link_libraries(test_latexpreviewpanel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_latexpreviewpanel PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME LatexPreviewPanelTests COMMAND test_latexpreviewpanel)

# PdfViewer test executable (only if PDF support is available)
if(HAVE_PDF_SUPPORT)
    add_executable(test_pdfviewer
        unit/test_pdfviewer.cpp
        ${CMAKE_SOURCE_DIR}/App/ui/viewers/pdfviewer.cpp
        ${CMAKE_SOURCE_DIR}/App/ui/dialogs/themedmessagebox.cpp
        ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
        ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
        ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    )

    target_include_directories(test_pdfviewer PRIVATE
        ${CMAKE_SOURCE_DIR}/App
        ${CMAKE_SOURCE_DIR}/App/ui/viewers
    )

    target_link_libraries(test_pdfviewer
        PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Pdf
        Qt6::PdfWidgets
        Qt6::Test
    )

    target_compile_definitions(test_pdfviewer PRIVATE QT_DEPRECATED_WARNINGS HAVE_PDF_SUPPORT)

    add_test(NAME PdfViewerTests COMMAND test_pdfviewer)
endif()

# SymbolNavigation test executable
add_executable(test_symbolnavigation
    unit/test_symbolnavigation.cpp
    ${CMAKE_SOURCE_DIR}/App/definition/idefinitionprovider.cpp
    ${CMAKE_SOURCE_DIR}/App/definition/symbolnavigationservice.cpp
    ${CMAKE_SOURCE_DIR}/App/definition/lspdefinitionprovider.cpp
    ${CMAKE_SOURCE_DIR}/App/definition/languagelspdefinitionprovider.cpp
    ${CMAKE_SOURCE_DIR}/App/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_symbolnavigation PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/definition
    ${CMAKE_SOURCE_DIR}/App/lsp
)

target_link_libraries(test_symbolnavigation
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_symbolnavigation PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME SymbolNavigationTests COMMAND test_symbolnavigation)

# TestOutputParsers test executable
qt_add_resources(TEST_TEMPLATE_RESOURCES ${CMAKE_SOURCE_DIR}/App/test_templates/test_templates.qrc)

add_executable(test_testoutputparsers
    unit/test_testoutputparsers.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testconfiguration.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testoutputparser.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testdiscovery.cpp
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    ${TEST_TEMPLATE_RESOURCES}
)

target_include_directories(test_testoutputparsers PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/test_templates
)

target_link_libraries(test_testoutputparsers
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_testoutputparsers PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME TestOutputParsersTests COMMAND test_testoutputparsers)

# AutoTestRunner test executable
add_executable(test_autotestrunner
    unit/test_autotestrunner.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/autotestrunner.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testrunmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testconfiguration.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testoutputparser.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testdiscovery.cpp
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    ${TEST_TEMPLATE_RESOURCES}
)

target_include_directories(test_autotestrunner PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/test_templates
)

target_link_libraries(test_autotestrunner
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_autotestrunner PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME AutoTestRunnerTests COMMAND test_autotestrunner)

# TestFileClassifier test executable
add_executable(test_testfileclassifier
    unit/test_testfileclassifier.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testfileclassifier.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_testfileclassifier PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/test_templates
)

target_link_libraries(test_testfileclassifier
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_testfileclassifier PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME TestFileClassifierTests COMMAND test_testfileclassifier)

# TestRunManager test executable
add_executable(test_testrunmanager
    unit/test_testrunmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testrunmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testconfiguration.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testoutputparser.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testdiscovery.cpp
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    ${TEST_TEMPLATE_RESOURCES}
)

target_include_directories(test_testrunmanager PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/test_templates
)

target_link_libraries(test_testrunmanager
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_testrunmanager PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME TestRunManagerTests COMMAND test_testrunmanager)

# TestPanel test executable
add_executable(test_testpanel
    unit/test_testpanel.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/testpanel.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/testconfigurationdialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/styleddialog.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/dialogs/themedmessagebox.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testrunmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/autotestrunner.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testconfiguration.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testoutputparser.cpp
    ${CMAKE_SOURCE_DIR}/App/test_templates/testdiscovery.cpp
    ${CMAKE_SOURCE_DIR}/App/python/pythonprojectenvironment.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
    ${TEST_TEMPLATE_RESOURCES}
)

target_include_directories(test_testpanel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/test_templates
    ${CMAKE_SOURCE_DIR}/App/ui/panels
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_testpanel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_testpanel PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME TestPanelTests COMMAND test_testpanel)

# ProblemsPanel test executable
add_executable(test_problemspanel
    unit/test_problemspanel.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/panels/problemspanel.cpp
    ${CMAKE_SOURCE_DIR}/App/ui/uistylehelper.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/theme.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_problemspanel PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/ui/panels
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_problemspanel
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_problemspanel PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME ProblemsPanelTests COMMAND test_problemspanel)

# RunConfiguration test executable
add_executable(test_runconfiguration
    unit/test_runconfiguration.cpp
    ${CMAKE_SOURCE_DIR}/App/run_templates/runconfiguration.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_runconfiguration PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/run_templates
)

target_link_libraries(test_runconfiguration
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_runconfiguration PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME RunConfigurationTests COMMAND test_runconfiguration)

# TextTransforms test executable
add_executable(test_texttransforms
    unit/test_texttransforms.cpp
    ${CMAKE_SOURCE_DIR}/App/core/editor/texttransforms.cpp
)

target_include_directories(test_texttransforms PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/core/editor
)

target_link_libraries(test_texttransforms
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_texttransforms PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME TextTransformsTests COMMAND test_texttransforms)

# CodeFolding test executable
add_executable(test_codefolding
    unit/test_codefolding.cpp
    ${CMAKE_SOURCE_DIR}/App/core/editor/codefolding.cpp
)

target_include_directories(test_codefolding PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/core/editor
)

target_link_libraries(test_codefolding
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_codefolding PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME CodeFoldingTests COMMAND test_codefolding)

# MultiCursor test executable
add_executable(test_multicursor
    unit/test_multicursor.cpp
    ${CMAKE_SOURCE_DIR}/App/core/editor/multicursor.cpp
)

target_include_directories(test_multicursor PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/core/editor
)

target_link_libraries(test_multicursor
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_multicursor PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME MultiCursorTests COMMAND test_multicursor)

# DiagnosticsRegression test executable
add_executable(test_diagnosticsregression
    unit/test_diagnosticsregression.cpp
    ${CMAKE_SOURCE_DIR}/App/diagnostics/diagnosticsmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_diagnosticsregression PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/diagnostics
    ${CMAKE_SOURCE_DIR}/App/lsp
)

target_link_libraries(test_diagnosticsregression
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_diagnosticsregression PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME DiagnosticsRegressionTests COMMAND test_diagnosticsregression)

# DocumentRegression test executable
add_executable(test_documentregression
    unit/test_documentregression.cpp
    ${CMAKE_SOURCE_DIR}/App/core/document.cpp
    ${CMAKE_SOURCE_DIR}/App/core/io/filemanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_documentregression PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/core
    ${CMAKE_SOURCE_DIR}/App/core/io
)

target_link_libraries(test_documentregression
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_documentregression PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME DocumentRegressionTests COMMAND test_documentregression)

# SettingsRegression test executable
add_executable(test_settingsregression
    unit/test_settingsregression.cpp
    ${CMAKE_SOURCE_DIR}/App/settings/settingsmanager.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_settingsregression PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/settings
)

target_link_libraries(test_settingsregression
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_settingsregression PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME SettingsRegressionTests COMMAND test_settingsregression)

# LspRegression test executable
add_executable(test_lspregression
    unit/test_lspregression.cpp
    ${CMAKE_SOURCE_DIR}/App/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/App/core/logging/logger.cpp
)

target_include_directories(test_lspregression PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/App/lsp
)

target_link_libraries(test_lspregression
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

target_compile_definitions(test_lspregression PRIVATE QT_DEPRECATED_WARNINGS)

add_test(NAME LspRegressionTests COMMAND test_lspregression)

# Set environment variable for headless testing
set(TEST_LIST
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
    TerminalTabWidgetTests
    SyntaxPluginRegistryTests 
    LatexSyntaxPluginTests
    DockerfileSyntaxPluginTests
    PluginBasedSyntaxHighlighterTests
    CompletionProviderRegistryTests 
    CompletionEngineTests
    DapTests
    DapIntegrationTests
    CMakeProjectTests
    GitIntegrationTests
    GitFileSystemModelTests
    GitWorkbenchDialogTests
    GoToLineDialogTests
    GoToSymbolDialogTests
    LspClientTests
    DiagnosticsManagerTests
    LanguageFeatureManagerTests
    RecentFilesManagerTests
    NavigationHistoryTests
    MinimapTests
    DockUtilsTests
    SplitEditorContainerTests
    FindReplacePanelTests
    ImageViewerTests
    SymbolNavigationTests
    TestOutputParsersTests
    AutoTestRunnerTests
    TestRunManagerTests
    TestPanelTests
    ProblemsPanelTests
    RunConfigurationTests
    TextTransformsTests
    CodeFoldingTests
    MultiCursorTests
    DiagnosticsRegressionTests
    DocumentRegressionTests
    SettingsRegressionTests
    LspRegressionTests
    MarkdownToolsTests
    MarkdownPreviewPanelTests
    LatexToolsTests
    LatexPreviewPanelTests
    TestFileClassifierTests
)

if(HAVE_PDF_SUPPORT)
    list(APPEND TEST_LIST PdfViewerTests)
endif()

set_tests_properties(${TEST_LIST}
    PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Set timeout for terminal tests that may take longer due to shell process management
set_tests_properties(TerminalTabWidgetTests PROPERTIES TIMEOUT 90)

# Set timeout for git tests that may take longer due to git operations
set_tests_properties(GitIntegrationTests PROPERTIES TIMEOUT 30)
set_tests_properties(GitFileSystemModelTests PROPERTIES TIMEOUT 30)
set_tests_properties(GitWorkbenchDialogTests PROPERTIES TIMEOUT 30)

# Custom target to run all tests
set(TEST_TARGETS
    test_logger test_theme test_filemanager test_document test_settingsmanager 
    test_asyncworker test_pluginmanager test_vimmode test_i18n test_accessibility 
    test_runtemplatemanager test_formattemplatemanager test_terminal test_terminaltabwidget
    test_syntaxpluginregistry test_completionproviderregistry test_completionengine test_dap test_dap_integration test_cmakeproject
    test_runtargetresolver test_filedirtreemodel test_compilerdiagnosticparser
    test_dockerfilesyntaxplugin
    test_pluginbasedsyntaxhighlighter
    test_gitintegration test_gitrepositorystate test_gitdiffmodel test_stagingcanvasdialog test_gitgraphwidget test_gitlogdialog test_gitcomparison test_compareanythingdialog test_gitsyncmodel test_branchsyncradardialog test_commitplan test_commitcraftingdialog test_gitfiletimeline test_gitoperationpreview test_gitintegrationadvice test_gitundoplan test_gittimetravel test_gitrebaseplan test_gitconflictmodel test_gitrecovery test_gitstashshelf test_gitcommandlog test_gitbranchhygiene test_gitworktreemap test_gitbisect test_gitprovenance test_gitsandbox test_repositorystatemapwidget test_gitfilesystemmodel test_gitworkbenchdialog test_gotolinedialog test_gotosymboldialog test_lspclient test_diagnosticsmanager test_languagefeaturemanager test_recentfilesmanager test_navigationhistory test_minimap test_findreplacepanel
    test_dockutils
    test_spliteditorcontainer
    test_imageviewer
    test_markdowntools
    test_markdownpreviewpanel
    test_latextools
    test_latexpreviewpanel
    test_symbolnavigation
    test_testoutputparsers
    test_testrunmanager
    test_testpanel
    test_testfileclassifier
    test_problemspanel
    test_texttransforms
    test_codefolding
    test_multicursor
    test_diagnosticsregression
    test_documentregression
    test_settingsregression
    test_lspregression
)

if(HAVE_PDF_SUPPORT)
    list(APPEND TEST_TARGETS test_pdfviewer)
endif()

add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${TEST_TARGETS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
