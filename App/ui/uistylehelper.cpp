#include "uistylehelper.h"
#include "../theme/themedefinition.h"

#include <QColor>

namespace {
QColor blend(const QColor &a, const QColor &b, qreal t) {
  t = qBound(0.0, t, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

ThemeColors activeTheme(const Theme &theme) {
  ThemeColors c;
  c.surfaceBase = theme.backgroundColor;
  c.surfaceRaised = theme.surfaceColor;
  c.surfaceOverlay = theme.surfaceAltColor;
  c.surfacePopover = theme.surfaceColor;
  c.borderDefault = theme.borderColor;
  c.borderSubtle = blend(theme.borderColor, theme.backgroundColor, 0.35);
  c.borderFocus = theme.accentColor;
  c.textPrimary = theme.foregroundColor;
  c.textSecondary = blend(theme.foregroundColor, theme.backgroundColor, 0.35);
  c.textMuted = blend(theme.foregroundColor, theme.backgroundColor, 0.55);
  c.inputBg = theme.surfaceColor;
  c.inputFg = theme.foregroundColor;
  c.inputBorder = theme.borderColor;
  c.inputBorderFocus = theme.accentColor;
  c.inputSelection = theme.accentSoftColor;
  c.btnPrimaryBg = theme.accentColor;
  c.btnPrimaryFg = theme.backgroundColor;
  c.btnPrimaryHover = blend(theme.accentColor, theme.foregroundColor, 0.15);
  c.btnPrimaryActive = blend(theme.accentColor, theme.backgroundColor, 0.2);
  c.btnSecondaryBg = theme.surfaceColor;
  c.btnSecondaryFg = theme.foregroundColor;
  c.btnSecondaryHover = theme.hoverColor;
  c.btnDangerBg = theme.errorColor;
  c.btnDangerFg = QColor("#ffffff");
  c.btnDangerHover = blend(theme.errorColor, theme.foregroundColor, 0.15);
  c.btnGhostHover = theme.hoverColor;
  c.btnGhostActive = theme.pressedColor;
  c.accentPrimary = theme.accentColor;
  c.accentSoft = theme.accentSoftColor;
  c.tabBg = theme.backgroundColor;
  c.tabHoverBg = theme.hoverColor;
  c.tabFg = blend(theme.foregroundColor, theme.backgroundColor, 0.4);
  c.tabActiveFg = theme.foregroundColor;
  c.tabActiveBorder = theme.accentColor;
  c.treeSelectedBg = theme.accentSoftColor;
  c.treeHoverBg = theme.hoverColor;
  return c;
}

QString rgba(QColor color, qreal alpha) {
  color.setAlphaF(qBound(0.0, alpha, 1.0));
  return color.name(QColor::HexArgb);
}

QString chrome(const Theme &theme, const QColor &color) {
  return rgba(color, theme.chromeOpacity);
}

QString borderRule(const Theme &theme, const QColor &color,
                   const QString &edge = QString()) {
  if (!theme.panelBorders)
    return edge.isEmpty() ? QString("border: none;")
                          : QString("border-%1: none;").arg(edge);
  return edge.isEmpty()
             ? QString("border: 1px solid %1;").arg(color.name())
             : QString("border-%1: 1px solid %2;").arg(edge, color.name());
}

int radius(const Theme &theme) { return qMax(0, theme.borderRadius); }
} // namespace

QString UIStyleHelper::popupDialogStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("background: %1; "
                 "%2 "
                 "border-radius: %3px;")
      .arg(chrome(theme, c.surfacePopover.isValid() ? c.surfacePopover
                                                    : theme.surfaceColor))
      .arg(borderRule(theme, c.borderDefault.isValid() ? c.borderDefault
                                                       : theme.borderColor))
      .arg(qMin(radius(theme) + 2, 10));
}

QString UIStyleHelper::searchBoxStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QLineEdit {"
                 "  padding: 8px;"
                 "  font-size: 14px;"
                 "  border: 1px solid %1;"
                 "  border-radius: %6px;"
                 "  background: %2;"
                 "  color: %3;"
                 "  selection-background-color: %5;"
                 "  selection-color: %4;"
                 "}"
                 "QLineEdit:focus {"
                 "  border-color: %4;"
                 "}")
      .arg((c.inputBorder.isValid() ? c.inputBorder : theme.borderColor).name())
      .arg(chrome(theme, c.inputBg.isValid() ? c.inputBg : theme.surfaceColor))
      .arg((c.inputFg.isValid() ? c.inputFg : theme.foregroundColor).name())
      .arg((c.inputBorderFocus.isValid() ? c.inputBorderFocus
                                         : theme.accentColor)
               .name())
      .arg((c.inputSelection.isValid() ? c.inputSelection
                                       : theme.accentSoftColor)
               .name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::resultListStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QListWidget {"
                 "  border: none;"
                 "  background: %1;"
                 "  color: %2;"
                 "}"
                 "QListWidget::item {"
                 "  padding: 8px;"
                 "  border-bottom: 1px solid %3;"
                 "}"
                 "QListWidget::item:selected {"
                 "  background: %4;"
                 "  color: %5;"
                 "}"
                 "QListWidget::item:hover {"
                 "  background: %4;"
                 "}")
      .arg(chrome(theme, c.surfaceBase.isValid() ? c.surfaceBase
                                                 : theme.backgroundColor))
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
               .name())
      .arg((c.borderSubtle.isValid() ? c.borderSubtle : theme.borderColor)
               .name())
      .arg((c.treeSelectedBg.isValid() ? c.treeSelectedBg
                                       : theme.accentSoftColor)
               .name())
      .arg(
          (c.textPrimary.isValid() ? c.textPrimary : theme.accentColor).name());
}

