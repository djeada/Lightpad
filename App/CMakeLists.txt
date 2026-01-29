cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

project(Lightpad LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
 
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)

if(WIN32)
  set(CMAKE_WIN32_EXECUTABLE ON)
endif()

# Core files
set(CORE_SOURCES
    src/core/main.cpp
    src/core/mainwindow.cpp
)
set(CORE_HEADERS
    src/core/mainwindow.h
)
set(CORE_UI
    src/core/mainwindow.ui
)

# Editor files
set(EDITOR_SOURCES
    src/editor/textarea.cpp
    src/editor/lightpadpage.cpp
    src/editor/lightpadsyntaxhighlighter.cpp
    src/editor/lightpadtabwidget.cpp
    src/editor/formatter.cpp
)
set(EDITOR_HEADERS
    src/editor/textarea.h
    src/editor/lightpadpage.h
    src/editor/lightpadsyntaxhighlighter.h
    src/editor/lightpadtabwidget.h
    src/editor/formatter.h
)

# Dialog files
set(DIALOGS_SOURCES
    src/dialogs/findreplacepanel.cpp
    src/dialogs/popup.cpp
    src/dialogs/shortcuts.cpp
    src/dialogs/runconfigurations.cpp
    src/dialogs/terminal.cpp
)
set(DIALOGS_HEADERS
    src/dialogs/findreplacepanel.h
    src/dialogs/popup.h
    src/dialogs/shortcuts.h
    src/dialogs/runconfigurations.h
    src/dialogs/terminal.h
)
set(DIALOGS_UI
    src/dialogs/findreplacepanel.ui
    src/dialogs/shortcuts.ui
    src/dialogs/runconfigurations.ui
    src/dialogs/terminal.ui
)

# Settings files
set(SETTINGS_SOURCES
    src/settings/prefrences.cpp
    src/settings/prefrenceseditor.cpp
    src/settings/prefrencesview.cpp
    src/settings/textareasettings.cpp
    src/settings/theme.cpp
    src/settings/colorpicker.cpp
)
set(SETTINGS_HEADERS
    src/settings/prefrences.h
    src/settings/prefrenceseditor.h
    src/settings/prefrencesview.h
    src/settings/textareasettings.h
    src/settings/theme.h
    src/settings/colorpicker.h
)
set(SETTINGS_UI
    src/settings/prefrences.ui
    src/settings/prefrenceseditor.ui
    src/settings/prefrencesview.ui
    src/settings/colorpicker.ui
)

# Combine all sources
set(project_sources
    ${CORE_SOURCES}
    ${EDITOR_SOURCES}
    ${DIALOGS_SOURCES}
    ${SETTINGS_SOURCES}
)

set(project_headers
    ${CORE_HEADERS}
    ${EDITOR_HEADERS}
    ${DIALOGS_HEADERS}
    ${SETTINGS_HEADERS}
)

set(project_ui
    ${CORE_UI}
    ${DIALOGS_UI}
    ${SETTINGS_UI}
)

# Add include directories for subdirectories
include_directories(
    src/core
    src/editor
    src/dialogs
    src/settings
)

list (APPEND RESOURCES icons.qrc)
list (APPEND RESOURCES highlight.qrc)

qt5_add_resources (RCC_SOURCES ${RESOURCES})
qt5_wrap_ui(project_headers_wrapped ${project_ui})
qt5_wrap_cpp(project_sources_moc ${project_headers})

add_executable(${PROJECT_NAME} ${project_headers} ${project_sources}
    ${project_sources_moc} ${project_headers_wrapped} ${RCC_SOURCES})

target_link_libraries(${PROJECT_NAME} 
    PUBLIC 
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets)
