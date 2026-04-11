#include "styledpopupdialog.h"
#include "../uistylehelper.h"

StyledPopupDialog::StyledPopupDialog(QWidget *parent)
    : StyledDialog(parent, Qt::Popup | Qt::FramelessWindowHint) {}

void StyledPopupDialog::applyTheme(const Theme &theme) {
  m_theme = theme;

  QString className = metaObject()->className();
  setStyleSheet(className + " { " + UIStyleHelper::popupDialogStyle(theme) +
                " }");

  for (auto *w : findChildren<QLineEdit *>())
    w->setStyleSheet(UIStyleHelper::searchBoxStyle(theme));
  for (auto *w : findChildren<QListWidget *>())
    w->setStyleSheet(UIStyleHelper::resultListStyle(theme));
}

void StyledPopupDialog::showCentered() {
  if (parentWidget()) {
    QPoint parentCenter =
        parentWidget()->mapToGlobal(parentWidget()->rect().center());
    int x = parentCenter.x() - width() / 2;
    int y = parentWidget()->mapToGlobal(QPoint(0, 0)).y() + 50;
    move(x, y);
  }
  show();
}

void StyledPopupDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    hide();
  } else {
    StyledDialog::keyPressEvent(event);
  }
}
