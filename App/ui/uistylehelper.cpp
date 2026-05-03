#include "uistylehelper.h"
#include "../theme/themedefinition.h"
#include <QColor>

namespace {
QColor withAlpha(QColor color, qreal alpha) {
  color.setAlphaF(qBound(0.0, alpha, 1.0));
  return color;
}

QColor blend(const QColor &a, const QColor &b, qreal t) {
  t = qBound(0.0, t, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

ThemeColors derivedTheme(const Theme &theme) {
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
  c.accentGlow =
      withAlpha(theme.accentColor, 0.10 + 0.22 * theme.glowIntensity);
  c.tabBg = theme.backgroundColor;
  c.tabHoverBg = theme.hoverColor;
  c.tabFg = blend(theme.foregroundColor, theme.backgroundColor, 0.4);
  c.tabActiveFg = theme.foregroundColor;
  c.tabActiveBorder = theme.accentColor;
  c.treeSelectedBg = theme.accentSoftColor;
  c.treeHoverBg = theme.hoverColor;
  return c;
}

ThemeColors activeTheme(const Theme &theme) { return derivedTheme(theme); }

QString rgba(QColor color, qreal alpha) {
  color.setAlphaF(qBound(0.0, alpha, 1.0));
  return color.name(QColor::HexArgb);
}

QString chrome(const Theme &theme, const QColor &color) {
  return rgba(color, theme.chromeOpacity);
}

QString chrome(const ThemeDefinition &theme, const QColor &color) {
  return rgba(color, theme.ui.chromeOpacity);
}

QString fillStyleTemplate(
    QString style,
    std::initializer_list<std::pair<const char *, QString>> replacements) {
  for (const auto &[key, value] : replacements) {
    style.replace(QString("{%1}").arg(QString::fromLatin1(key)), value);
  }
  return style;
}

qreal glow(const Theme &theme) { return qBound(0.0, theme.glowIntensity, 1.0); }

qreal glow(const ThemeDefinition &theme) {
  return qBound(0.0, theme.ui.glowIntensity, 1.0);
}

QColor glowAccent(const Theme &theme, const ThemeColors &c) {
  QColor accent =
      c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor;
  QColor glowColor = c.accentGlow.isValid() ? c.accentGlow : accent;
  glowColor.setAlpha(255);
  const QColor luminous = accent.lighter(130 + qRound(glow(theme) * 35.0));
  return blend(glowColor, luminous, 0.45);
}

QColor glowAccent(const ThemeDefinition &theme, const ThemeColors &c) {
  QColor glowColor = c.accentGlow.isValid() ? c.accentGlow : c.accentPrimary;
  glowColor.setAlpha(255);
  const QColor luminous =
      c.accentPrimary.lighter(130 + qRound(glow(theme) * 35.0));
  return blend(glowColor, luminous, 0.45);
}

QColor emphasizedBorder(const Theme &theme, const ThemeColors &c,
                        const QColor &border) {
  return blend(border, glowAccent(theme, c), 0.08 + 0.22 * glow(theme));
}

QColor emphasizedBorder(const ThemeDefinition &theme, const ThemeColors &c,
                        const QColor &border) {
  return blend(border, glowAccent(theme, c), 0.08 + 0.22 * glow(theme));
}

QColor glowSurface(const Theme &theme, const ThemeColors &c, const QColor &base,
                   qreal strength = 1.0) {
  return blend(base, glowAccent(theme, c),
               (0.04 + 0.12 * glow(theme)) * strength);
}

QColor glowSurface(const ThemeDefinition &theme, const ThemeColors &c,
                   const QColor &base, qreal strength = 1.0) {
  return blend(base, glowAccent(theme, c),
               (0.04 + 0.12 * glow(theme)) * strength);
}

QColor glowFocus(const Theme &theme, const ThemeColors &c,
                 const QColor &accent) {
  return blend(accent, glowAccent(theme, c), 0.18 + 0.28 * glow(theme));
}

QColor glowFocus(const ThemeDefinition &theme, const ThemeColors &c,
                 const QColor &accent) {
  return blend(accent, glowAccent(theme, c), 0.18 + 0.28 * glow(theme));
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

QString borderRule(const ThemeDefinition &theme, const QColor &color,
                   const QString &edge = QString()) {
  if (!theme.ui.panelBorders)
    return edge.isEmpty() ? QString("border: none;")
                          : QString("border-%1: none;").arg(edge);
  return edge.isEmpty()
             ? QString("border: 1px solid %1;").arg(color.name())
             : QString("border-%1: 1px solid %2;").arg(edge, color.name());
}

int radius(const Theme &theme) { return qMax(0, theme.borderRadius); }

int radius(const ThemeDefinition &theme) {
  return qMax(0, theme.ui.borderRadius);
}
} // namespace

QString UIStyleHelper::popupDialogStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor bg = glowSurface(
      theme, c,
      c.surfacePopover.isValid() ? c.surfacePopover : theme.surfaceColor, 0.7);
  const QColor border = emphasizedBorder(
      theme, c,
      c.borderDefault.isValid() ? c.borderDefault : theme.borderColor);
  return QString("background: %1; "
                 "%2 "
                 "border-radius: %3px;")
      .arg(chrome(theme, bg))
      .arg(borderRule(theme, border))
      .arg(qMin(radius(theme) + 2, 10));
}

QString UIStyleHelper::popupDialogStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor bg = glowSurface(theme, c, c.surfacePopover, 0.7);
  const QColor border = emphasizedBorder(theme, c, c.borderDefault);
  return QString("background: %1; "
                 "%2 "
                 "border-radius: %3px;")
      .arg(chrome(theme, bg))
      .arg(borderRule(theme, border))
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

QString UIStyleHelper::searchBoxStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
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
      .arg(c.inputBorder.name())
      .arg(chrome(theme, c.inputBg))
      .arg(c.inputFg.name())
      .arg(c.inputBorderFocus.name())
      .arg(c.inputSelection.name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::resultListStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor base =
      c.surfaceBase.isValid() ? c.surfaceBase : theme.backgroundColor;
  const QColor border = emphasizedBorder(
      theme, c, c.borderSubtle.isValid() ? c.borderSubtle : theme.borderColor);
  const QColor selected = glowSurface(
      theme, c,
      c.treeSelectedBg.isValid() ? c.treeSelectedBg : theme.accentSoftColor,
      1.0);
  const QColor hover = glowSurface(
      theme, c, c.treeHoverBg.isValid() ? c.treeHoverBg : theme.hoverColor,
      0.9);
  return QString("QListWidget {"
                 "  %6"
                 "  border-radius: %7px;"
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
                 "  background: %8;"
                 "}")
      .arg(chrome(theme, glowSurface(theme, c, base, 0.35)))
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
               .name())
      .arg(border.name())
      .arg(selected.name())
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.accentColor).name())
      .arg(borderRule(theme, border))
      .arg(qMax(3, radius(theme)))
      .arg(hover.name());
}

