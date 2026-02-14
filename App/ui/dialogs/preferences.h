#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QDialog>

#include "../mainwindow.h"

class ColorPicker;
class PreferencesView;
class PreferencesEditor;

namespace Ui {
class Preferences;
}

class Preferences : public QDialog {
  Q_OBJECT

public:
  Preferences(MainWindow *parent);
  ~Preferences();
  void setTabWidthLabel(const QString &text);

protected:
  void closeEvent(QCloseEvent *event);

private slots:
  void on_toolButton_clicked();

private:
  Ui::Preferences *ui;
  MainWindow *parentWindow;
  ColorPicker *colorPicker;
  PreferencesView *preferencesView;
  PreferencesEditor *preferencesEditor;
  void setupParent();
};

#endif
