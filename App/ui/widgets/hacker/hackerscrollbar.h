#ifndef HACKERSCROLLBAR_H
#define HACKERSCROLLBAR_H

#include "hackerwidget.h"
#include <QAbstractScrollArea>
#include <QTimer>

class HackerScrollBar : public HackerWidget {
  Q_OBJECT
  Q_PROPERTY(qreal barOpacity READ barOpacity WRITE setBarOpacity)
  Q_PROPERTY(qreal barWidth READ barWidth WRITE setBarWidth)
  Q_PROPERTY(qreal thumbPosition READ thumbPosition WRITE setThumbPosition)

public:
  explicit HackerScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);

  void attachTo(QAbstractScrollArea *area);
  void setRange(int min, int max);
  void setValue(int value);
  int value() const;
  void setPageStep(int step);

  qreal barOpacity() const { return m_barOpacity; }
  void setBarOpacity(qreal o);
  qreal barWidth() const { return m_barWidth; }
  void setBarWidth(qreal w);
  qreal thumbPosition() const { return m_thumbPosition; }
  void setThumbPosition(qreal pos);

  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

signals:
  void valueChanged(int value);

private:
  Qt::Orientation m_orientation;
  int m_min = 0;
  int m_max = 100;
  int m_value = 0;
  int m_pageStep = 10;

  qreal m_barOpacity = 0.0;
  qreal m_barWidth = 4.0;
  qreal m_thumbPosition = 0.0;

  QPropertyAnimation *m_opacityAnim = nullptr;
  QPropertyAnimation *m_widthAnim = nullptr;
  QPropertyAnimation *m_thumbAnim = nullptr;
  QTimer *m_idleTimer = nullptr;

  bool m_dragging = false;
  int m_dragOffset = 0;

  QRectF thumbRect() const;
  qreal trackLength() const;
  qreal thumbLength() const;
  void showBar();
  void scheduleHide();
};

#endif
