#include "hackertabbar.h"
#include "../../../theme/themeengine.h"
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <QToolTip>
#include <QLinearGradient>
#include <QPainterPath>

HackerTabBar::HackerTabBar(QWidget *parent)
    : HackerWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(32);

    m_indicatorAnim = new QPropertyAnimation(this, "indicatorX", this);
    m_indicatorAnim->setEasingCurve(QEasingCurve::InOutCubic);
}

int HackerTabBar::addTab(const QString &text)
{
    m_tabs.append({text, QIcon(), QString(), false});
    if (m_currentIndex < 0) setCurrentIndex(0);
    updateGeometry();
    update();
    return m_tabs.size() - 1;
}

int HackerTabBar::addTab(const QIcon &icon, const QString &text)
{
    m_tabs.append({text, icon, QString(), false});
    if (m_currentIndex < 0) setCurrentIndex(0);
    updateGeometry();
    update();
    return m_tabs.size() - 1;
}

void HackerTabBar::removeTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs.removeAt(index);
    if (m_currentIndex >= m_tabs.size())
        setCurrentIndex(m_tabs.size() - 1);
    else if (m_currentIndex == index)
        setCurrentIndex(qMax(0, index - 1));
    updateGeometry();
    update();
}

int HackerTabBar::count() const { return m_tabs.size(); }
int HackerTabBar::currentIndex() const { return m_currentIndex; }

void HackerTabBar::setCurrentIndex(int index)
{
    if (index < -1 || index >= m_tabs.size()) return;
    if (m_currentIndex == index) return;
    m_currentIndex = index;
    animateIndicatorTo(index);
    emit currentChanged(index);
    update();
}

QString HackerTabBar::tabText(int index) const
{
    if (index < 0 || index >= m_tabs.size()) return QString();
    return m_tabs[index].text;
}

void HackerTabBar::setTabText(int index, const QString &text)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs[index].text = text;
    update();
}

void HackerTabBar::setTabIcon(int index, const QIcon &icon)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs[index].icon = icon;
    update();
}

void HackerTabBar::setTabToolTip(int index, const QString &tip)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs[index].toolTip = tip;
}

void HackerTabBar::setTabsClosable(bool closable) { m_closable = closable; update(); }
void HackerTabBar::setMovable(bool movable) { m_movable = movable; }

void HackerTabBar::setTabModified(int index, bool modified)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs[index].modified = modified;
    update();
}

QSize HackerTabBar::sizeHint() const
{
    return QSize(tabWidth() * m_tabs.size(), 32);
}

int HackerTabBar::tabWidth() const { return 160; }

int HackerTabBar::tabAtPosition(const QPoint &pos) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (tabRect(i).contains(pos)) return i;
    }
    return -1;
}

QRect HackerTabBar::tabRect(int index) const
{
    int w = tabWidth();
    return QRect(index * w - m_scrollOffset, 0, w, height());
}

QRect HackerTabBar::closeButtonRect(int index) const
{
    QRect tr = tabRect(index);
    int btnSize = 14;
    int x = tr.right() - btnSize - 8;
    int y = (tr.height() - btnSize) / 2;
    return QRect(x, y, btnSize, btnSize);
}

bool HackerTabBar::isOverCloseButton(const QPoint &pos, int tabIndex) const
{
    if (!m_closable || tabIndex < 0) return false;
    return closeButtonRect(tabIndex).contains(pos);
}

void HackerTabBar::setIndicatorX(qreal x)
{
    if (qFuzzyCompare(m_indicatorX, x)) return;
    m_indicatorX = x;
    update();
}

void HackerTabBar::animateIndicatorTo(int index)
{
    if (index < 0) return;
    qreal targetX = tabRect(index).left();
    m_indicatorAnim->stop();
    int dur = animDuration();
    if (dur <= 0) {
        m_indicatorX = targetX;
        update();
        return;
    }
    m_indicatorAnim->setDuration(dur);
    m_indicatorAnim->setStartValue(m_indicatorX);
    m_indicatorAnim->setEndValue(targetX);
    m_indicatorAnim->start();
}