QString UIStyleHelper::resultListStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor border = emphasizedBorder(theme, c, c.borderSubtle);
  const QColor selected = glowSurface(theme, c, c.treeSelectedBg, 1.0);
  const QColor hover = glowSurface(theme, c, c.treeHoverBg, 0.9);
  return QString("QListWidget {"
                 "  %6"
                 "  border-radius: %7px;"
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
                 "  background: %8;"
                 "}")
      .arg(chrome(theme, glowSurface(theme, c, c.surfaceBase, 0.35)))
      .arg(c.textPrimary.name())
      .arg(border.name())
      .arg(selected.name())
      .arg(c.textPrimary.name())
      .arg(borderRule(theme, border))
      .arg(qMax(3, radius(theme)))
      .arg(hover.name());
}

QString UIStyleHelper::panelHeaderStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor surface = glowSurface(
      theme, c,
      c.surfaceRaised.isValid() ? c.surfaceRaised : theme.surfaceColor, 0.5);
  const QColor border = emphasizedBorder(
      theme, c, c.borderSubtle.isValid() ? c.borderSubtle : theme.borderColor);
  return QString("background: %1; "
                 "%2")
      .arg(chrome(theme, surface))
      .arg(borderRule(theme, border, "bottom"));
}

QString UIStyleHelper::panelHeaderStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor surface = glowSurface(theme, c, c.surfaceRaised, 0.5);
  const QColor border = emphasizedBorder(theme, c, c.borderSubtle);
  return QString("background: %1; "
                 "%2")
      .arg(chrome(theme, surface))
      .arg(borderRule(theme, border, "bottom"));
}

