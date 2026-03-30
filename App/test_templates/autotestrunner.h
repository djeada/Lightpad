#ifndef AUTOTESTRUNNER_H
#define AUTOTESTRUNNER_H

#include "testconfiguration.h"
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

class TestRunManager;

enum class AutoRunMode { Off, AllOnSave, CurrentFileOnSave, LastSelection };

class AutoTestRunner : public QObject {
  Q_OBJECT

public:
  explicit AutoTestRunner(TestRunManager *runManager,
                          QObject *parent = nullptr);
  ~AutoTestRunner();

  void setEnabled(bool enabled);
  bool isEnabled() const;

  void setMode(AutoRunMode mode);
  AutoRunMode mode() const;

  void setDebounceDelay(int ms);
  int debounceDelay() const;

  void setWorkspaceFolder(const QString &folder);
  QString workspaceFolder() const;

  void setConfiguration(const TestConfiguration &config);
  TestConfiguration configuration() const;

  void notifyFileSaved(const QString &filePath);

  void setLastFilePath(const QString &filePath);
  QString lastFilePath() const;

  bool isRunning() const;

  void saveSettings(const QString &workspaceFolder) const;
  void loadSettings(const QString &workspaceFolder);

signals:
  void autoRunTriggered();
  void enabledChanged(bool enabled);
  void modeChanged(AutoRunMode mode);

private slots:
  void onDebounceTimeout();
  void onRunFinished(int passed, int failed, int skipped, int errored);

private:
  void scheduleRun(const QString &filePath);

  TestRunManager *m_runManager;
  QTimer m_debounceTimer;
  bool m_enabled = false;
  AutoRunMode m_mode = AutoRunMode::Off;
  int m_debounceDelay = 1000;
  QString m_workspaceFolder;
  TestConfiguration m_config;
  QString m_pendingFilePath;
  QString m_lastFilePath;

  static const int DEFAULT_DEBOUNCE_MS = 1000;
};

#endif
