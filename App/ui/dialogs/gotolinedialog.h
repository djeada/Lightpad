#ifndef GOTOLINEDIALOG_H
#define GOTOLINEDIALOG_H

#include "../../settings/theme.h"
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

class GoToLineDialog : public QDialog {
  Q_OBJECT

public:
  explicit GoToLineDialog(QWidget *parent = nullptr, int maxLine = 1);
  ~GoToLineDialog();

  int lineNumber() const;

  void setMaxLine(int maxLine);

  void showDialog();

  void applyTheme(const Theme &theme);

signals:

  void lineSelected(int lineNumber);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void onTextChanged(const QString &text);
  void onReturnPressed();

private:
  void setupUI();

  QLineEdit *m_lineEdit;
  QLabel *m_infoLabel;
  int m_maxLine;
  Theme m_theme;
};

#endif
