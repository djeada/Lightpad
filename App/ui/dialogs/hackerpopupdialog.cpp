#include "hackerpopupdialog.h"

#include <QPainter>
#include <QPainterPath>

HackerPopupDialog::HackerPopupDialog(QWidget *parent)
    : HackerDialog(parent, Qt::Popup | Qt::FramelessWindowHint) {
  setAttribute(Qt::WA_TranslucentBackground);
}

void HackerPopupDialog::applyTheme(const ThemeDefinition &theme) {
  m_themeDef = theme;

  const auto &c = colors();
  int r = borderRadius();

  for (auto *w : findChildren<QLineEdit *>()) {
    w->setStyleSheet(QString("QLineEdit {"
                             "  padding: 10px 14px; font-size: 14px;"
                             "  border: none; border-bottom: 2px solid %1;"
                             "  border-radius: 0; background: %2; color: %3;"
                             "  font-family: monospace;"
                             "  selection-background-color: %4;"
                             "}"
                             "QLineEdit:focus { border-bottom-color: %5; }")
                         .arg(c.borderSubtle.name(), c.surfaceBase.name(),
                              c.textPrimary.name(), c.inputSelection.name(),
                              c.accentPrimary.name()));
  }
  for (auto *w : findChildren<QListWidget *>()) {
    w->setStyleSheet(
        QString("QListWidget {"
                "  border: none; background: transparent;"
                "  color: %1; outline: none;"
                "}"
                "QListWidget::item {"
                "  padding: 8px 14px; border-radius: 4px;"
                "  margin: 1px 4px;"
                "}"
                "QListWidget::item:selected {"
                "  background: %2; color: %1;"
                "}"
                "QListWidget::item:hover:!selected {"
                "  background: %3;"
                "}"
                "QScrollBar:vertical {"
                "  background: transparent; width: 4px;"
                "}"
                "QScrollBar::handle:vertical {"
                "  background: %4; min-height: 20px; border-radius: 2px;"
                "}"
                "QScrollBar::add-line:vertical,"
                "QScrollBar::sub-line:vertical { height: 0; }")
            .arg(c.textPrimary.name(), c.accentSoft.name(),
                 c.treeHoverBg.name(), c.scrollThumb.name()));
  }
}

void HackerPopupDialog::applyTheme(const Theme &theme) {
  applyTheme(ThemeDefinition::fromClassicTheme(theme));
}

void HackerPopupDialog::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const auto &c = colors();
  int r = borderRadius();
  qreal glow = ThemeEngine::instance().glowIntensity();

  QRectF outer = QRectF(rect()).adjusted(4, 4, -4, -4);

  if (glow > 0.01) {
    for (int i = 3; i >= 1; --i) {
      QColor shadow = c.accentGlow;
      shadow.setAlphaF(glow * 0.08 * i);
      QPainterPath sp;
      sp.addRoundedRect(outer.adjusted(-i * 2, -i * 2, i * 2, i * 2), r + i,
                        r + i);
      p.fillPath(sp, shadow);
    }
  }

  QPainterPath bg;
  bg.addRoundedRect(outer, r, r);
  p.fillPath(bg, c.surfaceOverlay);

  p.setPen(QPen(c.borderDefault, 1));
  p.drawRoundedRect(outer, r, r);
}

void HackerPopupDialog::showCentered() {
  if (parentWidget()) {
    QPoint parentCenter =
        parentWidget()->mapToGlobal(parentWidget()->rect().center());
    int x = parentCenter.x() - width() / 2;
    int y = parentWidget()->mapToGlobal(QPoint(0, 0)).y() + 50;
    move(x, y);
  }
  show();
}

void HackerPopupDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    hide();
  } else {
    HackerDialog::keyPressEvent(event);
  }
}
