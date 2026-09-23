#include "testrunmanager.h"

#include "core/logging/logger.h"
#include "python/pythonprojectenvironment.h"

TestRunManager::TestRunManager(QObject *parent) : QObject(parent) {}

TestRunManager::~TestRunManager() { stop(); }

void TestRunManager::runAll(const TestConfiguration &config,
                            const QString &workspaceFolder,
                            const QString &filePath) {
  startProcess(config, workspaceFolder, filePath, QString(), RunMode::All);
}

void TestRunManager::runFile(const TestConfiguration &config,
                             const QString &workspaceFolder,
                             const QString &filePath) {
  startProcess(config, workspaceFolder, filePath, QString(), RunMode::File);
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

  const QString filter = failedTestFilter(config, failed);
  if (filter.isEmpty()) {
    startProcess(config, workspaceFolder, QString(), QString(), RunMode::All);
    return;
  }
  startProcess(config, workspaceFolder, QString(), filter, RunMode::Failed);
}

void TestRunManager::runPattern(const TestConfiguration &config,
                                const QString &workspaceFolder,
                                const QString &pattern,
                                const QString &filePath) {
  if (pattern.isEmpty())
    return;
  startProcess(config, workspaceFolder, filePath, pattern, RunMode::Pattern);
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
  m_resultIndex.clear();
  m_passed = m_failed = m_skipped = m_errored = 0;
}

