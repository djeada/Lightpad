#ifndef TEXTAREASETTINGS_H
#define TEXTAREASETTINGS_H

#include <QColor>
#include <QString>
#include <QFont>
#include <QApplication>

#include "theme.h"

struct TextAreaSettings {
    QFont mainFont;
    Theme theme;
    bool autoIndent;
    bool showLineNumberArea;
    bool lineHighlighted;
    bool matchingBracketsHighlighted;
    int tabWidth;

    TextAreaSettings() :
      mainFont(QApplication::font()),
      autoIndent(true),
      showLineNumberArea(true),
      lineHighlighted(true),
      matchingBracketsHighlighted(true),
      tabWidth(4) {

    };
};

#endif // TEXTAREASETTINGS_H
