QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MocTest
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

HEADERS     = \
    colorpicker.h \
    findreplacepanel.h \
    formatter.h \
    lightpadpage.h \
    lightpadsyntaxhighlighter.h \
    lightpadtabwidget.h \
    mainwindow.h \
    popup.h \
    prefrences.h \
    prefrenceseditor.h \
    prefrencesview.h \
    runconfigurations.h \
    shortcuts.h \
    terminal.h \
    textarea.h \
    textareasettings.h \
    theme.h

SOURCES     = main.cpp \
      colorpicker.cpp \
      findreplacepanel.cpp \
      formatter.cpp \
      lightpadpage.cpp \
      lightpadsyntaxhighlighter.cpp \
      lightpadtabwidget.cpp \
      mainwindow.cpp \
      popup.cpp \
      prefrences.cpp \
      prefrenceseditor.cpp \
      prefrencesview.cpp \
      runconfigurations.cpp \
      shortcuts.cpp \
      terminal.cpp \
      textarea.cpp \
      textareasettings.cpp \
      theme.cpp

FORMS += \
    colorpicker.ui \
    findreplacepanel.ui \
    mainwindow.ui \
    prefrences.ui \
    prefrenceseditor.ui \
    prefrencesview.ui \
    runconfigurations.ui \
    shortcuts.ui \
    terminal.ui

RESOURCES += \
    highlight.qrc \
    icons.qrc \
    messages.qrc
