#include "testdiscovery.h"
#include "pytestintegration.h"
#include "python/pythonbackendproject.h"
#include "python/pythonprojectenvironment.h"
#include "testsource.h"

#include "core/logging/logger.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

CTestDiscoveryAdapter::CTestDiscoveryAdapter(QObject *parent)
    : ITestDiscoveryAdapter(parent) {}

CTestDiscoveryAdapter::~CTestDiscoveryAdapter() { cancel(); }

void CTestDiscoveryAdapter::discover(const QString &buildDir) {
  cancel();

  if (buildDir.isEmpty() || !QDir(buildDir).exists()) {
    emit discoveryError(tr("Build directory does not exist: %1").arg(buildDir));
    return;
  }

  m_process = new QProcess(this);
  m_process->setWorkingDirectory(buildDir);

  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &CTestDiscoveryAdapter::onProcessFinished);

  m_usingJson = true;
  reportStartFailure(m_process);
  m_process->start("ctest", {"--show-only=json-v1"});
}

void CTestDiscoveryAdapter::cancel() {
  if (m_process)
    m_process->disconnect(this);
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  disposeProcess(m_process);
}

void CTestDiscoveryAdapter::onProcessFinished(int exitCode,
                                              QProcess::ExitStatus status) {
  if (!m_process)
    return;

  QByteArray stdoutData = m_process->readAllStandardOutput();
  QByteArray stderrData = m_process->readAllStandardError();
  QString workDir = m_process->workingDirectory();

  if (m_usingJson && (exitCode != 0 || status != QProcess::NormalExit ||
                      stdoutData.trimmed().isEmpty())) {

    m_usingJson = false;
    disposeProcess(m_process);
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(workDir);
    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &CTestDiscoveryAdapter::onProcessFinished);
    reportStartFailure(m_process);
    m_process->start("ctest", {"-N"});
    return;
  }

  disposeProcess(m_process);

  if (exitCode != 0 || status != QProcess::NormalExit) {
    QString err = QString::fromUtf8(stderrData).trimmed();
    if (err.isEmpty())
      err = tr("ctest exited with code %1").arg(exitCode);
    emit discoveryError(err);
    return;
  }

  QList<DiscoveredTest> tests;
  if (m_usingJson) {
    tests = parseJsonOutput(stdoutData);
  } else {
    tests = parseDashNOutput(QString::fromUtf8(stdoutData));
  }

  LOG_INFO(QString("CTest discovery found %1 tests").arg(tests.size()));
  emit discoveryFinished(tests);
}

QList<DiscoveredTest>
CTestDiscoveryAdapter::parseJsonOutput(const QByteArray &data) {
  QList<DiscoveredTest> results;

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError)
    return results;

  QJsonObject root = doc.object();
  QJsonArray tests = root["tests"].toArray();

  int ordinal = 0;
  const QJsonObject graph = root["backtraceGraph"].toObject();
  for (const QJsonValue &val : tests) {
    ++ordinal;
    QJsonObject testObj = val.toObject();
    DiscoveredTest test;
    test.name = testObj["name"].toString();
    test.id = QString::number(ordinal);
    if (testObj.contains("backtrace")) {
      const auto nodes = graph["nodes"].toArray();
      const int backtrace = testObj["backtrace"].toInt(-1);
      const auto node = backtrace >= 0 && backtrace < nodes.size()
                            ? nodes.at(backtrace).toObject()
                            : QJsonObject();
      if (node.contains("file") && node.contains("line")) {
        const auto files = graph["files"].toArray();
        const int index = node["file"].toInt(-1);
        if (index >= 0 && index < files.size())
          test.filePath = files.at(index).toString();
        test.line = node["line"].toInt();
      }
    }

    if (!test.name.isEmpty())
      results.append(test);
  }

  return results;
}

QList<DiscoveredTest>
CTestDiscoveryAdapter::parseDashNOutput(const QString &output) {
  QList<DiscoveredTest> results;

  static QRegularExpression testLineRe(R"(^\s*Test\s+#(\d+):\s+(.+?)\s*$)");

  const QStringList lines = output.split('\n');
  for (const QString &line : lines) {
    auto match = testLineRe.match(line.trimmed());
    if (match.hasMatch()) {
      DiscoveredTest test;
      test.id = match.captured(1);
      test.name = match.captured(2);
      results.append(test);
    }
  }

  return results;
}

