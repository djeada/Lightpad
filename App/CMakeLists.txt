set(CMAKE_INCLUDE_CURRENT_DIR ON)
 
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)

if(WIN32)
  set(CMAKE_WIN32_EXECUTABLE ON)
endif()

# Subprocess library is available in libs/ but not currently used by the main application
# add_subdirectory(libs)

set(project_ui 
    ui/dialogs/colorpicker.ui
    ui/panels/findreplacepanel.ui
    ui/mainwindow.ui
    ui/dialogs/preferences.ui
    settings/preferenceseditor.ui
    ui/panels/preferencesview.ui
    ui/dialogs/runconfigurations.ui
    ui/dialogs/shortcuts.ui
    ui/panels/terminal.ui)

set(project_headers 
    ui/dialogs/colorpicker.h
    ui/panels/findreplacepanel.h
    core/async/asyncworker.h
    core/document.h
    core/formatter.h
    core/io/filemanager.h
    core/lightpadpage.h
    core/logging/logger.h
    editor/vimmode.h
    lsp/lspclient.h
    plugins/iplugin.h
    plugins/isyntaxplugin.h
    plugins/pluginmanager.h
    syntax/lightpadsyntaxhighlighter.h
    core/lightpadtabwidget.h
    ui/mainwindow.h
    ui/popup.h
    ui/dialogs/preferences.h
    settings/preferenceseditor.h
    settings/settingsmanager.h
    ui/panels/preferencesview.h
    ui/dialogs/runconfigurations.h
    ui/dialogs/shortcuts.h
    ui/panels/terminal.h
    core/textarea.h
    settings/textareasettings.h
    filetree/filedirtreemodel.h
    filetree/filedirtreecontroller.h
    settings/theme.h)

set(project_sources 
    main.cpp
    ui/dialogs/colorpicker.cpp
    ui/panels/findreplacepanel.cpp
    core/async/asyncworker.cpp
    core/document.cpp
    core/formatter.cpp
    core/io/filemanager.cpp
    core/lightpadpage.cpp
    core/logging/logger.cpp
    editor/vimmode.cpp
    lsp/lspclient.cpp
    plugins/pluginmanager.cpp
    syntax/lightpadsyntaxhighlighter.cpp
    core/lightpadtabwidget.cpp
    ui/mainwindow.cpp
    ui/popup.cpp
    ui/dialogs/preferences.cpp
    settings/preferenceseditor.cpp
    settings/settingsmanager.cpp
    ui/panels/preferencesview.cpp
    ui/dialogs/runconfigurations.cpp
    ui/dialogs/shortcuts.cpp
    ui/panels/terminal.cpp
    core/textarea.cpp
    settings/textareasettings.cpp
    filetree/filedirtreemodel.cpp
    filetree/filedirtreecontroller.cpp
    settings/theme.cpp)

set(project_resources
    highlight.qrc
    icons.qrc
    messages.qrc)

add_executable(Lightpad 
    ${project_headers} 
    ${project_sources}
    ${project_resources})

target_link_libraries(Lightpad 
    PRIVATE 
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets)

# Add QT_DEPRECATED_WARNINGS definition
target_compile_definitions(Lightpad PRIVATE QT_DEPRECATED_WARNINGS)
