#include "uistylehelper.h"
#include "../theme/colorcontrast.h"
#include "../theme/themedefinition.h"
#include "uimetrics.h"
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>

namespace {

QString chevronImagePath(const QColor &color) {
  static QHash<QRgb, QString> cache;
  const auto cached = cache.constFind(color.rgba());
  if (cached != cache.constEnd()) {
    return *cached;
  }

  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
      QStringLiteral("/chrome");
  if (!QDir().mkpath(dir)) {
    cache.insert(color.rgba(), QString());
    return QString();
  }
  const QString path = QStringLiteral("%1/chevron-%2.png")
                           .arg(dir, QString::number(color.rgba(), 16));

  if (!QFileInfo::exists(path)) {

    const int scale = 4;
    QImage image(9 * scale, 6 * scale, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath triangle;
    triangle.moveTo(0.5 * scale, 1.5 * scale);
    triangle.lineTo(8.5 * scale, 1.5 * scale);
    triangle.lineTo(4.5 * scale, 5.0 * scale);
    triangle.closeSubpath();
    painter.fillPath(triangle, color);
    painter.end();
    if (!image.save(path, "PNG")) {
      cache.insert(color.rgba(), QString());
      return QString();
    }
  }

  cache.insert(color.rgba(), path);
  return path;
}

QString chevronImageRule(const QColor &color) {
  const QString path = chevronImagePath(color);
  return path.isEmpty() ? QString()
                        : QStringLiteral("  image: url(\"%1\");").arg(path);
}

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

struct Style {
  ThemeColors c;
  int radius = 6;
  qreal glow = 0.0;
  qreal opacity = 1.0;
  bool borders = true;
};

Style styleFor(const Theme &theme) {
  using ColorContrast::bestOf;
  using ColorContrast::ensure;
  using ColorContrast::ensureOnAll;

  Style s;
  s.radius = qMax(0, theme.borderRadius);
  s.glow = qBound(0.0, theme.glowIntensity, 1.0);
  s.opacity = theme.chromeOpacity;
  s.borders = theme.panelBorders;

  ThemeColors &c = s.c;
  const QColor bg = theme.backgroundColor;
  const QColor fg = theme.foregroundColor;
  c.surfaceBase = bg;
  c.surfaceRaised = theme.surfaceColor;
  c.surfaceOverlay = theme.surfaceAltColor;
  c.surfaceSunken = blend(bg, QColor(Qt::black), 0.15);
  c.surfacePopover = theme.surfaceColor;
  c.borderDefault = theme.borderColor;
  c.borderSubtle = blend(theme.borderColor, bg, 0.35);
  c.borderStrong = blend(theme.borderColor, fg, 0.2);
  c.borderFocus = theme.accentColor;
  c.textPrimary =
      ensureOnAll(fg, {bg, theme.surfaceColor, theme.surfaceAltColor,
                       theme.accentSoftColor});
  c.textSecondary = ensureOnAll(blend(fg, bg, 0.35), {bg, theme.surfaceColor});
  c.textMuted = ensureOnAll(blend(fg, bg, 0.55), {bg, theme.surfaceColor},
                            ColorContrast::GlyphRatio);
  c.textDisabled = ensureOnAll(blend(fg, bg, 0.65), {bg}, 2.0);
  c.textInverse = bestOf(theme.accentColor, {bg, fg});
  c.textLink = theme.accentColor;
  c.inputBg = theme.surfaceColor;
  c.inputFg = c.textPrimary;
  c.inputBorder = theme.borderColor;
  c.inputBorderFocus = theme.accentColor;
  c.inputPlaceholder = c.textMuted;
  c.inputSelection = theme.accentSoftColor;
  c.btnPrimaryBg = theme.accentColor;
  c.btnPrimaryFg = c.textInverse;
  c.btnPrimaryHover = blend(theme.accentColor, fg, 0.15);
  c.btnPrimaryActive = blend(theme.accentColor, bg, 0.2);
  c.btnSecondaryBg = theme.surfaceColor;
  c.btnSecondaryFg = c.textPrimary;
  c.btnSecondaryHover = theme.hoverColor;
  c.btnSecondaryActive = theme.pressedColor;
  c.btnDangerBg = theme.errorColor;
  c.btnDangerFg = bestOf(theme.errorColor, {QColor(Qt::white), bg});
  c.btnDangerHover = blend(theme.errorColor, fg, 0.15);
  c.btnDangerActive = blend(theme.errorColor, bg, 0.2);
  c.btnGhostHover = theme.hoverColor;
  c.btnGhostActive = theme.pressedColor;
  c.accentPrimary = theme.accentColor;
  c.accentSoft = theme.accentSoftColor;
  c.accentGlow = withAlpha(theme.accentColor, 0.10 + 0.22 * s.glow);
  c.scrollThumb = theme.borderColor;
  c.scrollThumbHover = c.textMuted;
  c.tabBg = bg;
  c.tabActiveBg = theme.surfaceColor;
  c.tabHoverBg = theme.hoverColor;
  c.tabFg = ensure(blend(fg, bg, 0.4), bg, ColorContrast::SecondaryTextRatio);
  c.tabActiveFg = c.textPrimary;
  c.tabActiveBorder = theme.accentColor;
  c.treeSelectedBg = theme.accentSoftColor;
  c.treeHoverBg = theme.hoverColor;
  c.statusSuccess = theme.successColor;
  c.statusWarning = theme.warningColor;
  c.statusError = theme.errorColor;
  c.statusInfo =
      theme.infoColor.isValid() ? theme.infoColor : theme.accentColor;
  return s;
}

Style styleFor(const ThemeDefinition &theme) {
  Style s;
  s.c = theme.colors;
  s.radius = qMax(0, theme.ui.borderRadius);
  s.glow = qBound(0.0, theme.ui.glowIntensity, 1.0);
  s.opacity = theme.ui.chromeOpacity;
  s.borders = theme.ui.panelBorders;
  return s;
}

QString rgba(QColor color, qreal alpha) {
  color.setAlphaF(qBound(0.0, alpha, 1.0));
  return color.name(QColor::HexArgb);
}

QString chrome(const Style &s, const QColor &color) {
  return rgba(color, s.opacity);
}

QString fillStyleTemplate(
    QString style,
    std::initializer_list<std::pair<const char *, QString>> replacements) {
  for (const auto &[key, value] : replacements) {
    style.replace(QString("{%1}").arg(QString::fromLatin1(key)), value);
  }
  return style;
}

QColor glowAccent(const Style &s) {
  QColor glowColor =
      s.c.accentGlow.isValid() ? s.c.accentGlow : s.c.accentPrimary;
  glowColor.setAlpha(255);
  const QColor luminous = s.c.accentPrimary.lighter(130 + qRound(s.glow * 35));
  return blend(glowColor, luminous, 0.45);
}

QColor emphasizedBorder(const Style &s, const QColor &border) {
  return blend(border, glowAccent(s), 0.08 * s.glow);
}

QColor glowSurface(const Style &s, const QColor &base, qreal strength = 1.0) {
  return blend(base, glowAccent(s), 0.04 * s.glow * strength);
}

QColor glowFocus(const Style &s, const QColor &accent) {
  return blend(accent, glowAccent(s), 0.08 * s.glow);
}

QColor readableOn(const Style &s, const QColor &background,
                  const QColor &preferred = QColor()) {
  const QColor bg = ColorContrast::flatten(background, s.c.surfaceBase);
  return ColorContrast::bestOf(
      bg, {preferred.isValid() ? preferred : s.c.textPrimary, s.c.textPrimary,
           s.c.textInverse});
}

QColor accentTextOn(const Style &s, const QColor &background) {
  return ColorContrast::ensure(
      s.c.accentPrimary, ColorContrast::flatten(background, s.c.surfaceBase));
}

QString borderRule(const Style &s, const QColor &color,
                   const QString &edge = QString()) {
  if (!s.borders)
    return edge.isEmpty() ? QString("border: none;")
                          : QString("border-%1: none;").arg(edge);
  return edge.isEmpty()
             ? QString("border: 1px solid %1;").arg(color.name())
             : QString("border-%1: 1px solid %2;").arg(edge, color.name());
}

int radius(const Style &s) { return s.radius; }
int controlRadius(const Style &s) { return qMax(3, s.radius); }

QColor toneBase(const Style &s, UIStyleHelper::Tone tone) {
  switch (tone) {
  case UIStyleHelper::Tone::Accent:
    return s.c.accentPrimary;
  case UIStyleHelper::Tone::Success:
    return s.c.statusSuccess;
  case UIStyleHelper::Tone::Warning:
    return s.c.statusWarning;
  case UIStyleHelper::Tone::Error:
    return s.c.statusError;
  case UIStyleHelper::Tone::Info:
    return s.c.statusInfo;
  case UIStyleHelper::Tone::Neutral:
    break;
  }
  return s.c.textSecondary;
}

QColor toneFill(const Style &s, UIStyleHelper::Tone tone) {
  if (tone == UIStyleHelper::Tone::Neutral)
    return s.c.surfaceOverlay;
  const qreal amount = ColorContrast::isDark(s.c.surfaceBase) ? 0.16 : 0.10;
  return ColorContrast::mix(s.c.surfaceRaised, toneBase(s, tone), amount);
}

QColor toneText(const Style &s, UIStyleHelper::Tone tone) {
  const QColor fill = toneFill(s, tone);
  if (tone == UIStyleHelper::Tone::Neutral)
    return ColorContrast::ensure(s.c.textSecondary, fill);
  return ColorContrast::ensure(toneBase(s, tone), fill);
}

QString disabledButtonRule(const QColor &background, const QColor &foreground) {
  return QString("QPushButton:disabled {"
                 "  background: %1;"
                 "  border: 1px solid %1;"
                 "  color: %2;"
                 "}")
      .arg(background.name(), foreground.name());
}

QString scrollBarRules(const QColor &thumb, const QColor &thumbHover) {
  return QString("QScrollBar:vertical {"
                 "  background: transparent;"
                 "  width: 8px;"
                 "  margin: 0;"
                 "}"
                 "QScrollBar::handle:vertical {"
                 "  background: %1;"
                 "  min-height: 24px;"
                 "  border-radius: 3px;"
                 "  margin: 1px;"
                 "}"
                 "QScrollBar::handle:vertical:hover {"
                 "  background: %2;"
                 "}"
                 "QScrollBar::add-line:vertical,"
                 "QScrollBar::sub-line:vertical {"
                 "  height: 0;"
                 "}"
                 "QScrollBar::add-page:vertical,"
                 "QScrollBar::sub-page:vertical {"
                 "  background: none;"
                 "}"
                 "QScrollBar:horizontal {"
                 "  background: transparent;"
                 "  height: 8px;"
                 "  margin: 0;"
                 "}"
                 "QScrollBar::handle:horizontal {"
                 "  background: %1;"
                 "  min-width: 24px;"
                 "  border-radius: 3px;"
                 "  margin: 1px;"
                 "}"
                 "QScrollBar::handle:horizontal:hover {"
                 "  background: %2;"
                 "}"
                 "QScrollBar::add-line:horizontal,"
                 "QScrollBar::sub-line:horizontal {"
                 "  width: 0;"
                 "}"
                 "QScrollBar::add-page:horizontal,"
                 "QScrollBar::sub-page:horizontal {"
                 "  background: none;"
                 "}")
      .arg(thumb.name(), thumbHover.name());
}

QString panelStyleImpl(const Style &s, const QString &objectName) {
  const ThemeColors &c = s.c;
  const QColor hover =
      c.btnGhostHover.isValid() ? c.btnGhostHover : c.treeHoverBg;
  const QColor checked = c.accentSoft;
  return QStringLiteral(
             "QWidget#%1 { background: %2; color: %3; }"
             "QLabel { color: %3; background: transparent; }"
             "QToolButton { color: %4; background: transparent; "
             "border: 1px solid transparent; border-radius: %7px; padding: 4px "
             "8px; }"
             "QToolButton:hover { background: %5; border-color: %6; color: "
             "%3; }"
             "QToolButton:pressed { background: %9; }"
             "QToolButton:checked { background: %10; border-color: %6; color: "
             "%11; }"
             "QToolButton:disabled { color: %8; }"
             "QSplitter::handle { background: %6; }"
             "QScrollArea { background: %2; border: none; }")
      .arg(objectName, c.surfaceBase.name(), c.textPrimary.name(),
           c.textSecondary.name(), hover.name(), c.borderDefault.name())
      .arg(controlRadius(s))
      .arg(c.textDisabled.name())
      .arg((c.btnGhostActive.isValid() ? c.btnGhostActive : checked).name())
      .arg(checked.name())
      .arg(accentTextOn(s, checked).name());
}

QString popupDialogStyleImpl(const Style &s) {
  const QColor bg = glowSurface(s, s.c.surfacePopover, 0.7);
  const QColor border = emphasizedBorder(s, s.c.borderDefault);
  return QString("background: %1; "
                 "%2 "
                 "border-radius: %3px;")
      .arg(chrome(s, bg))
      .arg(borderRule(s, border))
      .arg(qMin(radius(s) + 2, 10));
}

QString searchBoxStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  return QString("QLineEdit {"
                 "  padding: 8px;"
                 "  font-size: 14px;"
                 "  border: 1px solid %1;"
                 "  border-radius: %6px;"
                 "  background: %2;"
                 "  color: %3;"
                 "  selection-background-color: %5;"
                 "  selection-color: %7;"
                 "}"
                 "QLineEdit:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(c.inputBorder.name())
      .arg(chrome(s, c.inputBg))
      .arg(c.inputFg.name())
      .arg(c.inputBorderFocus.name())
      .arg(c.inputSelection.name())
      .arg(controlRadius(s))
      .arg(readableOn(s, c.inputSelection, c.inputFg).name());
}

QString resultListStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor border = emphasizedBorder(s, c.borderSubtle);
  const QColor selected = glowSurface(s, c.treeSelectedBg, 1.0);
  const QColor hover = glowSurface(s, c.treeHoverBg, 0.9);
  return QString("QListWidget {"
                 "  %6"
                 "  border-radius: %7px;"
                 "  background: %1;"
                 "  color: %2;"
                 "  outline: none;"
                 "}"
                 "QListWidget::item {"
                 "  padding: 8px;"
                 "  border-bottom: 1px solid %3;"
                 "}"
                 "QListWidget::item:selected {"
                 "  background: %4;"
                 "  color: %5;"
                 "}"
                 "QListWidget::item:hover:!selected {"
                 "  background: %8;"
                 "}")
      .arg(chrome(s, glowSurface(s, c.surfaceBase, 0.35)))
      .arg(c.textPrimary.name())
      .arg(border.name())
      .arg(selected.name())
      .arg(readableOn(s, selected).name())
      .arg(borderRule(s, border))
      .arg(controlRadius(s))
      .arg(hover.name());
}

QString panelHeaderStyleImpl(const Style &s) {
  const QColor surface = glowSurface(s, s.c.surfaceRaised, 0.5);
  const QColor border = emphasizedBorder(s, s.c.borderSubtle);
  return QString("background: %1; "
                 "%2")
      .arg(chrome(s, surface))
      .arg(borderRule(s, border, "bottom"));
}

QString treeViewStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
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
                 "}")
             .arg(c.textPrimary.name())
             .arg(chrome(s, c.surfaceOverlay))
             .arg(c.textSecondary.name())
             .arg(c.borderDefault.name()) +
         scrollBarRules(
             c.scrollThumb.isValid() ? c.scrollThumb : c.borderDefault,
             c.scrollThumbHover.isValid() ? c.scrollThumbHover : c.textMuted);
}