GTestDiscoveryAdapter::GTestDiscoveryAdapter(QObject *parent)
    : ITestDiscoveryAdapter(parent) {}

GTestDiscoveryAdapter::~GTestDiscoveryAdapter() { cancel(); }

void GTestDiscoveryAdapter::discover(const QString &buildDir) {
  cancel();

  if (m_executablePath.isEmpty()) {
    emit discoveryError(tr("No GoogleTest executable path set"));
    return;
  }

  if (!QDir(buildDir).exists()) {
    emit discoveryError(tr("Build directory does not exist: %1").arg(buildDir));
    return;
  }

  m_process = new QProcess(this);
  m_process->setWorkingDirectory(buildDir);

  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &GTestDiscoveryAdapter::onProcessFinished);

  reportStartFailure(m_process);
  m_process->start(m_executablePath, {"--gtest_list_tests"});
}

void GTestDiscoveryAdapter::cancel() {
  if (m_process)
    m_process->disconnect(this);
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  disposeProcess(m_process);
}

void GTestDiscoveryAdapter::setExecutablePath(const QString &path) {
  m_executablePath = path;
}

void GTestDiscoveryAdapter::onProcessFinished(int exitCode,
                                              QProcess::ExitStatus status) {
  if (!m_process)
    return;

  QByteArray stdoutData = m_process->readAllStandardOutput();
  QByteArray stderrData = m_process->readAllStandardError();

  disposeProcess(m_process);

  if (exitCode != 0 || status != QProcess::NormalExit) {
    QString err = QString::fromUtf8(stderrData).trimmed();
    if (err.isEmpty())
      err = tr("GoogleTest executable exited with code %1").arg(exitCode);
    emit discoveryError(err);
    return;
  }

  QList<DiscoveredTest> tests =
      parseListTestsOutput(QString::fromUtf8(stdoutData));
  LOG_INFO(QString("GTest discovery found %1 tests").arg(tests.size()));
  emit discoveryFinished(tests);
}

QList<DiscoveredTest>
GTestDiscoveryAdapter::parseListTestsOutput(const QString &output) {
  QList<DiscoveredTest> results;
  QString currentSuite;

  const QStringList lines = output.split('\n');
  for (const QString &rawLine : lines) {
    if (rawLine.trimmed().isEmpty())
      continue;

    QString trimmed = rawLine.trimmed();
    bool isIndented = rawLine.startsWith(' ') || rawLine.startsWith('\t');

    if (!isIndented && trimmed.endsWith('.')) {
      currentSuite = trimmed.chopped(1);
      continue;
    }

    if (!isIndented || currentSuite.isEmpty())
      continue;

    QString testName = trimmed;
    int commentIdx = testName.indexOf('#');
    if (commentIdx >= 0)
      testName = testName.left(commentIdx).trimmed();

    if (!testName.isEmpty()) {
      DiscoveredTest test;
      test.suite = currentSuite;
      test.name = testName;
      test.id = currentSuite + "." + testName;
      results.append(test);
    }
  }

  return results;
}

QString GTestDiscoveryAdapter::buildGTestFilter(const QStringList &testNames) {
  if (testNames.isEmpty())
    return QString();
  return testNames.join(':');
}

PytestDiscoveryAdapter::PytestDiscoveryAdapter(QObject *parent)
    : ITestDiscoveryAdapter(parent) {}

PytestDiscoveryAdapter::~PytestDiscoveryAdapter() { cancel(); }

