#include "hackerstyle.h"
#include "../../../theme/themeengine.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QStyleOptionButton>
#include <QStyleOptionMenuItem>
#include <QStyleOptionProgressBar>
#include <QStyleOptionTab>

HackerStyle::HackerStyle(QStyle *baseStyle) : QProxyStyle(baseStyle) {}

// --- palette polish: dark base with accent glow ---
void HackerStyle::polish(QPalette &palette) {
  const auto &tc = ThemeEngine::instance().activeTheme().colors;
  palette.setColor(QPalette::Window, tc.surfaceBase);
  palette.setColor(QPalette::WindowText, tc.textPrimary);
  palette.setColor(QPalette::Base, tc.surfaceRaised);
  palette.setColor(QPalette::AlternateBase, tc.surfaceOverlay);
  palette.setColor(QPalette::Text, tc.textPrimary);
  palette.setColor(QPalette::BrightText, tc.accentPrimary);
  palette.setColor(QPalette::Button, tc.surfaceRaised);
  palette.setColor(QPalette::ButtonText, tc.textPrimary);
  palette.setColor(QPalette::Highlight, tc.accentSoft);
  palette.setColor(QPalette::HighlightedText, tc.accentPrimary);
  palette.setColor(QPalette::ToolTipBase, tc.surfaceOverlay);
  palette.setColor(QPalette::ToolTipText, tc.textPrimary);
  palette.setColor(QPalette::Link, tc.textLink);
  palette.setColor(QPalette::PlaceholderText, tc.textMuted);

  palette.setColor(QPalette::Disabled, QPalette::WindowText, tc.textDisabled);
  palette.setColor(QPalette::Disabled, QPalette::Text, tc.textDisabled);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, tc.textDisabled);
  palette.setColor(QPalette::Disabled, QPalette::Highlight, tc.borderDefault);
}