QString contextMenuStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor bg = glowSurface(s, c.surfacePopover, 0.55);
  const QColor border = emphasizedBorder(s, c.borderDefault);
  const QColor selected = glowSurface(s, c.accentSoft, 1.0);
  return QString("QMenu {"
                 "  background: %1;"
                 "  color: %2;"
                 "  %6"
                 "  border-radius: %7px;"
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
                 "QMenu::item:disabled {"
                 "  color: %8;"
                 "}"
                 "QMenu::separator {"
                 "  height: 1px;"
                 "  background: %3;"
                 "  margin: 4px 8px;"
                 "}")
      .arg(chrome(s, bg))
      .arg(c.textPrimary.name())
      .arg(border.name())
      .arg(selected.name())
      .arg(accentTextOn(s, selected).name())
      .arg(borderRule(s, border))
      .arg(qMin(controlRadius(s) + 1, 8))
      .arg(c.textDisabled.name());
}

QString treeWidgetStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor border = emphasizedBorder(s, c.borderDefault);
  const QColor selected = glowSurface(s, c.treeSelectedBg, 1.0);
  const QColor hover = glowSurface(s, c.treeHoverBg, 0.9);
  return fillStyleTemplate(
      QStringLiteral("QTreeWidget {"
                     "  background: {bg};"
                     "  alternate-background-color: {altBg};"
                     "  color: {fg};"
                     "  {widgetBorder}"
                     "  border-radius: {radius}px;"
                     "  selection-background-color: {selectedBg};"
                     "  selection-color: {selectedFg};"
                     "  outline: none;"
                     "}"
                     "QTreeWidget::item {"
                     "  padding: 4px;"
                     "}"
                     "QTreeWidget::item:selected,"
                     "QTreeWidget::item:!active:selected {"
                     "  background: {selectedBg};"
                     "  color: {selectedFg};"
                     "}"
                     "QTreeWidget::item:hover:!selected {"
                     "  background: {hoverBg};"
                     "}"
                     "QHeaderView::section {"
                     "  background: {headerBg};"
                     "  color: {headerFg};"
                     "  border: none;"
                     "  {headerBorder}"
                     "  padding: 4px 8px;"
                     "  font-weight: bold;"
                     "}"),
      {{"bg", chrome(s, glowSurface(s, c.surfaceBase, 0.35))},
       {"altBg", chrome(s, glowSurface(s, c.surfaceOverlay, 0.3))},
       {"fg", c.textPrimary.name()},
       {"widgetBorder", borderRule(s, border)},
       {"radius", QString::number(controlRadius(s))},
       {"selectedBg", selected.name()},
       {"selectedFg", readableOn(s, selected).name()},
       {"hoverBg", hover.name()},
       {"headerBg", chrome(s, glowSurface(s, c.surfaceRaised, 0.45))},
       {"headerFg", c.textSecondary.name()},
       {"headerBorder", borderRule(s, border, "bottom")}});
}

