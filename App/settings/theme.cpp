#include "theme.h"

#include <QJsonObject>
#include <QtGlobal>

namespace {
QColor mix(const QColor &a, const QColor &b, qreal t) {
  const qreal clamped = qBound(0.0, t, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * clamped,
                          a.greenF() + (b.greenF() - a.greenF()) * clamped,
                          a.blueF() + (b.blueF() - a.blueF()) * clamped,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * clamped);
}

void writeColor(QJsonObject &json, const char *key, const QColor &color) {
  if (!color.isValid())
    return;
  json[key] =
      color.name(color.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb);
}

void readColor(const QJsonObject &json, const char *key, QColor &dest) {
  if (json.contains(key) && json[key].isString())
    dest = QColor(json[key].toString());
}
} // namespace

Theme::Theme()
    : backgroundColor(QColor("#0a0e14")), foregroundColor(QColor("#b3b1ad")),
      highlightColor(QColor("#0f1419")), lineNumberAreaColor(QColor("#0a0e14")),
      keywordFormat_0(QColor("#ff8f40")), keywordFormat_1(QColor("#ffb454")),
      keywordFormat_2(QColor("#59c2ff")), searchFormat(QColor("#e6b450")),
      singleLineCommentFormat(QColor("#626a73")),
      functionFormat(QColor("#ffb454")), quotationFormat(QColor("#c2d94c")),
      classFormat(QColor("#59c2ff")), numberFormat(QColor("#e6b450")),
      surfaceColor(QColor("#0f1419")), surfaceAltColor(QColor("#141925")),
      borderColor(QColor("#1c2a1c")), hoverColor(QColor("#0d1218")),
      pressedColor(QColor("#141925")), accentColor(QColor("#00ff41")),
      accentSoftColor(QColor("#0a3a1a")), successColor(QColor("#00ff41")),
      warningColor(QColor("#e6b450")), errorColor(QColor("#f85149")),
      infoColor(QColor("#59c2ff")) {
  normalize();
}

void Theme::normalize() {
  auto ensure = [](QColor &dest, const QColor &fallback) {
    if (!dest.isValid())
      dest = fallback;
  };

  ensure(infoColor, accentColor);

  ensure(diagnosticErrorColor, errorColor);
  ensure(diagnosticWarningColor, warningColor);
  ensure(diagnosticInfoColor, infoColor);
  ensure(diagnosticHintColor,
         mix(singleLineCommentFormat, foregroundColor, 0.2));

  ensure(gitAddedColor, successColor);
  ensure(gitModifiedColor, warningColor);
  ensure(gitDeletedColor, errorColor);
  ensure(gitRenamedColor, infoColor);
  ensure(gitCopiedColor, mix(infoColor, accentColor, 0.35));
  ensure(gitUntrackedColor, mix(successColor, accentColor, 0.2));
  ensure(gitConflictedColor, errorColor);
  ensure(gitIgnoredColor, singleLineCommentFormat);

  ensure(diffAddedColor, mix(successColor, accentColor, 0.15));
  ensure(diffModifiedColor, warningColor);
  ensure(diffRemovedColor, errorColor);
  ensure(diffConflictColor, mix(errorColor, warningColor, 0.35));

  ensure(testPassedColor, successColor);
  ensure(testFailedColor, errorColor);
  ensure(testSkippedColor, mix(singleLineCommentFormat, warningColor, 0.15));
  ensure(testRunningColor, infoColor);
  ensure(testQueuedColor, singleLineCommentFormat);

  ensure(debugReadyColor, mix(foregroundColor, backgroundColor, 0.35));
  ensure(debugStartingColor, warningColor);
  ensure(debugRunningColor, infoColor);
  ensure(debugPausedColor, successColor);
  ensure(debugErrorColor, errorColor);
  ensure(debugBreakpointColor, mix(errorColor, accentColor, 0.12));
  ensure(debugCurrentLineColor, accentColor);
}