void PytestDiscoveryAdapter::discover(const QString &workDir) {
  cancel();

  if (workDir.isEmpty() || !QDir(workDir).exists()) {
    emit discoveryError(tr("Directory does not exist: %1").arg(workDir));
    return;
  }

  m_process = new QProcess(this);
  m_process->setWorkingDirectory(workDir);

  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &PytestDiscoveryAdapter::onProcessFinished);

  reportStartFailure(m_process);
  const QString workspace =
      m_workspaceFolder.isEmpty() ? workDir : m_workspaceFolder;
  const auto backend = PythonBackendProject::discover(workspace);
  const auto python =
      PythonProjectEnvironment::resolve(backend.pythonPreference(), workspace);
  auto environment = QProcessEnvironment::systemEnvironment();
  auto extra = PythonProjectEnvironment::activationEnvironment(python);
  if (backend.isValid())
    extra["PYTHONPATH"] = backend.environment().value("PYTHONPATH");
  for (auto it = extra.begin(); it != extra.end(); ++it)
    environment.insert(it.key(), it.value());
  for (auto it = m_configuration.env.begin(); it != m_configuration.env.end();
       ++it)
    environment.insert(it.key(), PythonProjectEnvironment::substituteVariables(
                                     it.value(), workspace, {}, workDir,
                                     backend.pythonPreference()));
  QString program =
      python.interpreter.isEmpty() ? QString("python3") : python.interpreter;
  QStringList arguments{"-m", "pytest"};
  if (PytestIntegration::supports(m_configuration.command,
                                  m_configuration.args)) {
    program = m_configuration.command == "${python}"
                  ? program
                  : TestConfigurationManager::substituteVariables(
                        m_configuration.command, {}, workspace);
    arguments = TestConfigurationManager::substituteArguments(
        m_configuration.command, m_configuration.args, {}, workspace, {});
  }
  arguments << "--collect-only" << "-q" << "--no-header";
  m_reporterDirectory = std::make_unique<QTemporaryDir>();
  PytestIntegration::install(*m_reporterDirectory, environment, arguments,
                             program);
  m_process->setProcessEnvironment(environment);
  m_process->start(program, arguments);
}

void PytestDiscoveryAdapter::cancel() {
  if (m_process)
    m_process->disconnect(this);
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  disposeProcess(m_process);
}

void PytestDiscoveryAdapter::onProcessFinished(int exitCode,
                                               QProcess::ExitStatus status) {
  if (!m_process)
    return;

  QByteArray stdoutData = m_process->readAllStandardOutput();
  QByteArray stderrData = m_process->readAllStandardError();

  disposeProcess(m_process);

  if (status != QProcess::NormalExit || (exitCode != 0 && exitCode != 5)) {
    QString err = QString::fromUtf8(stderrData).trimmed();
    QStringList outputLines;
    for (const QString &line : QString::fromUtf8(stdoutData).split('\n'))
      if (PytestIntegration::record(line).isEmpty())
        outputLines << line;
    const QString output = outputLines.join('\n').trimmed();
    if (!output.isEmpty())
      err = output + (err.isEmpty() ? QString() : '\n' + err);
    if (err.isEmpty())
      err = tr("pytest exited with code %1").arg(exitCode);
    emit discoveryError(err);
    return;
  }

  QList<DiscoveredTest> tests =
      parseCollectOutput(QString::fromUtf8(stdoutData));
  LOG_INFO(QString("pytest discovery found %1 tests").arg(tests.size()));
  emit discoveryFinished(tests);
}

QList<DiscoveredTest>
PytestDiscoveryAdapter::parseCollectOutput(const QString &output) {
  QList<DiscoveredTest> results;
  QList<DiscoveredTest> structured;

  const QStringList lines = output.split('\n');
  for (const QString &rawLine : lines) {
    QString line = rawLine.trimmed();
    const auto record = PytestIntegration::record(line);
    if (record.value("event") == "collected") {
      DiscoveredTest test;
      test.id = record.value("id").toString();
      test.name = record.value("name").toString();
      test.filePath = record.value("file").toString();
      test.line = record.value("line").toInt(-1);
      structured << test;
      continue;
    }
    if (line.isEmpty() || line.startsWith("no tests") ||
        line.startsWith("===") || line.startsWith("---"))
      continue;

    if (line.contains(" tests collected") || line.contains(" test collected") ||
        line.contains("warnings summary"))
      break;

    int firstSep = line.indexOf("::");
    if (firstSep < 0)
      continue;

    DiscoveredTest test;
    test.filePath = line.left(firstSep);
    QString remainder = line.mid(firstSep + 2);

    int secondSep = remainder.indexOf("::");
    if (secondSep >= 0) {

      test.suite = remainder.left(secondSep);
      test.name = remainder.mid(secondSep + 2);
    } else {
      test.name = remainder;
    }

    test.id = line;
    if (!test.name.isEmpty())
      results.append(test);
  }

  return structured.isEmpty() ? results : structured;
}

GoTestDiscoveryAdapter::GoTestDiscoveryAdapter(QObject *parent)
    : ITestDiscoveryAdapter(parent) {}

GoTestDiscoveryAdapter::~GoTestDiscoveryAdapter() { cancel(); }

