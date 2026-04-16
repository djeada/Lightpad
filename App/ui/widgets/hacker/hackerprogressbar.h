#ifndef HACKERPROGRESSBAR_H
#define HACKERPROGRESSBAR_H

#include "hackerwidget.h"
#include <QTimer>

class HackerProgressBar : public HackerWidget {
  Q_OBJECT
  Q_PROPERTY(qreal scanOffset READ scanOffset WRITE setScanOffset)

public:
  explicit HackerProgressBar(QWidget *parent = nullptr);

  void setValue(int value);
  int value() const;
  void setRange(int min, int max);
  void setMinimum(int min);
  void setMaximum(int max);
  int minimum() const;
  int maximum() const;
  void setTextVisible(bool visible);
  bool isTextVisible() const;
  void setScanEffect(bool enabled);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  qreal scanOffset() const { return m_scanOffset; }
  void setScanOffset(qreal offset);

private:
  int m_value = 0;
  int m_min = 0;
  int m_max = 100;
  bool m_textVisible = true;
  bool m_scanEffect = false;
  qreal m_scanOffset = 0.0;

  QPropertyAnimation *m_scanAnim = nullptr;
  QTimer *m_scanTimer;

  qreal fraction() const;

protected:
  void paintEvent(QPaintEvent *event) override;
};

#endif