void Theme::read(const QJsonObject &json) {
  const QJsonObject themeObject =
      (json.contains("theme") && json["theme"].isObject())
          ? json["theme"].toObject()
          : json;

  readColor(themeObject, "backgroundColor", backgroundColor);
  readColor(themeObject, "foregroundColor", foregroundColor);
  readColor(themeObject, "highlightColor", highlightColor);
  readColor(themeObject, "lineNumberAreaColor", lineNumberAreaColor);

  readColor(themeObject, "keywordFormat_0", keywordFormat_0);
  readColor(themeObject, "keywordFormat_1", keywordFormat_1);
  readColor(themeObject, "keywordFormat_2", keywordFormat_2);
  readColor(themeObject, "searchFormat", searchFormat);
  readColor(themeObject, "singleLineCommentFormat", singleLineCommentFormat);
  readColor(themeObject, "functionFormat", functionFormat);
  readColor(themeObject, "quotationFormat", quotationFormat);
  readColor(themeObject, "classFormat", classFormat);
  readColor(themeObject, "numberFormat", numberFormat);

  readColor(themeObject, "surfaceColor", surfaceColor);
  readColor(themeObject, "surfaceAltColor", surfaceAltColor);
  readColor(themeObject, "borderColor", borderColor);
  readColor(themeObject, "hoverColor", hoverColor);
  readColor(themeObject, "pressedColor", pressedColor);
  readColor(themeObject, "accentColor", accentColor);
  readColor(themeObject, "accentSoftColor", accentSoftColor);
  readColor(themeObject, "successColor", successColor);
  readColor(themeObject, "warningColor", warningColor);
  readColor(themeObject, "errorColor", errorColor);
  readColor(themeObject, "infoColor", infoColor);

  readColor(themeObject, "diagnosticErrorColor", diagnosticErrorColor);
  readColor(themeObject, "diagnosticWarningColor", diagnosticWarningColor);
  readColor(themeObject, "diagnosticInfoColor", diagnosticInfoColor);
  readColor(themeObject, "diagnosticHintColor", diagnosticHintColor);

  readColor(themeObject, "gitAddedColor", gitAddedColor);
  readColor(themeObject, "gitModifiedColor", gitModifiedColor);
  readColor(themeObject, "gitDeletedColor", gitDeletedColor);
  readColor(themeObject, "gitRenamedColor", gitRenamedColor);
  readColor(themeObject, "gitCopiedColor", gitCopiedColor);
  readColor(themeObject, "gitUntrackedColor", gitUntrackedColor);
  readColor(themeObject, "gitConflictedColor", gitConflictedColor);
  readColor(themeObject, "gitIgnoredColor", gitIgnoredColor);

  readColor(themeObject, "diffAddedColor", diffAddedColor);
  readColor(themeObject, "diffModifiedColor", diffModifiedColor);
  readColor(themeObject, "diffRemovedColor", diffRemovedColor);
  readColor(themeObject, "diffConflictColor", diffConflictColor);

  readColor(themeObject, "testPassedColor", testPassedColor);
  readColor(themeObject, "testFailedColor", testFailedColor);
  readColor(themeObject, "testSkippedColor", testSkippedColor);
  readColor(themeObject, "testRunningColor", testRunningColor);
  readColor(themeObject, "testQueuedColor", testQueuedColor);

  readColor(themeObject, "debugReadyColor", debugReadyColor);
  readColor(themeObject, "debugStartingColor", debugStartingColor);
  readColor(themeObject, "debugRunningColor", debugRunningColor);
  readColor(themeObject, "debugPausedColor", debugPausedColor);
  readColor(themeObject, "debugErrorColor", debugErrorColor);
  readColor(themeObject, "debugBreakpointColor", debugBreakpointColor);
  readColor(themeObject, "debugCurrentLineColor", debugCurrentLineColor);

  if (themeObject.contains("borderRadius") &&
      themeObject["borderRadius"].isDouble())
    borderRadius = themeObject["borderRadius"].toInt();

  if (themeObject.contains("glowIntensity") &&
      themeObject["glowIntensity"].isDouble())
    glowIntensity = themeObject["glowIntensity"].toDouble();

  if (themeObject.contains("chromeOpacity") &&
      themeObject["chromeOpacity"].isDouble())
    chromeOpacity = themeObject["chromeOpacity"].toDouble();

  if (themeObject.contains("scanlineEffect") &&
      themeObject["scanlineEffect"].isBool())
    scanlineEffect = themeObject["scanlineEffect"].toBool();

  if (themeObject.contains("panelBorders") &&
      themeObject["panelBorders"].isBool())
    panelBorders = themeObject["panelBorders"].toBool();

  normalize();
}

