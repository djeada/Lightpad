#ifndef TEXTAREASETTINGS_H
#define TEXTAREASETTINGS_H

#include <QColor>
#include <QString>
#include <QFont>

struct TextAreaSettings {
    QColor highlightColor;
    QColor lineNumberAreaPenColor;
    QColor defaultPenColor;
    QColor backgroundColor;
    QString bufferText;
    QString highlightLang;
    QFont mainFont;
    QString searchWord;
    bool areChangesUnsaved;
    bool autoIndent;
    bool showLineNumberArea;
};

#endif // TEXTAREASETTINGS_H
