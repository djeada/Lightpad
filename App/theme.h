#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QDebug>

struct Theme {
    QColor backgroundColor;
    QColor foregroundColor;
    QColor highlightColor;
    QColor lineNumberAreaColor;
    QColor keywordFormat_0;
    QColor keywordFormat_1;
    QColor keywordFormat_2;
    QColor searchFormat;
    QColor singleLineCommentFormat;
    QColor functionFormat;
    QColor quotationFormat;
    QColor classFormat;
    QColor numberFormat;

    Theme();
    void read(const QJsonObject& json);
    void write(QJsonObject& json);
};


#endif // THEME_H
