#include "testrunmanager.h"

#include "core/logging/logger.h"

TestRunManager::TestRunManager(QObject *parent) : QObject(parent) {}

TestRunManager::~TestRunManager() { stop(); }

void TestRunManager::runAll(const TestConfiguration &config,
                            const QString &workspaceFolder,
                            const QString &filePath) {
  startProcess(config, workspaceFolder, filePath, QString());
}

void TestRunManager::runSingleTest(const TestConfiguration &config,
                                   const QString &workspaceFolder,
                                   const QString &testName,
                                   const QString &filePath) {
  startProcess(config, workspaceFolder, filePath, testName);
}

void TestRunManager::runFailed(const TestConfiguration &config,
                               const QString &workspaceFolder) {
  QStringList failed = failedTestNames();
  if (failed.isEmpty())
    return;

  // For gtest-based configs, use --gtest_filter
  bool isGTestFormat = (config.outputFormat == "ctest" ||
                        config.outputFormat == "gtest" ||
                        config.outputFormat == "gtest_xml");
  if (isGTestFormat) {
    // Run each failed test via filter; join with ':'
    QString filter = failed.join(':');
    startProcess(config, workspaceFolder, QString(), filter);
    return;
  }

  // For pytest, use -k with 'or' join
  if (config.outputFormat == "pytest") {
    QString filter = failed.join(" or ");
    startProcess(config, workspaceFolder, QString(), filter);
    return;
  }

  // Default: run first failed as single test
  startProcess(config, workspaceFolder, QString(), failed.first());
}

void TestRunManager::runSuite(const TestConfiguration &config,
                              const QString &workspaceFolder,
                              const QString &suiteName) {
  // For gtest-based configs, suite filter is "SuiteName.*"
  bool isGTestFormat = (config.outputFormat == "ctest" ||
                        config.outputFormat == "gtest" ||
                        config.outputFormat == "gtest_xml");
  if (isGTestFormat) {
    startProcess(config, workspaceFolder, QString(), suiteName + ".*");
    return;
  }

  startProcess(config, workspaceFolder, QString(), suiteName);
}

void TestRunManager::stop() {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  delete m_process;
  m_process = nullptr;
  delete m_parser;
  m_parser = nullptr;
}

bool TestRunManager::isRunning() const {
  return m_process && m_process->state() != QProcess::NotRunning;
}

QList<TestResult> TestRunManager::results() const { return m_results; }

QStringList TestRunManager::failedTestNames() const {
  QStringList names;
  for (const TestResult &r : m_results) {
    if (r.status == TestStatus::Failed || r.status == TestStatus::Errored) {
      names.append(r.name);
    }
  }
  return names;
}

void TestRunManager::clearResults() {
  m_results.clear();
  m_passed = m_failed = m_skipped = m_errored = 0;
}

void TestRunManager::startProcess(const TestConfiguration &config,
                                  const QString &workspaceFolder,
                                  const QString &filePath,
                                  const QString &testName) {
  stop();
  clearResults();

  m_parser = TestOutputParserFactory::createParser(config.outputFormat, this);

  connect(m_parser, &ITestOutputParser::testStarted, this,
          [this](const TestResult &r) {
            emit testStarted(r);
          });

  connect(m_parser, &ITestOutputParser::testFinished, this,
          [this](const TestResult &r) {
            m_results.append(r);
            switch (r.status) {
            case TestStatus::Passed:
              m_passed++;
              break;
            case TestStatus::Failed:
              m_failed++;
              break;
            case TestStatus::Skipped:
              m_skipped++;
              break;
            case TestStatus::Errored:
              m_errored++;
              break;
            default:
              break;
            }
            emit testFinished(r);
          });

  connect(m_parser, &ITestOutputParser::testSuiteStarted, this,
          &TestRunManager::testSuiteStarted);
  connect(m_parser, &ITestOutputParser::testSuiteFinished, this,
          &TestRunManager::testSuiteFinished);
  connect(m_parser, &ITestOutputParser::outputLine, this,
          &TestRunManager::outputLine);

  m_process = new QProcess(this);

  // Resolve command and args
  QStringList args;
  if (!testName.isEmpty() && !config.runSingleTest.args.isEmpty()) {
    for (const QString &arg : config.runSingleTest.args) {
      args.append(TestConfigurationManager::substituteVariables(
          arg, filePath, workspaceFolder, testName));
    }
  } else {
    for (const QString &arg : config.args) {
      args.append(TestConfigurationManager::substituteVariables(
          arg, filePath, workspaceFolder, testName));
    }
  }

  QString command = TestConfigurationManager::substituteVariables(
      config.command, filePath, workspaceFolder, testName);

  QString workDir = TestConfigurationManager::substituteVariables(
      config.workingDirectory, filePath, workspaceFolder, testName);
  if (workDir.isEmpty())
    workDir = workspaceFolder;

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  for (auto it = config.env.begin(); it != config.env.end(); ++it) {
    env.insert(it.key(), TestConfigurationManager::substituteVariables(
                             it.value(), filePath, workspaceFolder, testName));
  }
  m_process->setProcessEnvironment(env);
  m_process->setWorkingDirectory(workDir);

  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &TestRunManager::onStdoutReady);
  connect(m_process, &QProcess::readyReadStandardError, this,
          &TestRunManager::onStderrReady);
  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &TestRunManager::onProcessFinished);

  LOG_INFO("Starting: " + command + " " + args.join(" "));

  emit runStarted();
  m_process->start(command, args);
}

void TestRunManager::onStdoutReady() {
  if (m_parser && m_process)
    m_parser->feed(m_process->readAllStandardOutput());
}

void TestRunManager::onStderrReady() {
  if (m_process) {
    QByteArray data = m_process->readAllStandardError();
    if (m_parser)
      m_parser->feed(data);
    emit outputLine(QString::fromUtf8(data), true);
  }
}

void TestRunManager::onProcessFinished(int exitCode,
                                       QProcess::ExitStatus exitStatus) {
  Q_UNUSED(exitCode)
  Q_UNUSED(exitStatus)

  if (m_parser)
    m_parser->finish();

  emit runFinished(m_passed, m_failed, m_skipped, m_errored);
}
