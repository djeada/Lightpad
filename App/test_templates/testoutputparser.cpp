#include "testoutputparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QXmlStreamReader>

// --- TAP Parser ---

TapParser::TapParser(QObject *parent) : ITestOutputParser(parent) {}

void TapParser::feed(const QByteArray &data) {
  m_buffer += QString::fromUtf8(data);
  while (m_buffer.contains('\n')) {
    int idx = m_buffer.indexOf('\n');
    QString line = m_buffer.left(idx).trimmed();
    m_buffer = m_buffer.mid(idx + 1);
    parseLine(line);
  }
}

void TapParser::finish() {
  if (!m_buffer.trimmed().isEmpty())
    parseLine(m_buffer.trimmed());
  m_buffer.clear();
}

void TapParser::parseLine(const QString &line) {
  emit outputLine(line, false);

  static QRegularExpression planRe(R"(^(\d+)\.\.(\d+)$)");
  auto planMatch = planRe.match(line);
  if (planMatch.hasMatch())
    return;

  static QRegularExpression resultRe(
      R"(^(ok|not ok)\s+(\d+)\s*-?\s*(.*?)(?:\s*#\s*(SKIP|TODO)\s*(.*))?$)",
      QRegularExpression::CaseInsensitiveOption);
  auto match = resultRe.match(line);
  if (match.hasMatch()) {
    m_testNumber++;
    TestResult result;
    result.id = QString::number(match.captured(2).toInt());
    result.name = match.captured(3).trimmed();
    if (result.name.isEmpty())
      result.name = "Test " + result.id;

    QString directive = match.captured(4).toUpper();
    bool ok = match.captured(1) == "ok";

    if (directive == "SKIP" || directive == "TODO") {
      result.status = TestStatus::Skipped;
      result.message = match.captured(5).trimmed();
    } else if (ok) {
      result.status = TestStatus::Passed;
      m_passed++;
    } else {
      result.status = TestStatus::Failed;
      result.message = match.captured(5).trimmed();
      m_failed++;
    }

    emit testFinished(result);
  }
}

// --- JUnit XML Parser ---

JunitXmlParser::JunitXmlParser(QObject *parent) : ITestOutputParser(parent) {}

void JunitXmlParser::feed(const QByteArray &data) { m_buffer += data; }

void JunitXmlParser::finish() {
  QXmlStreamReader xml(m_buffer);

  QString currentSuite;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement()) {
      if (xml.name() == u"testsuite") {
        currentSuite = xml.attributes().value("name").toString();
        emit testSuiteStarted(currentSuite);
      } else if (xml.name() == u"testcase") {
        TestResult result;
        result.name = xml.attributes().value("name").toString();
        result.suite = xml.attributes().value("classname").toString();
        if (result.suite.isEmpty())
          result.suite = currentSuite;
        result.id = result.suite + "::" + result.name;

        QString timeStr = xml.attributes().value("time").toString();
        if (!timeStr.isEmpty())
          result.durationMs = static_cast<int>(timeStr.toDouble() * 1000);

        result.status = TestStatus::Passed;

        while (!(xml.isEndElement() && xml.name() == u"testcase") &&
               !xml.atEnd()) {
          xml.readNext();
          if (xml.isStartElement()) {
            if (xml.name() == u"failure") {
              result.status = TestStatus::Failed;
              result.message = xml.attributes().value("message").toString();
              result.stackTrace = xml.readElementText();
            } else if (xml.name() == u"error") {
              result.status = TestStatus::Errored;
              result.message = xml.attributes().value("message").toString();
              result.stackTrace = xml.readElementText();
            } else if (xml.name() == u"skipped") {
              result.status = TestStatus::Skipped;
              result.message = xml.attributes().value("message").toString();
              xml.skipCurrentElement();
            } else if (xml.name() == u"system-out") {
              result.stdoutOutput = xml.readElementText();
            } else if (xml.name() == u"system-err") {
              result.stderrOutput = xml.readElementText();
            }
          }
        }

        emit testFinished(result);
      }
    } else if (xml.isEndElement()) {
      if (xml.name() == u"testsuite") {
        currentSuite.clear();
      }
    }
  }

  m_buffer.clear();
}

