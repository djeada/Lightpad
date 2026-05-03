#ifndef THEME_H
#define THEME_H

#include <QColor>

class QJsonObject;

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

  QColor surfaceColor;
  QColor surfaceAltColor;
  QColor borderColor;
  QColor hoverColor;
  QColor pressedColor;
  QColor accentColor;
  QColor accentSoftColor;
  QColor successColor;
  QColor warningColor;
  QColor errorColor;
  QColor infoColor;

  QColor diagnosticErrorColor;
  QColor diagnosticWarningColor;
  QColor diagnosticInfoColor;
  QColor diagnosticHintColor;

  QColor gitAddedColor;
  QColor gitModifiedColor;
  QColor gitDeletedColor;
  QColor gitRenamedColor;
  QColor gitCopiedColor;
  QColor gitUntrackedColor;
  QColor gitConflictedColor;
  QColor gitIgnoredColor;

  QColor diffAddedColor;
  QColor diffModifiedColor;
  QColor diffRemovedColor;
  QColor diffConflictColor;

  QColor testPassedColor;
  QColor testFailedColor;
  QColor testSkippedColor;
  QColor testRunningColor;
  QColor testQueuedColor;

  QColor debugReadyColor;
  QColor debugStartingColor;
  QColor debugRunningColor;
  QColor debugPausedColor;
  QColor debugErrorColor;
  QColor debugBreakpointColor;
  QColor debugCurrentLineColor;

  int borderRadius = 6;
  qreal glowIntensity = 0.0;
  qreal chromeOpacity = 1.0;
  bool scanlineEffect = false;
  bool panelBorders = true;

  Theme();
  void read(const QJsonObject &json);
  void write(QJsonObject &json);
  void normalize();
};

#endif
