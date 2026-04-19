#ifndef HACKERCHECKBOX_H
#define HACKERCHECKBOX_H

#include "hackerwidget.h"

class HackerCheckBox : public HackerWidget {
  Q_OBJECT
  Q_PROPERTY(qreal checkProgress READ checkProgress WRITE setCheckProgress)

public:
  explicit HackerCheckBox(const QString &text = QString(),
                          QWidget *parent = nullptr);

  void setText(const QString &text);
  QString text() const;
  bool isChecked() const;
  void setChecked(bool checked);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  qreal checkProgress() const { return m_checkProgress; }
  void setCheckProgress(qreal v);

signals:
  void toggled(bool checked);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  QString m_text;
  bool m_checked = false;
  qreal m_checkProgress = 0.0;
  QPropertyAnimation *m_checkAnim = nullptr;

  static constexpr int kBoxSize = 16;
  void toggle();
};

#endif
