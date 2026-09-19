#include "hackerdialog.h"
#include "../uistylehelper.h"

#include <QPainter>
#include <QPainterPath>

HackerDialog::HackerDialog(QWidget *parent, Qt::WindowFlags flags)
    : QDialog(parent, flags) {
  connectToThemeEngine();
}

HackerDialog::~HackerDialog() = default;

void HackerDialog::connectToThemeEngine() {
  connect(&ThemeEngine::instance(), &ThemeEngine::themeChanged, this,
          [this](const ThemeDefinition &t) { applyTheme(t); });
}

void HackerDialog::applyTheme(const ThemeDefinition &theme) {
  m_themeDef = theme;
  applyWidgetStyles();
  update();
}

void HackerDialog::applyTheme(const Theme &theme) {
  applyTheme(ThemeDefinition::fromClassicTheme(theme));
}

void HackerDialog::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const auto &c = colors();
  int r = borderRadius();

  QPainterPath bg;
  bg.addRoundedRect(rect(), r, r);
  p.fillPath(bg, c.surfaceBase);

  QColor topEdge = c.borderSubtle;
  topEdge.setAlpha(60);
  p.setPen(QPen(topEdge, 1));
  p.drawLine(r, 0, width() - r, 0);

  p.setPen(QPen(c.borderDefault, 1));
  p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), r, r);
}

QString HackerDialog::buttonStyle(const QColor &bg, const QColor &fg,
                                  const QColor &hover,
                                  const QColor &border) const {
  int r = borderRadius();
  return QString("QPushButton {"
                 "  background: %1; color: %2; border: 1px solid %3;"
                 "  border-radius: %4px; padding: 8px 16px;"
                 "  font-size: 12px; font-weight: bold;"
                 "}"
                 "QPushButton:hover {"
                 "  background: %5; border-color: %5;"
                 "}"
                 "QPushButton:pressed {"
                 "  background: %6;"
                 "}"
                 "QPushButton:disabled {"
                 "  background: %7; color: %8; border-color: %7;"
                 "}")
      .arg(bg.name(), fg.name(), border.name())
      .arg(r)
      .arg(hover.name(), hover.darker(110).name())
      .arg(colors().surfaceSunken.name(), colors().textDisabled.name());
}

QString HackerDialog::inputStyle() const {
  return UIStyleHelper::lineEditStyle(m_themeDef) +
         QStringLiteral("QLineEdit { font-family: monospace; }");
}

QString HackerDialog::comboStyle() const {
  return UIStyleHelper::comboBoxStyle(m_themeDef);
}

QString HackerDialog::checkBoxStyle() const {
  return UIStyleHelper::checkBoxStyle(m_themeDef);
}

QString HackerDialog::listWidgetStyle() const {
  const auto &c = colors();
  return QString("QListWidget {"
                 "  border: none; background: transparent;"
                 "  color: %1; outline: none;"
                 "}"
                 "QListWidget::item {"
                 "  padding: 8px 12px; border-radius: 4px; margin: 1px 4px;"
                 "}"
                 "QListWidget::item:selected {"
                 "  background: %2; color: %1;"
                 "}"
                 "QListWidget::item:hover:!selected {"
                 "  background: %3;"
                 "}")
      .arg(c.textPrimary.name(), c.accentSoft.name(), c.treeHoverBg.name());
}

QString HackerDialog::tableWidgetStyle() const {
  return UIStyleHelper::tableWidgetStyle(m_themeDef);
}

QString HackerDialog::tabWidgetStyle() const {
  return UIStyleHelper::tabWidgetStyle(m_themeDef);
}

QString HackerDialog::groupBoxStyle() const {
  return UIStyleHelper::groupBoxStyle(m_themeDef);
}

QString HackerDialog::textEditStyle() const {
  return UIStyleHelper::plainTextEditStyle(m_themeDef);
}

QString HackerDialog::spinBoxStyle() const {
  return UIStyleHelper::spinBoxStyle(m_themeDef);
}

