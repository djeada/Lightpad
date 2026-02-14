#include "logging/logger.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestLogger : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testSingletonInstance();
  void testDefaultLogLevel();
  void testSetLogLevel();
  void testConsoleLoggingEnabled();
  void testFileLogging();
  void testLogLevelFiltering();

private:
  QTemporaryDir m_tempDir;
};

void TestLogger::initTestCase() { QVERIFY(m_tempDir.isValid()); }

void TestLogger::cleanupTestCase() { Logger::instance().shutdown(); }

void TestLogger::testSingletonInstance() {
  Logger &logger1 = Logger::instance();
  Logger &logger2 = Logger::instance();

  QCOMPARE(&logger1, &logger2);
}

void TestLogger::testDefaultLogLevel() {
  Logger &logger = Logger::instance();

  QCOMPARE(logger.logLevel(), LogLevel::Info);
}

void TestLogger::testSetLogLevel() {
  Logger &logger = Logger::instance();

  logger.setLogLevel(LogLevel::Debug);
  QCOMPARE(logger.logLevel(), LogLevel::Debug);

  logger.setLogLevel(LogLevel::Warning);
  QCOMPARE(logger.logLevel(), LogLevel::Warning);

  logger.setLogLevel(LogLevel::Error);
  QCOMPARE(logger.logLevel(), LogLevel::Error);

  logger.setLogLevel(LogLevel::Info);
  QCOMPARE(logger.logLevel(), LogLevel::Info);
}

void TestLogger::testConsoleLoggingEnabled() {
  Logger &logger = Logger::instance();

  QVERIFY(logger.isConsoleLoggingEnabled());

  logger.setConsoleLoggingEnabled(false);
  QVERIFY(!logger.isConsoleLoggingEnabled());

  logger.setConsoleLoggingEnabled(true);
  QVERIFY(logger.isConsoleLoggingEnabled());
}

void TestLogger::testFileLogging() {
  Logger &logger = Logger::instance();

  QVERIFY(!logger.isFileLoggingEnabled());

  QString logPath = m_tempDir.path() + "/test.log";
  logger.setFileLoggingEnabled(true, logPath);
  QVERIFY(logger.isFileLoggingEnabled());

  logger.setLogLevel(LogLevel::Debug);
  logger.info("Test log message");

  logger.setFileLoggingEnabled(false);
  QVERIFY(!logger.isFileLoggingEnabled());

  QFile logFile(logPath);
  QVERIFY(logFile.exists());
  QVERIFY(logFile.open(QIODevice::ReadOnly));
  QString content = logFile.readAll();
  QVERIFY(content.contains("Test log message"));
  QVERIFY(content.contains("[INFO]"));
  logFile.close();
}

void TestLogger::testLogLevelFiltering() {
  Logger &logger = Logger::instance();

  QString logPath = m_tempDir.path() + "/filter_test.log";
  logger.setLogLevel(LogLevel::Warning);
  logger.setFileLoggingEnabled(true, logPath);

  logger.debug("Debug message");
  logger.info("Info message");
  logger.warning("Warning message");
  logger.error("Error message");

  logger.setFileLoggingEnabled(false);

  QFile logFile(logPath);
  QVERIFY(logFile.open(QIODevice::ReadOnly));
  QString content = logFile.readAll();

  QVERIFY(!content.contains("Debug message"));
  QVERIFY(!content.contains("Info message"));

  QVERIFY(content.contains("Warning message"));
  QVERIFY(content.contains("Error message"));

  logFile.close();

  logger.setLogLevel(LogLevel::Info);
}

QTEST_MAIN(TestLogger)
#include "test_logger.moc"
