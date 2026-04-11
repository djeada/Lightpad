#ifndef STYLEDPOPUPDIALOG_H
#define STYLEDPOPUPDIALOG_H

#include "styleddialog.h"
#include <QKeyEvent>

class StyledPopupDialog : public StyledDialog {
  Q_OBJECT

public:
  explicit StyledPopupDialog(QWidget *parent = nullptr);

  void applyTheme(const Theme &theme) override;

  void showCentered();

protected:
  void keyPressEvent(QKeyEvent *event) override;
};

#endif