void TestRunManager::startProcess(const TestConfiguration &config,
                                  const QString &workspaceFolder,
                                  const QString &filePath,
                                  const QString &testName, RunMode mode) {
  stop();
  clearResults();
  m_stdoutBuffer.clear();
  m_stderrBuffer.clear();

  m_parser = TestOutputParserFactory::createParser(config.outputFormat, this);

  connect(m_parser, &ITestOutputParser::testStarted, this,
          [this](const TestResult &r) { emit testStarted(r); });

  connect(m_parser, &ITestOutputParser::testFinished, this,
          [this](const TestResult &r) {
            auto adjust = [this](TestStatus status, int delta) {
              switch (status) {
              case TestStatus::Passed:
                m_passed += delta;
                break;
              case TestStatus::Failed:
                m_failed += delta;
                break;
              case TestStatus::Skipped:
                m_skipped += delta;
                break;
              case TestStatus::Errored:
                m_errored += delta;
                break;
              default:
                break;
              }
            };
            const auto existing = r.id.isEmpty()
                                      ? m_resultIndex.constEnd()
                                      : m_resultIndex.constFind(r.id);
            if (existing != m_resultIndex.constEnd()) {
              adjust(m_results[existing.value()].status, -1);
              m_results[existing.value()] = r;
            } else {
              if (!r.id.isEmpty())
                m_resultIndex.insert(r.id, m_results.size());
              m_results.append(r);
            }
            adjust(r.status, 1);
            emit testFinished(r);
          });

  connect(m_parser, &ITestOutputParser::testSuiteStarted, this,
          &TestRunManager::testSuiteStarted);
  connect(m_parser, &ITestOutputParser::testSuiteFinished, this,
          &TestRunManager::testSuiteFinished);
  connect(m_parser, &ITestOutputParser::outputLine, this,
          [this](const QString &line, bool isError) {
            if (m_forwardParserOutput)
              emit outputLine(line, isError);
          });

  m_process = new QProcess(this);

  QStringList templateArgs;
  switch (mode) {
  case RunMode::File:
    if (!filePath.isEmpty() && !config.runFile.args.isEmpty())
      templateArgs = config.runFile.args;
    else
      templateArgs = config.args;
    break;
  case RunMode::Failed:
  case RunMode::Pattern:
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

  const QStringList args = TestConfigurationManager::substituteArguments(
      config.command, templateArgs, filePath, workspaceFolder, testName);

  QString command = TestConfigurationManager::substituteVariables(
      config.command, filePath, workspaceFolder, testName);

  QString workDir = TestConfigurationManager::substituteVariables(
      config.workingDirectory, filePath, workspaceFolder, testName);
  if (workDir.isEmpty())
    workDir = workspaceFolder;

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  const bool usesPythonEnvironment =
      config.outputFormat == "pytest" ||
      config.language.compare("Python", Qt::CaseInsensitive) == 0 ||
      config.command.contains("${python}");
  if (usesPythonEnvironment) {
    const PythonEnvironmentInfo pythonEnvironment =
        PythonProjectEnvironment::resolve({}, workspaceFolder, filePath,
                                          workDir);
    const QMap<QString, QString> activationEnv =
        PythonProjectEnvironment::activationEnvironment(pythonEnvironment);
    for (auto it = activationEnv.begin(); it != activationEnv.end(); ++it)
      env.insert(it.key(), it.value());
  }
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
  connect(m_process, &QProcess::errorOccurred, this,
          &TestRunManager::onProcessError);

  LOG_INFO("Starting: " + command + " " + args.join(" "));

  emit processStarted(command, args, workDir);
  emit runStarted();
  m_process->start(command, args);
}

void TestRunManager::feedParser(const QByteArray &data, bool isError) {
  if (!m_parser || data.isEmpty())
    return;
  if (isError) {
    emit outputLine(QString::fromUtf8(data), true);
    m_forwardParserOutput = false;
    m_parser->feed(data);
    m_forwardParserOutput = true;
  } else {
    m_parser->feed(data);
  }
}

void TestRunManager::consumeOutput(QByteArray &buffer, const QByteArray &data,
                                   bool isError) {
  buffer += data;
  const int lastNewline = buffer.lastIndexOf('\n');
  if (lastNewline < 0)
    return;
  const QByteArray complete = buffer.left(lastNewline + 1);
  buffer.remove(0, lastNewline + 1);
  feedParser(complete, isError);
}

void TestRunManager::flushOutput() {
  if (m_process) {
    consumeOutput(m_stdoutBuffer, m_process->readAllStandardOutput(), false);
    consumeOutput(m_stderrBuffer, m_process->readAllStandardError(), true);
  }
  if (!m_stdoutBuffer.isEmpty()) {
    feedParser(m_stdoutBuffer + '\n', false);
    m_stdoutBuffer.clear();
  }
  if (!m_stderrBuffer.isEmpty()) {
    feedParser(m_stderrBuffer + '\n', true);
    m_stderrBuffer.clear();
  }
}

void TestRunManager::onStdoutReady() {
  if (m_process)
    consumeOutput(m_stdoutBuffer, m_process->readAllStandardOutput(), false);
}

void TestRunManager::onStderrReady() {
  if (m_process)
    consumeOutput(m_stderrBuffer, m_process->readAllStandardError(), true);
}

void TestRunManager::onProcessFinished(int exitCode,
                                       QProcess::ExitStatus exitStatus) {
  flushOutput();
  if (m_parser)
    m_parser->finish();

  emit processFinished(exitCode, exitStatus == QProcess::NormalExit);
  emit runFinished(m_passed, m_failed, m_skipped, m_errored);
}

void TestRunManager::onProcessError(QProcess::ProcessError error) {
  if (error != QProcess::FailedToStart || !m_process)
    return;

  emit outputLine(tr("Failed to start %1: %2")
                      .arg(m_process->program(), m_process->errorString()),
                  true);
  emit processFinished(-1, false);
  emit runFinished(m_passed, m_failed, m_skipped, m_errored);
}

QString TestRunManager::failedTestFilter(const TestConfiguration &config,
                                         const QStringList &names) {
  if (names.isEmpty())
    return {};
  if (config.outputFormat == "pytest")
    return names.join(" or ");

  const QStringList templateArgs = !config.runFailed.args.isEmpty()
                                       ? config.runFailed.args
                                       : config.runSingleTest.args;
  if (templateArgs.join(' ').contains("gtest_filter"))
    return names.join(':');

  if (config.outputFormat == "cargo_json")
    return names.size() == 1 ? names.first() : QString();

  auto escape = [](const QString &name) {
    static const QString special = QStringLiteral("\\^$.|?*+()[]{}");
    QString escaped;
    for (const QChar c : name) {
      if (special.contains(c))
        escaped += '\\';
      escaped += c;
    }
    return escaped;
  };

  QStringList patterns;
  for (const QString &name : names) {
    const QString base =
        config.outputFormat == "go_json" ? name.section('/', 0, 0) : name;
    const QString pattern = escape(base);
    if (!pattern.isEmpty() && !patterns.contains(pattern))
      patterns.append(pattern);
  }

  if (config.outputFormat == "go_json" || config.outputFormat == "ctest")
    return "^(" + patterns.join('|') + ")$";
  return patterns.join('|');
}
