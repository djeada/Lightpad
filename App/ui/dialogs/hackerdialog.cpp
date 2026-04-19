#include "hackerdialog.h"

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
  const auto &c = colors();
  int r = borderRadius();
  return QString("QLineEdit {"
                 "  background: %1; color: %2; border: 1px solid %3;"
                 "  border-radius: %4px; padding: 8px 12px;"
                 "  font-size: 12px; font-family: monospace;"
                 "  selection-background-color: %5;"
                 "}"
                 "QLineEdit:focus {"
                 "  border-color: %6;"
                 "  border-bottom: 2px solid %6;"
                 "}")
      .arg(c.inputBg.name(), c.inputFg.name(), c.inputBorder.name())
      .arg(r)
      .arg(c.inputSelection.name(), c.inputBorderFocus.name());
}

QString HackerDialog::comboStyle() const {
  const auto &c = colors();
  int r = borderRadius();
  return QString("QComboBox {"
                 "  background: %1; color: %2; border: 1px solid %3;"
                 "  padding: 6px 12px; border-radius: %4px;"
                 "  font-family: monospace;"
                 "}"
                 "QComboBox::drop-down { border: none; width: 20px; }"
                 "QComboBox::down-arrow { image: none; }"
                 "QComboBox QAbstractItemView {"
                 "  background: %5; color: %2; border: 1px solid %3;"
                 "  selection-background-color: %6;"
                 "  outline: none;"
                 "}")
      .arg(c.inputBg.name(), c.inputFg.name(), c.inputBorder.name())
      .arg(r)
      .arg(c.surfacePopover.name(), c.accentSoft.name());
}

QString HackerDialog::checkBoxStyle() const {
  const auto &c = colors();
  return QString("QCheckBox { color: %1; spacing: 8px; }"
                 "QCheckBox::indicator {"
                 "  width: 16px; height: 16px; border-radius: 4px;"
                 "  border: 1px solid %2; background: %3;"
                 "}"
                 "QCheckBox::indicator:checked {"
                 "  background: %4; border-color: %4;"
                 "}"
                 "QCheckBox::indicator:hover {"
                 "  border-color: %5;"
                 "}")
      .arg(c.textPrimary.name(), c.borderDefault.name(), c.surfaceSunken.name(),
           c.accentPrimary.name(), c.accentPrimary.name());
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
  const auto &c = colors();
  return QString("QTableWidget {"
                 "  background: %1; alternate-background-color: %2;"
                 "  color: %3; border: 1px solid %4;"
                 "  border-radius: 4px; gridline-color: %4;"
                 "  selection-background-color: %5;"
                 "}"
                 "QTableWidget::item { padding: 4px; }"
                 "QTableWidget::item:selected {"
                 "  background: %5; color: %3;"
                 "}"
                 "QHeaderView::section {"
                 "  background: %2; color: %6; border: none;"
                 "  border-bottom: 1px solid %4; padding: 6px 8px;"
                 "  font-size: 11px; font-weight: bold;"
                 "}")
      .arg(c.surfaceBase.name(), c.surfaceRaised.name(), c.textPrimary.name(),
           c.borderDefault.name(), c.accentSoft.name(), c.textMuted.name());
}

QString HackerDialog::tabWidgetStyle() const {
  const auto &c = colors();
  return QString("QTabWidget::pane {"
                 "  border: 1px solid %1; background: %2; border-radius: 4px;"
                 "}"
                 "QTabBar::tab {"
                 "  background: %3; color: %4; padding: 8px 20px;"
                 "  border: none; border-bottom: 2px solid transparent;"
                 "}"
                 "QTabBar::tab:selected {"
                 "  background: %2; color: %5;"
                 "  border-bottom: 2px solid %6;"
                 "}"
                 "QTabBar::tab:hover:!selected {"
                 "  background: %7;"
                 "}")
      .arg(c.borderDefault.name(), c.surfaceBase.name(), c.surfaceRaised.name(),
           c.textMuted.name(), c.textPrimary.name(), c.accentPrimary.name(),
           c.treeHoverBg.name());
}

QString HackerDialog::groupBoxStyle() const {
  const auto &c = colors();
  int r = borderRadius();
  return QString("QGroupBox {"
                 "  background: %1; border: 1px solid %2;"
                 "  border-radius: %3px; margin-top: 16px;"
                 "  padding: 16px; padding-top: 28px;"
                 "  font-weight: bold; color: %4;"
                 "}"
                 "QGroupBox::title {"
                 "  subcontrol-origin: margin;"
                 "  subcontrol-position: top left;"
                 "  left: 12px; padding: 0 6px;"
                 "  color: %5; font-size: 11px;"
                 "}")
      .arg(c.surfaceRaised.name(), c.borderSubtle.name())
      .arg(r)
      .arg(c.textPrimary.name(), c.textMuted.name());
}

QString HackerDialog::textEditStyle() const {
  const auto &c = colors();
  int r = borderRadius();
  return QString("QPlainTextEdit, QTextEdit {"
                 "  background: %1; color: %2; border: 1px solid %3;"
                 "  border-radius: %4px; padding: 8px;"
                 "  font-family: monospace; font-size: 12px;"
                 "  selection-background-color: %5;"
                 "}"
                 "QPlainTextEdit:focus, QTextEdit:focus {"
                 "  border-color: %6;"
                 "}")
      .arg(c.inputBg.name(), c.inputFg.name(), c.inputBorder.name())
      .arg(r)
      .arg(c.inputSelection.name(), c.inputBorderFocus.name());
}

QString HackerDialog::spinBoxStyle() const {
  const auto &c = colors();
  int r = borderRadius();
  return QString("QSpinBox {"
                 "  background: %1; color: %2; border: 1px solid %3;"
                 "  border-radius: %4px; padding: 4px 8px;"
                 "  font-family: monospace;"
                 "}"
                 "QSpinBox:focus { border-color: %5; }"
                 "QSpinBox::up-button, QSpinBox::down-button {"
                 "  background: %6; border: none; width: 16px;"
                 "}"
                 "QSpinBox::up-button:hover, QSpinBox::down-button:hover {"
                 "  background: %7;"
                 "}")
      .arg(c.inputBg.name(), c.inputFg.name(), c.inputBorder.name())
      .arg(r)
      .arg(c.inputBorderFocus.name(), c.surfaceRaised.name(),
           c.treeHoverBg.name());
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
