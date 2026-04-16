#ifndef HACKERWIDGET_H
#define HACKERWIDGET_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QPainter>
#include <QEnterEvent>

class ThemeDefinition;
struct ThemeColors;

class HackerWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
  Q_PROPERTY(qreal focusProgress READ focusProgress WRITE setFocusProgress)
  Q_PROPERTY(qreal pressProgress READ pressProgress WRITE setPressProgress)

public:
  explicit HackerWidget(QWidget *parent = nullptr);
  ~HackerWidget() override;

  qreal hoverProgress() const { return m_hoverProgress; }
  void setHoverProgress(qreal v);
  qreal focusProgress() const { return m_focusProgress; }
  void setFocusProgress(qreal v);
  qreal pressProgress() const { return m_pressProgress; }
  void setPressProgress(qreal v);

protected:
  const ThemeColors &colors() const;
  int animDuration() const;
  qreal glowAmount() const;
  int radius() const;

  void drawGlowBorder(QPainter &p, const QRectF &rect, const QColor &color,
                       qreal intensity, qreal radius);
  void drawRoundedSurface(QPainter &p, const QRectF &rect,
                           const QColor &bg, qreal radius);
  void drawTextInRect(QPainter &p, const QRectF &rect, const QString &text,
                       const QColor &color, int alignment = Qt::AlignCenter,
                       const QFont &font = QFont());
  static QColor blendColors(const QColor &a, const QColor &b, qreal t);

  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;

  QPropertyAnimation *m_hoverAnim = nullptr;
  QPropertyAnimation *m_focusAnim = nullptr;
  QPropertyAnimation *m_pressAnim = nullptr;

  qreal m_hoverProgress = 0.0;
  qreal m_focusProgress = 0.0;
  qreal m_pressProgress = 0.0;

  void startAnimation(QPropertyAnimation *anim, qreal from, qreal to);
};

#endif
