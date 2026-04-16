#include "hackercheckbox.h"
#include "../../../theme/themeengine.h"
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFontMetrics>
#include <QPainterPath>

HackerCheckBox::HackerCheckBox(const QString &text, QWidget *parent)
    : HackerWidget(parent), m_text(text)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    m_checkAnim = new QPropertyAnimation(this, "checkProgress", this);
    m_checkAnim->setEasingCurve(QEasingCurve::InOutCubic);
}

void HackerCheckBox::setText(const QString &text)
{
    m_text = text;
    updateGeometry();
    update();
}

QString HackerCheckBox::text() const { return m_text; }
bool HackerCheckBox::isChecked() const { return m_checked; }

void HackerCheckBox::setChecked(bool checked)
{
    if (m_checked == checked) return;
    m_checked = checked;

    m_checkAnim->stop();
    int dur = animDuration();
    if (dur <= 0) {
        m_checkProgress = checked ? 1.0 : 0.0;
        update();
    } else {
        m_checkAnim->setDuration(dur);
        m_checkAnim->setStartValue(m_checkProgress);
        m_checkAnim->setEndValue(checked ? 1.0 : 0.0);
        m_checkAnim->start();
    }

    emit toggled(m_checked);
}

void HackerCheckBox::setCheckProgress(qreal v)
{
    if (qFuzzyCompare(m_checkProgress, v)) return;
    m_checkProgress = v;
    update();
}

QSize HackerCheckBox::sizeHint() const
{
    QFont f = font();
    f.setFamily(QStringLiteral("monospace"));
    QFontMetrics fm(f);
    int textW = m_text.isEmpty() ? 0 : fm.horizontalAdvance(m_text) + 8;
    return QSize(kBoxSize + textW + 8, qMax(kBoxSize + 8, fm.height() + 8));
}

QSize HackerCheckBox::minimumSizeHint() const
{
    return QSize(kBoxSize + 8, kBoxSize + 8);
}

void HackerCheckBox::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto &c = colors();
    const int rad = qMin(radius(), 4);

    // Checkbox box
    int boxY = (height() - kBoxSize) / 2;
    QRectF boxRect(4, boxY, kBoxSize, kBoxSize);

    // Border (brightens on hover)
    QColor borderColor = blendColors(c.borderDefault, c.borderStrong, m_hoverProgress);
    borderColor = blendColors(borderColor, c.borderFocus, m_focusProgress);

    // Fill transition: transparent → accentPrimary
    QColor fillColor = blendColors(Qt::transparent, c.accentPrimary, m_checkProgress);
    drawRoundedSurface(p, boxRect, fillColor, rad);

    p.setPen(QPen(borderColor, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(boxRect, rad, rad);

    // Checkmark
    if (m_checkProgress > 0.05) {
        p.save();
        p.setClipRect(boxRect);
        QPen checkPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(checkPen);
        p.setBrush(Qt::NoBrush);

        // Animate checkmark drawing via progress
        QPainterPath checkPath;
        QPointF p1(boxRect.left() + 4, boxRect.center().y());
        QPointF p2(boxRect.center().x() - 1, boxRect.bottom() - 4);
        QPointF p3(boxRect.right() - 3, boxRect.top() + 4);

        if (m_checkProgress < 0.5) {
            // First leg
            qreal t = m_checkProgress * 2.0;
            QPointF mid = p1 + (p2 - p1) * t;
            checkPath.moveTo(p1);
            checkPath.lineTo(mid);
        } else {
            // Full check
            qreal t = (m_checkProgress - 0.5) * 2.0;
            QPointF mid = p2 + (p3 - p2) * t;
            checkPath.moveTo(p1);
            checkPath.lineTo(p2);
            checkPath.lineTo(mid);
        }
        p.drawPath(checkPath);
        p.restore();
    }

    // Text label
    if (!m_text.isEmpty()) {
        QFont f = font();
        f.setFamily(QStringLiteral("monospace"));
        p.setFont(f);
        p.setPen(c.textPrimary);
        QRectF textRect(kBoxSize + 12, 0, width() - kBoxSize - 16, height());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, m_text);
    }
}

void HackerCheckBox::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        startAnimation(m_pressAnim, m_pressProgress, 1.0);
    }
    HackerWidget::mousePressEvent(event);
}

void HackerCheckBox::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        startAnimation(m_pressAnim, m_pressProgress, 0.0);
        if (rect().contains(event->pos())) toggle();
    }
    HackerWidget::mouseReleaseEvent(event);
}

void HackerCheckBox::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) {
        toggle();
        return;
    }
    HackerWidget::keyPressEvent(event);
}

void HackerCheckBox::toggle()
{
    setChecked(!m_checked);
}
