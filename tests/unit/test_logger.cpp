#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include "logging/logger.h"

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

void TestLogger::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestLogger::cleanupTestCase()
{
    Logger::instance().shutdown();
}

void TestLogger::testSingletonInstance()
{
    Logger& logger1 = Logger::instance();
    Logger& logger2 = Logger::instance();
    
    // Both references should point to the same instance
    QCOMPARE(&logger1, &logger2);
}

void TestLogger::testDefaultLogLevel()
{
    Logger& logger = Logger::instance();
    
    // Default log level should be Info
    QCOMPARE(logger.logLevel(), LogLevel::Info);
}

void TestLogger::testSetLogLevel()
{
    Logger& logger = Logger::instance();
    
    logger.setLogLevel(LogLevel::Debug);
    QCOMPARE(logger.logLevel(), LogLevel::Debug);
    
    logger.setLogLevel(LogLevel::Warning);
    QCOMPARE(logger.logLevel(), LogLevel::Warning);
    
    logger.setLogLevel(LogLevel::Error);
    QCOMPARE(logger.logLevel(), LogLevel::Error);
    
    // Reset to Info
    logger.setLogLevel(LogLevel::Info);
    QCOMPARE(logger.logLevel(), LogLevel::Info);
}

void TestLogger::testConsoleLoggingEnabled()
{
    Logger& logger = Logger::instance();
    
    // Should be enabled by default
    QVERIFY(logger.isConsoleLoggingEnabled());
    
    logger.setConsoleLoggingEnabled(false);
    QVERIFY(!logger.isConsoleLoggingEnabled());
    
    logger.setConsoleLoggingEnabled(true);
    QVERIFY(logger.isConsoleLoggingEnabled());
}

void TestLogger::testFileLogging()
{
    Logger& logger = Logger::instance();
    
    // Should be disabled by default
    QVERIFY(!logger.isFileLoggingEnabled());
    
    // Enable file logging to temp directory
    QString logPath = m_tempDir.path() + "/test.log";
    logger.setFileLoggingEnabled(true, logPath);
    QVERIFY(logger.isFileLoggingEnabled());
    
    // Log a test message
    logger.setLogLevel(LogLevel::Debug);
    logger.info("Test log message");
    
    // Disable logging to flush
    logger.setFileLoggingEnabled(false);
    QVERIFY(!logger.isFileLoggingEnabled());
    
    // Verify log file was created and contains content
    QFile logFile(logPath);
    QVERIFY(logFile.exists());
    QVERIFY(logFile.open(QIODevice::ReadOnly));
    QString content = logFile.readAll();
    QVERIFY(content.contains("Test log message"));
    QVERIFY(content.contains("[INFO]"));
    logFile.close();
}

void TestLogger::testLogLevelFiltering()
{
    Logger& logger = Logger::instance();
    
    QString logPath = m_tempDir.path() + "/filter_test.log";
    logger.setLogLevel(LogLevel::Warning);
    logger.setFileLoggingEnabled(true, logPath);
    
    // Debug and Info should be filtered out
    logger.debug("Debug message");
    logger.info("Info message");
    logger.warning("Warning message");
    logger.error("Error message");
    
    logger.setFileLoggingEnabled(false);
    
    QFile logFile(logPath);
    QVERIFY(logFile.open(QIODevice::ReadOnly));
    QString content = logFile.readAll();
    
    // Debug and Info should not appear
    QVERIFY(!content.contains("Debug message"));
    QVERIFY(!content.contains("Info message"));
    
    // Warning and Error should appear
    QVERIFY(content.contains("Warning message"));
    QVERIFY(content.contains("Error message"));
    
    logFile.close();
    
    // Reset log level
    logger.setLogLevel(LogLevel::Info);
}

QTEST_MAIN(TestLogger)
#include "test_logger.moc"