// --- JSON Parser (go test -json, jest json, cargo json) ---

JsonTestParser::JsonTestParser(QObject *parent) : ITestOutputParser(parent) {}

void JsonTestParser::feed(const QByteArray &data) {
  m_buffer += QString::fromUtf8(data);
  while (m_buffer.contains('\n')) {
    int idx = m_buffer.indexOf('\n');
    QString line = m_buffer.left(idx).trimmed();
    m_buffer = m_buffer.mid(idx + 1);
    if (!line.isEmpty())
      parseLine(line);
  }
}

void JsonTestParser::finish() {
  if (!m_buffer.trimmed().isEmpty())
    parseLine(m_buffer.trimmed());
  m_buffer.clear();
}

void JsonTestParser::parseLine(const QString &line) {
  emit outputLine(line, false);

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError)
    return;

  QJsonObject obj = doc.object();

  // Go test -json format
  if (obj.contains("Action")) {
    QString action = obj["Action"].toString();
    QString testName = obj["Test"].toString();
    QString pkg = obj["Package"].toString();

    if (testName.isEmpty())
      return;

    if (action == "run") {
      TestResult result;
      result.id = pkg + "/" + testName;
      result.name = testName;
      result.suite = pkg;
      result.status = TestStatus::Running;
      emit testStarted(result);
    } else if (action == "pass" || action == "fail" || action == "skip") {
      TestResult result;
      result.id = pkg + "/" + testName;
      result.name = testName;
      result.suite = pkg;
      result.status = (action == "pass")    ? TestStatus::Passed
                      : (action == "fail")  ? TestStatus::Failed
                      : (action == "skip")  ? TestStatus::Skipped
                                            : TestStatus::Errored;
      double elapsed = obj["Elapsed"].toDouble();
      if (elapsed > 0)
        result.durationMs = static_cast<int>(elapsed * 1000);
      emit testFinished(result);
    }
    return;
  }

  // Jest JSON format (batch)
  if (obj.contains("testResults")) {
    QJsonArray results = obj["testResults"].toArray();
    for (const QJsonValue &suiteVal : results) {
      QJsonObject suiteObj = suiteVal.toObject();
      QString suiteName = suiteObj["testFilePath"].toString();
      emit testSuiteStarted(suiteName);

      QJsonArray tests = suiteObj["testResults"].toArray();
      for (const QJsonValue &testVal : tests) {
        QJsonObject testObj = testVal.toObject();
        TestResult result;
        result.name = testObj["fullName"].toString();
        if (result.name.isEmpty())
          result.name = testObj["title"].toString();
        result.suite = suiteName;
        result.id = result.suite + "::" + result.name;
        result.durationMs = testObj["duration"].toInt();

        QString status = testObj["status"].toString();
        result.status = (status == "passed")  ? TestStatus::Passed
                        : (status == "failed") ? TestStatus::Failed
                        : (status == "pending" || status == "skipped")
                            ? TestStatus::Skipped
                            : TestStatus::Errored;

        if (!testObj["failureMessages"].toArray().isEmpty()) {
          QStringList msgs;
          for (const QJsonValue &m : testObj["failureMessages"].toArray())
            msgs.append(m.toString());
          result.message = msgs.join("\n");
        }

        emit testFinished(result);
      }
    }
    return;
  }

  // Cargo test JSON format
  if (obj.contains("type")) {
    QString type = obj["type"].toString();
    if (type == "test") {
      QString event = obj["event"].toString();
      QString name = obj["name"].toString();
      if (event == "started") {
        TestResult result;
        result.id = name;
        result.name = name;
        result.status = TestStatus::Running;
        emit testStarted(result);
      } else if (event == "ok" || event == "failed" || event == "ignored") {
        TestResult result;
        result.id = name;
        result.name = name;
        result.status = (event == "ok")      ? TestStatus::Passed
                        : (event == "failed") ? TestStatus::Failed
                                              : TestStatus::Skipped;
        if (obj.contains("stdout"))
          result.stdoutOutput = obj["stdout"].toString();
        emit testFinished(result);
      }
    }
  }
}

// --- Pytest Parser ---

PytestParser::PytestParser(QObject *parent) : ITestOutputParser(parent) {}

