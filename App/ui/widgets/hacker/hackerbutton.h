#ifndef HACKERBUTTON_H
#define HACKERBUTTON_H

#include "hackerwidget.h"
#include <QIcon>

class HackerButton : public HackerWidget {
  Q_OBJECT
public:
  enum Variant { Primary, Secondary, Danger, Ghost };
  Q_ENUM(Variant)

  explicit HackerButton(const QString &text = QString(), QWidget *parent = nullptr);
  explicit HackerButton(const QIcon &icon, const QString &text, QWidget *parent = nullptr);

  void setText(const QString &text);
  QString text() const;
  void setIcon(const QIcon &icon);
  QIcon icon() const;
  void setVariant(Variant v);
  Variant variant() const;
  void setEnabled(bool enabled);
  void setCheckable(bool checkable);
  bool isCheckable() const;
  bool isChecked() const;
  void setChecked(bool checked);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

signals:
  void clicked();
  void toggled(bool checked);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  QString m_text;
  QIcon m_icon;
  Variant m_variant = Secondary;
  bool m_enabled = true;
  bool m_checkable = false;
  bool m_checked = false;

  QColor bgColor() const;
  QColor fgColor() const;
  QColor hoverBgColor() const;
  QColor activeBgColor() const;
};

#endif
