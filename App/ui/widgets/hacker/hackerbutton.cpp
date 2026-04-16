#include "hackerbutton.h"
#include "../../../theme/themeengine.h"
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFontMetrics>

HackerButton::HackerButton(const QString &text, QWidget *parent)
    : HackerWidget(parent), m_text(text)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

HackerButton::HackerButton(const QIcon &icon, const QString &text, QWidget *parent)
    : HackerWidget(parent), m_text(text), m_icon(icon)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

void HackerButton::setText(const QString &text)
{
    m_text = text;
    updateGeometry();
    update();
}

QString HackerButton::text() const { return m_text; }

void HackerButton::setIcon(const QIcon &icon)
{
    m_icon = icon;
    updateGeometry();
    update();
}

QIcon HackerButton::icon() const { return m_icon; }

void HackerButton::setVariant(Variant v)
{
    m_variant = v;
    update();
}

HackerButton::Variant HackerButton::variant() const { return m_variant; }

void HackerButton::setEnabled(bool enabled)
{
    m_enabled = enabled;
    QWidget::setEnabled(enabled);
    update();
}

void HackerButton::setCheckable(bool checkable)
{
    m_checkable = checkable;
}

bool HackerButton::isCheckable() const { return m_checkable; }
bool HackerButton::isChecked() const { return m_checked; }

void HackerButton::setChecked(bool checked)
{
    if (m_checked == checked) return;
    m_checked = checked;
    emit toggled(m_checked);
    update();
}

QSize HackerButton::sizeHint() const
{
    QFont f = font();
    f.setFamily(QStringLiteral("monospace"));
    QFontMetrics fm(f);
    int textW = fm.horizontalAdvance(m_text);
    int iconW = m_icon.isNull() ? 0 : 20;
    int spacing = (!m_icon.isNull() && !m_text.isEmpty()) ? 6 : 0;
    return QSize(textW + iconW + spacing + 32, 32);
}

QSize HackerButton::minimumSizeHint() const
{
    return QSize(48, 32);
}

QColor HackerButton::bgColor() const
{
    const auto &c = colors();
    if (m_checked) return activeBgColor();
    switch (m_variant) {
    case Primary:   return c.btnPrimaryBg;
    case Secondary: return c.btnSecondaryBg;
    case Danger:    return c.btnDangerBg;
    case Ghost:     return Qt::transparent;
    }
    return c.btnSecondaryBg;
}

QColor HackerButton::fgColor() const
{
    const auto &c = colors();
    switch (m_variant) {
    case Primary:   return c.btnPrimaryFg;
    case Secondary: return c.btnSecondaryFg;
    case Danger:    return c.btnDangerFg;
    case Ghost:     return c.textPrimary;
    }
    return c.btnSecondaryFg;
}

QColor HackerButton::hoverBgColor() const
{
    const auto &c = colors();
    switch (m_variant) {
    case Primary:   return c.btnPrimaryHover;
    case Secondary: return c.btnSecondaryHover;
    case Danger:    return c.btnDangerHover;
    case Ghost:     return c.btnGhostHover;
    }
    return c.btnSecondaryHover;
}

QColor HackerButton::activeBgColor() const
{
    const auto &c = colors();
    switch (m_variant) {
    case Primary:   return c.btnPrimaryActive;
    case Secondary: return c.btnSecondaryActive;
    case Danger:    return c.btnDangerActive;
    case Ghost:     return c.btnGhostActive;
    }
    return c.btnSecondaryActive;
}

void HackerButton::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -1, -1);
    const int rad = radius();
    const auto &c = colors();

    // Background: blend base → hover → active
    QColor bg = blendColors(bgColor(), hoverBgColor(), m_hoverProgress);
    bg = blendColors(bg, activeBgColor(), m_pressProgress);
    drawRoundedSurface(p, r, bg, rad);

    // Glow border for Primary on hover/focus
    if (m_variant == Primary && (m_hoverProgress > 0.01 || m_focusProgress > 0.01)) {
        qreal intensity = qMax(m_hoverProgress, m_focusProgress) * glowAmount();
        drawGlowBorder(p, r, c.accentGlow, intensity, rad);
    }

    // Border
    QColor borderColor = blendColors(c.borderDefault, c.borderFocus, m_focusProgress);
    p.setPen(QPen(borderColor, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, rad, rad);

    // Content layout
    const int padding = 16;
    const int iconSize = 16;
    QRectF contentRect = r.adjusted(padding, 0, -padding, 0);

    QFont f = font();
    f.setFamily(QStringLiteral("monospace"));
    p.setFont(f);
    QFontMetrics fm(f);

    int textW = fm.horizontalAdvance(m_text);
    int iconW = m_icon.isNull() ? 0 : iconSize;
    int spacing = (!m_icon.isNull() && !m_text.isEmpty()) ? 6 : 0;
    int totalW = iconW + spacing + textW;
    qreal startX = contentRect.left() + (contentRect.width() - totalW) / 2.0;
    qreal centerY = contentRect.center().y();

    // Icon
    if (!m_icon.isNull()) {
        QRectF iconRect(startX, centerY - iconSize / 2.0, iconSize, iconSize);
        m_icon.paint(&p, iconRect.toRect());
        startX += iconSize + spacing;
    }

    // Text
    if (!m_text.isEmpty()) {
        p.setPen(fgColor());
        QRectF textRect(startX, contentRect.top(), textW, contentRect.height());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, m_text);
    }

    // Disabled overlay
    if (!m_enabled) {
        QColor overlay = c.surfaceBase;
        overlay.setAlphaF(0.5);
        drawRoundedSurface(p, r, overlay, rad);
    }
}

void HackerButton::mousePressEvent(QMouseEvent *event)
{
    if (!m_enabled || event->button() != Qt::LeftButton) return;
    startAnimation(m_pressAnim, m_pressProgress, 1.0);
    HackerWidget::mousePressEvent(event);
}

void HackerButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_enabled || event->button() != Qt::LeftButton) return;
    startAnimation(m_pressAnim, m_pressProgress, 0.0);
    if (rect().contains(event->pos())) {
        if (m_checkable) setChecked(!m_checked);
        emit clicked();
    }
    HackerWidget::mouseReleaseEvent(event);
}

void HackerButton::keyPressEvent(QKeyEvent *event)
{
    if (!m_enabled) return;
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) {
        if (m_checkable) setChecked(!m_checked);
        emit clicked();
        return;
    }
    HackerWidget::keyPressEvent(event);
}