void GoTestDiscoveryAdapter::discover(const QString &workDir) {
  cancel();

  if (workDir.isEmpty() || !QDir(workDir).exists()) {
    emit discoveryError(tr("Directory does not exist: %1").arg(workDir));
    return;
  }

  m_process = new QProcess(this);
  m_process->setWorkingDirectory(workDir);

  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &GoTestDiscoveryAdapter::onProcessFinished);

  reportStartFailure(m_process);
  m_process->start("go", {"test", "-list", ".*", "./..."});
}

void GoTestDiscoveryAdapter::cancel() {
  if (m_process)
    m_process->disconnect(this);
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  disposeProcess(m_process);
}

void GoTestDiscoveryAdapter::onProcessFinished(int exitCode,
                                               QProcess::ExitStatus status) {
  if (!m_process)
    return;

  QByteArray stdoutData = m_process->readAllStandardOutput();
  QByteArray stderrData = m_process->readAllStandardError();

  disposeProcess(m_process);

  if (exitCode != 0 || status != QProcess::NormalExit) {
    QString err = QString::fromUtf8(stderrData).trimmed();
    if (err.isEmpty())
      err = tr("go test exited with code %1").arg(exitCode);
    emit discoveryError(err);
    return;
  }

  QList<DiscoveredTest> tests = parseListOutput(QString::fromUtf8(stdoutData));
  LOG_INFO(QString("Go test discovery found %1 tests").arg(tests.size()));
  emit discoveryFinished(tests);
}

QList<DiscoveredTest>
GoTestDiscoveryAdapter::parseListOutput(const QString &output) {
  QList<DiscoveredTest> results;

  const QStringList lines = output.split('\n');
  for (const QString &rawLine : lines) {
    QString line = rawLine.trimmed();
    if (line.isEmpty())
      continue;

    if (line.startsWith("ok ") || line.startsWith("? "))
      continue;

    if (line.startsWith("Test") || line.startsWith("Benchmark") ||
        line.startsWith("Example") || line.startsWith("Fuzz")) {
      DiscoveredTest test;
      test.name = line;
      test.id = line;

      int underscoreIdx = line.indexOf('_');
      if (underscoreIdx > 0 && line.startsWith("Test"))
        test.suite = line.left(underscoreIdx);

      results.append(test);
    }
  }

  return results;
}

CargoTestDiscoveryAdapter::CargoTestDiscoveryAdapter(QObject *parent)
    : ITestDiscoveryAdapter(parent) {}

CargoTestDiscoveryAdapter::~CargoTestDiscoveryAdapter() { cancel(); }

void CargoTestDiscoveryAdapter::discover(const QString &workDir) {
  cancel();

  if (workDir.isEmpty() || !QDir(workDir).exists()) {
    emit discoveryError(tr("Directory does not exist: %1").arg(workDir));
    return;
  }

  m_process = new QProcess(this);
  m_process->setWorkingDirectory(workDir);

  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &CargoTestDiscoveryAdapter::onProcessFinished);

  reportStartFailure(m_process);
  m_process->start("cargo", {"test", "--", "--list"});
}

void CargoTestDiscoveryAdapter::cancel() {
  if (m_process)
    m_process->disconnect(this);
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  disposeProcess(m_process);
}

void CargoTestDiscoveryAdapter::onProcessFinished(int exitCode,
                                                  QProcess::ExitStatus status) {
  if (!m_process)
    return;

  QByteArray stdoutData = m_process->readAllStandardOutput();
  QByteArray stderrData = m_process->readAllStandardError();
  const QString workspace = m_process->workingDirectory();

  disposeProcess(m_process);

  if (exitCode != 0 || status != QProcess::NormalExit) {
    QString err = QString::fromUtf8(stderrData).trimmed();
    if (err.isEmpty())
      err = tr("cargo test exited with code %1").arg(exitCode);
    emit discoveryError(err);
    return;
  }

  QList<DiscoveredTest> tests = parseListOutput(QString::fromUtf8(stdoutData));
  for (auto &test : tests)
    TestSource::rustDefinition(workspace, test.id, &test.filePath, &test.line);
  LOG_INFO(QString("Cargo test discovery found %1 tests").arg(tests.size()));
  emit discoveryFinished(tests);
}