QString UIStyleHelper::treeViewStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
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
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
               .name())
      .arg(chrome(theme, c.surfaceOverlay.isValid() ? c.surfaceOverlay
                                                    : theme.surfaceAltColor))
      .arg((c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor)
               .name())
      .arg((c.borderDefault.isValid() ? c.borderDefault : theme.borderColor)
               .name());
}

QString UIStyleHelper::contextMenuStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor bg = glowSurface(
      theme, c,
      c.surfaceRaised.isValid() ? c.surfaceRaised : theme.surfaceColor, 0.55);
  const QColor border = emphasizedBorder(
      theme, c,
      c.borderDefault.isValid() ? c.borderDefault : theme.borderColor);
  const QColor selected = glowSurface(
      theme, c, c.accentSoft.isValid() ? c.accentSoft : theme.accentSoftColor,
      1.0);
  return QString("QMenu {"
                 "  background: %1;"
                 "  color: %2;"
                 "  %6"
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
      .arg(chrome(theme, bg))
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
               .name())
      .arg(border.name())
      .arg(selected.name())
      .arg((c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor)
               .name())
      .arg(borderRule(theme, border));
}

QString UIStyleHelper::treeWidgetStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor border = emphasizedBorder(
      theme, c,
      c.borderDefault.isValid() ? c.borderDefault : theme.borderColor);
  const QColor selected = glowSurface(
      theme, c,
      c.treeSelectedBg.isValid() ? c.treeSelectedBg : theme.accentSoftColor,
      1.0);
  const QColor hover = glowSurface(
      theme, c, c.treeHoverBg.isValid() ? c.treeHoverBg : theme.accentSoftColor,
      0.9);
  return fillStyleTemplate(
      QStringLiteral("QTreeWidget {"
                     "  background: {bg};"
                     "  alternate-background-color: {altBg};"
                     "  color: {fg};"
                     "  {widgetBorder}"
                     "  border-radius: {radius}px;"
                     "  selection-background-color: {selectedBg};"
                     "  selection-color: {selectedFg};"
                     "}"
                     "QTreeWidget::item {"
                     "  padding: 4px;"
                     "}"
                     "QTreeWidget::item:selected {"
                     "  background: {selectedBg};"
                     "  color: {selectedFg};"
                     "}"
                     "QTreeWidget::item:!active:selected {"
                     "  background: {selectedBg};"
                     "  color: {selectedFg};"
                     "}"
                     "QTreeWidget::item:hover {"
                     "  background: {hoverBg};"
                     "}"
                     "QHeaderView::section {"
                     "  background: {headerBg};"
                     "  color: {headerFg};"
                     "  border: none;"
                     "  {headerBorder}"
                     "  padding: 4px;"
                     "}"),
      {{"bg", chrome(theme, glowSurface(theme, c,
                                        c.surfaceBase.isValid()
                                            ? c.surfaceBase
                                            : theme.backgroundColor,
                                        0.35))},
       {"altBg", chrome(theme, glowSurface(theme, c,
                                           c.surfaceOverlay.isValid()
                                               ? c.surfaceOverlay
                                               : theme.surfaceAltColor,
                                           0.3))},
       {"fg", (c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
                  .name()},
       {"widgetBorder", borderRule(theme, border)},
       {"radius", QString::number(qMax(3, radius(theme)))},
       {"selectedBg", selected.name()},
       {"selectedFg",
        (c.textInverse.isValid() ? c.textInverse : theme.accentColor).name()},
       {"hoverBg", hover.name()},
       {"headerBg", chrome(theme, glowSurface(theme, c,
                                              c.surfaceRaised.isValid()
                                                  ? c.surfaceRaised
                                                  : theme.surfaceColor,
                                              0.45))},
       {"headerFg",
        (c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor)
            .name()},
       {"headerBorder", borderRule(theme, border, "right")}});
}

