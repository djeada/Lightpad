#ifndef HACKERCOMBOBOX_H
#define HACKERCOMBOBOX_H

#include "hackerwidget.h"
#include <QComboBox>

class HackerComboBox : public HackerWidget {
  Q_OBJECT
  Q_PROPERTY(qreal chevronRotation READ chevronRotation WRITE setChevronRotation)

public:
  explicit HackerComboBox(QWidget *parent = nullptr);

  void addItem(const QString &text, const QVariant &userData = QVariant());
  void addItem(const QIcon &icon, const QString &text, const QVariant &userData = QVariant());
  void addItems(const QStringList &texts);
  void insertItem(int index, const QString &text, const QVariant &userData = QVariant());
  void removeItem(int index);
  void clear();

  int currentIndex() const;
  void setCurrentIndex(int index);
  QString currentText() const;
  int count() const;
  QString itemText(int index) const;
  QVariant itemData(int index) const;
  QComboBox *innerComboBox() const { return m_combo; }

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  qreal chevronRotation() const { return m_chevronRotation; }
  void setChevronRotation(qreal r);

signals:
  void currentIndexChanged(int index);
  void currentTextChanged(const QString &text);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  QComboBox *m_combo;
  QPropertyAnimation *m_chevronAnim = nullptr;
  qreal m_chevronRotation = 0.0;
  void setupInnerCombo();
};

#endif