QString HackerDialog::scrollBarStyle() const {
  const auto &c = colors();
  return QString(
             "QScrollBar:vertical {"
             "  background: transparent; width: 6px; margin: 0;"
             "}"
             "QScrollBar::handle:vertical {"
             "  background: %1; min-height: 20px; border-radius: 3px;"
             "}"
             "QScrollBar::handle:vertical:hover { background: %2; }"
             "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
             "  height: 0;"
             "}"
             "QScrollBar:horizontal {"
             "  background: transparent; height: 6px; margin: 0;"
             "}"
             "QScrollBar::handle:horizontal {"
             "  background: %1; min-width: 20px; border-radius: 3px;"
             "}"
             "QScrollBar::handle:horizontal:hover { background: %2; }"
             "QScrollBar::add-line:horizontal, "
             "QScrollBar::sub-line:horizontal {"
             "  width: 0;"
             "}")
      .arg(c.scrollThumb.name(), c.scrollThumbHover.name());
}

void HackerDialog::applyWidgetStyles() {
  const auto &c = colors();

  setStyleSheet(
      QString("QDialog { background: %1; }").arg(c.surfaceBase.name()));

  for (auto *w : findChildren<QGroupBox *>())
    w->setStyleSheet(groupBoxStyle());
  for (auto *w : findChildren<QLineEdit *>())
    w->setStyleSheet(inputStyle());
  for (auto *w : findChildren<QComboBox *>())
    w->setStyleSheet(comboStyle());
  for (auto *w : findChildren<QCheckBox *>())
    w->setStyleSheet(checkBoxStyle());
  for (auto *w : findChildren<QListWidget *>())
    w->setStyleSheet(listWidgetStyle() + scrollBarStyle());
  for (auto *w : findChildren<QPushButton *>())
    w->setStyleSheet(buttonStyle(c.btnSecondaryBg, c.btnSecondaryFg,
                                 c.btnSecondaryHover, c.borderDefault));
  for (auto *w : findChildren<QTableWidget *>())
    w->setStyleSheet(tableWidgetStyle() + scrollBarStyle());
  for (auto *w : findChildren<QPlainTextEdit *>())
    w->setStyleSheet(textEditStyle() + scrollBarStyle());
  for (auto *w : findChildren<QTextEdit *>())
    w->setStyleSheet(textEditStyle() + scrollBarStyle());
  for (auto *w : findChildren<QSpinBox *>())
    w->setStyleSheet(spinBoxStyle());
  for (auto *w : findChildren<QTabWidget *>())
    w->setStyleSheet(tabWidgetStyle());
}

void HackerDialog::stylePrimaryButton(QPushButton *btn) {
  if (btn) {
    const auto &c = colors();
    btn->setStyleSheet(buttonStyle(c.btnPrimaryBg, c.btnPrimaryFg,
                                   c.btnPrimaryHover, c.btnPrimaryBg));
  }
}

void HackerDialog::styleSecondaryButton(QPushButton *btn) {
  if (btn) {
    const auto &c = colors();
    btn->setStyleSheet(buttonStyle(c.btnSecondaryBg, c.btnSecondaryFg,
                                   c.btnSecondaryHover, c.borderDefault));
  }
}

void HackerDialog::styleDangerButton(QPushButton *btn) {
  if (btn) {
    const auto &c = colors();
    btn->setStyleSheet(buttonStyle(c.btnDangerBg, c.btnDangerFg,
                                   c.btnDangerHover, c.btnDangerBg));
  }
}

void HackerDialog::styleTitleLabel(QLabel *label) {
  if (label)
    label->setStyleSheet(QString("font-weight: bold; color: %1;")
                             .arg(colors().textPrimary.name()));
}

void HackerDialog::styleSubduedLabel(QLabel *label) {
  if (label)
    label->setStyleSheet(QString("color: %1;").arg(colors().textMuted.name()));
}