QString UIStyleHelper::subduedLabelStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("color: %1;")
      .arg((c.textMuted.isValid() ? c.textMuted : theme.singleLineCommentFormat)
               .name());
}

QString UIStyleHelper::subduedLabelStyle(const ThemeDefinition &theme) {
  return QString("color: %1;").arg(theme.colors.textMuted.name());
}

QString UIStyleHelper::titleLabelStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("font-weight: bold; color: %1;")
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
               .name());
}

QString UIStyleHelper::titleLabelStyle(const ThemeDefinition &theme) {
  return QString("font-weight: bold; color: %1;")
      .arg(theme.colors.textPrimary.name());
}

QString UIStyleHelper::comboBoxStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor focus = glowFocus(
      theme, c,
      c.inputBorderFocus.isValid() ? c.inputBorderFocus : theme.accentColor);
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
      .arg(focus.name())
      .arg(theme.backgroundColor.name());
}

QString UIStyleHelper::comboBoxStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor focus = glowFocus(theme, c, c.inputBorderFocus);
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
      .arg(chrome(theme, c.inputBg))
      .arg(c.inputFg.name())
      .arg(c.inputBorder.name())
      .arg(c.inputSelection.name())
      .arg(qMax(3, radius(theme)))
      .arg(focus.name())
      .arg(c.textInverse.name());
}

QString UIStyleHelper::checkBoxStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
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
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
               .name())
      .arg((c.inputBorder.isValid() ? c.inputBorder : theme.borderColor).name())
      .arg(chrome(theme,
                  c.inputBg.isValid() ? c.inputBg : theme.surfaceAltColor))
      .arg((c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor)
               .name());
}

QString UIStyleHelper::checkBoxStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
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
      .arg(c.textPrimary.name())
      .arg(c.inputBorder.name())
      .arg(chrome(theme, c.inputBg))
      .arg(c.accentPrimary.name());
}

QString UIStyleHelper::formDialogStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor bg = glowSurface(
      theme, c, c.surfaceBase.isValid() ? c.surfaceBase : theme.backgroundColor,
      0.35);
  const QColor border = emphasizedBorder(
      theme, c, c.borderSubtle.isValid() ? c.borderSubtle : theme.borderColor);
  return QString("QDialog {"
                 "  background: %1;"
                 "  %2"
                 "  border-radius: %3px;"
                 "}")
      .arg(chrome(theme, bg))
      .arg(borderRule(theme, border))
      .arg(qMax(4, radius(theme)));
}

QString UIStyleHelper::formDialogStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor bg = glowSurface(theme, c, c.surfaceBase, 0.35);
  const QColor border = emphasizedBorder(theme, c, c.borderSubtle);
  return QString("QDialog {"
                 "  background: %1;"
                 "  %2"
                 "  border-radius: %3px;"
                 "}")
      .arg(chrome(theme, bg))
      .arg(borderRule(theme, border))
      .arg(qMax(4, radius(theme)));
}

QString UIStyleHelper::groupBoxStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor bg = glowSurface(
      theme, c,
      c.surfaceRaised.isValid() ? c.surfaceRaised : theme.surfaceColor, 0.45);
  const QColor border = emphasizedBorder(
      theme, c,
      c.borderDefault.isValid() ? c.borderDefault : theme.borderColor);
  const QColor accent = glowFocus(
      theme, c,
      c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor);
  return QString("QGroupBox {"
                 "  background: %1;"
                 "  %2"
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
      .arg(chrome(theme, bg))
      .arg(borderRule(theme, border))
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
               .name())
      .arg(accent.name());
}