void HackerTabBar::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto &c = colors();
    const QRectF r = rect();

    // Background
    p.fillRect(r, c.surfaceBase);

    QFont f = font();
    f.setFamily(QStringLiteral("monospace"));
    p.setFont(f);
    QFontMetrics fm(f);

    // Draw each tab
    for (int i = 0; i < m_tabs.size(); ++i) {
        QRect tr = tabRect(i);
        if (!tr.intersects(rect())) continue;

        const auto &tab = m_tabs[i];
        bool isCurrent = (i == m_currentIndex);
        bool isHovered = (i == m_hoveredIndex);

        // Tab background
        if (isCurrent) {
            p.fillRect(tr, c.tabActiveBg);
        } else if (isHovered) {
            p.fillRect(tr, c.tabHoverBg);
        }

        // Icon + text
        int iconSize = 16;
        int contentX = tr.left() + 12;
        if (!tab.icon.isNull()) {
            QRect iconRect(contentX, (tr.height() - iconSize) / 2, iconSize, iconSize);
            tab.icon.paint(&p, iconRect);
            contentX += iconSize + 6;
        }

        // Modified dot
        if (tab.modified) {
            p.setPen(Qt::NoPen);
            p.setBrush(c.statusWarning);
            p.drawEllipse(QPointF(contentX + 2, tr.center().y()), 3, 3);
            contentX += 10;
        }

        // Text
        QColor textColor = isCurrent ? c.tabActiveFg : c.tabFg;
        p.setPen(textColor);
        int closeSpace = m_closable ? 24 : 8;
        QRect textRect(contentX, tr.top(), tr.right() - contentX - closeSpace, tr.height());
        QString elidedText = fm.elidedText(tab.text, Qt::ElideRight, textRect.width());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedText);

        // Close button
        if (m_closable && (isHovered || isCurrent)) {
            QRect cbr = closeButtonRect(i);
            bool closeHover = isOverCloseButton(mapFromGlobal(QCursor::pos()), i);
            QColor closeColor = closeHover ? c.textPrimary : c.textMuted;
            p.setPen(QPen(closeColor, 1.5));
            int m = 3;
            p.drawLine(cbr.topLeft() + QPoint(m, m), cbr.bottomRight() - QPoint(m, m));
            p.drawLine(cbr.topRight() + QPoint(-m, m), cbr.bottomLeft() + QPoint(m, -m));
        }
    }

    // Active tab indicator line (animated)
    if (m_currentIndex >= 0) {
        int tw = tabWidth();
        p.setPen(Qt::NoPen);
        p.setBrush(c.accentPrimary);
        p.drawRect(QRectF(m_indicatorX, height() - 2, tw, 2));
    }

    // Fade gradients at edges if tabs overflow
    int totalW = tabWidth() * m_tabs.size();
    if (totalW > width()) {
        if (m_scrollOffset > 0) {
            QLinearGradient leftFade(0, 0, 30, 0);
            leftFade.setColorAt(0, c.surfaceBase);
            QColor transparent = c.surfaceBase;
            transparent.setAlpha(0);
            leftFade.setColorAt(1, transparent);
            p.fillRect(QRect(0, 0, 30, height()), leftFade);
        }
        if (m_scrollOffset + width() < totalW) {
            QLinearGradient rightFade(width() - 30, 0, width(), 0);
            QColor transparent = c.surfaceBase;
            transparent.setAlpha(0);
            rightFade.setColorAt(0, transparent);
            rightFade.setColorAt(1, c.surfaceBase);
            p.fillRect(QRect(width() - 30, 0, 30, height()), rightFade);
        }
    }
}

void HackerTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    int idx = tabAtPosition(event->pos());
    if (idx < 0) return;

    if (m_closable && isOverCloseButton(event->pos(), idx)) {
        emit tabCloseRequested(idx);
        return;
    }
    m_pressedIndex = idx;
    setCurrentIndex(idx);
}

void HackerTabBar::mouseMoveEvent(QMouseEvent *event)
{
    int idx = tabAtPosition(event->pos());
    if (idx != m_hoveredIndex) {
        m_hoveredIndex = idx;
        if (idx >= 0 && !m_tabs[idx].toolTip.isEmpty())
            setToolTip(m_tabs[idx].toolTip);
        else
            setToolTip(QString());
        update();
    }
    HackerWidget::mouseMoveEvent(event);
}

void HackerTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressedIndex = -1;
    HackerWidget::mouseReleaseEvent(event);
}

void HackerTabBar::wheelEvent(QWheelEvent *event)
{
    int totalW = tabWidth() * m_tabs.size();
    if (totalW <= width()) return;
    int delta = event->angleDelta().y() > 0 ? -30 : 30;
    m_scrollOffset = qBound(0, m_scrollOffset + delta, totalW - width());
    update();
}
