#include "theme.h"

Theme::Theme() :
    backgroundColor(QColor("black")),
    foregroundColor(QColor("lightGray")),
    highlightColor(QColor("lightGray").darker(250)),
    lineNumberAreaColor(QColor("black")),
    keywordFormat_0(QColor("green").lighter(130)),
    keywordFormat_1(QColor("yellow").darker(140)),
    keywordFormat_2(QColor("violet")),
    searchFormat(QColor("yellow")),
    singleLineCommentFormat(QColor("green").darker(150)),
    functionFormat(QColor("lightGray").darker(150)),
    quotationFormat(QColor("orange")),
    classFormat(QColor("blue").lighter(150)),
    numberFormat(QColor("#ff405d")) {
}

void Theme::read(const QJsonObject &json)
{

}

void Theme::write(QJsonObject &json)
{

}