QString comboBoxStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor focus = glowFocus(s, c.inputBorderFocus);
  const QColor popupBg =
      c.surfacePopover.isValid() ? c.surfacePopover : c.inputBg;
  const QColor popupFg = readableOn(s, popupBg, c.inputFg);
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
                 "  subcontrol-origin: padding;"
                 "  subcontrol-position: center right;"
                 "  border: none;"
                 "  width: 22px;"
                 "}"
                 "QComboBox::down-arrow {"
                 "  width: 9px;"
                 "  height: 6px;"
                 "%8"
                 "}"
                 "QComboBox::down-arrow:disabled {"
                 "%9"
                 "}"
                 "QComboBox:disabled {"
                 "  color: %10;"
                 "}"
                 "QComboBox:editable {"
                 "  padding: 0px 0px 0px 10px;"
                 "  min-height: 28px;"
                 "}"
                 "QComboBox QLineEdit {"
                 "  background: transparent;"
                 "  border: none;"
                 "  border-radius: 0;"
                 "  margin: 0;"
                 "  padding: 0px;"
                 "  color: %2;"
                 "  selection-background-color: %4;"
                 "  selection-color: %7;"
                 "}"
                 "QComboBox QAbstractItemView {"
                 "  background: %11;"
                 "  color: %12;"
                 "  border: 1px solid %3;"
                 "  selection-background-color: %4;"
                 "  selection-color: %7;"
                 "  padding: 4px;"
                 "  outline: none;"
                 "}"
                 "QComboBox QAbstractItemView::item {"
                 "  min-height: 22px;"
                 "  padding: 6px 10px;"
                 "  color: %12;"
                 "  background: %11;"
                 "}"
                 "QComboBox QAbstractItemView::item:selected {"
                 "  background: %4;"
                 "  color: %7;"
                 "}")
      .arg(chrome(s, c.inputBg))
      .arg(c.inputFg.name())
      .arg(c.inputBorder.name())
      .arg(c.inputSelection.name())
      .arg(controlRadius(s))
      .arg(focus.name())
      .arg(readableOn(s, c.inputSelection, c.inputFg).name())
      .arg(chevronImageRule(c.textMuted))
      .arg(chevronImageRule(c.textDisabled))
      .arg(c.textDisabled.name())
      .arg(popupBg.name())
      .arg(popupFg.name());
}

