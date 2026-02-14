#ifndef PREFERENCESVIEW_H
#define PREFERENCESVIEW_H

#include <QWidget>

class MainWindow;

namespace Ui {
class PreferencesView;
}

class PreferencesView : public QWidget {
  Q_OBJECT

public:
  PreferencesView(MainWindow *parent);
  ~PreferencesView();

private slots:
  void on_checkBoxLineNumbers_clicked(bool checked);
  void on_checkBoxCurrentLine_clicked(bool checked);
  void on_checkBoxBracket_clicked(bool checked);

private:
  Ui::PreferencesView *ui;
  MainWindow *parentWindow;
};

#endif
