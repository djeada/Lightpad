#ifndef DEBUGGLYPHS_H
#define DEBUGGLYPHS_H

#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

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

// Breakpoint markers for lists, drawn like the editor gutter: a dot for a
// line breakpoint (with bars when conditional), a diamond for a logpoint, a
// rounded square for a function breakpoint, a hexagon for a data breakpoint
// and a "!" badge for an exception filter. Breakpoints the debugger has not
// bound yet are drawn as rings.
enum class Marker { Line, Conditional, Logpoint, Function, Data, Exception };

inline QIcon breakpointMarker(Marker marker, const QColor &color,
                              bool bound = true, int size = 14) {
  const qreal ratio =
      QGuiApplication::instance() ? qGuiApp->devicePixelRatio() : 1.0;
  QPixmap pixmap(QSize(size, size) * ratio);
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.scale(size / 16.0, size / 16.0);
  if (bound) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
  } else {
    painter.setPen(QPen(color, 1.8));
    painter.setBrush(Qt::NoBrush);
  }
  const QRectF box(2.5, 2.5, 11, 11);
  switch (marker) {
  case Marker::Line:
  case Marker::Conditional:
    painter.drawEllipse(box);
    if (marker == Marker::Conditional && bound) {
      painter.setPen(QPen(QColor(255, 255, 255, 230), 1.6));
      painter.drawLine(QPointF(5.5, 6.6), QPointF(10.5, 6.6));
      painter.drawLine(QPointF(5.5, 9.4), QPointF(10.5, 9.4));
    }
    break;
  case Marker::Logpoint: {
    QPolygonF diamond;
    diamond << QPointF(8, 1.8) << QPointF(14.2, 8) << QPointF(8, 14.2)
            << QPointF(1.8, 8);
    painter.drawPolygon(diamond);
    break;
  }
  case Marker::Function:
    painter.drawRoundedRect(box, 3, 3);
    break;
  case Marker::Data: {
    QPolygonF hexagon;
    hexagon << QPointF(8, 1.8) << QPointF(13.6, 5) << QPointF(13.6, 11)
            << QPointF(8, 14.2) << QPointF(2.4, 11) << QPointF(2.4, 5);
    painter.drawPolygon(hexagon);
    break;
  }
  case Marker::Exception: {
    painter.drawEllipse(box);
    painter.setPen(QPen(bound ? QColor(255, 255, 255, 235) : color, 1.8,
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(8, 5), QPointF(8, 8.8));
    painter.drawPoint(QPointF(8, 11.2));
    break;
  }
  }
  painter.end();
  return QIcon(pixmap);
}

} // namespace DebugGlyphs

#endif