QString checkBoxStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  return QString("QCheckBox, QRadioButton {"
                 "  color: %1;"
                 "  spacing: 8px;"
                 "}"
                 "QCheckBox::indicator {"
                 "  width: 14px;"
                 "  height: 14px;"
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
                 "}"
                 "QCheckBox:disabled, QRadioButton:disabled {"
                 "  color: %5;"
                 "}")
      .arg(c.textPrimary.name())
      .arg(c.borderStrong.isValid() ? c.borderStrong.name()
                                    : c.inputBorder.name())
      .arg(chrome(s, c.inputBg))
      .arg(c.accentPrimary.name())
      .arg(c.textDisabled.name());
}

QString formDialogStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor bg = glowSurface(s, c.surfaceBase, 0.35);
  const QColor border = emphasizedBorder(s, c.borderSubtle);
  return QString("QDialog {"
                 "  background: %1;"
                 "  %2"
                 "  border-radius: %3px;"
                 "}"
                 "QLabel { color: %4; background: transparent; }")
      .arg(chrome(s, bg))
      .arg(borderRule(s, border))
      .arg(qMax(4, radius(s)))
      .arg(c.textPrimary.name());
}

QString groupBoxStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor bg = glowSurface(s, c.surfaceRaised, 0.45);
  const QColor border = emphasizedBorder(s, c.borderDefault);
  return QString("QGroupBox {"
                 "  background: %1;"
                 "  %2"
                 "  border-radius: %5px;"
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
      .arg(chrome(s, bg))
      .arg(borderRule(s, border))
      .arg(c.textPrimary.name())
      .arg(accentTextOn(s, c.surfaceBase).name())
      .arg(qMax(4, radius(s)));
}

QString lineEditStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor focus = glowFocus(s, c.inputBorderFocus);
  return QString("QLineEdit {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: %6px;"
                 "  padding: 8px 12px;"
                 "  font-size: 12px;"
                 "  selection-background-color: %5;"
                 "  selection-color: %9;"
                 "}"
                 "QLineEdit:focus {"
                 "  border-color: %4;"
                 "}"
                 "QLineEdit:disabled {"
                 "  background: %7;"
                 "  color: %8;"
                 "  border-color: %7;"
                 "}")
      .arg(chrome(s, c.inputBg))
      .arg(c.inputFg.name())
      .arg(c.inputBorder.name())
      .arg(focus.name())
      .arg(c.inputSelection.name())
      .arg(controlRadius(s))
      .arg(chrome(s, c.surfaceSunken))
      .arg(c.textDisabled.name())
      .arg(readableOn(s, c.inputSelection, c.inputFg).name());
}

