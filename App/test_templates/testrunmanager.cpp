#include "testrunmanager.h"

#include "core/logging/logger.h"

TestRunManager::TestRunManager(QObject *parent) : QObject(parent) {}

TestRunManager::~TestRunManager() { stop(); }

void TestRunManager::runAll(const TestConfiguration &config,
                            const QString &workspaceFolder,
                            const QString &filePath) {
  startProcess(config, workspaceFolder, filePath, QString(), RunMode::All);
}

void TestRunManager::runSingleTest(const TestConfiguration &config,
                                   const QString &workspaceFolder,
                                   const QString &testName,
                                   const QString &filePath) {
  startProcess(config, workspaceFolder, filePath, testName,
               RunMode::SingleTest);
}

void TestRunManager::runFailed(const TestConfiguration &config,
                               const QString &workspaceFolder) {
  QStringList failed = failedTestNames();
  if (failed.isEmpty())
    return;

  // Join failed test names as the filter string; the configuration's
  // runFailed.args template uses ${testName} for substitution
  QString filter = failed.join(':');
  startProcess(config, workspaceFolder, QString(), filter, RunMode::Failed);
}

void TestRunManager::runSuite(const TestConfiguration &config,
                              const QString &workspaceFolder,
                              const QString &suiteName) {
  startProcess(config, workspaceFolder, QString(), suiteName, RunMode::Suite);
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
                                  const QString &testName,
                                  RunMode mode) {
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

  // Select the appropriate args template based on run mode
  QStringList templateArgs;
  switch (mode) {
  case RunMode::Failed:
    if (!config.runFailed.args.isEmpty())
      templateArgs = config.runFailed.args;
    else if (!config.runSingleTest.args.isEmpty())
      templateArgs = config.runSingleTest.args;
    else
      templateArgs = config.args;
    break;
  case RunMode::Suite:
    if (!config.runSuite.args.isEmpty())
      templateArgs = config.runSuite.args;
    else if (!config.runSingleTest.args.isEmpty())
      templateArgs = config.runSingleTest.args;
    else
      templateArgs = config.args;
    break;
  case RunMode::SingleTest:
    if (!testName.isEmpty() && !config.runSingleTest.args.isEmpty())
      templateArgs = config.runSingleTest.args;
    else
      templateArgs = config.args;
    break;
  case RunMode::All:
  default:
    templateArgs = config.args;
    break;
  }

  // Resolve command and args via variable substitution
  QStringList args;
  for (const QString &arg : templateArgs) {
    args.append(TestConfigurationManager::substituteVariables(
        arg, filePath, workspaceFolder, testName));
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
