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
    colorpicker.ui
    findreplacepanel.ui
    mainwindow.ui
    prefrences.ui
    prefrenceseditor.ui
    prefrencesview.ui
    runconfigurations.ui
    shortcuts.ui
    terminal.ui)

set(project_headers 
    colorpicker.h
    findreplacepanel.h
    formatter.h
    lightpadpage.h
    lightpadsyntaxhighlighter.h
    lightpadtabwidget.h
    mainwindow.h
    popup.h
    prefrences.h
    prefrenceseditor.h
    prefrencesview.h
    runconfigurations.h
    shortcuts.h
    terminal.h
    textarea.h
    textareasettings.h
    filedirtreemodel.h
    filedirtreecontroller.h)
    theme.h)

set(project_sources 
    main.cpp
    colorpicker.cpp
    findreplacepanel.cpp
    formatter.cpp
    lightpadpage.cpp
    lightpadsyntaxhighlighter.cpp
    lightpadtabwidget.cpp
    mainwindow.cpp
    popup.cpp
    prefrences.cpp
    prefrenceseditor.cpp
    prefrencesview.cpp
    runconfigurations.cpp
    shortcuts.cpp
    terminal.cpp
    textarea.cpp
    textareasettings.cpp
    filedirtreemodel.cpp
    filedirtreecontroller.cpp)
    theme.cpp)

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
