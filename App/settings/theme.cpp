#include "theme.h"

#include <QJsonObject>

Theme::Theme()
    : backgroundColor(QColor("#1e1e2e"))          // Softer dark background (catppuccin-inspired)
    , foregroundColor(QColor("#cdd6f4"))          // Softer light foreground
    , highlightColor(QColor("#313244"))           // Subtle line highlight
    , lineNumberAreaColor(QColor("#1e1e2e"))      // Match main background
    , keywordFormat_0(QColor("#a6e3a1"))          // Soft green for keywords
    , keywordFormat_1(QColor("#f9e2af"))          // Soft yellow for types
    , keywordFormat_2(QColor("#cba6f7"))          // Soft purple for special
    , searchFormat(QColor("#f9e2af"))             // Soft yellow for search
    , singleLineCommentFormat(QColor("#6c7086"))  // Muted gray for comments
    , functionFormat(QColor("#89b4fa"))           // Soft blue for functions
    , quotationFormat(QColor("#fab387"))          // Soft peach for strings
    , classFormat(QColor("#89dceb"))              // Soft cyan for classes
    , numberFormat(QColor("#f38ba8"))             // Soft pink for numbers
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
