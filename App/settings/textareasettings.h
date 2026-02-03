#ifndef TEXTAREASETTINGS_H
#define TEXTAREASETTINGS_H

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QString>

#include "theme.h"

struct TextAreaSettings {
  QFont mainFont;
  Theme theme;
  bool autoIndent;
  bool showLineNumberArea;
  bool lineHighlighted;
  bool matchingBracketsHighlighted;
  bool vimModeEnabled;
  int tabWidth;

  TextAreaSettings();
  void loadSettings(const QString &path);
  void saveSettings(const QString &path);

private:
  void read(const QJsonObject &json);
  void write(QJsonObject &json);
};

#endif // TEXTAREASETTINGS_H
