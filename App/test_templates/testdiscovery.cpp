#include "testdiscovery.h"

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

  for (const QJsonValue &val : tests) {
    QJsonObject testObj = val.toObject();
    DiscoveredTest test;
    test.name = testObj["name"].toString();
    test.id = QString::number(testObj["index"].toInt());

    if (test.id == "0")
      test.id = test.name;

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

  m_process->start("python3",
                   {"-m", "pytest", "--collect-only", "-q", "--no-header"});
}

void PytestDiscoveryAdapter::cancel() {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  delete m_process;
  m_process = nullptr;
}

void PytestDiscoveryAdapter::onProcessFinished(int exitCode,
                                               QProcess::ExitStatus status) {
  if (!m_process)
    return;

  QByteArray stdoutData = m_process->readAllStandardOutput();
  QByteArray stderrData = m_process->readAllStandardError();

  delete m_process;
  m_process = nullptr;

  if (status != QProcess::NormalExit || (exitCode != 0 && exitCode != 5)) {
    QString err = QString::fromUtf8(stderrData).trimmed();
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

  const QStringList lines = output.split('\n');
  for (const QString &rawLine : lines) {
    QString line = rawLine.trimmed();
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

  return results;
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

  m_process->start("go", {"test", "-list", ".*", "./..."});
}

void GoTestDiscoveryAdapter::cancel() {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  delete m_process;
  m_process = nullptr;
}

void GoTestDiscoveryAdapter::onProcessFinished(int exitCode,
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

  m_process->start("cargo", {"test", "--", "--list"});
}

void CargoTestDiscoveryAdapter::cancel() {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  delete m_process;
  m_process = nullptr;
}

void CargoTestDiscoveryAdapter::onProcessFinished(int exitCode,
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
      err = tr("cargo test exited with code %1").arg(exitCode);
    emit discoveryError(err);
    return;
  }

  QList<DiscoveredTest> tests = parseListOutput(QString::fromUtf8(stdoutData));
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

  m_process->start("npx", {"jest", "--listTests"});
}

void JestDiscoveryAdapter::cancel() {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  delete m_process;
  m_process = nullptr;
}

void JestDiscoveryAdapter::onProcessFinished(int exitCode,
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

    test.suite = fi.dir().dirName();

    if (!test.name.isEmpty())
      results.append(test);
  }

  return results;
}
