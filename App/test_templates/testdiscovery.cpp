#include "testdiscovery.h"

#include "core/logging/logger.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

// --- CTestDiscoveryAdapter ---

CTestDiscoveryAdapter::CTestDiscoveryAdapter(QObject *parent)
    : ITestDiscoveryAdapter(parent) {}

CTestDiscoveryAdapter::~CTestDiscoveryAdapter() { cancel(); }

void CTestDiscoveryAdapter::discover(const QString &buildDir) {
  cancel();

  if (buildDir.isEmpty() || !QDir(buildDir).exists()) {
    emit discoveryError(
        tr("Build directory does not exist: %1").arg(buildDir));
    return;
  }

  m_process = new QProcess(this);
  m_process->setWorkingDirectory(buildDir);

  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &CTestDiscoveryAdapter::onProcessFinished);

  // Try JSON output first
  m_usingJson = true;
  m_process->start("ctest", {"--show-only=json-v1"});
}

void CTestDiscoveryAdapter::cancel() {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  delete m_process;
  m_process = nullptr;
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
    // JSON mode failed; fall back to ctest -N
    m_usingJson = false;
    delete m_process;
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(workDir);
    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &CTestDiscoveryAdapter::onProcessFinished);
    m_process->start("ctest", {"-N"});
    return;
  }

  delete m_process;
  m_process = nullptr;

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

  LOG_INFO(
      QString("CTest discovery found %1 tests").arg(tests.size()));
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

  for (const QJsonValue &val : tests) {
    QJsonObject testObj = val.toObject();
    DiscoveredTest test;
    test.name = testObj["name"].toString();
    test.id = QString::number(testObj["index"].toInt());
    // CTest JSON uses 1-based indices; if index is missing or 0,
    // fall back to the test name as a unique identifier
    if (test.id == "0")
      test.id = test.name;

    // CTest JSON may include properties with working directory info
    QJsonArray properties = testObj["properties"].toArray();
    for (const QJsonValue &propVal : properties) {
      QJsonObject prop = propVal.toObject();
      if (prop["name"].toString() == "WORKING_DIRECTORY")
        test.filePath = prop["value"].toString();
    }

    if (!test.name.isEmpty())
      results.append(test);
  }

  return results;
}

QList<DiscoveredTest>
CTestDiscoveryAdapter::parseDashNOutput(const QString &output) {
  QList<DiscoveredTest> results;

  static QRegularExpression testLineRe(
      R"(^\s*Test\s+#(\d+):\s+(.+?)\s*$)");

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

// --- GTestDiscoveryAdapter ---

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
    emit discoveryError(
        tr("Build directory does not exist: %1").arg(buildDir));
    return;
  }

  m_process = new QProcess(this);
  m_process->setWorkingDirectory(buildDir);

  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &GTestDiscoveryAdapter::onProcessFinished);

  m_process->start(m_executablePath, {"--gtest_list_tests"});
}

void GTestDiscoveryAdapter::cancel() {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  delete m_process;
  m_process = nullptr;
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

  delete m_process;
  m_process = nullptr;

  if (exitCode != 0 || status != QProcess::NormalExit) {
    QString err = QString::fromUtf8(stderrData).trimmed();
    if (err.isEmpty())
      err = tr("GoogleTest executable exited with code %1").arg(exitCode);
    emit discoveryError(err);
    return;
  }

  QList<DiscoveredTest> tests =
      parseListTestsOutput(QString::fromUtf8(stdoutData));
  LOG_INFO(
      QString("GTest discovery found %1 tests").arg(tests.size()));
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

    // Suite lines start at column 0 (no leading whitespace) and end with '.'
    QString trimmed = rawLine.trimmed();
    bool isIndented = rawLine.startsWith(' ') || rawLine.startsWith('\t');

    if (!isIndented && trimmed.endsWith('.')) {
      currentSuite = trimmed.chopped(1); // remove trailing '.'
      continue;
    }

    // Test case lines are indented under a suite
    if (!isIndented || currentSuite.isEmpty())
      continue;

    // Remove comments after '#'
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
