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

class Terminal : public QWidget {
  Q_OBJECT

public:
  explicit Terminal(QWidget *parent = nullptr);
  ~Terminal();

  void
  executeCommand(const QString &command, const QStringList &args,
                 const QString &workingDirectory,
                 const QMap<QString, QString> &env = QMap<QString, QString>());

  bool runFile(const QString &filePath, const QString &languageId = QString());

  void stopProcess();

  bool startShell(const QString &workingDirectory = QString());

  void stopShell();

  bool isRunning() const;

  void executeCommand(const QString &command);

  void setWorkingDirectory(const QString &directory);

  void clear();

  void applyTheme(const QString &backgroundColor, const QString &textColor,
                  const QString &errorColor = QString());

  static QString closeButtonStyle(const QString &textColor,
                                  const QString &pressedColor);

  void setShellProfile(const ShellProfile &profile);

  ShellProfile shellProfile() const;

  QStringList availableShellProfiles() const;

  bool setShellProfileByName(const QString &profileName);

  void sendText(const QString &text, bool appendNewline = false);

  void setScrollbackLines(int lines);

  int scrollbackLines() const;

  void setLinkDetectionEnabled(bool enabled);

  bool isLinkDetectionEnabled() const;

signals:
  void processStarted();
  void processFinished(int exitCode);
  void processError(const QString &errorMessage);

  void shellStarted();

  void shellFinished(int exitCode);

  void errorOccurred(const QString &message);

  void outputReceived(const QString &output);

  void linkClicked(const QString &link);

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

  QString m_backgroundColor;
  QString m_textColor;
  QString m_errorColor;
  QString m_linkColor;

  ShellProfile m_shellProfile;

  int m_scrollbackLines;
  static const int kDefaultScrollbackLines = 10000;

  bool m_linkDetectionEnabled;
  QRegularExpression m_urlRegex;
  QRegularExpression m_filePathRegex;

  int m_inputStartPosition;
};

#endif
