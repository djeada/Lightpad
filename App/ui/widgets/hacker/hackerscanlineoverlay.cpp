#include "hackerscanlineoverlay.h"

#include <QEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>

HackerScanlineOverlay::HackerScanlineOverlay(QWidget *parent)
    : QWidget(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_TranslucentBackground);
  setFocusPolicy(Qt::NoFocus);
  hide();
  if (parent)
    parent->installEventFilter(this);
}

void HackerScanlineOverlay::setEnabled(bool enabled) {
  m_enabled = enabled;
  setVisible(enabled);
  if (enabled && parentWidget()) {
    setGeometry(parentWidget()->rect());
    raise();
  }
  update();
}

void HackerScanlineOverlay::setLineSpacing(int px) {
  m_lineSpacing = qMax(2, px);
  update();
}

void HackerScanlineOverlay::setLineOpacity(qreal opacity) {
  m_lineOpacity = qBound(0.0, opacity, 1.0);
  update();
}

bool HackerScanlineOverlay::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parent() && event->type() == QEvent::Resize) {
    if (parentWidget())
      setGeometry(parentWidget()->rect());
    if (m_enabled)
      raise();
  }
  return QWidget::eventFilter(watched, event);
}

void HackerScanlineOverlay::paintEvent(QPaintEvent *) {
  if (!m_enabled)
    return;
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);
  QColor line(0, 0, 0);
  line.setAlphaF(m_lineOpacity);
  p.setPen(line);
  for (int y = 0; y < height(); y += m_lineSpacing) {
    p.drawLine(0, y, width(), y);
  }
}