// --- primitive elements ---
void HackerStyle::drawPrimitive(PrimitiveElement element,
                                const QStyleOption *option, QPainter *painter,
                                const QWidget *widget) const {
  const auto &tc = ThemeEngine::instance().activeTheme().colors;
  painter->setRenderHint(QPainter::Antialiasing, true);

  switch (element) {

  case PE_FrameFocusRect:
    // Replace dotted focus rect with accent glow line
    {
      painter->save();
      QPen pen(tc.accentPrimary, 1.5);
      painter->setPen(pen);
      painter->setBrush(Qt::NoBrush);
      painter->drawRoundedRect(
          QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
      painter->restore();
    }
    return;

  case PE_PanelItemViewItem:
    // Tree/list/table item backgrounds with accent tint
    {
      painter->save();
      QRectF r = QRectF(option->rect).adjusted(1, 0.5, -1, -0.5);

      if (option->state & QStyle::State_Selected) {
        painter->setBrush(tc.accentSoft);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(r, 4, 4);
      } else if (option->state & QStyle::State_MouseOver) {
        QColor hover = tc.accentSoft;
        hover.setAlpha(40);
        painter->setBrush(hover);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(r, 4, 4);
      }
      painter->restore();
    }
    return;

  case PE_IndicatorCheckBox:
    // Custom checkbox with rounded corners
    {
      painter->save();
      QRectF boxRect(option->rect);
      boxRect = boxRect.adjusted(1, 1, -1, -1);
      qreal radius = 3;

      bool checked = option->state & QStyle::State_On;
      bool hovered = option->state & QStyle::State_MouseOver;

      // Box background
      QColor bg = checked ? tc.accentPrimary : tc.inputBg;
      QColor border = checked ? tc.accentPrimary
                     : hovered ? tc.accentPrimary
                               : tc.borderDefault;

      painter->setBrush(bg);
      painter->setPen(QPen(border, 1.5));
      painter->drawRoundedRect(boxRect, radius, radius);

      // Checkmark
      if (checked) {
        painter->setPen(QPen(tc.surfaceBase, 2.0, Qt::SolidLine, Qt::RoundCap,
                             Qt::RoundJoin));
        qreal cx = boxRect.center().x();
        qreal cy = boxRect.center().y();
        qreal s = boxRect.width() * 0.25;
        QPainterPath checkPath;
        checkPath.moveTo(cx - s, cy);
        checkPath.lineTo(cx - s * 0.3, cy + s * 0.7);
        checkPath.lineTo(cx + s, cy - s * 0.6);
        painter->drawPath(checkPath);
      }
      painter->restore();
    }
    return;

  case PE_IndicatorRadioButton:
    // Custom radio with glow dot
    {
      painter->save();
      QRectF r(option->rect);
      r = r.adjusted(1, 1, -1, -1);

      bool checked = option->state & QStyle::State_On;
      bool hovered = option->state & QStyle::State_MouseOver;

      QColor border = checked  ? tc.accentPrimary
                      : hovered ? tc.accentPrimary
                                : tc.borderDefault;
      QColor bg = tc.inputBg;

      painter->setPen(QPen(border, 1.5));
      painter->setBrush(bg);
      painter->drawEllipse(r);

      if (checked) {
        QRectF dotR = r.adjusted(4, 4, -4, -4);
        painter->setPen(Qt::NoPen);
        painter->setBrush(tc.accentPrimary);
        painter->drawEllipse(dotR);
      }
      painter->restore();
    }
    return;

  case PE_FrameGroupBox:
    // Rounded group box border
    {
      painter->save();
      QRectF r = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
      painter->setPen(QPen(tc.borderDefault, 1));
      painter->setBrush(Qt::NoBrush);
      painter->drawRoundedRect(r, 8, 8);
      painter->restore();
    }
    return;

  case PE_PanelToolBar:
    // Flat toolbar background
    {
      painter->save();
      painter->fillRect(option->rect, tc.surfaceRaised);
      // subtle bottom border
      painter->setPen(QPen(tc.borderSubtle, 1));
      painter->drawLine(option->rect.bottomLeft(), option->rect.bottomRight());
      painter->restore();
    }
    return;

  default:
    break;
  }

  QProxyStyle::drawPrimitive(element, option, painter, widget);
}

// --- control elements ---
void HackerStyle::drawControl(ControlElement element,
                              const QStyleOption *option, QPainter *painter,
                              const QWidget *widget) const {
  switch (element) {
  case CE_MenuItem:
    drawMenuItem(option, painter, widget);
    return;
  case CE_ProgressBarContents:
    drawProgressBarContents(option, painter, widget);
    return;
  case CE_TabBarTabShape:
    drawTabShape(option, painter, widget);
    return;
  case CE_HeaderSection:
    drawHeaderSection(option, painter, widget);
    return;
  default:
    break;
  }
  QProxyStyle::drawControl(element, option, painter, widget);
}

// --- complex controls ---
void HackerStyle::drawComplexControl(ComplexControl control,
                                     const QStyleOptionComplex *option,
                                     QPainter *painter,
                                     const QWidget *widget) const {
  switch (control) {
  case CC_ScrollBar:
    drawScrollBarHandle(option, painter, widget);
    return;
  default:
    break;
  }
  QProxyStyle::drawComplexControl(control, option, painter, widget);
}

// --- pixel metrics ---
int HackerStyle::pixelMetric(PixelMetric metric, const QStyleOption *option,
                             const QWidget *widget) const {
  switch (metric) {
  case PM_ScrollBarExtent:
    return 8; // thin scrollbars
  case PM_ScrollBarSliderMin:
    return 30;
  case PM_IndicatorWidth:
  case PM_IndicatorHeight:
    return 18;
  case PM_ExclusiveIndicatorWidth:
  case PM_ExclusiveIndicatorHeight:
    return 18;
  case PM_MenuPanelWidth:
    return 0;
  case PM_MenuHMargin:
    return 4;
  case PM_MenuVMargin:
    return 4;
  case PM_SplitterWidth:
    return 2;
  default:
    break;
  }
  return QProxyStyle::pixelMetric(metric, option, widget);
}

QSize HackerStyle::sizeFromContents(ContentsType type,
                                    const QStyleOption *option,
                                    const QSize &contentsSize,
                                    const QWidget *widget) const {
  QSize s = QProxyStyle::sizeFromContents(type, option, contentsSize, widget);
  switch (type) {
  case CT_MenuItem: {
    const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (mi && mi->menuItemType == QStyleOptionMenuItem::Separator)
      return QSize(s.width(), 9);
    s.setHeight(qMax(s.height(), 32));
    return s;
  }
  case CT_TabBarTab:
    s.setHeight(qMax(s.height(), 36));
    return s;
  default:
    break;
  }
  return s;
}

int HackerStyle::styleHint(StyleHint hint, const QStyleOption *option,
                           const QWidget *widget,
                           QStyleHintReturn *returnData) const {
  switch (hint) {
  case SH_ScrollBar_ContextMenu:
    return 0; // no context menu on scrollbars
  case SH_ItemView_ShowDecorationSelected:
    return 1;
  default:
    break;
  }
  return QProxyStyle::styleHint(hint, option, widget, returnData);
}

// === Private draw helpers ===

void HackerStyle::drawMenuItem(const QStyleOption *option, QPainter *painter,
                               const QWidget *widget) const {
  const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
  if (!mi)
    return;
  const auto &tc = ThemeEngine::instance().activeTheme().colors;
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->save();

  QRect r = mi->rect;

  // Separator
  if (mi->menuItemType == QStyleOptionMenuItem::Separator) {
    painter->setPen(QPen(tc.borderSubtle, 1));
    int y = r.center().y();
    painter->drawLine(r.left() + 8, y, r.right() - 8, y);
    painter->restore();
    return;
  }

  // Background
  bool selected = mi->state & QStyle::State_Selected;
  bool enabled = mi->state & QStyle::State_Enabled;

  if (selected && enabled) {
    QPainterPath path;
    QRectF bgR = QRectF(r).adjusted(4, 1, -4, -1);
    path.addRoundedRect(bgR, 4, 4);
    painter->fillPath(path, tc.accentSoft);
  }

  // Icon
  QRect iconRect(r.x() + 8, r.y(), 24, r.height());
  if (!mi->icon.isNull()) {
    QPixmap pix = mi->icon.pixmap(16, 16,
                                  enabled ? QIcon::Normal : QIcon::Disabled);
    QRect pr = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                                   pix.size(), iconRect);
    painter->drawPixmap(pr, pix);
  }

  // Text
  int textX = iconRect.right() + 4;
  QRect textRect(textX, r.y(), r.width() - textX - 8, r.height());
  QColor textColor = !enabled ? tc.textDisabled
                     : selected ? tc.accentPrimary
                                : tc.textPrimary;
  painter->setPen(textColor);

  QString text = mi->text;
  int tabIdx = text.indexOf(QLatin1Char('\t'));
  if (tabIdx >= 0) {
    QString shortcut = text.mid(tabIdx + 1);
    text = text.left(tabIdx);

    // Shortcut in muted text
    QRect scRect = textRect;
    painter->setPen(selected ? tc.textSecondary : tc.textMuted);
    painter->drawText(scRect, Qt::AlignVCenter | Qt::AlignRight, shortcut);
  }

  painter->setPen(textColor);
  painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

  // Submenu arrow
  if (mi->menuItemType == QStyleOptionMenuItem::SubMenu) {
    int arrowX = r.right() - 16;
    int arrowY = r.center().y();
    QPainterPath arrow;
    arrow.moveTo(arrowX, arrowY - 4);
    arrow.lineTo(arrowX + 5, arrowY);
    arrow.lineTo(arrowX, arrowY + 4);
    painter->setPen(QPen(tc.textMuted, 1.2));
    painter->drawPath(arrow);
  }

  painter->restore();
}

