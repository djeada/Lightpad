#ifndef TERMINAL_H
#define TERMINAL_H

#include <QColor>
#include <QElapsedTimer>
#include <QMap>
#include <QMenu>
#include <QProcess>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QStringList>
#include <QTextCharFormat>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QWidget>

class QLabel;
class QAction;
class QTextCursor;
#ifndef Q_OS_WIN
class TerminalPty;
#endif

#include "../../python/pythonprojectenvironment.h"
#include "shellprofile.h"

namespace Ui {
class Terminal;
}

class QEvent;

class Terminal : public QWidget {
  Q_OBJECT

  friend class TestTerminal;

public:
  explicit Terminal(QWidget *parent = nullptr,
                    const QString &workingDirectory = QString());
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
  bool hasActiveRunProcess() const;
  bool canInterruptActiveProcess() const;
  bool interruptActiveProcess();
  qint64 runProcessId() const;

  void executeCommand(const QString &command);

  void setWorkingDirectory(const QString &directory);

  void clear();

  void applyTheme(const QString &backgroundColor, const QString &textColor,
                  const QString &errorColor = QString());

  static QString closeButtonStyle(const QString &textColor,
                                  const QString &pressedColor);

  void setShellProfile(const ShellProfile &profile);

  ShellProfile shellProfile() const;

  void setPythonEnvironmentBanner(const PythonEnvironmentInfo &info);

  QStringList availableShellProfiles() const;

  QString runTranscript() const { return m_runTranscript; }

  void appendNotice(const QString &text, bool isError = false);

  bool setShellProfileByName(const QString &profileName);

  void sendText(const QString &text, bool appendNewline = false);

  void refreshTerminalSize();

  void setScrollbackLines(int lines);

  int scrollbackLines() const;

  void setLinkDetectionEnabled(bool enabled);

  bool isLinkDetectionEnabled() const;

  void zoomIn();

  void zoomOut();

  void zoomReset();

  int currentFontSize() const;

signals:
  void fontSizeChanged(int newSize);
  void processStarted();
  void processFinished(int exitCode);
  void processError(const QString &errorMessage);

  void shellStarted();

  void shellFinished(int exitCode);

  void errorOccurred(const QString &message);

  void linkClicked(const QString &link);

  void shellProfileChanged(const QString &profileName);

private slots:
  void recordRunOutput(const QString &text);
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
#ifndef Q_OS_WIN
  void onPtyReadyRead(const QByteArray &data);
  void onPtyFinished(int exitCode, bool crashed);
  void onPtyError(const QString &message);
#endif

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void setupTerminal();
  void setupContextMenu();
  void appendOutput(const QString &text, bool isError = false);
  void appendPrompt();
  static bool looksLikeInputPrompt(const QString &text);
  QString getShellCommand() const;
  QStringList getShellArguments() const;
  void scrollToBottom();
  void handleHistoryNavigation(bool up);
  void handleTabCompletion();
  void cleanupRunProcess(bool restartShell);
  void cleanupProcess();
  void scheduleAutoRestart();
  void setupInputIndicator();
  void setRunInputIndicatorActive(bool active);
  void updateRunInputIndicator();
  void updateStyleSheet();
  void setFontSize(int pointSize);
  void updateCwdLabel();
  bool handleCommonInputKey(QKeyEvent *keyEvent);
  bool handlePtyKeyPress(QKeyEvent *keyEvent);
  QByteArray ptyKeySequence(QKeyEvent *keyEvent) const;
  bool shouldTerminalConsumeShortcut(QKeyEvent *keyEvent) const;
  bool isShellInForeground() const;
  QString shellCurrentDirectory() const;
  void placeCaretAtAnsiCursor();
  bool isPtyShellActive() const;
  void writeToShell(const QByteArray &data);
  void updatePtySize();
  void handleRunInputHistoryNavigation(bool up);
  QColor ansi256Color(int index) const;
  QString formatPythonBanner(const PythonEnvironmentInfo &info) const;
  QString filterShellStartupNoise(const QString &text) const;
  bool isShellStartupNoiseLine(const QString &line) const;
  QString processTextForLinks(const QString &text);
  void enforceScrollbackLimit();
  QTextCursor clampedInputCursor(bool moveToEndWhenOutsideInput = false) const;
  void copySelectionToClipboard() const;
  void insertInputText(const QString &text);
  void pasteClipboardText();
  static QByteArray ptyPasteData(const QString &text, bool bracketed);
  void removeInputText(bool backwards);
  QString takePendingInput();
  QString getLinkAtPosition(const QPoint &pos);
  void resetAnsiState();
  QTextCharFormat currentAnsiFormat() const;
  void ensureAnsiLineExists(int row);
  QTextCursor ansiCursor(bool padToColumn = false);
  void syncAnsiCursor(const QTextCursor &cursor);
  void syncAnsiCursorToDocumentEnd();
  int screenRows() const;
  int screenColumns() const;
  int scrollRegionTop() const;
  int scrollRegionBottom() const;
  void keepCursorOnScreen();
  void handleScreenResize();
  void removeLines(int first, int count);
  void insertBlankLines(int at, int count);
  void scrollUp(int count, int top, int bottom, bool allowScrollback);
  void scrollDown(int count, int top, int bottom);
  void lineFeed();
  void reverseIndex();
  void writePrintable(const QString &text);
  void eraseInLine(int mode);
  void eraseInDisplay(int mode);
  void trimTrailingBlanksAfterCursor();
  void saveCursorState();
  void restoreCursorState();
  void setPrivateMode(int mode, bool enable);
  void applySgr(const QString &params);
  int handleEscapeSequence(const QString &text, int index);
  void handleCsi(const QString &body, QChar finalByte);
  void sendTerminalReply(const QByteArray &reply);
  void setCursorShown(bool shown);
  void clearDocument();
  void enterAlternateScreen();
  void leaveAlternateScreen();
  bool hasProtectedInputSurface() const;
  static QString stripAnsiEscapeCodes(const QString &text);
  void appendAnsiText(const QString &text, QTextCursor &cursor);

