#ifndef TERMINAL_H
#define TERMINAL_H

#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include "shellprofile.h"

namespace Ui {
class Terminal;
}

class QEvent;

/**
 * @brief Terminal widget providing full shell interaction using QProcess
 *
 * Provides an embedded terminal that spawns a shell process and allows
 * interactive command execution with real-time output display.
 */
class Terminal : public QWidget {
  Q_OBJECT

public:
  explicit Terminal(QWidget *parent = nullptr);
  ~Terminal();

  /**
   * @brief Execute a command with the given arguments
   * @param command The command to execute
   * @param args Arguments for the command
   * @param workingDirectory Working directory for execution
   * @param env Environment variables (optional)
   */
  void
  executeCommand(const QString &command, const QStringList &args,
                 const QString &workingDirectory,
                 const QMap<QString, QString> &env = QMap<QString, QString>());

  /**
   * @brief Execute the run template for a file
   * @param filePath Path to the file to run
   * @return true if a command was started
   */
  bool runFile(const QString &filePath);

  /**
   * @brief Stop the currently running process
   */
  void stopProcess();

  /**
   * @brief Start the terminal shell process
   * @param workingDirectory Optional working directory for the shell
   * @return true if shell started successfully
   */
  bool startShell(const QString &workingDirectory = QString());

  /**
   * @brief Stop the terminal shell process
   */
  void stopShell();

  /**
   * @brief Check if shell process is running
   * @return true if shell is running
   */
  bool isRunning() const;

  /**
   * @brief Execute a command in the terminal
   * @param command Command to execute
   */
  void executeCommand(const QString &command);

  /**
   * @brief Set the working directory for the shell
   * @param directory Working directory path
   */
  void setWorkingDirectory(const QString &directory);

  /**
   * @brief Clear the terminal output
   */
  void clear();

  /**
   * @brief Apply theme colors to the terminal
   * @param backgroundColor Background color
   * @param textColor Text color
   * @param errorColor Error text color
   */
  void applyTheme(const QString &backgroundColor, const QString &textColor,
                  const QString &errorColor = QString());

  static QString closeButtonStyle(const QString &textColor,
                                  const QString &pressedColor);

  /**
   * @brief Set the shell profile to use
   * @param profile Shell profile
   */
  void setShellProfile(const ShellProfile &profile);

  /**
   * @brief Get the current shell profile
   * @return Current shell profile
   */
  ShellProfile shellProfile() const;

  /**
   * @brief Get available shell profiles
   * @return List of shell profile names
   */
  QStringList availableShellProfiles() const;

  /**
   * @brief Set the shell profile by name
   * @param profileName Name of the profile to use
   * @return true if profile was found and set
   */
  bool setShellProfileByName(const QString &profileName);

  /**
   * @brief Send text directly to the terminal
   * @param text Text to send (will be written to stdin)
   * @param appendNewline Whether to append a newline
   */
  void sendText(const QString &text, bool appendNewline = false);

  /**
   * @brief Set the scrollback buffer size
   * @param lines Number of lines to keep (0 for unlimited)
   */
  void setScrollbackLines(int lines);

  /**
   * @brief Get the scrollback buffer size
   * @return Number of lines in scrollback buffer
   */
  int scrollbackLines() const;

  /**
   * @brief Enable or disable link detection in output
   * @param enabled Whether to detect and highlight links
   */
  void setLinkDetectionEnabled(bool enabled);

  /**
   * @brief Check if link detection is enabled
   * @return true if link detection is enabled
   */
  bool isLinkDetectionEnabled() const;

signals:
  void processStarted();
  void processFinished(int exitCode);
  void processError(const QString &errorMessage);

  /**
   * @brief Emitted when the shell process starts
   */
  void shellStarted();

  /**
   * @brief Emitted when the shell process finishes
   * @param exitCode Exit code of the shell
   */
  void shellFinished(int exitCode);

  /**
   * @brief Emitted when an error occurs
   * @param message Error message
   */
  void errorOccurred(const QString &message);

  /**
   * @brief Emitted when output is available
   * @param output Output text
   */
  void outputReceived(const QString &output);

  /**
   * @brief Emitted when a link is clicked in the terminal
   * @param link The clicked link (URL or file path)
   */
  void linkClicked(const QString &link);

  /**
   * @brief Emitted when the shell profile changes
   * @param profileName Name of the new profile
   */
  void shellProfileChanged(const QString &profileName);

private slots:
  void on_closeButton_clicked();

  void onRunProcessReadyReadStdout();
  void onRunProcessReadyReadStderr();
  void onRunProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void onRunProcessError(QProcess::ProcessError error);

  void onReadyReadStandardOutput();
  void onReadyReadStandardError();
  void onProcessError(QProcess::ProcessError error);
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void onInputSubmitted();
  void onLinkActivated(const QString &link);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  void setupTerminal();
  void appendOutput(const QString &text, bool isError = false);
  void appendPrompt();
  QString getShellCommand() const;
  QStringList getShellArguments() const;
  void scrollToBottom();
  void handleHistoryNavigation(bool up);
  void handleTabCompletion();
  void cleanupRunProcess(bool restartShell);
  void cleanupProcess();
  void scheduleAutoRestart();
  void updateStyleSheet();
  QString filterShellStartupNoise(const QString &text) const;
  bool isShellStartupNoiseLine(const QString &line) const;
  QString processTextForLinks(const QString &text);
  void enforceScrollbackLimit();
  QString getLinkAtPosition(const QPoint &pos);
  QString stripAnsiEscapeCodes(const QString &text);

  Ui::Terminal *ui;
  QProcess *m_process;
  QProcess *m_runProcess;
  QTimer *m_restartTimer;
  QString m_workingDirectory;
  QString m_currentInput;
  QStringList m_commandHistory;
  int m_historyIndex;
  bool m_processRunning;
  bool m_restartShellAfterRun;
  bool m_autoRestartEnabled;
  int m_restartAttempts;
  static const int kMaxRestartAttempts = 3;
  static const int kRestartDelayMs = 1000;

  // Theme colors
  QString m_backgroundColor;
  QString m_textColor;
  QString m_errorColor;
  QString m_linkColor;

  // Shell profile
  ShellProfile m_shellProfile;

  // Scrollback configuration
  int m_scrollbackLines;
  static const int kDefaultScrollbackLines = 10000;

  // Link detection
  bool m_linkDetectionEnabled;
  QRegularExpression m_urlRegex;
  QRegularExpression m_filePathRegex;

  // Input tracking - position where user input begins (after shell
  // output/prompt)
  int m_inputStartPosition;
};

#endif // TERMINAL_H
