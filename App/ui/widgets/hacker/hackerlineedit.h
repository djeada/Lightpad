#ifndef HACKERLINEEDIT_H
#define HACKERLINEEDIT_H

#include "hackerwidget.h"
#include <QLineEdit>

class HackerLineEdit : public HackerWidget {
  Q_OBJECT
public:
  explicit HackerLineEdit(QWidget *parent = nullptr);
  explicit HackerLineEdit(const QString &placeholder, QWidget *parent = nullptr);

  void setText(const QString &text);
  QString text() const;
  void setPlaceholderText(const QString &text);
  QString placeholderText() const;
  void setReadOnly(bool ro);
  bool isReadOnly() const;
  void setEchoMode(QLineEdit::EchoMode mode);
  void clear();
  void selectAll();
  void setFocus();
  void setMaxLength(int length);
  QLineEdit *innerEdit() const { return m_edit; }

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

signals:
  void textChanged(const QString &text);
  void returnPressed();
  void editingFinished();

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  QLineEdit *m_edit;
  void setupInnerEdit();
};

#endif