QString UIStyleHelper::panelHeaderStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("background: %1; "
                 "%2")
      .arg(chrome(theme, c.surfaceRaised.isValid() ? c.surfaceRaised
                                                   : theme.surfaceColor))
      .arg(borderRule(
          theme, c.borderSubtle.isValid() ? c.borderSubtle : theme.borderColor,
          "bottom"));
}

QString UIStyleHelper::treeViewStyle(const Theme &theme) {
  return QString("QTreeView {"
                 "  background: transparent;"
                 "  color: %1;"
                 "  border: none;"
                 "  outline: none;"
                 "  font-size: 12px;"
                 "  padding: 6px 4px;"
                 "}"
                 "QTreeView::item {"
                 "  padding: 6px 8px;"
                 "  border-radius: 8px;"
                 "  margin: 1px 2px;"
                 "}"
                 "QTreeView::item:focus {"
                 "  outline: none;"
                 "  border: none;"
                 "}"
                 "QTreeView::item:selected {"
                 "  background: transparent;"
                 "  color: %1;"
                 "}"
                 "QTreeView::item:hover:!selected {"
                 "  background: transparent;"
                 "}"
                 "QTreeView::branch {"
                 "  background: transparent;"
                 "  border: none;"
                 "}"
                 "QTreeView::branch:has-children:closed,"
                 "QTreeView::branch:closed:has-children:has-siblings {"
                 "  image: url(:/resources/icons/branch_closed.png);"
                 "}"
                 "QTreeView::branch:has-children:open,"
                 "QTreeView::branch:open:has-children:has-siblings {"
                 "  image: url(:/resources/icons/branch_open.png);"
                 "}"
                 "QTreeView::branch:selected {"
                 "  background: transparent;"
                 "  border: none;"
                 "}"
                 "QTreeView::branch:hover {"
                 "  background: transparent;"
                 "}"
                 "QHeaderView::section {"
                 "  background: %2;"
                 "  color: %3;"
                 "  border: none;"
                 "  border-bottom: 1px solid %4;"
                 "  padding: 6px 12px;"
                 "  font-size: 11px;"
                 "  font-weight: bold;"
                 "  text-transform: uppercase;"
                 "}"
                 "QScrollBar:vertical {"
                 "  background: transparent;"
                 "  width: 6px;"
                 "  margin: 0;"
                 "}"
                 "QScrollBar::handle:vertical {"
                 "  background: %4;"
                 "  min-height: 20px;"
                 "  border-radius: 4px;"
                 "}"
                 "QScrollBar::handle:vertical:hover {"
                 "  background: %3;"
                 "}"
                 "QScrollBar::add-line:vertical,"
                 "QScrollBar::sub-line:vertical {"
                 "  height: 0;"
                 "}"
                 "QScrollBar:horizontal {"
                 "  background: transparent;"
                 "  height: 6px;"
                 "  margin: 0;"
                 "}"
                 "QScrollBar::handle:horizontal {"
                 "  background: %4;"
                 "  min-width: 20px;"
                 "  border-radius: 4px;"
                 "}"
                 "QScrollBar::handle:horizontal:hover {"
                 "  background: %3;"
                 "}"
                 "QScrollBar::add-line:horizontal,"
                 "QScrollBar::sub-line:horizontal {"
                 "  width: 0;"
                 "}")
      .arg(theme.foregroundColor.name())
      .arg(theme.surfaceAltColor.name())
      .arg(theme.accentColor.name())
      .arg(theme.borderColor.name());
}