QString UIStyleHelper::groupBoxStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor bg = glowSurface(theme, c, c.surfaceRaised, 0.45);
  const QColor border = emphasizedBorder(theme, c, c.borderDefault);
  const QColor accent = glowFocus(theme, c, c.accentPrimary);
  return QString("QGroupBox {"
                 "  background: %1;"
                 "  %2"
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
      .arg(chrome(theme, bg))
      .arg(borderRule(theme, border))
      .arg(c.textPrimary.name())
      .arg(accent.name());
}

QString UIStyleHelper::lineEditStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor focus = glowFocus(
      theme, c,
      c.inputBorderFocus.isValid() ? c.inputBorderFocus : theme.accentColor);
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
      .arg(focus.name())
      .arg((c.inputSelection.isValid() ? c.inputSelection
                                       : theme.accentSoftColor)
               .name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::lineEditStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor focus = glowFocus(theme, c, c.inputBorderFocus);
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
      .arg(chrome(theme, c.inputBg))
      .arg(c.inputFg.name())
      .arg(c.inputBorder.name())
      .arg(focus.name())
      .arg(c.inputSelection.name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::primaryButtonStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor hover =
      glowSurface(theme, c,
                  c.btnPrimaryHover.isValid() ? c.btnPrimaryHover
                                              : theme.accentColor.lighter(120),
                  1.0);
  const QColor pressed =
      glowSurface(theme, c,
                  c.btnPrimaryActive.isValid() ? c.btnPrimaryActive
                                               : theme.accentColor.darker(110),
                  0.9);
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
      .arg(hover.name())
      .arg(pressed.name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::primaryButtonStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor hover = glowSurface(theme, c, c.btnPrimaryHover, 1.0);
  const QColor pressed = glowSurface(theme, c, c.btnPrimaryActive, 0.9);
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
      .arg(c.btnPrimaryBg.name())
      .arg(c.btnPrimaryFg.name())
      .arg(hover.name())
      .arg(pressed.name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::secondaryButtonStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor border = emphasizedBorder(
      theme, c,
      c.borderDefault.isValid() ? c.borderDefault : theme.borderColor);
  const QColor hover =
      glowSurface(theme, c,
                  c.btnSecondaryHover.isValid() ? c.btnSecondaryHover
                                                : theme.accentSoftColor,
                  1.0);
  const QColor focus = glowFocus(
      theme, c, c.borderFocus.isValid() ? c.borderFocus : theme.accentColor);
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
      .arg(border.name())
      .arg(chrome(theme, hover))
      .arg(focus.name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::secondaryButtonStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor border = emphasizedBorder(theme, c, c.borderDefault);
  const QColor hover = glowSurface(theme, c, c.btnSecondaryHover, 1.0);
  const QColor focus = glowFocus(theme, c, c.borderFocus);
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
      .arg(chrome(theme, c.btnSecondaryBg))
      .arg(c.btnSecondaryFg.name())
      .arg(border.name())
      .arg(chrome(theme, hover))
      .arg(focus.name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::breadcrumbButtonStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
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
      .arg((c.textMuted.isValid() ? c.textMuted : theme.singleLineCommentFormat)
               .name())
      .arg((c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor)
               .name())
      .arg((c.accentSoft.isValid() ? c.accentSoft : theme.accentSoftColor)
               .name());
}

QString UIStyleHelper::breadcrumbButtonStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
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
      .arg(c.textMuted.name())
      .arg(c.accentPrimary.name())
      .arg(c.accentSoft.name());
}

QString UIStyleHelper::breadcrumbActiveButtonStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor accent =
      c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor;
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
      .arg(accent.name())
      .arg(accent.lighter(115).name())
      .arg((c.accentSoft.isValid() ? c.accentSoft : theme.accentSoftColor)
               .name());
}

QString
UIStyleHelper::breadcrumbActiveButtonStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
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
      .arg(c.accentPrimary.name())
      .arg(c.accentPrimary.lighter(115).name())
      .arg(c.accentSoft.name());
}

QString UIStyleHelper::breadcrumbSeparatorStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("QLabel {"
                 "  color: %1;"
                 "  font-family: 'Ubuntu Mono', 'JetBrains Mono', 'Monospace';"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "  padding: 0 2px;"
                 "}")
      .arg((c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor)
               .name());
}

QString UIStyleHelper::breadcrumbSeparatorStyle(const ThemeDefinition &theme) {
  return QString("QLabel {"
                 "  color: %1;"
                 "  font-family: 'Ubuntu Mono', 'JetBrains Mono', 'Monospace';"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "  padding: 0 2px;"
                 "}")
      .arg(theme.colors.accentPrimary.name());
}

QString UIStyleHelper::infoLabelStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("color: %1; font-size: 11px;")
      .arg((c.textMuted.isValid() ? c.textMuted : theme.singleLineCommentFormat)
               .name());
}

