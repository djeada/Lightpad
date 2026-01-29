#include "theme.h"

#include <QJsonObject>

Theme::Theme()
    : backgroundColor(QColor("black"))
    , foregroundColor(QColor("lightGray"))
    , highlightColor(QColor("lightGray").darker(250))
    , lineNumberAreaColor(QColor("black"))
    , keywordFormat_0(QColor("green").lighter(130))
    , keywordFormat_1(QColor("yellow").darker(140))
    , keywordFormat_2(QColor("violet"))
    , searchFormat(QColor("yellow"))
    , singleLineCommentFormat(QColor("green").darker(150))
    , functionFormat(QColor("lightGray").darker(150))
    , quotationFormat(QColor("orange"))
    , classFormat(QColor("blue").lighter(150))
    , numberFormat(QColor("#ff405d"))
{
}

void Theme::read(const QJsonObject& json)
{
    if (json.contains("backgroundColor") && json["backgroundColor"].isString())
        backgroundColor = QColor(json["backgroundColor"].toString());

    if (json.contains("foregroundColor") && json["foregroundColor"].isString())
        backgroundColor = QColor(json["foregroundColor"].toString());

    if (json.contains("highlightColor") && json["highlightColor"].isString())
        backgroundColor = QColor(json["highlightColor"].toString());

    if (json.contains("keywordFormat_1") && json["keywordFormat_1"].isString())
        backgroundColor = QColor(json["keywordFormat_1"].toString());

    if (json.contains("keywordFormat_2") && json["keywordFormat_2"].isString())
        backgroundColor = QColor(json["keywordFormat_2"].toString());

    if (json.contains("searchFormat") && json["searchFormat"].isString())
        backgroundColor = QColor(json["searchFormat"].toString());

    if (json.contains("singleLineCommentFormat") && json["singleLineCommentFormat"].isString())
        backgroundColor = QColor(json["singleLineCommentFormat"].toString());

    if (json.contains("functionFormat") && json["functionFormat"].isString())
        backgroundColor = QColor(json["functionFormat"].toString());

    if (json.contains("quotationFormat") && json["quotationFormat"].isString())
        backgroundColor = QColor(json["quotationFormat"].toString());

    if (json.contains("classFormat") && json["classFormat"].isString())
        backgroundColor = QColor(json["classFormat"].toString());

    if (json.contains("numberFormat") && json["numberFormat"].isString())
        backgroundColor = QColor(json["numberFormat"].toString());
}

void Theme::write(QJsonObject& json)
{
    json["backgroundColor"] = backgroundColor.name();
    json["foregroundColor"] = foregroundColor.name();
    json["highlightColor"] = highlightColor.name();
    json["keywordFormat_1"] = keywordFormat_1.name();
    json["keywordFormat_2"] = keywordFormat_2.name();
    json["searchFormat"] = searchFormat.name();
    json["singleLineCommentFormat"] = singleLineCommentFormat.name();
    json["functionFormat"] = functionFormat.name();
    json["quotationFormat"] = quotationFormat.name();
    json["classFormat"] = classFormat.name();
    json["numberFormat"] = numberFormat.name();
}
