#ifndef TESTOUTPUTPARSER_H
#define TESTOUTPUTPARSER_H

#include "testconfiguration.h"
#include <QObject>
#include <QRegularExpression>

class ITestOutputParser : public QObject {
  Q_OBJECT

public:
  explicit ITestOutputParser(QObject *parent = nullptr) : QObject(parent) {}
  virtual ~ITestOutputParser() = default;

  virtual QString formatId() const = 0;
  virtual void feed(const QByteArray &data) = 0;
  virtual void finish() = 0;

signals:
  void testStarted(const TestResult &result);
  void testFinished(const TestResult &result);
  void testSuiteStarted(const QString &name);
  void testSuiteFinished(const QString &name, int passed, int failed);
  void outputLine(const QString &line, bool isError);
};

class TapParser : public ITestOutputParser {
  Q_OBJECT
public:
  explicit TapParser(QObject *parent = nullptr);
  QString formatId() const override { return "tap"; }
  void feed(const QByteArray &data) override;
  void finish() override;

private:
  void parseLine(const QString &line);
  QString m_buffer;
  int m_testNumber = 0;
  int m_passed = 0;
  int m_failed = 0;
};

class JunitXmlParser : public ITestOutputParser {
  Q_OBJECT
public:
  explicit JunitXmlParser(QObject *parent = nullptr);
  QString formatId() const override { return "junit_xml"; }
  void feed(const QByteArray &data) override;
  void finish() override;

private:
  QByteArray m_buffer;
};

class JsonTestParser : public ITestOutputParser {
  Q_OBJECT
public:
  explicit JsonTestParser(QObject *parent = nullptr);
  QString formatId() const override { return "json"; }
  void feed(const QByteArray &data) override;
  void finish() override;

private:
  void parseLine(const QString &line);
  QString m_buffer;
};

class PytestParser : public ITestOutputParser {
  Q_OBJECT
public:
  explicit PytestParser(QObject *parent = nullptr);
  QString formatId() const override { return "pytest"; }
  void feed(const QByteArray &data) override;
  void finish() override;

private:
  void parseLine(const QString &line);
  QString m_buffer;
  QString m_currentSuite;
  bool m_inFailures = false;
  QString m_failureTestName;
  QString m_failureMessage;
};

class CtestParser : public ITestOutputParser {
  Q_OBJECT
public:
  explicit CtestParser(QObject *parent = nullptr);
  QString formatId() const override { return "ctest"; }
  void feed(const QByteArray &data) override;
  void finish() override;

private:
  void parseLine(const QString &line);
  QString m_buffer;
};

class GenericRegexParser : public ITestOutputParser {
  Q_OBJECT
public:
  explicit GenericRegexParser(QObject *parent = nullptr);
  GenericRegexParser(const QString &passPattern, const QString &failPattern,
                     const QString &skipPattern, QObject *parent = nullptr);
  QString formatId() const override { return "generic"; }
  void feed(const QByteArray &data) override;
  void finish() override;

  void setPassPattern(const QString &pattern);
  void setFailPattern(const QString &pattern);
  void setSkipPattern(const QString &pattern);

private:
  void parseLine(const QString &line);
  QString m_buffer;
  QRegularExpression m_passRegex;
  QRegularExpression m_failRegex;
  QRegularExpression m_skipRegex;
};

class TestOutputParserFactory {
public:
  static ITestOutputParser *createParser(const QString &formatId,
                                         QObject *parent = nullptr);
};

#endif