QString primaryButtonStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor hover = glowSurface(s, c.btnPrimaryHover, 1.0);
  const QColor pressed = glowSurface(s, c.btnPrimaryActive, 0.9);
  return disabledButtonRule(c.surfaceSunken, c.textDisabled) +
         QString("QPushButton {"
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
                 "  color: %6;"
                 "}"
                 "QPushButton:pressed {"
                 "  background: %4;"
                 "  border-color: %4;"
                 "}")
             .arg(c.btnPrimaryBg.name())
             .arg(readableOn(s, c.btnPrimaryBg, c.btnPrimaryFg).name())
             .arg(hover.name())
             .arg(pressed.name())
             .arg(controlRadius(s))
             .arg(readableOn(s, hover, c.btnPrimaryFg).name());
}

QString secondaryButtonStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor border = emphasizedBorder(s, c.borderDefault);
  const QColor hover = glowSurface(s, c.btnSecondaryHover, 1.0);
  const QColor focus = glowFocus(s, c.borderFocus);
  return disabledButtonRule(c.surfaceSunken, c.textDisabled) +
         QString("QPushButton {"
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
                 "  color: %7;"
                 "}"
                 "QPushButton:pressed {"
                 "  background: %8;"
                 "}")
             .arg(chrome(s, c.btnSecondaryBg))
             .arg(c.btnSecondaryFg.name())
             .arg(border.name())
             .arg(chrome(s, hover))
             .arg(focus.name())
             .arg(controlRadius(s))
             .arg(readableOn(s, hover, c.btnSecondaryFg).name())
             .arg(chrome(s, c.btnSecondaryActive.isValid()
                                ? c.btnSecondaryActive
                                : c.btnGhostActive));
}

QString breadcrumbButtonStyleImpl(const Style &s, bool active) {
  const ThemeColors &c = s.c;
  const QColor hoverBg = c.accentSoft;
  return QString("QPushButton {"
                 "  background: transparent;"
                 "  color: %1;"
                 "  border: none;"
                 "  padding: 2px 8px;"
                 "  font-family: 'Ubuntu Mono', 'JetBrains Mono', 'Monospace';"
                 "  font-size: 12px;"
                 "%4"
                 "}"
                 "QPushButton:hover {"
                 "  color: %2;"
                 "  background: %3;"
                 "  border-radius: 3px;"
                 "}")
      .arg(active ? accentTextOn(s, c.surfaceRaised).name()
                  : c.textSecondary.name())
      .arg(accentTextOn(s, hoverBg).name())
      .arg(hoverBg.name())
      .arg(active ? QStringLiteral("  font-weight: bold;") : QString());
}

QString breadcrumbSeparatorStyleImpl(const Style &s) {
  return QString("QLabel {"
                 "  color: %1;"
                 "  font-family: 'Ubuntu Mono', 'JetBrains Mono', 'Monospace';"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "  padding: 0 2px;"
                 "}")
      .arg(s.c.textMuted.name());
}

QString tabWidgetStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor paneBorder = emphasizedBorder(s, c.borderSubtle);
  const QColor activeBorder = glowFocus(s, c.tabActiveBorder);
  const QColor hover = glowSurface(s, c.tabHoverBg, 1.0);
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
                 "  color: %9;"
                 "}")
      .arg(borderRule(s, paneBorder))
      .arg(chrome(s, c.surfaceRaised))
      .arg(chrome(s, c.tabBg))
      .arg(c.tabFg.name())
      .arg(readableOn(s, c.surfaceRaised, c.tabActiveFg).name())
      .arg(activeBorder.name())
      .arg(chrome(s, hover))
      .arg(controlRadius(s))
      .arg(readableOn(s, hover, c.tabActiveFg).name());
}

QString tableWidgetStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor border = emphasizedBorder(s, c.borderDefault);
  const QColor selected = glowSurface(s, c.accentSoft, 1.0);
  return QString("QTableWidget, QTableView {"
                 "  background: %1;"
                 "  color: %2;"
                 "  %8"
                 "  border-radius: %9px;"
                 "  gridline-color: %3;"
                 "  outline: none;"
                 "  selection-background-color: %4;"
                 "  selection-color: %7;"
                 "}"
                 "QTableWidget::item, QTableView::item {"
                 "  padding: 4px;"
                 "}"
                 "QTableWidget::item:selected, QTableView::item:selected {"
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
                 "}"
                 "QTableCornerButton::section {"
                 "  background: %5;"
                 "  border: none;"
                 "}")
      .arg(chrome(s, c.surfaceOverlay))
      .arg(c.textPrimary.name())
      .arg(border.name())
      .arg(selected.name())
      .arg(chrome(s, c.surfaceRaised))
      .arg(c.textSecondary.name())
      .arg(readableOn(s, selected).name())
      .arg(borderRule(s, border))
      .arg(qMin(controlRadius(s), 4));
}

QString plainTextEditStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor focus = glowFocus(s, c.inputBorderFocus);
  return QString("QPlainTextEdit, QTextEdit {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: %5px;"
                 "  padding: 8px;"
                 "  font-family: monospace;"
                 "  font-size: 12px;"
                 "  selection-background-color: %6;"
                 "  selection-color: %7;"
                 "}"
                 "QPlainTextEdit:focus, QTextEdit:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(chrome(s, c.inputBg))
      .arg(c.inputFg.name())
      .arg(c.inputBorder.name())
      .arg(focus.name())
      .arg(controlRadius(s))
      .arg(c.inputSelection.name())
      .arg(readableOn(s, c.inputSelection, c.inputFg).name());
}

