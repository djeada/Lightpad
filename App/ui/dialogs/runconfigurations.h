#ifndef RUNCOFIGURATIONS_H
#define RUNCOFIGURATIONS_H

#include <QDialog>
#include <QEnterEvent>
#include <QIcon>
#include <QLineEdit>
#include <QToolButton>

class RunConfigurations;

class LineEditIcon : public QLineEdit {
  Q_OBJECT

public:
  LineEditIcon(QWidget *parent = nullptr);
  ~LineEditIcon();
  void setIcon(QIcon icon);
  void connectFunctionWithIcon(void (RunConfigurations::*f)());
  void setText(const QString &text);
  QString text();

protected:
  virtual void paintEvent(QPaintEvent *event) override;
  virtual void enterEvent(QEnterEvent *event) override;
  virtual void leaveEvent(QEvent *event) override;

private:
  QLineEdit edit;
  QToolButton button;
};

namespace Ui {
class runconfigurations;
}

class RunConfigurations : public QDialog {
  Q_OBJECT

public:
  RunConfigurations(QWidget *parent = nullptr);
  ~RunConfigurations();
  void choosePath();
  QString getScriptPath();
  QString getParameters();

private:
  Ui::runconfigurations *ui;
};

#endif