void PytestParser::feed(const QByteArray &data) {
  m_buffer += QString::fromUtf8(data);
  while (m_buffer.contains('\n')) {
    int idx = m_buffer.indexOf('\n');
    QString line = m_buffer.left(idx);
    m_buffer = m_buffer.mid(idx + 1);
    parseLine(line);
  }
}

void PytestParser::finish() {
  if (!m_buffer.trimmed().isEmpty())
    parseLine(m_buffer);
  m_buffer.clear();

  if (m_inFailures && !m_failureTestName.isEmpty()) {
    TestResult result;
    result.id = m_failureTestName;
    result.name = m_failureTestName;
    result.status = TestStatus::Failed;
    result.stackTrace = m_failureMessage;
    emit testFinished(result);
  }
}

void PytestParser::parseLine(const QString &line) {
  emit outputLine(line, false);

  // Match pytest verbose output: path/test_file.py::test_name PASSED/FAILED/SKIPPED/ERROR
  static QRegularExpression resultRe(
      R"(^(.+?)::(.+?)\s+(PASSED|FAILED|SKIPPED|ERROR|XFAIL|XPASS)(?:\s+\[\s*\d+%\])?$)");
  auto match = resultRe.match(line.trimmed());
  if (match.hasMatch()) {
    TestResult result;
    result.filePath = match.captured(1);
    result.name = match.captured(2);
    result.suite = match.captured(1);
    result.id = result.filePath + "::" + result.name;

    QString status = match.captured(3);
    if (status == "PASSED" || status == "XFAIL")
      result.status = TestStatus::Passed;
    else if (status == "FAILED" || status == "XPASS")
      result.status = TestStatus::Failed;
    else if (status == "SKIPPED")
      result.status = TestStatus::Skipped;
    else if (status == "ERROR")
      result.status = TestStatus::Errored;

    emit testFinished(result);
    return;
  }

  // Match FAILURES section header
  static QRegularExpression failureHeaderRe(R"(^=+ FAILURES =+$)");
  if (failureHeaderRe.match(line.trimmed()).hasMatch()) {
    m_inFailures = true;
    return;
  }

  // Match individual failure header
  if (m_inFailures) {
    static QRegularExpression failNameRe(R"(^_+ (.+?) _+$)");
    auto failMatch = failNameRe.match(line.trimmed());
    if (failMatch.hasMatch()) {
      if (!m_failureTestName.isEmpty()) {
        TestResult result;
        result.id = m_failureTestName;
        result.name = m_failureTestName;
        result.status = TestStatus::Failed;
        result.stackTrace = m_failureMessage;
        emit testFinished(result);
      }
      m_failureTestName = failMatch.captured(1);
      m_failureMessage.clear();
    } else {
      m_failureMessage += line + "\n";
    }
  }

  // Duration line: e.g., "===== 2 passed in 0.12s ====="
  static QRegularExpression summaryRe(R"(=+\s+(\d+)\s+passed)");
  if (summaryRe.match(line.trimmed()).hasMatch()) {
    m_inFailures = false;
  }
}

// --- CTest Parser ---

CtestParser::CtestParser(QObject *parent) : ITestOutputParser(parent) {}

void CtestParser::feed(const QByteArray &data) {
  m_buffer += QString::fromUtf8(data);
  while (m_buffer.contains('\n')) {
    int idx = m_buffer.indexOf('\n');
    QString line = m_buffer.left(idx);
    m_buffer = m_buffer.mid(idx + 1);
    parseLine(line);
  }
}

void CtestParser::finish() {
  if (!m_buffer.trimmed().isEmpty())
    parseLine(m_buffer);
  m_buffer.clear();
}

