#include "logger.h"
#include <QDebug>
#include <iostream>

Logger& Logger::instance()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
    : m_logLevel(LogLevel::Info)
    , m_fileLoggingEnabled(false)
    , m_consoleLoggingEnabled(true)
{
}

Logger::~Logger()
{
    shutdown();
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

LogLevel Logger::logLevel() const
{
    return m_logLevel;
}

void Logger::setFileLoggingEnabled(bool enabled, const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    
    if (enabled && !m_fileLoggingEnabled) {
        QString path = filePath.isEmpty() ? getDefaultLogPath() : filePath;
        
        // Ensure directory exists
        QFileInfo fileInfo(path);
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        
        m_logFile.setFileName(path);
        if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            m_logStream.setDevice(&m_logFile);
            m_fileLoggingEnabled = true;
        }
    } else if (!enabled && m_fileLoggingEnabled) {
        m_logStream.flush();
        m_logFile.close();
        m_fileLoggingEnabled = false;
    }
}

bool Logger::isFileLoggingEnabled() const
{
    return m_fileLoggingEnabled;
}

void Logger::setConsoleLoggingEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_consoleLoggingEnabled = enabled;
}

bool Logger::isConsoleLoggingEnabled() const
{
    return m_consoleLoggingEnabled;
}

void Logger::debug(const QString& message, const char* file, int line)
{
    log(LogLevel::Debug, message, file, line);
}

void Logger::info(const QString& message, const char* file, int line)
{
    log(LogLevel::Info, message, file, line);
}

void Logger::warning(const QString& message, const char* file, int line)
{
    log(LogLevel::Warning, message, file, line);
}

void Logger::error(const QString& message, const char* file, int line)
{
    log(LogLevel::Error, message, file, line);
}

void Logger::shutdown()
{
    QMutexLocker locker(&m_mutex);
    if (m_fileLoggingEnabled) {
        m_logStream.flush();
        m_logFile.close();
        m_fileLoggingEnabled = false;
    }
}

void Logger::log(LogLevel level, const QString& message, const char* file, int line)
{
    if (level < m_logLevel) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = levelToString(level);
    
    QString logMessage;
    if (file && line > 0) {
        QString fileName = QFileInfo(file).fileName();
        logMessage = QString("[%1] [%2] [%3:%4] %5")
            .arg(timestamp)
            .arg(levelStr)
            .arg(fileName)
            .arg(line)
            .arg(message);
    } else {
        logMessage = QString("[%1] [%2] %3")
            .arg(timestamp)
            .arg(levelStr)
            .arg(message);
    }

    if (m_consoleLoggingEnabled) {
        switch (level) {
        case LogLevel::Debug:
            qDebug().noquote() << logMessage;
            break;
        case LogLevel::Info:
            qInfo().noquote() << logMessage;
            break;
        case LogLevel::Warning:
            qWarning().noquote() << logMessage;
            break;
        case LogLevel::Error:
            qCritical().noquote() << logMessage;
            break;
        }
    }

    if (m_fileLoggingEnabled && m_logFile.isOpen()) {
        m_logStream << logMessage << "\n";
        m_logStream.flush();
    }
}

QString Logger::levelToString(LogLevel level) const
{
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

QString Logger::getDefaultLogPath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        appDataPath = QDir::homePath() + "/.lightpad";
    }
    return appDataPath + "/lightpad.log";
}
