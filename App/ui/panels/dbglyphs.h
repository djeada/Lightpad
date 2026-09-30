#ifndef DBGLYPHS_H
#define DBGLYPHS_H

#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace DbGlyphs {

enum class Glyph {
  Run,
  RunAll,
  Stop,
  Explain,
  Refresh,
  Add,
  History,
  Plug,
  Unplug,
  Copy,
  Export,
  Insights,
  Gear,
  Table,
  View,
  Column,
  Key,
  Schema,
  Database,
  Clear,
  Filter
};

inline QIcon icon(Glyph glyph, const QColor &color, int size = 16) {
  const qreal ratio =
      QGuiApplication::instance() ? qGuiApp->devicePixelRatio() : 1.0;
  QPixmap pixmap(QSize(size, size) * ratio);
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);

  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.scale(size / 16.0, size / 16.0);

  QPen stroke(color, 1.5);
  stroke.setCapStyle(Qt::RoundCap);
  stroke.setJoinStyle(Qt::RoundJoin);
  auto fill = [&]() {
    p.setPen(Qt::NoPen);
    p.setBrush(color);
  };
  auto line = [&]() {
    p.setPen(stroke);
    p.setBrush(Qt::NoBrush);
  };
  auto triangle = [&](qreal x) {
    QPainterPath path;
    path.moveTo(x, 3.0);
    path.lineTo(x + 7.5, 8.0);
    path.lineTo(x, 13.0);
    path.closeSubpath();
    p.drawPath(path);
  };

  switch (glyph) {
  case Glyph::Run:
    fill();
    triangle(4.5);
    break;
  case Glyph::RunAll:
    fill();
    p.save();
    p.scale(0.75, 0.75);
    triangle(1.5);
    triangle(9.0);
    p.restore();
    break;
  case Glyph::Stop:
    fill();
    p.drawRoundedRect(QRectF(3.5, 3.5, 9.0, 9.0), 1.4, 1.4);
    break;
  case Glyph::Explain: {
    line();
    p.drawEllipse(QRectF(2.5, 2.5, 7.5, 7.5));
    p.drawLine(QPointF(9.0, 9.0), QPointF(13.0, 13.0));
    break;
  }
  case Glyph::Refresh: {
    line();
    QPainterPath arc;
    arc.arcMoveTo(QRectF(3.0, 3.0, 10.0, 10.0), 40);
    arc.arcTo(QRectF(3.0, 3.0, 10.0, 10.0), 40, 280);
    p.drawPath(arc);
    fill();
    QPainterPath head;
    head.moveTo(11.0, 1.8);
    head.lineTo(13.8, 5.6);
    head.lineTo(9.4, 6.2);
    head.closeSubpath();
    p.drawPath(head);
    break;
  }
  case Glyph::Add:
    line();
    p.drawLine(QPointF(8, 3), QPointF(8, 13));
    p.drawLine(QPointF(3, 8), QPointF(13, 8));
    break;
  case Glyph::History: {
    line();
    p.drawEllipse(QRectF(2.5, 2.5, 11, 11));
    p.drawLine(QPointF(8, 5), QPointF(8, 8.2));
    p.drawLine(QPointF(8, 8.2), QPointF(10.4, 9.6));
    break;
  }
  case Glyph::Plug:
    line();
    p.drawLine(QPointF(6, 2.5), QPointF(6, 6));
    p.drawLine(QPointF(10, 2.5), QPointF(10, 6));
    p.drawRoundedRect(QRectF(4, 6, 8, 4), 1.2, 1.2);
    p.drawLine(QPointF(8, 10), QPointF(8, 13.5));
    break;
  case Glyph::Unplug:
    line();
    p.drawLine(QPointF(6, 2.5), QPointF(6, 6));
    p.drawLine(QPointF(10, 2.5), QPointF(10, 6));
    p.drawRoundedRect(QRectF(4, 6, 8, 4), 1.2, 1.2);
    p.drawLine(QPointF(2.5, 13.5), QPointF(13.5, 2.5));
    break;
  case Glyph::Copy:
    line();
    p.drawRoundedRect(QRectF(5.5, 5.5, 8, 8), 1.4, 1.4);
    p.drawLine(QPointF(3.5, 10.5), QPointF(3.5, 3.5));
    p.drawLine(QPointF(3.5, 3.5), QPointF(10.5, 3.5));
    break;
  case Glyph::Export:
    line();
    p.drawLine(QPointF(8, 2.5), QPointF(8, 10));
    p.drawLine(QPointF(5, 7.2), QPointF(8, 10.2));
    p.drawLine(QPointF(11, 7.2), QPointF(8, 10.2));
    p.drawLine(QPointF(3, 13), QPointF(13, 13));
    break;
  case Glyph::Insights:
    fill();
    p.drawRoundedRect(QRectF(2.5, 8.5, 3, 5), 0.8, 0.8);
    p.drawRoundedRect(QRectF(6.5, 4.5, 3, 9), 0.8, 0.8);
    p.drawRoundedRect(QRectF(10.5, 2.5, 3, 11), 0.8, 0.8);
    break;
  case Glyph::Gear: {
    line();
    p.drawEllipse(QRectF(5, 5, 6, 6));
    for (int i = 0; i < 8; ++i) {
      p.save();
      p.translate(8, 8);
      p.rotate(i * 45);
      p.drawLine(QPointF(0, -5.2), QPointF(0, -7));
      p.restore();
    }
    break;
  }
  case Glyph::Table:
    line();
    p.drawRoundedRect(QRectF(2.5, 3.5, 11, 9), 1.2, 1.2);
    p.drawLine(QPointF(2.5, 6.8), QPointF(13.5, 6.8));
    p.drawLine(QPointF(6.5, 6.8), QPointF(6.5, 12.5));
    break;
  case Glyph::View:
    line();
    p.drawRoundedRect(QRectF(2.5, 3.5, 11, 9), 1.2, 1.2);
    p.drawEllipse(QRectF(5.6, 6, 4.8, 4));
    break;
  case Glyph::Column:
    fill();
    p.drawRoundedRect(QRectF(3, 6.2, 10, 3.6), 1.2, 1.2);
    break;
  case Glyph::Key: {
    line();
    p.drawEllipse(QRectF(2.5, 2.5, 5, 5));
    p.drawLine(QPointF(6.5, 6.5), QPointF(13, 13));
    p.drawLine(QPointF(10.5, 10.5), QPointF(12.5, 8.5));
    break;
  }
  case Glyph::Schema:
    line();
    p.drawRoundedRect(QRectF(2.5, 3.5, 11, 3), 1, 1);
    p.drawRoundedRect(QRectF(2.5, 9.5, 11, 3), 1, 1);
    break;
  case Glyph::Database: {
    line();
    p.drawEllipse(QRectF(3, 2.5, 10, 4));
    p.drawLine(QPointF(3, 4.5), QPointF(3, 11.5));
    p.drawLine(QPointF(13, 4.5), QPointF(13, 11.5));
    QPainterPath bottom;
    bottom.moveTo(3, 11.5);
    bottom.arcTo(QRectF(3, 9.5, 10, 4), 180, 180);
    p.drawPath(bottom);
    break;
  }
  case Glyph::Clear:
    line();
    p.drawLine(QPointF(4, 4), QPointF(12, 12));
    p.drawLine(QPointF(12, 4), QPointF(4, 12));
    break;
  case Glyph::Filter: {
    fill();
    QPainterPath path;
    path.moveTo(2.5, 3.5);
    path.lineTo(13.5, 3.5);
    path.lineTo(9.2, 8.6);
    path.lineTo(9.2, 12.8);
    path.lineTo(6.8, 11.4);
    path.lineTo(6.8, 8.6);
    path.closeSubpath();
    p.drawPath(path);
    break;
  }
  }
  return QIcon(pixmap);
}

inline QIcon dot(const QColor &color, int size = 12) {
  const qreal ratio =
      QGuiApplication::instance() ? qGuiApp->devicePixelRatio() : 1.0;
  QPixmap pixmap(QSize(size, size) * ratio);
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen);
  p.setBrush(color);
  const qreal r = size * 0.28;
  p.drawEllipse(QPointF(size / 2.0, size / 2.0), r + 1, r + 1);
  return QIcon(pixmap);
}

} // namespace DbGlyphs

#endif
