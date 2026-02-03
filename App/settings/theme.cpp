#include "theme.h"

#include <QJsonObject>

Theme::Theme()
    // Editor colors
    : backgroundColor(QColor("#0d1117")), foregroundColor(QColor("#e6edf3")),
      highlightColor(QColor("#1a2230")), lineNumberAreaColor(QColor("#0d1117"))
      // Syntax highlighting colors
      ,
      keywordFormat_0(QColor("#7ee787")), keywordFormat_1(QColor("#f2cc60")),
      keywordFormat_2(QColor("#58a6ff")), searchFormat(QColor("#f2cc60")),
      singleLineCommentFormat(QColor("#8b949e")),
      functionFormat(QColor("#79c0ff")), quotationFormat(QColor("#a5d6ff")),
      classFormat(QColor("#56d4dd")), numberFormat(QColor("#ff7b72"))
      // Modern UI design tokens
      ,
      surfaceColor(QColor("#161b22")), surfaceAltColor(QColor("#1c2128")),
      borderColor(QColor("#30363d")), hoverColor(QColor("#21262d")),
      pressedColor(QColor("#2d333b")), accentColor(QColor("#58a6ff")),
      accentSoftColor(QColor("#1f3a5f")), successColor(QColor("#3fb950")),
      warningColor(QColor("#d29922")), errorColor(QColor("#f85149")) {}

void Theme::read(const QJsonObject &json) {
  const QJsonObject themeObject =
      (json.contains("theme") && json["theme"].isObject())
          ? json["theme"].toObject()
          : json;

  if (themeObject.contains("backgroundColor") &&
      themeObject["backgroundColor"].isString())
    backgroundColor = QColor(themeObject["backgroundColor"].toString());

  if (themeObject.contains("foregroundColor") &&
      themeObject["foregroundColor"].isString())
    foregroundColor = QColor(themeObject["foregroundColor"].toString());

  if (themeObject.contains("highlightColor") &&
      themeObject["highlightColor"].isString())
    highlightColor = QColor(themeObject["highlightColor"].toString());

  if (themeObject.contains("lineNumberAreaColor") &&
      themeObject["lineNumberAreaColor"].isString())
    lineNumberAreaColor = QColor(themeObject["lineNumberAreaColor"].toString());

  if (themeObject.contains("keywordFormat_0") &&
      themeObject["keywordFormat_0"].isString())
    keywordFormat_0 = QColor(themeObject["keywordFormat_0"].toString());

  if (themeObject.contains("keywordFormat_1") &&
      themeObject["keywordFormat_1"].isString())
    keywordFormat_1 = QColor(themeObject["keywordFormat_1"].toString());

  if (themeObject.contains("keywordFormat_2") &&
      themeObject["keywordFormat_2"].isString())
    keywordFormat_2 = QColor(themeObject["keywordFormat_2"].toString());

  if (themeObject.contains("searchFormat") &&
      themeObject["searchFormat"].isString())
    searchFormat = QColor(themeObject["searchFormat"].toString());

  if (themeObject.contains("singleLineCommentFormat") &&
      themeObject["singleLineCommentFormat"].isString())
    singleLineCommentFormat =
        QColor(themeObject["singleLineCommentFormat"].toString());

  if (themeObject.contains("functionFormat") &&
      themeObject["functionFormat"].isString())
    functionFormat = QColor(themeObject["functionFormat"].toString());

  if (themeObject.contains("quotationFormat") &&
      themeObject["quotationFormat"].isString())
    quotationFormat = QColor(themeObject["quotationFormat"].toString());

  if (themeObject.contains("classFormat") &&
      themeObject["classFormat"].isString())
    classFormat = QColor(themeObject["classFormat"].toString());

  if (themeObject.contains("numberFormat") &&
      themeObject["numberFormat"].isString())
    numberFormat = QColor(themeObject["numberFormat"].toString());

  // Modern UI tokens
  if (themeObject.contains("surfaceColor") &&
      themeObject["surfaceColor"].isString())
    surfaceColor = QColor(themeObject["surfaceColor"].toString());

  if (themeObject.contains("surfaceAltColor") &&
      themeObject["surfaceAltColor"].isString())
    surfaceAltColor = QColor(themeObject["surfaceAltColor"].toString());

  if (themeObject.contains("borderColor") &&
      themeObject["borderColor"].isString())
    borderColor = QColor(themeObject["borderColor"].toString());

  if (themeObject.contains("hoverColor") &&
      themeObject["hoverColor"].isString())
    hoverColor = QColor(themeObject["hoverColor"].toString());

  if (themeObject.contains("pressedColor") &&
      themeObject["pressedColor"].isString())
    pressedColor = QColor(themeObject["pressedColor"].toString());

  if (themeObject.contains("accentColor") &&
      themeObject["accentColor"].isString())
    accentColor = QColor(themeObject["accentColor"].toString());

  if (themeObject.contains("accentSoftColor") &&
      themeObject["accentSoftColor"].isString())
    accentSoftColor = QColor(themeObject["accentSoftColor"].toString());

  if (themeObject.contains("successColor") &&
      themeObject["successColor"].isString())
    successColor = QColor(themeObject["successColor"].toString());

  if (themeObject.contains("warningColor") &&
      themeObject["warningColor"].isString())
    warningColor = QColor(themeObject["warningColor"].toString());

  if (themeObject.contains("errorColor") &&
      themeObject["errorColor"].isString())
    errorColor = QColor(themeObject["errorColor"].toString());
}

void Theme::write(QJsonObject &json) {
  json["backgroundColor"] = backgroundColor.name();
  json["foregroundColor"] = foregroundColor.name();
  json["highlightColor"] = highlightColor.name();
  json["lineNumberAreaColor"] = lineNumberAreaColor.name();
  json["keywordFormat_0"] = keywordFormat_0.name();
  json["keywordFormat_1"] = keywordFormat_1.name();
  json["keywordFormat_2"] = keywordFormat_2.name();
  json["searchFormat"] = searchFormat.name();
  json["singleLineCommentFormat"] = singleLineCommentFormat.name();
  json["functionFormat"] = functionFormat.name();
  json["quotationFormat"] = quotationFormat.name();
  json["classFormat"] = classFormat.name();
  json["numberFormat"] = numberFormat.name();

  // Modern UI tokens
  json["surfaceColor"] = surfaceColor.name();
  json["surfaceAltColor"] = surfaceAltColor.name();
  json["borderColor"] = borderColor.name();
  json["hoverColor"] = hoverColor.name();
  json["pressedColor"] = pressedColor.name();
  json["accentColor"] = accentColor.name();
  json["accentSoftColor"] = accentSoftColor.name();
  json["successColor"] = successColor.name();
  json["warningColor"] = warningColor.name();
  json["errorColor"] = errorColor.name();
}
