#include "terminalview.h"

#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QRadialGradient>
#include <QScrollBar>

TerminalView::TerminalView(QWidget *parent)
    : QPlainTextEdit(parent), m_background("#101418"), m_foreground("#b8c9c1"),
      m_accent("#7dffb2"), m_selection("#213a35"), m_border("#263832"),
      m_glow("#7dffb2"), m_scanlines(false), m_glowIntensity(0.3) {
  setFrameShape(QFrame::NoFrame);
  viewport()->setAttribute(Qt::WA_Hover, true);
  viewport()->setAutoFillBackground(false);
  setAttribute(Qt::WA_Hover, true);
}

void TerminalView::setVisualTheme(const QColor &background,
                                  const QColor &foreground,
                                  const QColor &accent, const QColor &selection,
                                  const QColor &border, const QColor &glow,
                                  bool scanlines, qreal glowIntensity) {
  m_background = background.isValid() ? background : m_background;
  m_foreground = foreground.isValid() ? foreground : m_foreground;
  m_accent = accent.isValid() ? accent : m_accent;
  m_selection = selection.isValid() ? selection : m_selection;
  m_border = border.isValid() ? border : m_border;
  m_glow = glow.isValid() ? glow : m_accent;
  m_scanlines = scanlines;
  m_glowIntensity = qBound(0.0, glowIntensity, 1.0);

  QPalette pal = palette();
  pal.setColor(QPalette::Base, m_background);
  pal.setColor(QPalette::Text, m_foreground);
  pal.setColor(QPalette::Highlight, m_selection);
  pal.setColor(QPalette::HighlightedText, m_foreground);
  setPalette(pal);
  viewport()->update();
}

void TerminalView::paintEvent(QPaintEvent *event) {
  {
    QPainter backgroundPainter(viewport());
    backgroundPainter.setRenderHint(QPainter::Antialiasing, false);
    const QRect r = viewport()->rect();

    QLinearGradient base(r.topLeft(), r.bottomLeft());
    base.setColorAt(0.0, m_background.lighter(102));
    base.setColorAt(0.52, m_background);
    base.setColorAt(1.0, m_background.darker(103));
    backgroundPainter.fillRect(r, base);

    if (m_glowIntensity > 0.01) {
      QRadialGradient upperLeft(
          QPointF(r.left() + r.width() * 0.16, r.top() + r.height() * 0.08),
          qMax(r.width(), r.height()) * 0.42);
      upperLeft.setColorAt(0.0, withAlpha(m_glow, 0.035 * m_glowIntensity));
      upperLeft.setColorAt(0.42, withAlpha(m_accent, 0.012 * m_glowIntensity));
      upperLeft.setColorAt(1.0, Qt::transparent);
      backgroundPainter.fillRect(r, upperLeft);
    }
  }

  QPlainTextEdit::paintEvent(event);

  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing, false);
  const QRect r = viewport()->rect();

  const QRect cursor = cursorRect();
  if (cursor.isValid() && hasFocus()) {
    QColor row = withAlpha(m_accent, 0.025 + 0.030 * m_glowIntensity);
    painter.fillRect(QRect(0, cursor.y(), r.width(), cursor.height()), row);

    QColor caretGlow = withAlpha(m_accent, 0.14 + 0.14 * m_glowIntensity);
    painter.fillRect(QRect(cursor.x() - 1, cursor.y(), 2, cursor.height()),
                     caretGlow);
  }

  if (m_glowIntensity > 0.01) {
    QLinearGradient topGlow(r.topLeft(), QPoint(r.left(), r.top() + 52));
    topGlow.setColorAt(0.0, withAlpha(m_glow, 0.012 * m_glowIntensity));
    topGlow.setColorAt(1.0, Qt::transparent);
    painter.fillRect(QRect(r.left(), r.top(), r.width(), 52), topGlow);

    QLinearGradient leftGlow(r.topLeft(), QPoint(r.left() + 72, r.top()));
    leftGlow.setColorAt(0.0, withAlpha(m_accent, 0.014 * m_glowIntensity));
    leftGlow.setColorAt(1.0, Qt::transparent);
    painter.fillRect(QRect(r.left(), r.top(), 72, r.height()), leftGlow);
  }

  if (m_scanlines) {
    painter.setPen(withAlpha(m_foreground, 0.018));
    for (int y = r.top(); y < r.bottom(); y += 5) {
      painter.drawLine(r.left(), y, r.right(), y);
    }
  }

  QRadialGradient vignette(r.center(), qMax(r.width(), r.height()) * 0.72);
  vignette.setColorAt(0.0, Qt::transparent);
  vignette.setColorAt(0.86, Qt::transparent);
  vignette.setColorAt(1.0, QColor(0, 0, 0, 18));
  painter.fillRect(r, vignette);

  QPen edgePen(withAlpha(m_border, 0.28));
  painter.setPen(edgePen);
  painter.drawLine(r.bottomLeft(), r.bottomRight());

  QColor accentLine = withAlpha(m_accent, 0.14 + 0.12 * m_glowIntensity);
  painter.fillRect(QRect(r.left(), r.top(), 1, r.height()), accentLine);

  const int corner = 18;
  painter.setPen(QPen(withAlpha(m_accent, 0.11 + 0.14 * m_glowIntensity), 1));
  painter.drawLine(r.left() + 6, r.top() + 6, r.left() + corner, r.top() + 6);
  painter.drawLine(r.left() + 6, r.top() + 6, r.left() + 6, r.top() + corner);
  painter.drawLine(r.right() - 6, r.top() + 6, r.right() - corner, r.top() + 6);
  painter.drawLine(r.right() - 6, r.top() + 6, r.right() - 6, r.top() + corner);
}

QColor TerminalView::withAlpha(const QColor &color, qreal alpha) const {
  QColor c = color;
  c.setAlphaF(qBound(0.0, alpha, 1.0));
  return c;
}