void CtestParser::parseLine(const QString &line) {
  emit outputLine(line, false);

  // Match CTest verbose output: "N/M Test #N: TestName .............. Passed X.XX sec"
  static QRegularExpression resultRe(
      R"(^\s*\d+/\d+\s+Test\s+#(\d+):\s+(\S+)\s+\.+\s*(Passed|Failed|\*\*\*Failed|Not Run|Timeout)\s+(\d+\.\d+)\s+sec$)");
  auto match = resultRe.match(line.trimmed());
  if (match.hasMatch()) {
    TestResult result;
    result.id = match.captured(1);
    result.name = match.captured(2);

    QString status = match.captured(3);
    if (status == "Passed")
      result.status = TestStatus::Passed;
    else if (status == "Not Run")
      result.status = TestStatus::Skipped;
    else
      result.status = TestStatus::Failed;

    result.durationMs = static_cast<int>(match.captured(4).toDouble() * 1000);
    emit testFinished(result);
    return;
  }

  // Match "Start N: TestName"
  static QRegularExpression startRe(R"(^\s*Start\s+(\d+):\s+(\S+)\s*$)");
  auto startMatch = startRe.match(line.trimmed());
  if (startMatch.hasMatch()) {
    TestResult result;
    result.id = startMatch.captured(1);
    result.name = startMatch.captured(2);
    result.status = TestStatus::Running;
    emit testStarted(result);
  }
}

// --- Generic Regex Parser ---

GenericRegexParser::GenericRegexParser(QObject *parent)
    : ITestOutputParser(parent) {
  m_passRegex.setPattern(R"(^PASS:\s*(.+)$)");
  m_failRegex.setPattern(R"(^FAIL:\s*(.+)$)");
  m_skipRegex.setPattern(R"(^SKIP:\s*(.+)$)");
}

GenericRegexParser::GenericRegexParser(const QString &passPattern,
                                       const QString &failPattern,
                                       const QString &skipPattern,
                                       QObject *parent)
    : ITestOutputParser(parent), m_passRegex(passPattern),
      m_failRegex(failPattern), m_skipRegex(skipPattern) {}

void GenericRegexParser::setPassPattern(const QString &pattern) {
  m_passRegex.setPattern(pattern);
}

void GenericRegexParser::setFailPattern(const QString &pattern) {
  m_failRegex.setPattern(pattern);
}

void GenericRegexParser::setSkipPattern(const QString &pattern) {
  m_skipRegex.setPattern(pattern);
}

void GenericRegexParser::feed(const QByteArray &data) {
  m_buffer += QString::fromUtf8(data);
  while (m_buffer.contains('\n')) {
    int idx = m_buffer.indexOf('\n');
    QString line = m_buffer.left(idx).trimmed();
    m_buffer = m_buffer.mid(idx + 1);
    if (!line.isEmpty())
      parseLine(line);
  }
}

void GenericRegexParser::finish() {
  if (!m_buffer.trimmed().isEmpty())
    parseLine(m_buffer.trimmed());
  m_buffer.clear();
}

void GenericRegexParser::parseLine(const QString &line) {
  emit outputLine(line, false);

  auto passMatch = m_passRegex.match(line);
  if (passMatch.hasMatch()) {
    TestResult result;
    result.name =
        passMatch.captured(passMatch.lastCapturedIndex()).trimmed();
    result.id = result.name;
    result.status = TestStatus::Passed;
    emit testFinished(result);
    return;
  }

  auto failMatch = m_failRegex.match(line);
  if (failMatch.hasMatch()) {
    TestResult result;
    result.name =
        failMatch.captured(failMatch.lastCapturedIndex()).trimmed();
    result.id = result.name;
    result.status = TestStatus::Failed;
    emit testFinished(result);
    return;
  }

  auto skipMatch = m_skipRegex.match(line);
  if (skipMatch.hasMatch()) {
    TestResult result;
    result.name =
        skipMatch.captured(skipMatch.lastCapturedIndex()).trimmed();
    result.id = result.name;
    result.status = TestStatus::Skipped;
    emit testFinished(result);
    return;
  }
}

// --- Factory ---

ITestOutputParser *TestOutputParserFactory::createParser(
    const QString &formatId, QObject *parent) {
  if (formatId == "tap")
    return new TapParser(parent);
  if (formatId == "junit_xml")
    return new JunitXmlParser(parent);
  if (formatId == "go_json" || formatId == "jest_json" ||
      formatId == "cargo_json" || formatId == "json")
    return new JsonTestParser(parent);
  if (formatId == "pytest")
    return new PytestParser(parent);
  if (formatId == "ctest")
    return new CtestParser(parent);
  if (formatId == "generic")
    return new GenericRegexParser(parent);

  // Default to generic
  return new GenericRegexParser(parent);
}
