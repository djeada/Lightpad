#ifndef PYTHONENVIRONMENTDIALOG_H
#define PYTHONENVIRONMENTDIALOG_H

#include "../../settings/theme.h"
#include <QDialog>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class PythonEnvironmentWidget;

class PythonEnvironmentDialog : public QDialog {
  Q_OBJECT

public:
  explicit PythonEnvironmentDialog(const QString &workspaceFolder,
                                   const QString &filePath, const Theme &theme,
                                   QWidget *parent = nullptr);

signals:
  void configurationSaved();

private slots:
  void refreshDiagnostics();
  void saveConfiguration();

private:
  QString m_workspaceFolder;
  QString m_filePath;
  Theme m_theme;

  QLabel *m_contextLabel;
  PythonEnvironmentWidget *m_environmentWidget;
  QPlainTextEdit *m_detailsEdit;
  QPushButton *m_saveButton;
  QPushButton *m_closeButton;
};

#endif