QString UIStyleHelper::infoLabelStyle(const ThemeDefinition &theme) {
  return QString("color: %1; font-size: 11px;")
      .arg(theme.colors.textMuted.name());
}

QString UIStyleHelper::successInfoLabelStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("color: %1; font-size: 11px;")
      .arg((c.statusSuccess.isValid() ? c.statusSuccess : theme.successColor)
               .name());
}

QString UIStyleHelper::successInfoLabelStyle(const ThemeDefinition &theme) {
  return QString("color: %1; font-size: 11px;")
      .arg(theme.colors.statusSuccess.name());
}

QString UIStyleHelper::errorInfoLabelStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  return QString("color: %1; font-size: 11px;")
      .arg((c.statusError.isValid() ? c.statusError : theme.errorColor).name());
}

QString UIStyleHelper::errorInfoLabelStyle(const ThemeDefinition &theme) {
  return QString("color: %1; font-size: 11px;")
      .arg(theme.colors.statusError.name());
}

QString UIStyleHelper::tabWidgetStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor paneBorder = emphasizedBorder(
      theme, c, c.borderSubtle.isValid() ? c.borderSubtle : theme.borderColor);
  const QColor activeBorder = glowFocus(
      theme, c,
      c.tabActiveBorder.isValid() ? c.tabActiveBorder : theme.accentColor);
  const QColor hover = glowSurface(
      theme, c, c.tabHoverBg.isValid() ? c.tabHoverBg : theme.hoverColor, 1.0);
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
      .arg(borderRule(theme, paneBorder))
      .arg(chrome(theme, c.surfaceRaised.isValid() ? c.surfaceRaised
                                                   : theme.surfaceColor))
      .arg(chrome(theme, c.tabBg.isValid() ? c.tabBg : theme.surfaceAltColor))
      .arg((c.tabFg.isValid() ? c.tabFg : theme.singleLineCommentFormat).name())
      .arg((c.tabActiveFg.isValid() ? c.tabActiveFg : theme.foregroundColor)
               .name())
      .arg(activeBorder.name())
      .arg(chrome(theme, hover))
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::tabWidgetStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor paneBorder = emphasizedBorder(theme, c, c.borderSubtle);
  const QColor activeBorder = glowFocus(theme, c, c.tabActiveBorder);
  const QColor hover = glowSurface(theme, c, c.tabHoverBg, 1.0);
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
      .arg(borderRule(theme, paneBorder))
      .arg(chrome(theme, c.surfaceRaised))
      .arg(chrome(theme, c.tabBg))
      .arg(c.tabFg.name())
      .arg(c.tabActiveFg.name())
      .arg(activeBorder.name())
      .arg(chrome(theme, hover))
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::tableWidgetStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor border = emphasizedBorder(
      theme, c,
      c.borderDefault.isValid() ? c.borderDefault : theme.borderColor);
  const QColor selected = glowSurface(
      theme, c, c.accentSoft.isValid() ? c.accentSoft : theme.accentSoftColor,
      1.0);
  return QString("QTableWidget {"
                 "  background: %1;"
                 "  color: %2;"
                 "  %8"
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
      .arg(chrome(theme, c.surfaceOverlay.isValid() ? c.surfaceOverlay
                                                    : theme.surfaceAltColor))
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
               .name())
      .arg(border.name())
      .arg(selected.name())
      .arg(chrome(theme, c.surfaceRaised.isValid() ? c.surfaceRaised
                                                   : theme.surfaceColor))
      .arg((c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor)
               .name())
      .arg((c.textInverse.isValid() ? c.textInverse : theme.accentColor).name())
      .arg(borderRule(theme, border));
}

