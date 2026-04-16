#include "hackerprogressbar.h"
#include "../../../theme/themeengine.h"
#include <QPaintEvent>
#include <QLinearGradient>
#include <QPainterPath>

HackerProgressBar::HackerProgressBar(QWidget *parent)
    : HackerWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(20);

    m_scanAnim = new QPropertyAnimation(this, "scanOffset", this);
    m_scanAnim->setEasingCurve(QEasingCurve::Linear);

    m_scanTimer = new QTimer(this);
    m_scanTimer->setInterval(2000);
    connect(m_scanTimer, &QTimer::timeout, this, [this]() {
        if (!m_scanEffect || m_value <= m_min) return;
        m_scanAnim->stop();
        m_scanAnim->setDuration(1200);
        m_scanAnim->setStartValue(0.0);
        m_scanAnim->setEndValue(1.0);
        m_scanAnim->start();
    });
}

void HackerProgressBar::setValue(int value)
{
    m_value = qBound(m_min, value, m_max);
    update();
}

int HackerProgressBar::value() const { return m_value; }

void HackerProgressBar::setRange(int min, int max)
{
    m_min = min;
    m_max = qMax(min, max);
    m_value = qBound(m_min, m_value, m_max);
    update();
}

void HackerProgressBar::setMinimum(int min) { setRange(min, m_max); }
void HackerProgressBar::setMaximum(int max) { setRange(m_min, max); }
int HackerProgressBar::minimum() const { return m_min; }
int HackerProgressBar::maximum() const { return m_max; }

void HackerProgressBar::setTextVisible(bool visible)
{
    m_textVisible = visible;
    update();
}

bool HackerProgressBar::isTextVisible() const { return m_textVisible; }

void HackerProgressBar::setScanEffect(bool enabled)
{
    m_scanEffect = enabled;
    if (enabled)
        m_scanTimer->start();
    else
        m_scanTimer->stop();
}

void HackerProgressBar::setScanOffset(qreal offset)
{
    m_scanOffset = offset;
    update();
}

qreal HackerProgressBar::fraction() const
{
    if (m_max == m_min) return 0.0;
    return static_cast<qreal>(m_value - m_min) / (m_max - m_min);
}

QSize HackerProgressBar::sizeHint() const { return QSize(200, 20); }
QSize HackerProgressBar::minimumSizeHint() const { return QSize(60, 12); }

void HackerProgressBar::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -1, -1);
    const int rad = qMin(radius(), static_cast<int>(r.height() / 2));
    const auto &c = colors();

    // Track
    drawRoundedSurface(p, r, c.surfaceSunken, rad);

    // Fill
    qreal frac = fraction();
    if (frac > 0.0) {
        qreal fillW = r.width() * frac;
        QRectF fillRect(r.left(), r.top(), fillW, r.height());

        // Gradient fill
        QLinearGradient grad(fillRect.topLeft(), fillRect.topRight());
        grad.setColorAt(0.0, c.accentPrimary);
        grad.setColorAt(1.0, c.accentSoft);

        p.save();
        QPainterPath clip;
        clip.addRoundedRect(r, rad, rad);
        p.setClipPath(clip);

        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawRoundedRect(fillRect, rad, rad);

        // Scan effect: bright band sweeping across
        if (m_scanEffect && m_scanOffset > 0.0 && m_scanOffset < 1.0) {
            qreal bandX = fillRect.left() + fillRect.width() * m_scanOffset;
            qreal bandW = 30.0;
            QLinearGradient scanGrad(bandX - bandW, 0, bandX + bandW, 0);
            QColor bright = c.accentGlow;
            bright.setAlphaF(0.5);
            scanGrad.setColorAt(0.0, Qt::transparent);
            scanGrad.setColorAt(0.5, bright);
            scanGrad.setColorAt(1.0, Qt::transparent);
            p.setBrush(scanGrad);
            p.drawRect(QRectF(bandX - bandW, fillRect.top(), bandW * 2, fillRect.height()));
        }

        // Leading edge glow
        qreal glowI = glowAmount();
        if (glowI > 0.01 && fillW > 2) {
            QColor gc = c.accentGlow;
            gc.setAlphaF(glowI * 0.4);
            qreal edgeX = fillRect.right();
            QLinearGradient edgeGrad(edgeX - 8, 0, edgeX + 4, 0);
            edgeGrad.setColorAt(0.0, Qt::transparent);
            edgeGrad.setColorAt(0.5, gc);
            edgeGrad.setColorAt(1.0, Qt::transparent);
            p.setBrush(edgeGrad);
            p.drawRect(QRectF(edgeX - 8, fillRect.top(), 12, fillRect.height()));
        }

        p.restore();
    }

    // Text overlay
    if (m_textVisible) {
        int pct = qRound(frac * 100.0);
        QString text = QString::number(pct) + QStringLiteral("%");
        QFont f = font();
        f.setFamily(QStringLiteral("monospace"));
        f.setPointSizeF(f.pointSizeF() * 0.85);
        drawTextInRect(p, r, text, c.textPrimary, Qt::AlignCenter, f);
    }
}
