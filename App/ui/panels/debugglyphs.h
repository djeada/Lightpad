#ifndef DEBUGGLYPHS_H
#define DEBUGGLYPHS_H

#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

// Monochrome transport glyphs for the debugger toolbar.
//
// QStyle::standardIcon gives platform icons in fixed colours - an orange
// chevron and a blue reload arrow sit badly in a themed editor, and they cannot
// follow the palette. These are drawn from paths in a caller-supplied colour so
// the toolbar reads in whatever theme is active.
namespace DebugGlyphs {

enum class Glyph { Play, Pause, StepOver, StepInto, StepOut, Restart, Stop };

inline QIcon icon(Glyph glyph, const QColor &color, int size = 16) {
  const qreal ratio =
      QGuiApplication::instance() ? qGuiApp->devicePixelRatio() : 1.0;
  QPixmap pixmap(QSize(size, size) * ratio);
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);

  // Work in a 16x16 design grid whatever the requested size.
  painter.scale(size / 16.0, size / 16.0);
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);

  QPen stroke(color, 1.6);
  stroke.setCapStyle(Qt::RoundCap);
  stroke.setJoinStyle(Qt::RoundJoin);

  switch (glyph) {
  case Glyph::Play: {
    QPainterPath path;
    path.moveTo(4.5, 3.0);
    path.lineTo(12.5, 8.0);
    path.lineTo(4.5, 13.0);
    path.closeSubpath();
    painter.drawPath(path);
    break;
  }
  case Glyph::Pause:
    painter.drawRoundedRect(QRectF(4.0, 3.0, 3.0, 10.0), 0.8, 0.8);
    painter.drawRoundedRect(QRectF(9.0, 3.0, 3.0, 10.0), 0.8, 0.8);
    break;
  case Glyph::Stop:
    painter.drawRoundedRect(QRectF(3.5, 3.5, 9.0, 9.0), 1.2, 1.2);
    break;
  case Glyph::StepOver: {
    // An arc hopping over the statement, which stays put as a dot.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(stroke);
    QPainterPath arc;
    arc.moveTo(3.0, 9.5);
    arc.arcTo(QRectF(3.0, 3.0, 10.0, 11.0), 180.0, -150.0);
    painter.drawPath(arc);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    QPainterPath head;
    head.moveTo(13.2, 9.6);
    head.lineTo(9.6, 8.6);
    head.lineTo(12.0, 6.2);
    head.closeSubpath();
    painter.drawPath(head);
    painter.drawEllipse(QPointF(8.0, 12.4), 1.5, 1.5);
    break;
  }
  case Glyph::StepInto: {
    // An arrow descending into the statement.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(stroke);
    painter.drawLine(QPointF(8.0, 2.5), QPointF(8.0, 8.4));
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    QPainterPath head;
    head.moveTo(8.0, 11.0);
    head.lineTo(5.0, 7.4);
    head.lineTo(11.0, 7.4);
    head.closeSubpath();
    painter.drawPath(head);
    painter.drawEllipse(QPointF(8.0, 13.6), 1.4, 1.4);
    break;
  }
  case Glyph::StepOut: {
    // The same arrow, climbing back out to the caller.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(stroke);
    painter.drawLine(QPointF(8.0, 13.5), QPointF(8.0, 7.6));
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    QPainterPath head;
    head.moveTo(8.0, 5.0);
    head.lineTo(5.0, 8.6);
    head.lineTo(11.0, 8.6);
    head.closeSubpath();
    painter.drawPath(head);
    painter.drawEllipse(QPointF(8.0, 2.4), 1.4, 1.4);
    break;
  }
  case Glyph::Restart: {
    painter.setBrush(Qt::NoBrush);
    painter.setPen(stroke);
    QPainterPath arc;
    arc.arcMoveTo(QRectF(3.0, 3.0, 10.0, 10.0), 70.0);
    arc.arcTo(QRectF(3.0, 3.0, 10.0, 10.0), 70.0, 290.0);
    painter.drawPath(arc);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    QPainterPath head;
    head.moveTo(10.9, 1.6);
    head.lineTo(11.6, 5.6);
    head.lineTo(7.7, 4.6);
    head.closeSubpath();
    painter.drawPath(head);
    break;
  }
  }

  painter.end();
  return QIcon(pixmap);
}

} // namespace DebugGlyphs

#endif