QString spinBoxStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor focus = glowFocus(s, c.inputBorderFocus);
  return QString("QSpinBox, QDoubleSpinBox {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: %5px;"
                 "  padding: 4px 8px;"
                 "}"
                 "QSpinBox:focus, QDoubleSpinBox:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(chrome(s, c.inputBg))
      .arg(c.inputFg.name())
      .arg(c.inputBorder.name())
      .arg(focus.name())
      .arg(qMin(controlRadius(s), 4));
}

QString dangerButtonStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor hover = glowSurface(s, c.btnDangerHover, 1.0);
  return disabledButtonRule(c.surfaceSunken, c.textDisabled) +
         QString("QPushButton {"
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
                 "  color: %5;"
                 "}")
             .arg(c.btnDangerBg.name())
             .arg(hover.name())
             .arg(readableOn(s, c.btnDangerBg, c.btnDangerFg).name())
             .arg(controlRadius(s))
             .arg(readableOn(s, hover, c.btnDangerFg).name());
}

QString progressBarStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
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
      .arg(chrome(s, c.surfaceOverlay))
      .arg(c.borderDefault.name())
      .arg(c.textPrimary.name())
      .arg(c.accentPrimary.name());
}

QString toolBarStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor hover = c.btnGhostHover;
  const QColor checked = c.accentSoft;
  return QString("QToolBar {"
                 "  background: %1;"
                 "  border: none;"
                 "  spacing: 2px;"
                 "  padding: 2px;"
                 "}"
                 "QToolBar::separator {"
                 "  background: %4;"
                 "  width: 1px;"
                 "  margin: 4px 4px;"
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
                 "  color: %9;"
                 "}"
                 "QToolButton:pressed {"
                 "  background: %5;"
                 "}"
                 "QToolButton:checked {"
                 "  background: %6;"
                 "  border-color: %7;"
                 "  color: %10;"
                 "}"
                 "QToolButton:disabled {"
                 "  color: %11;"
                 "}")
      .arg(chrome(s, c.surfaceRaised))
      .arg(c.textSecondary.name())
      .arg(chrome(s, hover))
      .arg(c.borderDefault.name())
      .arg(chrome(s, c.btnGhostActive))
      .arg(chrome(s, checked))
      .arg(c.accentPrimary.name())
      .arg(controlRadius(s))
      .arg(readableOn(s, hover).name())
      .arg(accentTextOn(s, checked).name())
      .arg(c.textDisabled.name());
}

QString sectionLabelStyleImpl(const Style &s) {
  return QString("color: %1; background: transparent; font-size: 10px; "
                 "font-weight: bold; letter-spacing: 1px; "
                 "text-transform: uppercase; padding: 2px 0;")
      .arg(s.c.textMuted.name());
}

QString badgeStyleImpl(const Style &s, UIStyleHelper::Tone tone) {
  const QColor fill = toneFill(s, tone);
  const QColor text = toneText(s, tone);
  const QColor border = tone == UIStyleHelper::Tone::Neutral
                            ? s.c.borderSubtle
                            : ColorContrast::mix(fill, toneBase(s, tone), 0.35);
  return QString("background: %1; color: %2; border: 1px solid %3; "
                 "border-radius: 8px; padding: 1px 7px; font-size: 11px; "
                 "font-weight: bold;")
      .arg(fill.name(), text.name(), border.name());
}

QString bannerStyleImpl(const Style &s, UIStyleHelper::Tone tone) {
  const QColor fill = toneFill(s, tone);
  const QColor stripe = tone == UIStyleHelper::Tone::Neutral
                            ? s.c.borderStrong
                            : toneBase(s, tone);
  return QString("background: %1; color: %2; border: none; "
                 "border-left: 3px solid %3; border-radius: %4px; "
                 "padding: 6px 10px;")
      .arg(fill.name(), readableOn(s, fill).name(), stripe.name())
      .arg(qMin(controlRadius(s), 4));
}

QString emptyStateStyleImpl(const Style &s) {
  return QString("color: %1; background: transparent; font-size: 12px; "
                 "padding: 16px;")
      .arg(s.c.textMuted.name());
}

QString cardStyleImpl(const Style &s, const QString &objectName) {
  const QColor bg = glowSurface(s, s.c.surfaceRaised, 0.4);
  const QColor border = emphasizedBorder(s, s.c.borderSubtle);
  const QString body = QString("background: %1; %2 border-radius: %3px;")
                           .arg(chrome(s, bg), borderRule(s, border))
                           .arg(qMax(4, radius(s)));
  if (objectName.isEmpty())
    return body;
  return QString("#%1 { %2 } #%1 QLabel { background: transparent; }")
      .arg(objectName, body);
}

QString iconButtonStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor hover = c.btnGhostHover;
  const QColor checked = c.accentSoft;
  return QString("QToolButton, QPushButton {"
                 "  background: transparent;"
                 "  color: %1;"
                 "  border: 1px solid transparent;"
                 "  border-radius: %2px;"
                 "  padding: 3px 6px;"
                 "  min-height: 0;"
                 "}"
                 "QToolButton:hover, QPushButton:hover {"
                 "  background: %3;"
                 "  color: %4;"
                 "}"
                 "QToolButton:pressed, QPushButton:pressed {"
                 "  background: %5;"
                 "}"
                 "QToolButton:checked, QPushButton:checked {"
                 "  background: %6;"
                 "  color: %7;"
                 "}"
                 "QToolButton:disabled, QPushButton:disabled {"
                 "  color: %8;"
                 "}"
                 "QToolButton::menu-indicator { image: none; width: 0; }")
      .arg(c.textSecondary.name())
      .arg(controlRadius(s))
      .arg(chrome(s, hover))
      .arg(readableOn(s, hover).name())
      .arg(chrome(s, c.btnGhostActive))
      .arg(chrome(s, checked))
      .arg(accentTextOn(s, checked).name())
      .arg(c.textDisabled.name());
}

