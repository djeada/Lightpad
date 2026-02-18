#ifndef TESTRUNMANAGER_H
#define TESTRUNMANAGER_H

#include "testconfiguration.h"
#include "testoutputparser.h"
#include <QObject>
#include <QProcess>

class TestRunManager : public QObject {
  Q_OBJECT

public:
  explicit TestRunManager(QObject *parent = nullptr);
  ~TestRunManager();

  void runAll(const TestConfiguration &config, const QString &workspaceFolder,
              const QString &filePath = QString());
  void runSingleTest(const TestConfiguration &config,
                     const QString &workspaceFolder,
                     const QString &testName,
                     const QString &filePath = QString());
  void runFailed(const TestConfiguration &config,
                 const QString &workspaceFolder);
  void runSuite(const TestConfiguration &config,
                const QString &workspaceFolder,
                const QString &suiteName);
  void stop();

  bool isRunning() const;
  QList<TestResult> results() const;
  QStringList failedTestNames() const;
  void clearResults();

signals:
  void testStarted(const TestResult &result);
  void testFinished(const TestResult &result);
  void testSuiteStarted(const QString &name);
  void testSuiteFinished(const QString &name, int passed, int failed);
  void outputLine(const QString &line, bool isError);
  void runStarted();
  void runFinished(int passed, int failed, int skipped, int errored);

private slots:
  void onStdoutReady();
  void onStderrReady();
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
  void startProcess(const TestConfiguration &config,
                    const QString &workspaceFolder,
                    const QString &filePath, const QString &testName);

  QProcess *m_process = nullptr;
  ITestOutputParser *m_parser = nullptr;
  QList<TestResult> m_results;
  int m_passed = 0;
  int m_failed = 0;
  int m_skipped = 0;
  int m_errored = 0;
};

#endif