QList<DiscoveredTest>
CargoTestDiscoveryAdapter::parseListOutput(const QString &output) {
  QList<DiscoveredTest> results;

  const QStringList lines = output.split('\n');
  for (const QString &rawLine : lines) {
    QString line = rawLine.trimmed();
    if (line.isEmpty())
      continue;

    if (!line.endsWith(": test"))
      continue;

    QString fullName = line.left(line.length() - 6);

    DiscoveredTest test;
    test.id = fullName;

    int lastSep = fullName.lastIndexOf("::");
    if (lastSep >= 0) {
      test.suite = fullName.left(lastSep);
      test.name = fullName.mid(lastSep + 2);
    } else {
      test.name = fullName;
    }

    if (!test.name.isEmpty())
      results.append(test);
  }

  return results;
}

JestDiscoveryAdapter::JestDiscoveryAdapter(QObject *parent)
    : ITestDiscoveryAdapter(parent) {}

JestDiscoveryAdapter::~JestDiscoveryAdapter() { cancel(); }

void JestDiscoveryAdapter::discover(const QString &workDir) {
  cancel();

  if (workDir.isEmpty() || !QDir(workDir).exists()) {
    emit discoveryError(tr("Directory does not exist: %1").arg(workDir));
    return;
  }

  m_process = new QProcess(this);
  m_process->setWorkingDirectory(workDir);

  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &JestDiscoveryAdapter::onProcessFinished);

  reportStartFailure(m_process);
  m_reporterDirectory = std::make_unique<QTemporaryDir>();
  QFile source(":/test_templates/lightpad_jest_discover.cjs");
  const QString script = m_reporterDirectory->filePath("discover.cjs");
  QFile target(script);
  if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) {
    disposeProcess(m_process);
    emit discoveryError(tr("Cannot load the bundled Jest discovery script."));
    return;
  }
  target.write(source.readAll());
  target.close();
  m_process->start("node", {script});
}

void JestDiscoveryAdapter::cancel() {
  if (m_process)
    m_process->disconnect(this);
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  disposeProcess(m_process);
}

void JestDiscoveryAdapter::onProcessFinished(int exitCode,
                                             QProcess::ExitStatus status) {
  if (!m_process)
    return;

  QByteArray stdoutData = m_process->readAllStandardOutput();
  QByteArray stderrData = m_process->readAllStandardError();

  disposeProcess(m_process);

  if (exitCode != 0 || status != QProcess::NormalExit) {
    QString err = QString::fromUtf8(stderrData).trimmed();
    if (err.isEmpty())
      err = tr("jest exited with code %1").arg(exitCode);
    emit discoveryError(err);
    return;
  }

  QList<DiscoveredTest> tests = parseListOutput(QString::fromUtf8(stdoutData));
  LOG_INFO(QString("Jest discovery found %1 tests").arg(tests.size()));
  emit discoveryFinished(tests);
}

QList<DiscoveredTest>
JestDiscoveryAdapter::parseListOutput(const QString &output) {
  QList<DiscoveredTest> results;
  const auto document = QJsonDocument::fromJson(output.toUtf8());
  if (document.isArray()) {
    for (const auto &value : document.array()) {
      if (value.isString()) {
        const QString file = value.toString();
        results << DiscoveredTest{
            QFileInfo(file).fileName(), file, file, file, 1, true};
      } else {
        const auto entry = value.toObject();
        const QString file = entry["file"].toString();
        results << DiscoveredTest{
            entry["name"].toString(), file,
            entry["id"].toString(),   file,
            entry["line"].toInt(-1),  entry["isFile"].toBool()};
      }
    }
    return results;
  }

  const QStringList lines = output.split('\n');
  for (const QString &rawLine : lines) {
    QString line = rawLine.trimmed();
    if (line.isEmpty())
      continue;

    QFileInfo fi(line);
    DiscoveredTest test;
    test.filePath = line;
    test.name = fi.fileName();
    test.id = line;
    test.isFile = true;

    test.suite = fi.dir().dirName();

    if (!test.name.isEmpty())
      results.append(test);
  }

  return results;
}

ITestDiscoveryAdapter *
TestDiscoveryAdapterFactory::createForConfiguration(const QString &configId,
                                                    QObject *parent) {
  if (configId == "pytest")
    return new PytestDiscoveryAdapter(parent);
  if (configId == "gtest_cmake" || configId == "gtest_make" ||
      configId == "ctest_c")
    return new CTestDiscoveryAdapter(parent);
  if (configId == "go_test")
    return new GoTestDiscoveryAdapter(parent);
  if (configId == "cargo_test")
    return new CargoTestDiscoveryAdapter(parent);
  if (configId == "jest")
    return new JestDiscoveryAdapter(parent);
  return nullptr;
}