  Ui::Terminal *ui;
  QProcess *m_process;
#ifndef Q_OS_WIN
  TerminalPty *m_shellPty;
#endif
  QProcess *m_runProcess;
  QTimer *m_restartTimer;
  QString m_workingDirectory;
  QString m_currentInput;
  QStringList m_commandHistory;
  int m_historyIndex;
  bool m_processRunning;
  bool m_shellStopRequested;
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
  static const int kMaxOutputChunkCharacters = 65536;
  static const int kMaxDocumentCharacters = 2097152;

  bool m_linkDetectionEnabled;
  QRegularExpression m_urlRegex;
  QRegularExpression m_filePathRegex;

  int m_inputStartPosition;
  QColor m_ansiForeground;
  QColor m_ansiBackground;
  int m_ansiRow;
  int m_ansiColumn;
  bool m_ansiBold;
  bool m_ansiDim;
  bool m_ansiItalic;
  bool m_ansiUnderline;
  bool m_ansiInverse;
  bool m_ansiStrikeOut;
  bool m_ansiHidden;
  bool m_alternateScreenActive;
  int m_terminalColumns;
  int m_terminalRows;

  int m_screenTop;
  int m_scrollTop;
  int m_scrollBottom;
  bool m_autoWrap;
  bool m_insertMode;
  bool m_applicationCursorKeys;
  bool m_bracketedPaste;
  bool m_cursorShown;
  bool m_processingPtyOutput;
  struct SavedCursor {
    int row = 0;
    int column = 0;
    QColor foreground;
    QColor background;
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool inverse = false;
    bool strikeOut = false;
    bool hidden = false;
  };
  SavedCursor m_savedCursor;
  QTextDocumentFragment m_savedPrimaryScreen;
  QString m_pendingAnsiText;
  QStringDecoder m_ptyDecoder;
  int m_savedPrimaryInputStartPosition;
  int m_savedPrimaryScreenTop;
  int m_savedPrimaryAnsiRow;
  int m_savedPrimaryAnsiColumn;

  int m_baseFontSize;
  static const int kMinFontSize = 6;
  static const int kMaxFontSize = 48;
  static const int kDefaultFontSize = 11;

  QMenu *m_contextMenu;
  QAction *m_copyAction;
  QAction *m_stopAction;
  QString m_pythonEnvironmentBanner;

  QElapsedTimer m_runProcessTimer;
  QStringList m_runInputHistory;
  int m_runInputHistoryIndex;
  QLabel *m_runInputIndicator;
  QTimer *m_runInputIndicatorTimer;
  bool m_runInputIndicatorActive;
  bool m_runInputCursorVisible;
  static const int kInputIndicatorBlinkMs = 500;

  QTimer *m_inputIndicatorDebounceTimer;
  QString m_lastRunProcessOutput;
  QString m_runTranscript;
  static const int kInputIndicatorDebounceMs = 80;
};

#endif
