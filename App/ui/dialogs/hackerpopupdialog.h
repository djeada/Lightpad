#ifndef HACKERPOPUPDIALOG_H
#define HACKERPOPUPDIALOG_H

#include "hackerdialog.h"
#include <QKeyEvent>

class HackerPopupDialog : public HackerDialog {
  Q_OBJECT

public:
  explicit HackerPopupDialog(QWidget *parent = nullptr);

  void applyTheme(const ThemeDefinition &theme) override;
  void applyTheme(const Theme &theme) override;

  void showCentered();

protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
};

#endif
