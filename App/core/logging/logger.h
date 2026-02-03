#ifndef LOGGER_H
#define LOGGER_H

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

/**
 * @brief Log levels for the logging framework
 */
enum class LogLevel { Debug, Info, Warning, Error };

/**
 * @brief Singleton logger class for centralized logging
 *
 * Provides configurable log levels, file output, and thread-safe logging.
 * Replaces scattered qDebug() calls throughout the codebase.
 */
class Logger {
public:
  /**
   * @brief Get the singleton instance of the logger
   * @return Reference to the Logger instance
   */
  static Logger &instance();

  /**
   * @brief Set the minimum log level to output
   * @param level The minimum level to log
   */
  void setLogLevel(LogLevel level);

  /**
   * @brief Get the current log level
   * @return The current minimum log level
   */
  LogLevel logLevel() const;

  /**
   * @brief Enable or disable file logging
   * @param enabled Whether to log to file
   * @param filePath Optional custom file path (defaults to app data directory)
   */
  void setFileLoggingEnabled(bool enabled, const QString &filePath = QString());

  /**
   * @brief Check if file logging is enabled
   * @return true if logging to file
   */
  bool isFileLoggingEnabled() const;

  /**
   * @brief Enable or disable console logging
   * @param enabled Whether to log to console
   */
  void setConsoleLoggingEnabled(bool enabled);

  /**
   * @brief Check if console logging is enabled
   * @return true if logging to console
   */
  bool isConsoleLoggingEnabled() const;

  /**
   * @brief Log a debug message
   * @param message The message to log
   * @param file Source file name (optional)
   * @param line Source line number (optional)
   */
  void debug(const QString &message, const char *file = nullptr, int line = 0);

  /**
   * @brief Log an info message
   * @param message The message to log
   * @param file Source file name (optional)
   * @param line Source line number (optional)
   */
  void info(const QString &message, const char *file = nullptr, int line = 0);

  /**
   * @brief Log a warning message
   * @param message The message to log
   * @param file Source file name (optional)
   * @param line Source line number (optional)
   */
  void warning(const QString &message, const char *file = nullptr,
               int line = 0);

  /**
   * @brief Log an error message
   * @param message The message to log
   * @param file Source file name (optional)
   * @param line Source line number (optional)
   */
  void error(const QString &message, const char *file = nullptr, int line = 0);

  /**
   * @brief Close the log file and cleanup
   */
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

// Convenience macros for logging with file and line info
#define LOG_DEBUG(msg) Logger::instance().debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg) Logger::instance().info(msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) Logger::instance().warning(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().error(msg, __FILE__, __LINE__)

#endif // LOGGER_H
