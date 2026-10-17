#ifndef WORKSPACEICONS_H
#define WORKSPACEICONS_H

#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace WorkspaceIcons {
enum class Glyph { Run, Debug, Tests, Format, Search };

inline QIcon icon(Glyph glyph, const QColor &color, int size = 16) {
  const qreal ratio = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
  QPixmap pixmap(QSize(size, size) * ratio);
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.scale(size / 16.0, size / 16.0);
  painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);
  switch (glyph) {
  case Glyph::Run: {
    QPainterPath path;
    path.moveTo(5, 3);
    path.lineTo(13, 8);
    path.lineTo(5, 13);
    path.closeSubpath();
    painter.fillPath(path, color);
    break;
  }
  case Glyph::Debug:
    painter.drawRoundedRect(QRectF(5, 4, 6, 9), 3, 3);
    painter.drawLine(QPointF(6, 2), QPointF(7, 4));
    painter.drawLine(QPointF(10, 2), QPointF(9, 4));
    for (int y : {6, 9, 12}) {
      painter.drawLine(QPointF(2, y), QPointF(5, y));
      painter.drawLine(QPointF(11, y), QPointF(14, y));
    }
    break;
  case Glyph::Tests: {
    QPainterPath flask;
    flask.moveTo(6, 2);
    flask.lineTo(6, 6);
    flask.lineTo(3, 12);
    flask.quadTo(2.5, 14, 5, 14);
    flask.lineTo(11, 14);
    flask.quadTo(13.5, 14, 13, 12);
    flask.lineTo(10, 6);
    flask.lineTo(10, 2);
    painter.drawPath(flask);
    painter.drawLine(QPointF(5, 2), QPointF(11, 2));
    painter.drawLine(QPointF(5, 10), QPointF(11, 10));
    break;
  }
  case Glyph::Format: {
    QPainterPath braces;
    braces.moveTo(5, 2);
    braces.cubicTo(2, 2, 6, 7, 2, 8);
    braces.cubicTo(6, 9, 2, 14, 5, 14);
    braces.moveTo(11, 2);
    braces.cubicTo(14, 2, 10, 7, 14, 8);
    braces.cubicTo(10, 9, 14, 14, 11, 14);
    painter.drawPath(braces);
    break;
  }
  case Glyph::Search:
    painter.drawEllipse(QPointF(7, 7), 4, 4);
    painter.drawLine(QPointF(10, 10), QPointF(14, 14));
    break;
  }
  return QIcon(pixmap);
}
} // namespace WorkspaceIcons
#endif
