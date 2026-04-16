#include "hackerscrollbar.h"
#include "../../../theme/themeengine.h"
#include <QPaintEvent>
#include <QMouseEvent>
#include <QScrollBar>

HackerScrollBar::HackerScrollBar(Qt::Orientation orientation, QWidget *parent)
    : HackerWidget(parent), m_orientation(orientation)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMouseTracking(true);

    m_opacityAnim = new QPropertyAnimation(this, "barOpacity", this);
    m_opacityAnim->setEasingCurve(QEasingCurve::InOutCubic);
    m_widthAnim = new QPropertyAnimation(this, "barWidth", this);
    m_widthAnim->setEasingCurve(QEasingCurve::InOutCubic);
    m_thumbAnim = new QPropertyAnimation(this, "thumbPosition", this);
    m_thumbAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(1500);
    connect(m_idleTimer, &QTimer::timeout, this, [this]() {
        if (!m_dragging && !underMouse()) {
            m_opacityAnim->stop();
            m_opacityAnim->setDuration(animDuration() * 2);
            m_opacityAnim->setStartValue(m_barOpacity);
            m_opacityAnim->setEndValue(0.0);
            m_opacityAnim->start();
        }
    });

    if (orientation == Qt::Vertical) {
        setFixedWidth(12);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    } else {
        setFixedHeight(12);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
}

void HackerScrollBar::attachTo(QAbstractScrollArea *area)
{
    QScrollBar *sb = (m_orientation == Qt::Vertical)
                         ? area->verticalScrollBar()
                         : area->horizontalScrollBar();
    connect(sb, &QScrollBar::rangeChanged, this, [this](int min, int max) {
        setRange(min, max);
    });
    connect(sb, &QScrollBar::valueChanged, this, [this](int val) {
        setValue(val);
    });
    connect(this, &HackerScrollBar::valueChanged, sb, &QScrollBar::setValue);
    setRange(sb->minimum(), sb->maximum());
    setPageStep(sb->pageStep());
    setValue(sb->value());
}

void HackerScrollBar::setRange(int min, int max)
{
    m_min = min;
    m_max = max;
    update();
}

void HackerScrollBar::setValue(int value)
{
    int clamped = qBound(m_min, value, m_max);
    if (m_value == clamped) return;
    m_value = clamped;
    showBar();

    // Animate thumb position
    qreal target = (m_max == m_min)
                       ? 0.0
                       : static_cast<qreal>(m_value - m_min) / (m_max - m_min);
    m_thumbAnim->stop();
    int dur = animDuration();
    if (dur <= 0 || m_dragging) {
        m_thumbPosition = target;
        update();
    } else {
        m_thumbAnim->setDuration(dur);
        m_thumbAnim->setStartValue(m_thumbPosition);
        m_thumbAnim->setEndValue(target);
        m_thumbAnim->start();
    }

    emit valueChanged(m_value);
}

int HackerScrollBar::value() const { return m_value; }
void HackerScrollBar::setPageStep(int step) { m_pageStep = step; update(); }

void HackerScrollBar::setBarOpacity(qreal o)
{
    m_barOpacity = qBound(0.0, o, 1.0);
    update();
}

void HackerScrollBar::setBarWidth(qreal w)
{
    m_barWidth = w;
    update();
}

void HackerScrollBar::setThumbPosition(qreal pos)
{
    m_thumbPosition = qBound(0.0, pos, 1.0);
    update();
}

QSize HackerScrollBar::sizeHint() const
{
    if (m_orientation == Qt::Vertical)
        return QSize(12, 100);
    return QSize(100, 12);
}

qreal HackerScrollBar::trackLength() const
{
    return (m_orientation == Qt::Vertical) ? height() : width();
}

qreal HackerScrollBar::thumbLength() const
{
    if (m_max == m_min) return trackLength();
    int range = m_max - m_min + m_pageStep;
    qreal ratio = static_cast<qreal>(m_pageStep) / range;
    return qMax(20.0, trackLength() * ratio);
}