void HackerStyle::drawScrollBarHandle(const QStyleOptionComplex *option,
                                      QPainter *painter,
                                      const QWidget *widget) const {
  Q_UNUSED(widget)
  const auto *sbOpt = qstyleoption_cast<const QStyleOptionSlider *>(option);
  if (!sbOpt)
    return;
  const auto &tc = ThemeEngine::instance().activeTheme().colors;
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->save();

  // Transparent track
  painter->fillRect(option->rect, Qt::transparent);

  // Thumb
  QRect thumbRect =
      proxy()->subControlRect(CC_ScrollBar, sbOpt, SC_ScrollBarSlider, widget);

  bool hovered = sbOpt->activeSubControls & SC_ScrollBarSlider;
  QColor thumbColor = hovered ? tc.scrollThumbHover : tc.scrollThumb;

  if (sbOpt->orientation == Qt::Vertical) {
    thumbRect.adjust(1, 2, -1, -2);
  } else {
    thumbRect.adjust(2, 1, -2, -1);
  }

  QPainterPath path;
  path.addRoundedRect(QRectF(thumbRect), 3, 3);
  painter->fillPath(path, thumbColor);

  painter->restore();
}

void HackerStyle::drawProgressBarContents(const QStyleOption *option,
                                          QPainter *painter,
                                          const QWidget *widget) const {
  Q_UNUSED(widget)
  const auto *pbOpt =
      qstyleoption_cast<const QStyleOptionProgressBar *>(option);
  if (!pbOpt)
    return;
  const auto &tc = ThemeEngine::instance().activeTheme().colors;
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->save();

  QRect r = option->rect;
  qreal progress = 0;
  if (pbOpt->maximum > pbOpt->minimum)
    progress = qreal(pbOpt->progress - pbOpt->minimum) /
               qreal(pbOpt->maximum - pbOpt->minimum);

  int fillW = int(r.width() * progress);
  if (fillW > 0) {
    QRectF fillR(r.x(), r.y(), fillW, r.height());
    QLinearGradient grad(fillR.topLeft(), fillR.topRight());
    grad.setColorAt(0, tc.accentPrimary);
    QColor endColor = tc.accentPrimary;
    endColor.setAlpha(180);
    grad.setColorAt(1, endColor);

    QPainterPath path;
    path.addRoundedRect(fillR.adjusted(0, 0.5, 0, -0.5), 3, 3);
    painter->fillPath(path, grad);

    // Glow at leading edge
    if (fillW > 4) {
      QRectF glowR(fillR.right() - 6, fillR.top(), 6, fillR.height());
      QLinearGradient glowGrad(glowR.topLeft(), glowR.topRight());
      glowGrad.setColorAt(0, Qt::transparent);
      QColor glow = tc.accentPrimary;
      glow.setAlpha(100);
      glowGrad.setColorAt(1, glow);
      painter->fillRect(glowR, glowGrad);
    }
  }

  painter->restore();
}