QString UIStyleHelper::contextMenuStyle(const Theme &theme) {
  return QString("QMenu {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: 6px;"
                 "  padding: 4px;"
                 "}"
                 "QMenu::item {"
                 "  padding: 6px 24px 6px 8px;"
                 "  border-radius: 4px;"
                 "  margin: 1px 4px;"
                 "}"
                 "QMenu::item:selected {"
                 "  background: %4;"
                 "  color: %5;"
                 "}"
                 "QMenu::separator {"
                 "  height: 1px;"
                 "  background: %3;"
                 "  margin: 4px 8px;"
                 "}")
      .arg(theme.surfaceColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::treeWidgetStyle(const Theme &theme) {
  return QString("QTreeWidget {"
                 "  background: %1;"
                 "  alternate-background-color: %8;"
                 "  color: %2;"
                 "  border: none;"
                 "  selection-background-color: %3;"
                 "  selection-color: %9;"
                 "}"
                 "QTreeWidget::item {"
                 "  padding: 4px;"
                 "}"
                 "QTreeWidget::item:selected {"
                 "  background: %3;"
                 "  color: %9;"
                 "}"
                 "QTreeWidget::item:!active:selected {"
                 "  background: %3;"
                 "  color: %9;"
                 "}"
                 "QTreeWidget::item:hover {"
                 "  background: %4;"
                 "}"
                 "QHeaderView::section {"
                 "  background: %5;"
                 "  color: %6;"
                 "  padding: 4px;"
                 "  border: none;"
                 "  border-right: 1px solid %7;"
                 "}")
      .arg(theme.backgroundColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.surfaceColor.name())
      .arg(theme.accentColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.surfaceAltColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::subduedLabelStyle(const Theme &theme) {
  return QString("color: %1;").arg(theme.singleLineCommentFormat.name());
}

QString UIStyleHelper::titleLabelStyle(const Theme &theme) {
  return QString("font-weight: bold; color: %1;")
      .arg(theme.foregroundColor.name());
}

QString UIStyleHelper::comboBoxStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QComboBox {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  padding: 6px 10px;"
                 "  min-height: 20px;"
                 "  border-radius: %5px;"
                 "}"
                 "QComboBox:hover {"
                 "  border-color: %6;"
                 "}"
                 "QComboBox:focus {"
                  "  border-color: %6;"
                  "}"
                  "QComboBox::drop-down {"
                 "  border: none;"
                 "  width: 24px;"
                  "}"
                  "QComboBox QAbstractItemView {"
                  "  background: %1;"
                  "  color: %2;"
                  "  border: 1px solid %3;"
                  "  selection-background-color: %4;"
                 "  selection-color: %7;"
                 "  padding: 4px;"
                 "  outline: none;"
                 "}"
                 "QComboBox QAbstractItemView::item {"
                 "  min-height: 22px;"
                 "  padding: 6px 10px;"
                 "  color: %2;"
                 "  background: %1;"
                 "}"
                 "QComboBox QAbstractItemView::item:selected {"
                 "  background: %4;"
                 "  color: %7;"
                 "}")
      .arg(chrome(theme,
                  c.inputBg.isValid() ? c.inputBg : theme.surfaceAltColor))
      .arg((c.inputFg.isValid() ? c.inputFg : theme.foregroundColor).name())
      .arg((c.inputBorder.isValid() ? c.inputBorder : theme.borderColor).name())
      .arg((c.inputSelection.isValid() ? c.inputSelection
                                       : theme.accentSoftColor)
               .name())
      .arg(qMax(3, radius(theme)))
      .arg((c.inputBorderFocus.isValid() ? c.inputBorderFocus
                                          : theme.accentColor)
               .name())
      .arg(theme.backgroundColor.name());
}

QString UIStyleHelper::checkBoxStyle(const Theme &theme) {
  return QString("QCheckBox {"
                 "  color: %1;"
                 "  spacing: 8px;"
                 "}"
                 "QCheckBox::indicator {"
                 "  width: 16px;"
                 "  height: 16px;"
                 "  border-radius: 4px;"
                 "  border: 1px solid %2;"
                 "  background: %3;"
                 "}"
                 "QCheckBox::indicator:checked {"
                 "  background: %4;"
                 "  border-color: %4;"
                 "}"
                 "QCheckBox::indicator:hover {"
                 "  border-color: %4;"
                 "}")
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.surfaceAltColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::formDialogStyle(const Theme &theme) {
  return QString("QDialog {"
                 "  background: %1;"
                 "}")
      .arg(theme.backgroundColor.name());
}

QString UIStyleHelper::groupBoxStyle(const Theme &theme) {
  return QString("QGroupBox {"
                 "  background: %1;"
                 "  border: 1px solid %2;"
                 "  border-radius: 6px;"
                 "  margin-top: 12px;"
                 "  padding: 12px;"
                 "  padding-top: 24px;"
                 "  font-weight: bold;"
                 "  color: %3;"
                 "}"
                 "QGroupBox::title {"
                 "  subcontrol-origin: margin;"
                 "  subcontrol-position: top left;"
                 "  left: 12px;"
                 "  padding: 0 6px;"
                 "  color: %4;"
                 "  font-size: 11px;"
                 "  text-transform: uppercase;"
                 "  letter-spacing: 1px;"
                 "}")
      .arg(theme.surfaceColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::lineEditStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QLineEdit {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: %6px;"
                 "  padding: 8px 12px;"
                 "  font-size: 12px;"
                 "  selection-background-color: %5;"
                 "  selection-color: %4;"
                 "}"
                 "QLineEdit:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(chrome(theme, c.inputBg.isValid() ? c.inputBg : theme.surfaceColor))
      .arg((c.inputFg.isValid() ? c.inputFg : theme.foregroundColor).name())
      .arg((c.inputBorder.isValid() ? c.inputBorder : theme.borderColor).name())
      .arg((c.inputBorderFocus.isValid() ? c.inputBorderFocus
                                         : theme.accentColor)
               .name())
      .arg((c.inputSelection.isValid() ? c.inputSelection
                                       : theme.accentSoftColor)
               .name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::primaryButtonStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QPushButton {"
                 "  background: %1;"
                 "  border: 1px solid %1;"
                 "  color: %2;"
                 "  border-radius: %5px;"
                 "  padding: 8px 16px;"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "}"
                 "QPushButton:hover {"
                 "  background: %3;"
                 "  border-color: %3;"
                 "}"
                 "QPushButton:pressed {"
                 "  background: %4;"
                 "  border-color: %4;"
                 "}")
      .arg((c.btnPrimaryBg.isValid() ? c.btnPrimaryBg : theme.accentColor)
               .name())
      .arg((c.btnPrimaryFg.isValid() ? c.btnPrimaryFg : theme.backgroundColor)
               .name())
      .arg((c.btnPrimaryHover.isValid() ? c.btnPrimaryHover
                                        : theme.accentColor.lighter(120))
               .name())
      .arg((c.btnPrimaryActive.isValid() ? c.btnPrimaryActive
                                         : theme.accentColor.darker(110))
               .name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::secondaryButtonStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QPushButton {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: %6px;"
                 "  padding: 8px 16px;"
                 "  font-size: 12px;"
                 "}"
                 "QPushButton:hover {"
                 "  background: %4;"
                 "  border-color: %5;"
                 "  color: %5;"
                 "}")
      .arg(chrome(theme, c.btnSecondaryBg.isValid() ? c.btnSecondaryBg
                                                    : theme.surfaceColor))
      .arg((c.btnSecondaryFg.isValid() ? c.btnSecondaryFg
                                       : theme.foregroundColor)
               .name())
      .arg((c.borderDefault.isValid() ? c.borderDefault : theme.borderColor)
               .name())
      .arg(chrome(theme, c.btnSecondaryHover.isValid() ? c.btnSecondaryHover
                                                       : theme.accentSoftColor))
      .arg((c.borderFocus.isValid() ? c.borderFocus : theme.accentColor).name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::breadcrumbButtonStyle(const Theme &theme) {
  return QString("QPushButton {"
                 "  background: transparent;"
                 "  color: %1;"
                 "  border: none;"
                 "  padding: 2px 8px;"
                 "  font-family: 'Ubuntu Mono', 'JetBrains Mono', 'Monospace';"
                 "  font-size: 12px;"
                 "}"
                 "QPushButton:hover {"
                 "  color: %2;"
                 "  background: %3;"
                 "  border-radius: 3px;"
                 "}")
      .arg(theme.singleLineCommentFormat.name())
      .arg(theme.accentColor.name())
      .arg(theme.accentSoftColor.name());
}

QString UIStyleHelper::breadcrumbActiveButtonStyle(const Theme &theme) {
  return QString("QPushButton {"
                 "  background: transparent;"
                 "  color: %1;"
                 "  border: none;"
                 "  padding: 2px 8px;"
                 "  font-family: 'Ubuntu Mono', 'JetBrains Mono', 'Monospace';"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "}"
                 "QPushButton:hover {"
                 "  color: %2;"
                 "  background: %3;"
                 "  border-radius: 3px;"
                 "}")
      .arg(theme.accentColor.name())
      .arg(theme.accentColor.lighter(115).name())
      .arg(theme.accentSoftColor.name());
}

QString UIStyleHelper::breadcrumbSeparatorStyle(const Theme &theme) {
  return QString("QLabel {"
                 "  color: %1;"
                 "  font-family: 'Ubuntu Mono', 'JetBrains Mono', 'Monospace';"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "  padding: 0 2px;"
                 "}")
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::infoLabelStyle(const Theme &theme) {
  return QString("color: %1; font-size: 11px;")
      .arg(theme.singleLineCommentFormat.name());
}

QString UIStyleHelper::successInfoLabelStyle(const Theme &theme) {
  return QString("color: %1; font-size: 11px;").arg(theme.successColor.name());
}

QString UIStyleHelper::errorInfoLabelStyle(const Theme &theme) {
  return QString("color: %1; font-size: 11px;").arg(theme.errorColor.name());
}

QString UIStyleHelper::tabWidgetStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QTabWidget::pane {"
                 "  %1"
                 "  background: %2;"
                 "  border-radius: %8px;"
                 "}"
                 "QTabBar {"
                 "  background: %3;"
                 "  border: none;"
                 "  qproperty-drawBase: 0;"
                 "}"
                 "QTabBar::base {"
                 "  height: 0px;"
                 "  border: none;"
                 "  background: transparent;"
                 "}"
                 "QTabBar::tab {"
                 "  background: %3;"
                 "  color: %4;"
                 "  padding: 8px 20px;"
                 "  border: none;"
                 "  border-bottom: 2px solid transparent;"
                 "}"
                 "QTabBar::tab:selected {"
                 "  background: %2;"
                 "  color: %5;"
                 "  border-bottom: 2px solid %6;"
                 "}"
                 "QTabBar::tab:hover:!selected {"
                 "  background: %7;"
                 "  color: %5;"
                 "}")
      .arg(borderRule(theme, c.borderSubtle.isValid() ? c.borderSubtle
                                                      : theme.borderColor))
      .arg(chrome(theme, c.surfaceRaised.isValid() ? c.surfaceRaised
                                                   : theme.surfaceColor))
      .arg(chrome(theme, c.tabBg.isValid() ? c.tabBg : theme.surfaceAltColor))
      .arg((c.tabFg.isValid() ? c.tabFg : theme.singleLineCommentFormat).name())
      .arg((c.tabActiveFg.isValid() ? c.tabActiveFg : theme.foregroundColor)
               .name())
      .arg((c.tabActiveBorder.isValid() ? c.tabActiveBorder : theme.accentColor)
               .name())
      .arg(chrome(theme,
                  c.tabHoverBg.isValid() ? c.tabHoverBg : theme.hoverColor))
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::tableWidgetStyle(const Theme &theme) {
  return QString("QTableWidget {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: 4px;"
                 "  gridline-color: %3;"
                 "}"
                 "QTableWidget::item {"
                 "  padding: 4px;"
                 "}"
                 "QTableWidget::item:selected {"
                 "  background: %4;"
                 "  color: %7;"
                 "}"
                 "QHeaderView::section {"
                 "  background: %5;"
                 "  color: %6;"
                 "  border: none;"
                 "  border-bottom: 1px solid %3;"
                 "  padding: 4px 8px;"
                 "  font-size: 11px;"
                 "  text-transform: uppercase;"
                 "  letter-spacing: 1px;"
                 "}")
      .arg(theme.surfaceAltColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.surfaceColor.name())
      .arg(theme.accentColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::plainTextEditStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QPlainTextEdit, QTextEdit {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: %5px;"
                 "  padding: 8px;"
                 "  font-family: monospace;"
                 "  font-size: 12px;"
                 "}"
                 "QPlainTextEdit:focus, QTextEdit:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(chrome(theme,
                  c.inputBg.isValid() ? c.inputBg : theme.surfaceAltColor))
      .arg((c.inputFg.isValid() ? c.inputFg : theme.foregroundColor).name())
      .arg((c.inputBorder.isValid() ? c.inputBorder : theme.borderColor).name())
      .arg((c.inputBorderFocus.isValid() ? c.inputBorderFocus
                                         : theme.accentColor)
               .name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::spinBoxStyle(const Theme &theme) {
  return QString("QSpinBox {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: 4px;"
                 "  padding: 4px 8px;"
                 "}"
                 "QSpinBox:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(theme.surfaceAltColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::dangerButtonStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QPushButton {"
                 "  background: %1;"
                 "  color: %3;"
                 "  border: 1px solid %1;"
                 "  border-radius: %4px;"
                 "  padding: 8px 16px;"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "}"
                 "QPushButton:hover {"
                 "  background: %2;"
                 "  border-color: %2;"
                 "}")
      .arg((c.btnDangerBg.isValid() ? c.btnDangerBg : theme.errorColor).name())
      .arg((c.btnDangerHover.isValid() ? c.btnDangerHover
                                       : theme.errorColor.lighter(110))
               .name())
      .arg((c.btnDangerFg.isValid() ? c.btnDangerFg : QColor("#ffffff")).name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::progressBarStyle(const Theme &theme) {
  return QString("QProgressBar {"
                 "  background: %1;"
                 "  border: 1px solid %2;"
                 "  border-radius: 4px;"
                 "  text-align: center;"
                 "  color: %3;"
                 "  height: 8px;"
                 "}"
                 "QProgressBar::chunk {"
                 "  background: %4;"
                 "  border-radius: 3px;"
                 "}")
      .arg(theme.surfaceAltColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::toolBarStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QToolBar {"
                 "  background: %1;"
                 "  border: none;"
                 "  spacing: 2px;"
                 "  padding: 2px;"
                 "}"
                 "QToolButton {"
                 "  background: transparent;"
                 "  color: %2;"
                 "  border: 1px solid transparent;"
                 "  border-radius: %8px;"
                 "  padding: 4px 8px;"
                 "}"
                 "QToolButton:hover {"
                 "  background: %3;"
                 "  border-color: %4;"
                 "  color: %4;"
                 "}"
                 "QToolButton:pressed {"
                 "  background: %5;"
                 "}"
                 "QToolButton:checked {"
                 "  background: %6;"
                 "  border-color: %7;"
                 "}")
      .arg(chrome(theme, c.surfaceRaised.isValid() ? c.surfaceRaised
                                                   : theme.surfaceColor))
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
               .name())
      .arg(chrome(theme, c.btnGhostHover.isValid() ? c.btnGhostHover
                                                   : theme.hoverColor))
      .arg((c.borderDefault.isValid() ? c.borderDefault : theme.borderColor)
               .name())
      .arg(chrome(theme, c.btnGhostActive.isValid() ? c.btnGhostActive
                                                    : theme.pressedColor))
      .arg(chrome(theme, c.accentSoft.isValid() ? c.accentSoft
                                                : theme.accentSoftColor))
      .arg((c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor)
               .name())
      .arg(qMax(3, radius(theme)));
}