QString UIStyleHelper::tableWidgetStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor border = emphasizedBorder(theme, c, c.borderDefault);
  const QColor selected = glowSurface(theme, c, c.accentSoft, 1.0);
  return QString("QTableWidget {"
                 "  background: %1;"
                 "  color: %2;"
                 "  %8"
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
      .arg(chrome(theme, c.surfaceOverlay))
      .arg(c.textPrimary.name())
      .arg(border.name())
      .arg(selected.name())
      .arg(chrome(theme, c.surfaceRaised))
      .arg(c.accentPrimary.name())
      .arg(c.textInverse.name())
      .arg(borderRule(theme, border));
}

QString UIStyleHelper::plainTextEditStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor focus = glowFocus(
      theme, c,
      c.inputBorderFocus.isValid() ? c.inputBorderFocus : theme.accentColor);
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
      .arg(focus.name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::plainTextEditStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor focus = glowFocus(theme, c, c.inputBorderFocus);
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
      .arg(chrome(theme, c.inputBg))
      .arg(c.inputFg.name())
      .arg(c.inputBorder.name())
      .arg(focus.name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::spinBoxStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor focus = glowFocus(
      theme, c,
      c.inputBorderFocus.isValid() ? c.inputBorderFocus : theme.accentColor);
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
      .arg(chrome(theme,
                  c.inputBg.isValid() ? c.inputBg : theme.surfaceAltColor))
      .arg((c.inputFg.isValid() ? c.inputFg : theme.foregroundColor).name())
      .arg((c.inputBorder.isValid() ? c.inputBorder : theme.borderColor).name())
      .arg(focus.name());
}

QString UIStyleHelper::spinBoxStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor focus = glowFocus(theme, c, c.inputBorderFocus);
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
      .arg(chrome(theme, c.inputBg))
      .arg(c.inputFg.name())
      .arg(c.inputBorder.name())
      .arg(focus.name());
}

QString UIStyleHelper::dangerButtonStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
  const QColor hover =
      glowSurface(theme, c,
                  c.btnDangerHover.isValid() ? c.btnDangerHover
                                             : theme.errorColor.lighter(110),
                  1.0);
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
      .arg(hover.name())
      .arg((c.btnDangerFg.isValid() ? c.btnDangerFg : QColor("#ffffff")).name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::dangerButtonStyle(const ThemeDefinition &theme) {
  const ThemeColors &c = theme.colors;
  const QColor hover = glowSurface(theme, c, c.btnDangerHover, 1.0);
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
      .arg(c.btnDangerBg.name())
      .arg(hover.name())
      .arg(c.btnDangerFg.name())
      .arg(qMax(3, radius(theme)));
}

QString UIStyleHelper::progressBarStyle(const Theme &theme) {
  const ThemeColors c = activeTheme(theme);
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
      .arg(chrome(theme, c.surfaceOverlay.isValid() ? c.surfaceOverlay
                                                    : theme.surfaceAltColor))
      .arg((c.borderDefault.isValid() ? c.borderDefault : theme.borderColor)
               .name())
      .arg((c.textPrimary.isValid() ? c.textPrimary : theme.foregroundColor)
               .name())
      .arg((c.accentPrimary.isValid() ? c.accentPrimary : theme.accentColor)
               .name());
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