void HackerStyle::drawTabShape(const QStyleOption *option, QPainter *painter,
                               const QWidget *widget) const {
  Q_UNUSED(widget)
  const auto *tabOpt = qstyleoption_cast<const QStyleOptionTab *>(option);
  if (!tabOpt)
    return;
  const auto &tc = ThemeEngine::instance().activeTheme().colors;
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->save();

  QRectF r = QRectF(option->rect);
  bool selected = tabOpt->state & QStyle::State_Selected;
  bool hovered = tabOpt->state & QStyle::State_MouseOver;

  // Background
  QColor bg = selected ? tc.tabActiveBg
              : hovered ? tc.accentSoft
                        : tc.surfaceRaised;
  painter->fillRect(r, bg);

  // Active indicator line at bottom
  if (selected) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(tc.accentPrimary);
    QRectF indicator(r.left(), r.bottom() - 2, r.width(), 2);
    painter->drawRect(indicator);
  }

  painter->restore();
}

void HackerStyle::drawHeaderSection(const QStyleOption *option,
                                    QPainter *painter,
                                    const QWidget *widget) const {
  Q_UNUSED(widget)
  const auto &tc = ThemeEngine::instance().activeTheme().colors;
  painter->save();

  QRect r = option->rect;
  painter->fillRect(r, tc.surfaceRaised);

  // Bottom border
  painter->setPen(QPen(tc.borderDefault, 1));
  painter->drawLine(r.bottomLeft(), r.bottomRight());

  // Right separator
  painter->setPen(QPen(tc.borderSubtle, 1));
  painter->drawLine(r.topRight(), r.bottomRight());

  painter->restore();
}
