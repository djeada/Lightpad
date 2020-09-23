QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MocTest
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

HEADERS     = \
    findreplacepanel.h \
    lightpadpage.h \
    lightpadsyntaxhighlighter.h \
    lightpadtabwidget.h \
    mainwindow.h \
    textarea.h \
    theme.h

SOURCES     = main.cpp \
      findreplacepanel.cpp \
      lightpadpage.cpp \
      lightpadsyntaxhighlighter.cpp \
      lightpadtabwidget.cpp \
      mainwindow.cpp \
      textarea.cpp \
      theme.cpp

FORMS += \
    findreplacepanel.ui \
    mainwindow.ui

RESOURCES += \
    highlight.qrc \
    icons.qrc \
    messages.qrc
