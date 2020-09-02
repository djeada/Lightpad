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

set(project_ui 
    mainwindow.ui
    findreplacepanel.ui)

set(project_headers 
    mainwindow.h
    findreplacepanel.h
    lightpadpage.h
    lightpadsyntaxhighlighter.h
    lightpadtabwidget.h
    textarea.h)

set(project_sources 
    main.cpp
    mainwindow.cpp
    findreplacepanel.cpp
    lightpadpage.cpp
    lightpadsyntaxhighlighter.cpp
    lightpadtabwidget.cpp
    textarea.cpp)

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
