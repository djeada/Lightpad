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

    TextAreaSettings();
    void loadSettings(const QString& path);
    void saveSettings(const QString& path);

    private:
        void read(const QJsonObject& json);
        void write(QJsonObject& json);
};

#endif // TEXTAREASETTINGS_H
