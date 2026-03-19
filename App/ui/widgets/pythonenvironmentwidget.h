#ifndef PYTHONENVIRONMENTWIDGET_H
#define PYTHONENVIRONMENTWIDGET_H

#include "../../python/pythonprojectenvironment.h"

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>

class PythonEnvironmentWidget : public QGroupBox {
  Q_OBJECT

public:
  explicit PythonEnvironmentWidget(QWidget *parent = nullptr);

  void setContext(const QString &workspaceFolder, const QString &filePath,
                  const QString &workingDirectory = {});
  void setPreference(const PythonEnvironmentPreference &preference);
  PythonEnvironmentPreference preference() const;

  void setDebugToolsVisible(bool visible);
  void refreshStatus();

signals:
  void preferenceChanged();
  void environmentChanged();

private slots:
  void onModeChanged();
  void onBrowseInterpreter();
  void onBrowseVenv();
  void onBrowseRequirements();
  void onCreateVenv();
  void onInstallRequirements();
  void onInstallDebugpy();
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void onProcessError(QProcess::ProcessError error);

private:
  void updateUiVisibility();
  void startProcess(const QString &title, const QString &program,
                    const QStringList &arguments,
                    const QString &workingDirectory = {});
  QString creationInterpreter() const;
  void finishProcess(const QString &message, bool success);

  QString m_workspaceFolder;
  QString m_filePath;
  QString m_workingDirectory;
  QString m_pendingOperation;
  QString m_pendingTitle;

  QComboBox *m_modeCombo;
  QLineEdit *m_venvPathEdit;
  QPushButton *m_browseVenvButton;
  QLineEdit *m_interpreterEdit;
  QPushButton *m_browseInterpreterButton;
  QLineEdit *m_requirementsEdit;
  QPushButton *m_browseRequirementsButton;
  QPushButton *m_createVenvButton;
  QPushButton *m_installRequirementsButton;
  QPushButton *m_installDebugpyButton;
  QLabel *m_statusLabel;
  QLabel *m_hintLabel;
  QProcess *m_process = nullptr;
};

#endif
