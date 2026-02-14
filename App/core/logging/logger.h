#ifndef LOGGER_H
#define LOGGER_H

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
  static Logger &instance();

  void setLogLevel(LogLevel level);

  LogLevel logLevel() const;

  void setFileLoggingEnabled(bool enabled, const QString &filePath = QString());

  bool isFileLoggingEnabled() const;

  void setConsoleLoggingEnabled(bool enabled);

  bool isConsoleLoggingEnabled() const;

  void debug(const QString &message, const char *file = nullptr, int line = 0);

  void info(const QString &message, const char *file = nullptr, int line = 0);

  void warning(const QString &message, const char *file = nullptr,
               int line = 0);

  void error(const QString &message, const char *file = nullptr, int line = 0);

  void shutdown();

private:
  Logger();
  ~Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void log(LogLevel level, const QString &message, const char *file, int line);
  QString levelToString(LogLevel level) const;
  QString getDefaultLogPath() const;

  LogLevel m_logLevel;
  bool m_fileLoggingEnabled;
  bool m_consoleLoggingEnabled;
  QFile m_logFile;
  QTextStream m_logStream;
  mutable QMutex m_mutex;
};

#define LOG_DEBUG(msg) Logger::instance().debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg) Logger::instance().info(msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) Logger::instance().warning(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().error(msg, __FILE__, __LINE__)

#endif
