#include "hackerwidget.h"
#include "../../../theme/themeengine.h"
#include <QEasingCurve>

HackerWidget::HackerWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover, true);

    m_hoverAnim = new QPropertyAnimation(this, "hoverProgress", this);
    m_focusAnim = new QPropertyAnimation(this, "focusProgress", this);
    m_pressAnim = new QPropertyAnimation(this, "pressProgress", this);

    for (auto *anim : {m_hoverAnim, m_focusAnim, m_pressAnim}) {
        anim->setEasingCurve(QEasingCurve::InOutCubic);
    }

    connect(&ThemeEngine::instance(), &ThemeEngine::themeChanged,
            this, QOverload<>::of(&QWidget::update));
}

HackerWidget::~HackerWidget() = default;

void HackerWidget::setHoverProgress(qreal v)
{
    if (qFuzzyCompare(m_hoverProgress, v)) return;
    m_hoverProgress = v;
    update();
}

void HackerWidget::setFocusProgress(qreal v)
{
    if (qFuzzyCompare(m_focusProgress, v)) return;
    m_focusProgress = v;
    update();
}

void HackerWidget::setPressProgress(qreal v)
{
    if (qFuzzyCompare(m_pressProgress, v)) return;
    m_pressProgress = v;
    update();
}

const ThemeColors &HackerWidget::colors() const
{
    return ThemeEngine::instance().activeTheme().colors;
}

int HackerWidget::animDuration() const
{
    return ThemeEngine::instance().animationDuration();
}

qreal HackerWidget::glowAmount() const
{
    return ThemeEngine::instance().glowIntensity();
}

int HackerWidget::radius() const
{
    return ThemeEngine::instance().borderRadius();
}

void HackerWidget::drawGlowBorder(QPainter &p, const QRectF &rect,
                                    const QColor &color, qreal intensity,
                                    qreal rad)
{
    if (intensity < 0.01) return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    constexpr int layers = 4;
    for (int i = layers; i >= 1; --i) {
        qreal spread = i * 2.0;
        QColor c = color;
        c.setAlphaF(intensity * (0.12 / i));
        p.setBrush(c);
        p.drawRoundedRect(rect.adjusted(-spread, -spread, spread, spread),
                          rad + spread, rad + spread);
    }
    p.restore();
}

void HackerWidget::drawRoundedSurface(QPainter &p, const QRectF &rect,
                                       const QColor &bg, qreal rad)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(rect, rad, rad);
    p.restore();
}

void HackerWidget::drawTextInRect(QPainter &p, const QRectF &rect,
                                   const QString &text, const QColor &color,
                                   int alignment, const QFont &font)
{
    p.save();
    if (font != QFont())
        p.setFont(font);
    p.setPen(color);
    p.drawText(rect, alignment, text);
    p.restore();
}

QColor HackerWidget::blendColors(const QColor &a, const QColor &b, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(
        a.redF()   + (b.redF()   - a.redF())   * t,
        a.greenF() + (b.greenF() - a.greenF()) * t,
        a.blueF()  + (b.blueF()  - a.blueF())  * t,
        a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

void HackerWidget::startAnimation(QPropertyAnimation *anim,
                                   qreal from, qreal to)
{
    anim->stop();
    int dur = animDuration();
    if (dur <= 0) {
        anim->setDuration(0);
        anim->setStartValue(to);
        anim->setEndValue(to);
    } else {
        anim->setDuration(dur);
        anim->setStartValue(from);
        anim->setEndValue(to);
    }
    anim->start();
}

void HackerWidget::enterEvent(QEnterEvent *event)
{
    startAnimation(m_hoverAnim, m_hoverProgress, 1.0);
    QWidget::enterEvent(event);
}

void HackerWidget::leaveEvent(QEvent *event)
{
    startAnimation(m_hoverAnim, m_hoverProgress, 0.0);
    QWidget::leaveEvent(event);
}

void HackerWidget::focusInEvent(QFocusEvent *event)
{
    startAnimation(m_focusAnim, m_focusProgress, 1.0);
    QWidget::focusInEvent(event);
}

void HackerWidget::focusOutEvent(QFocusEvent *event)
{
    startAnimation(m_focusAnim, m_focusProgress, 0.0);
    QWidget::focusOutEvent(event);
}