void Theme::write(QJsonObject &json) {
  writeColor(json, "backgroundColor", backgroundColor);
  writeColor(json, "foregroundColor", foregroundColor);
  writeColor(json, "highlightColor", highlightColor);
  writeColor(json, "lineNumberAreaColor", lineNumberAreaColor);
  writeColor(json, "keywordFormat_0", keywordFormat_0);
  writeColor(json, "keywordFormat_1", keywordFormat_1);
  writeColor(json, "keywordFormat_2", keywordFormat_2);
  writeColor(json, "searchFormat", searchFormat);
  writeColor(json, "singleLineCommentFormat", singleLineCommentFormat);
  writeColor(json, "functionFormat", functionFormat);
  writeColor(json, "quotationFormat", quotationFormat);
  writeColor(json, "classFormat", classFormat);
  writeColor(json, "numberFormat", numberFormat);

  writeColor(json, "surfaceColor", surfaceColor);
  writeColor(json, "surfaceAltColor", surfaceAltColor);
  writeColor(json, "borderColor", borderColor);
  writeColor(json, "hoverColor", hoverColor);
  writeColor(json, "pressedColor", pressedColor);
  writeColor(json, "accentColor", accentColor);
  writeColor(json, "accentSoftColor", accentSoftColor);
  writeColor(json, "successColor", successColor);
  writeColor(json, "warningColor", warningColor);
  writeColor(json, "errorColor", errorColor);
  writeColor(json, "infoColor", infoColor);

  writeColor(json, "diagnosticErrorColor", diagnosticErrorColor);
  writeColor(json, "diagnosticWarningColor", diagnosticWarningColor);
  writeColor(json, "diagnosticInfoColor", diagnosticInfoColor);
  writeColor(json, "diagnosticHintColor", diagnosticHintColor);

  writeColor(json, "gitAddedColor", gitAddedColor);
  writeColor(json, "gitModifiedColor", gitModifiedColor);
  writeColor(json, "gitDeletedColor", gitDeletedColor);
  writeColor(json, "gitRenamedColor", gitRenamedColor);
  writeColor(json, "gitCopiedColor", gitCopiedColor);
  writeColor(json, "gitUntrackedColor", gitUntrackedColor);
  writeColor(json, "gitConflictedColor", gitConflictedColor);
  writeColor(json, "gitIgnoredColor", gitIgnoredColor);

  writeColor(json, "diffAddedColor", diffAddedColor);
  writeColor(json, "diffModifiedColor", diffModifiedColor);
  writeColor(json, "diffRemovedColor", diffRemovedColor);
  writeColor(json, "diffConflictColor", diffConflictColor);

  writeColor(json, "testPassedColor", testPassedColor);
  writeColor(json, "testFailedColor", testFailedColor);
  writeColor(json, "testSkippedColor", testSkippedColor);
  writeColor(json, "testRunningColor", testRunningColor);
  writeColor(json, "testQueuedColor", testQueuedColor);

  writeColor(json, "debugReadyColor", debugReadyColor);
  writeColor(json, "debugStartingColor", debugStartingColor);
  writeColor(json, "debugRunningColor", debugRunningColor);
  writeColor(json, "debugPausedColor", debugPausedColor);
  writeColor(json, "debugErrorColor", debugErrorColor);
  writeColor(json, "debugBreakpointColor", debugBreakpointColor);
  writeColor(json, "debugCurrentLineColor", debugCurrentLineColor);

  json["borderRadius"] = borderRadius;
  json["glowIntensity"] = glowIntensity;
  json["chromeOpacity"] = chromeOpacity;
  json["scanlineEffect"] = scanlineEffect;
  json["panelBorders"] = panelBorders;
}