QString listWidgetStyleImpl(const Style &s) {
  const ThemeColors &c = s.c;
  const QColor border = emphasizedBorder(s, c.borderSubtle);
  const QColor selected = glowSurface(s, c.treeSelectedBg, 1.0);
  const QColor hover = glowSurface(s, c.treeHoverBg, 0.9);
  return QString("QListWidget, QListView {"
                 "  background: %1;"
                 "  color: %2;"
                 "  %3"
                 "  border-radius: %4px;"
                 "  outline: none;"
                 "  padding: 2px;"
                 "}"
                 "QListWidget::item, QListView::item {"
                 "  padding: 4px 6px;"
                 "  border-radius: 3px;"
                 "}"
                 "QListWidget::item:selected, QListView::item:selected {"
                 "  background: %5;"
                 "  color: %6;"
                 "}"
                 "QListWidget::item:hover:!selected,"
                 "QListView::item:hover:!selected {"
                 "  background: %7;"
                 "}")
      .arg(chrome(s, c.surfaceRaised))
      .arg(c.textPrimary.name())
      .arg(borderRule(s, border))
      .arg(qMin(controlRadius(s), 6))
      .arg(selected.name())
      .arg(readableOn(s, selected).name())
      .arg(hover.name());
}

QString headingStyleImpl(const Style &s, int pixelSize) {
  return QString("color: %1; background: transparent; font-weight: bold; "
                 "font-size: %2px;")
      .arg(s.c.textPrimary.name())
      .arg(qBound(10, pixelSize, 28));
}

QVector<QColor> seriesColorsImpl(const Style &s, const ThemeColors &syntax) {
  const QColor bg = ColorContrast::flatten(s.c.surfaceBase, QColor(Qt::black));
  QVector<QColor> colors;
  for (const QColor &candidate :
       {s.c.accentPrimary, syntax.syntaxString, syntax.syntaxKeyword2,
        s.c.statusWarning, syntax.syntaxKeyword3, syntax.syntaxFunction,
        s.c.statusError, syntax.syntaxNumber, syntax.syntaxClass,
        syntax.syntaxConstant}) {
    if (!candidate.isValid())
      continue;
    const QColor readable =
        ColorContrast::ensure(candidate, bg, ColorContrast::GlyphRatio);
    bool distinct = true;
    for (const QColor &existing : colors) {
      const int dr = existing.red() - readable.red();
      const int dg = existing.green() - readable.green();
      const int db = existing.blue() - readable.blue();
      if (dr * dr + dg * dg + db * db < 2500) {
        distinct = false;
        break;
      }
    }
    if (distinct)
      colors.append(readable);
  }
  if (colors.isEmpty())
    colors.append(s.c.textPrimary);
  return colors;
}

} // namespace

#define LP_STYLE_PAIR(name)                                                    \
  QString UIStyleHelper::name(const Theme &theme) {                            \
    return name##Impl(styleFor(theme));                                        \
  }                                                                            \
  QString UIStyleHelper::name(const ThemeDefinition &theme) {                  \
    return name##Impl(styleFor(theme));                                        \
  }

LP_STYLE_PAIR(popupDialogStyle)
LP_STYLE_PAIR(searchBoxStyle)
LP_STYLE_PAIR(resultListStyle)
LP_STYLE_PAIR(panelHeaderStyle)
LP_STYLE_PAIR(treeWidgetStyle)
LP_STYLE_PAIR(treeViewStyle)
LP_STYLE_PAIR(contextMenuStyle)
LP_STYLE_PAIR(comboBoxStyle)
LP_STYLE_PAIR(checkBoxStyle)
LP_STYLE_PAIR(formDialogStyle)
LP_STYLE_PAIR(groupBoxStyle)
LP_STYLE_PAIR(lineEditStyle)
LP_STYLE_PAIR(primaryButtonStyle)
LP_STYLE_PAIR(secondaryButtonStyle)
LP_STYLE_PAIR(breadcrumbSeparatorStyle)
LP_STYLE_PAIR(tabWidgetStyle)
LP_STYLE_PAIR(tableWidgetStyle)
LP_STYLE_PAIR(plainTextEditStyle)
LP_STYLE_PAIR(spinBoxStyle)
LP_STYLE_PAIR(dangerButtonStyle)
LP_STYLE_PAIR(progressBarStyle)
LP_STYLE_PAIR(toolBarStyle)
LP_STYLE_PAIR(sectionLabelStyle)
LP_STYLE_PAIR(emptyStateStyle)
LP_STYLE_PAIR(iconButtonStyle)
LP_STYLE_PAIR(listWidgetStyle)

#undef LP_STYLE_PAIR

QString UIStyleHelper::panelStyle(const Theme &theme,
                                  const QString &objectName) {
  return panelStyleImpl(styleFor(theme), objectName);
}

QString UIStyleHelper::panelStyle(const ThemeDefinition &theme,
                                  const QString &objectName) {
  return panelStyleImpl(styleFor(theme), objectName);
}

QString UIStyleHelper::panelHeaderStyle(const Theme &theme,
                                        const QString &objectName) {
  return QStringLiteral("QWidget#%1 { %2 }")
      .arg(objectName, panelHeaderStyle(theme));
}

QString UIStyleHelper::panelHeaderStyle(const ThemeDefinition &theme,
                                        const QString &objectName) {
  return QStringLiteral("QWidget#%1 { %2 }")
      .arg(objectName, panelHeaderStyle(theme));
}

QString UIStyleHelper::subduedLabelStyle(const Theme &theme) {
  return QString("color: %1;").arg(styleFor(theme).c.textMuted.name());
}

QString UIStyleHelper::subduedLabelStyle(const ThemeDefinition &theme) {
  return QString("color: %1;").arg(theme.colors.textMuted.name());
}

QString UIStyleHelper::titleLabelStyle(const Theme &theme) {
  return QString("font-weight: bold; color: %1;")
      .arg(styleFor(theme).c.textPrimary.name());
}

QString UIStyleHelper::titleLabelStyle(const ThemeDefinition &theme) {
  return QString("font-weight: bold; color: %1;")
      .arg(theme.colors.textPrimary.name());
}

