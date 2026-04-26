#include "terminalview.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
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
    backgroundPainter.fillRect(r, m_background);
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

  if (m_scanlines) {
    painter.setPen(withAlpha(m_foreground, 0.018));
    for (int y = r.top(); y < r.bottom(); y += 5) {
      painter.drawLine(r.left(), y, r.right(), y);
    }
  }
}

QColor TerminalView::withAlpha(const QColor &color, qreal alpha) const {
  QColor c = color;
  c.setAlphaF(qBound(0.0, alpha, 1.0));
  return c;
}
