QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MocTest
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

HEADERS     = \
    colorpicker.h \
    findreplacepanel.h \
    lightpadpage.h \
    lightpadsyntaxhighlighter.h \
    lightpadtabwidget.h \
    mainwindow.h \
    prefrences.h \
    prefrencesview.h \
    shortcuts.h \
    textarea.h \
    theme.h

SOURCES     = main.cpp \
      colorpicker.cpp \
      findreplacepanel.cpp \
      lightpadpage.cpp \
      lightpadsyntaxhighlighter.cpp \
      lightpadtabwidget.cpp \
      mainwindow.cpp \
      prefrences.cpp \
      prefrencesview.cpp \
      shortcuts.cpp \
      textarea.cpp

FORMS += \
    colorpicker.ui \
    findreplacepanel.ui \
    mainwindow.ui \
    prefrences.ui \
    prefrencesview.ui \
    shortcuts.ui

RESOURCES += \
    highlight.qrc \
    icons.qrc \
    messages.qrc