QRectF HackerScrollBar::thumbRect() const
{
    qreal tl = trackLength();
    qreal th = thumbLength();
    qreal available = tl - th;
    qreal offset = available * m_thumbPosition;

    if (m_orientation == Qt::Vertical) {
        qreal x = (width() - m_barWidth) / 2.0;
        return QRectF(x, offset, m_barWidth, th);
    }
    qreal y = (height() - m_barWidth) / 2.0;
    return QRectF(offset, y, th, m_barWidth);
}

void HackerScrollBar::showBar()
{
    m_opacityAnim->stop();
    int dur = animDuration();
    if (dur <= 0) {
        m_barOpacity = 1.0;
    } else {
        m_opacityAnim->setDuration(dur);
        m_opacityAnim->setStartValue(m_barOpacity);
        m_opacityAnim->setEndValue(1.0);
        m_opacityAnim->start();
    }
    scheduleHide();
}

void HackerScrollBar::scheduleHide()
{
    m_idleTimer->start();
}

void HackerScrollBar::paintEvent(QPaintEvent * /*event*/)
{
    if (m_barOpacity < 0.01 || m_max <= m_min) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setOpacity(m_barOpacity);

    const auto &c = colors();

    // Track
    QRectF trackRect;
    qreal trackW = m_barWidth + 2;
    if (m_orientation == Qt::Vertical) {
        qreal x = (width() - trackW) / 2.0;
        trackRect = QRectF(x, 0, trackW, height());
    } else {
        qreal y = (height() - trackW) / 2.0;
        trackRect = QRectF(0, y, width(), trackW);
    }
    drawRoundedSurface(p, trackRect, c.scrollTrack, trackW / 2.0);

    // Thumb
    QRectF tr = thumbRect();
    QColor thumbColor = underMouse() ? c.scrollThumbHover : c.scrollThumb;
    drawRoundedSurface(p, tr, thumbColor, m_barWidth / 2.0);
}

void HackerScrollBar::enterEvent(QEnterEvent *event)
{
    showBar();
    m_widthAnim->stop();
    m_widthAnim->setDuration(animDuration());
    m_widthAnim->setStartValue(m_barWidth);
    m_widthAnim->setEndValue(8.0);
    m_widthAnim->start();
    HackerWidget::enterEvent(event);
}

void HackerScrollBar::leaveEvent(QEvent *event)
{
    m_widthAnim->stop();
    m_widthAnim->setDuration(animDuration());
    m_widthAnim->setStartValue(m_barWidth);
    m_widthAnim->setEndValue(4.0);
    m_widthAnim->start();
    scheduleHide();
    HackerWidget::leaveEvent(event);
}

void HackerScrollBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    QRectF tr = thumbRect();
    if (tr.contains(event->position())) {
        m_dragging = true;
        m_dragOffset = (m_orientation == Qt::Vertical)
                           ? event->position().y() - tr.top()
                           : event->position().x() - tr.left();
    } else {
        // Click on track: jump
        qreal pos = (m_orientation == Qt::Vertical)
                        ? event->position().y()
                        : event->position().x();
        qreal tl = trackLength();
        qreal th = thumbLength();
        qreal available = tl - th;
        if (available > 0) {
            qreal ratio = qBound(0.0, (pos - th / 2.0) / available, 1.0);
            setValue(m_min + qRound(ratio * (m_max - m_min)));
        }
    }
}

void HackerScrollBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) return;
    qreal pos = (m_orientation == Qt::Vertical)
                    ? event->position().y() - m_dragOffset
                    : event->position().x() - m_dragOffset;
    qreal tl = trackLength();
    qreal th = thumbLength();
    qreal available = tl - th;
    if (available > 0) {
        qreal ratio = qBound(0.0, pos / available, 1.0);
        setValue(m_min + qRound(ratio * (m_max - m_min)));
    }
}

void HackerScrollBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        scheduleHide();
    }
    HackerWidget::mouseReleaseEvent(event);
}
