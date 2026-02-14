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

  Theme();
  void read(const QJsonObject &json);
  void write(QJsonObject &json);
};

#endif
