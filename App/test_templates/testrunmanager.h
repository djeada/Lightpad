#ifndef TESTRUNMANAGER_H
#define TESTRUNMANAGER_H

#include "testconfiguration.h"
#include "testoutputparser.h"
#include <QHash>
#include <QObject>
#include <QProcess>

class TestRunManager : public QObject {
  Q_OBJECT

public:
  explicit TestRunManager(QObject *parent = nullptr);
  ~TestRunManager();

  void runAll(const TestConfiguration &config, const QString &workspaceFolder,
              const QString &filePath = QString());
  void runFile(const TestConfiguration &config, const QString &workspaceFolder,
               const QString &filePath);
  void runSingleTest(const TestConfiguration &config,
                     const QString &workspaceFolder, const QString &testName,
                     const QString &filePath = QString());
  void runFailed(const TestConfiguration &config,
                 const QString &workspaceFolder);
  void runPattern(const TestConfiguration &config,
                  const QString &workspaceFolder, const QString &pattern,
                  const QString &filePath = QString());
  void runSuite(const TestConfiguration &config, const QString &workspaceFolder,
                const QString &suiteName);
  void stop();

  bool isRunning() const;
  QList<TestResult> results() const;
  QStringList failedTestNames() const;
  void clearResults();

  static QString failedTestFilter(const TestConfiguration &config,
                                  const QStringList &names);

signals:
  void testStarted(const TestResult &result);
  void testFinished(const TestResult &result);
  void testSuiteStarted(const QString &name);
  void testSuiteFinished(const QString &name, int passed, int failed);
  void outputLine(const QString &line, bool isError);
  void processStarted(const QString &command, const QStringList &args,
                      const QString &workingDirectory);
  void processFinished(int exitCode, bool normalExit);
  void runStarted();
  void runFinished(int passed, int failed, int skipped, int errored);

private slots:
  void onStdoutReady();
  void onStderrReady();
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void onProcessError(QProcess::ProcessError error);

private:
  enum class RunMode { All, File, SingleTest, Failed, Pattern, Suite };

  void startProcess(const TestConfiguration &config,
                    const QString &workspaceFolder, const QString &filePath,
                    const QString &testName,
                    RunMode mode = RunMode::SingleTest);

  void feedParser(const QByteArray &data, bool isError);
  void consumeOutput(QByteArray &buffer, const QByteArray &data, bool isError);
  void flushOutput();

  QProcess *m_process = nullptr;
  ITestOutputParser *m_parser = nullptr;
  QList<TestResult> m_results;
  QHash<QString, int> m_resultIndex;
  QByteArray m_stdoutBuffer;
  QByteArray m_stderrBuffer;
  bool m_forwardParserOutput = true;
  int m_passed = 0;
  int m_failed = 0;
  int m_skipped = 0;
  int m_errored = 0;
};

#endif