QString UIStyleHelper::comboArrowImageRule(const QColor &color) {
  return chevronImageRule(color);
}

QString UIStyleHelper::breadcrumbButtonStyle(const Theme &theme) {
  return breadcrumbButtonStyleImpl(styleFor(theme), false);
}

QString UIStyleHelper::breadcrumbButtonStyle(const ThemeDefinition &theme) {
  return breadcrumbButtonStyleImpl(styleFor(theme), false);
}

QString UIStyleHelper::breadcrumbActiveButtonStyle(const Theme &theme) {
  return breadcrumbButtonStyleImpl(styleFor(theme), true);
}

QString
UIStyleHelper::breadcrumbActiveButtonStyle(const ThemeDefinition &theme) {
  return breadcrumbButtonStyleImpl(styleFor(theme), true);
}

QString UIStyleHelper::infoLabelStyle(const Theme &theme) {
  return QString("color: %1; font-size: 11px;")
      .arg(styleFor(theme).c.textMuted.name());
}

QString UIStyleHelper::infoLabelStyle(const ThemeDefinition &theme) {
  return QString("color: %1; font-size: 11px;")
      .arg(theme.colors.textMuted.name());
}

QString UIStyleHelper::successInfoLabelStyle(const Theme &theme) {
  return toneLabelStyle(theme, Tone::Success);
}

QString UIStyleHelper::successInfoLabelStyle(const ThemeDefinition &theme) {
  return toneLabelStyle(theme, Tone::Success);
}

QString UIStyleHelper::errorInfoLabelStyle(const Theme &theme) {
  return toneLabelStyle(theme, Tone::Error);
}

QString UIStyleHelper::errorInfoLabelStyle(const ThemeDefinition &theme) {
  return toneLabelStyle(theme, Tone::Error);
}

QString UIStyleHelper::toneLabelStyle(const Theme &theme, Tone tone) {
  const Style s = styleFor(theme);
  return QString("color: %1; font-size: 11px;")
      .arg(ColorContrast::ensureOnAll(toneBase(s, tone),
                                      {s.c.surfaceBase, s.c.surfaceRaised},
                                      ColorContrast::SecondaryTextRatio)
               .name());
}

QString UIStyleHelper::toneLabelStyle(const ThemeDefinition &theme, Tone tone) {
  const Style s = styleFor(theme);
  return QString("color: %1; font-size: 11px;")
      .arg(ColorContrast::ensureOnAll(toneBase(s, tone),
                                      {s.c.surfaceBase, s.c.surfaceRaised},
                                      ColorContrast::SecondaryTextRatio)
               .name());
}

QString UIStyleHelper::badgeStyle(const Theme &theme, Tone tone) {
  return badgeStyleImpl(styleFor(theme), tone);
}

QString UIStyleHelper::badgeStyle(const ThemeDefinition &theme, Tone tone) {
  return badgeStyleImpl(styleFor(theme), tone);
}

QString UIStyleHelper::bannerStyle(const Theme &theme, Tone tone) {
  return bannerStyleImpl(styleFor(theme), tone);
}

QString UIStyleHelper::bannerStyle(const ThemeDefinition &theme, Tone tone) {
  return bannerStyleImpl(styleFor(theme), tone);
}

QString UIStyleHelper::cardStyle(const Theme &theme,
                                 const QString &objectName) {
  return cardStyleImpl(styleFor(theme), objectName);
}

QString UIStyleHelper::cardStyle(const ThemeDefinition &theme,
                                 const QString &objectName) {
  return cardStyleImpl(styleFor(theme), objectName);
}

QColor UIStyleHelper::toneColor(const Theme &theme, Tone tone) {
  return toneText(styleFor(theme), tone);
}

QColor UIStyleHelper::toneColor(const ThemeDefinition &theme, Tone tone) {
  return toneText(styleFor(theme), tone);
}

QColor UIStyleHelper::readableText(const Theme &theme, const QColor &background,
                                   const QColor &preferred) {
  return readableOn(styleFor(theme), background, preferred);
}

QColor UIStyleHelper::readableText(const ThemeDefinition &theme,
                                   const QColor &background,
                                   const QColor &preferred) {
  return readableOn(styleFor(theme), background, preferred);
}

QString UIStyleHelper::headingStyle(const Theme &theme, int pixelSize) {
  return headingStyleImpl(styleFor(theme), pixelSize);
}

QString UIStyleHelper::headingStyle(const ThemeDefinition &theme,
                                    int pixelSize) {
  return headingStyleImpl(styleFor(theme), pixelSize);
}

QColor UIStyleHelper::mutedTextColor(const Theme &theme) {
  return styleFor(theme).c.textMuted;
}

QColor UIStyleHelper::mutedTextColor(const ThemeDefinition &theme) {
  return theme.colors.textMuted;
}

QColor UIStyleHelper::secondaryTextColor(const Theme &theme) {
  return styleFor(theme).c.textSecondary;
}

QColor UIStyleHelper::secondaryTextColor(const ThemeDefinition &theme) {
  return theme.colors.textSecondary;
}

QVector<QColor> UIStyleHelper::seriesColors(const Theme &theme) {
  ThemeColors syntax;
  syntax.syntaxString = theme.quotationFormat;
  syntax.syntaxKeyword2 = theme.keywordFormat_1;
  syntax.syntaxKeyword3 = theme.keywordFormat_2;
  syntax.syntaxFunction = theme.functionFormat;
  syntax.syntaxNumber = theme.numberFormat;
  syntax.syntaxClass = theme.classFormat;
  syntax.syntaxConstant = theme.constantFormat;
  return seriesColorsImpl(styleFor(theme), syntax);
}

QVector<QColor> UIStyleHelper::seriesColors(const ThemeDefinition &theme) {
  return seriesColorsImpl(styleFor(theme), theme.colors);
}
