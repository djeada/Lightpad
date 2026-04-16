#ifndef HACKERSPINBOX_H
#define HACKERSPINBOX_H

#include "hackerwidget.h"
#include <QSpinBox>

class HackerSpinBox : public HackerWidget {
  Q_OBJECT
public:
  explicit HackerSpinBox(QWidget *parent = nullptr);

  void setValue(int value);
  int value() const;
  void setRange(int min, int max);
  void setMinimum(int min);
  void setMaximum(int max);
  int minimum() const;
  int maximum() const;
  void setSingleStep(int step);
  int singleStep() const;
  void setPrefix(const QString &prefix);
  void setSuffix(const QString &suffix);
  void setWrapping(bool wrapping);
  QSpinBox *innerSpinBox() const { return m_spin; }

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

signals:
  void valueChanged(int value);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  QSpinBox *m_spin;
  bool m_upHovered = false;
  bool m_downHovered = false;

  void setupInnerSpin();
  QRectF upArrowRect() const;
  QRectF downArrowRect() const;
};

#endif
