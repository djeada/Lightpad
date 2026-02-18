#include "terminal.h"
#include "../../run_templates/runtemplatemanager.h"
#include "ui_terminal.h"

#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QUrl>

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

Terminal::Terminal(QWidget *parent)
    : QWidget(parent), ui(new Ui::Terminal), m_process(nullptr),
      m_runProcess(nullptr), m_restartTimer(nullptr), m_historyIndex(0),
      m_processRunning(false), m_restartShellAfterRun(false),
      m_autoRestartEnabled(true), m_restartAttempts(0),
      m_backgroundColor("#0e1116"), m_textColor("#e6edf3"),
      m_errorColor("#f44336"), m_linkColor("#58a6ff"),
      m_scrollbackLines(kDefaultScrollbackLines), m_linkDetectionEnabled(true),
      m_urlRegex(R"((https?://|ftp://|file://)[^\s<>\"\'\]\)]+)"),
      m_filePathRegex(R"((?:^|[\s:])(/[^\s:]+|[A-Za-z]:\\[^\s:]+))"),
      m_inputStartPosition(0) {
  ui->setupUi(this);
  ui->closeButton->setText(QStringLiteral("\u00D7"));
  ui->closeButton->setToolTip(tr("Close Terminal"));
  ui->closeButton->setAutoRaise(true);
  ui->closeButton->setCursor(Qt::ArrowCursor);
  ui->closeButton->setFixedSize(QSize(18, 18));
  ui->closeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  ui->closeButton->setCheckable(false);
  ui->closeButton->setAutoExclusive(false);

  ui->closeButton->setStyleSheet(closeButtonStyle(m_textColor, m_errorColor));

  m_shellProfile = ShellProfileManager::instance().defaultProfile();

  m_restartTimer = new QTimer(this);
  m_restartTimer->setSingleShot(true);
  connect(m_restartTimer, &QTimer::timeout, this, [this]() {
    if (m_autoRestartEnabled && !m_processRunning) {
      appendOutput("Attempting to restart shell...\n");
      if (startShell()) {
        m_restartAttempts = 0;
      }
    }
  });

  setupTerminal();
}

Terminal::~Terminal() {

  m_autoRestartEnabled = false;
  if (m_restartTimer) {
    m_restartTimer->stop();
  }
  cleanupRunProcess(false);
  stopShell();
  delete ui;
}

void Terminal::setupTerminal() {

  ui->textEdit->setReadOnly(false);
  ui->textEdit->setTextInteractionFlags(Qt::TextEditorInteraction);
  ui->textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

  QFont monoFont("Monospace");
  monoFont.setStyleHint(QFont::TypeWriter);
  monoFont.setPointSize(10);
  ui->textEdit->setFont(monoFont);

  ui->textEdit->installEventFilter(this);

  updateStyleSheet();

  startShell();
}

bool Terminal::startShell(const QString &workingDirectory) {

  if (m_restartTimer && m_restartTimer->isActive()) {
    m_restartTimer->stop();
  }

  if (m_process && m_processRunning) {
    return true;
  }

  if (m_process) {
    disconnect(m_process, nullptr, this, nullptr);
    if (m_process->state() != QProcess::NotRunning) {
      m_process->terminate();
      m_process->waitForFinished(1000);
    }
    delete m_process;
    m_process = nullptr;
  }

  m_process = new QProcess(this);

  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &Terminal::onReadyReadStandardOutput);
  connect(m_process, &QProcess::readyReadStandardError, this,
          &Terminal::onReadyReadStandardError);
  connect(m_process, &QProcess::errorOccurred, this, &Terminal::onProcessError);
  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &Terminal::onProcessFinished);

  if (!workingDirectory.isEmpty()) {
    m_workingDirectory = workingDirectory;
  }

  if (m_workingDirectory.isEmpty()) {
    m_workingDirectory = QDir::homePath();
  }

  if (!m_workingDirectory.isEmpty() && !QDir(m_workingDirectory).exists()) {
    appendOutput(
        QString(
            "Warning: Directory '%1' does not exist, using home directory.\n")
            .arg(m_workingDirectory),
        true);
    m_workingDirectory = QDir::homePath();
  }

  m_process->setWorkingDirectory(m_workingDirectory);

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("TERM", "dumb");
  m_process->setProcessEnvironment(env);

#ifndef Q_OS_WIN

  m_process->setChildProcessModifier([]() { setsid(); });
#endif

  QString shell = getShellCommand();
  QStringList args = getShellArguments();

  m_process->setProgram(shell);
  m_process->setArguments(args);
  m_process->start();

  if (!m_process->waitForStarted(5000)) {
    appendOutput("Error: Failed to start shell process.\n", true);
    emit errorOccurred("Failed to start shell");

    delete m_process;
    m_process = nullptr;

    return false;
  }

  m_processRunning = true;
  m_restartAttempts = 0;
  ui->textEdit->setReadOnly(false);
  emit shellStarted();

  return true;
}

void Terminal::stopShell() {

  if (m_restartTimer) {
    m_restartTimer->stop();
  }

  if (!m_process) {
    return;
  }

  m_processRunning = false;

  disconnect(m_process, nullptr, this, nullptr);

  if (m_process->state() != QProcess::NotRunning) {
    m_process->terminate();
    if (!m_process->waitForFinished(2000)) {
      m_process->kill();
      m_process->waitForFinished(1000);
    }
  }

  delete m_process;
  m_process = nullptr;
}

bool Terminal::isRunning() const {
  return m_processRunning && m_process &&
         m_process->state() == QProcess::Running;
}

void Terminal::executeCommand(const QString &command) {
  if (!isRunning()) {
    appendOutput("Error: Shell not running. Restarting...\n", true);
    if (!startShell()) {
      return;
    }
  }

  if (!command.trimmed().isEmpty()) {
    if (m_commandHistory.isEmpty() || m_commandHistory.last() != command) {
      m_commandHistory.append(command);
    }
    m_historyIndex = m_commandHistory.size();
  }

  QString cmdWithNewline = command + "\n";
  m_process->write(cmdWithNewline.toUtf8());
}

void Terminal::setWorkingDirectory(const QString &directory) {
  m_workingDirectory = directory;
  if (isRunning()) {

    executeCommand(QString("cd \"%1\"").arg(directory));
  }
}

void Terminal::clear() {
  ui->textEdit->clear();
  m_inputStartPosition = 0;
  if (isRunning() &&
      !(m_runProcess && m_runProcess->state() != QProcess::NotRunning)) {
    appendPrompt();
  }
}

void Terminal::executeCommand(const QString &command, const QStringList &args,
                              const QString &workingDirectory,
                              const QMap<QString, QString> &env) {

  cleanupRunProcess(false);

  bool wasShellRunning = isRunning();
  if (wasShellRunning) {
    stopShell();
  }
  m_restartShellAfterRun = wasShellRunning;

  if (!workingDirectory.isEmpty()) {
    m_workingDirectory = workingDirectory;
  }

  m_runProcess = new QProcess(this);

  connect(m_runProcess, &QProcess::readyReadStandardOutput, this,
          &Terminal::onRunProcessReadyReadStdout);
  connect(m_runProcess, &QProcess::readyReadStandardError, this,
          &Terminal::onRunProcessReadyReadStderr);
  connect(m_runProcess,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &Terminal::onRunProcessFinished);
  connect(m_runProcess, &QProcess::errorOccurred, this,
          &Terminal::onRunProcessError);

  m_runProcess->setWorkingDirectory(workingDirectory);

  QProcessEnvironment processEnv = QProcessEnvironment::systemEnvironment();
  for (auto it = env.begin(); it != env.end(); ++it) {
    processEnv.insert(it.key(), it.value());
  }
  m_runProcess->setProcessEnvironment(processEnv);

  ui->textEdit->setReadOnly(true);
  clear();
  appendOutput(QString("$ %1 %2\n").arg(command, args.join(" ")));
  if (!workingDirectory.isEmpty()) {
    appendOutput(QString("Working directory: %1\n\n").arg(workingDirectory));
  }

  m_runProcess->start(command, args);

  emit processStarted();
}

bool Terminal::runFile(const QString &filePath, const QString &languageId) {
  RunTemplateManager &manager = RunTemplateManager::instance();

  if (manager.getAllTemplates().isEmpty()) {
    manager.loadTemplates();
  }

  QPair<QString, QStringList> command =
      manager.buildCommand(filePath, languageId);

  if (command.first.isEmpty()) {
    clear();
    appendOutput("Error: No run template found for this file type.\n", true);
    appendOutput("Use Edit > Run Configurations to assign a template.\n");
    return false;
  }

  QString workingDir = manager.getWorkingDirectory(filePath, languageId);
  QMap<QString, QString> env = manager.getEnvironment(filePath, languageId);

  executeCommand(command.first, command.second, workingDir, env);
  return true;
}

void Terminal::stopProcess() { cleanupRunProcess(true); }

void Terminal::cleanupRunProcess(bool restartShell) {
  if (m_runProcess) {

    disconnect(m_runProcess, nullptr, this, nullptr);

    if (m_runProcess->state() != QProcess::NotRunning) {
      m_runProcess->terminate();
      if (!m_runProcess->waitForFinished(3000)) {
        m_runProcess->kill();
        m_runProcess->waitForFinished(1000);
      }
    }
    delete m_runProcess;
    m_runProcess = nullptr;
  }

  ui->textEdit->setReadOnly(false);

  if (restartShell && m_restartShellAfterRun && !isRunning()) {
    startShell(m_workingDirectory);
  }

  if (restartShell) {
    m_restartShellAfterRun = false;
  }
}

void Terminal::cleanupProcess() {
  if (!m_process) {
    return;
  }

  disconnect(m_process, nullptr, this, nullptr);

  m_process->deleteLater();
  m_process = nullptr;
}

void Terminal::scheduleAutoRestart() {
  if (!m_autoRestartEnabled || !m_restartTimer) {
    return;
  }

  ++m_restartAttempts;

  if (m_restartAttempts > kMaxRestartAttempts) {
    appendOutput(QString("Auto-restart disabled after %1 failed attempts.\n")
                     .arg(kMaxRestartAttempts),
                 true);
    appendOutput("Use the terminal controls to manually restart the shell.\n");
    m_restartAttempts = 0;
    return;
  }

  appendOutput(
      QString("Will attempt restart in %1 second(s) (attempt %2/%3)...\n")
          .arg(kRestartDelayMs / 1000)
          .arg(m_restartAttempts)
          .arg(kMaxRestartAttempts));

  m_restartTimer->start(kRestartDelayMs);
}

void Terminal::onRunProcessReadyReadStdout() {
  if (m_runProcess) {
    QString output = QString::fromUtf8(m_runProcess->readAllStandardOutput());
    appendOutput(output);
    emit outputReceived(output);
  }
}

void Terminal::onRunProcessReadyReadStderr() {
  if (m_runProcess) {
    QString output = QString::fromUtf8(m_runProcess->readAllStandardError());
    appendOutput(output, true);
    emit outputReceived(output);
  }
}

void Terminal::onRunProcessFinished(int exitCode,
                                    QProcess::ExitStatus exitStatus) {
  if (exitStatus == QProcess::CrashExit) {
    appendOutput(QString("\n\nProcess crashed (exit code: %1)\n").arg(exitCode),
                 true);
  } else {
    appendOutput(
        QString("\n\nProcess finished with exit code %1\n").arg(exitCode));
  }
  emit processFinished(exitCode);
  cleanupRunProcess(true);
}

void Terminal::onRunProcessError(QProcess::ProcessError error) {
  QString errorMessage;
  switch (error) {
  case QProcess::FailedToStart:
    errorMessage =
        "Failed to start. The program may not be installed or not in PATH.";
    break;
  case QProcess::Crashed:
    errorMessage = "The process crashed.";
    break;
  case QProcess::Timedout:
    errorMessage = "The process timed out.";
    break;
  case QProcess::WriteError:
    errorMessage = "Write error occurred.";
    break;
  case QProcess::ReadError:
    errorMessage = "Read error occurred.";
    break;
  default:
    errorMessage = "An unknown error occurred.";
    break;
  }

  appendOutput(QString("\nError: %1\n").arg(errorMessage), true);
  emit processError(errorMessage);

  if (error == QProcess::FailedToStart || error == QProcess::Crashed) {
    cleanupRunProcess(true);
  }
}

void Terminal::on_closeButton_clicked() {
  stopProcess();
  stopShell();
  emit destroyed();
  close();
}

void Terminal::onReadyReadStandardOutput() {
  if (!m_process) {
    return;
  }

  QByteArray data = m_process->readAllStandardOutput();
  QString output = QString::fromLocal8Bit(data);

  appendOutput(output);
  emit outputReceived(output);
}

void Terminal::onReadyReadStandardError() {
  if (!m_process) {
    return;
  }

  QByteArray data = m_process->readAllStandardError();
  QString output = QString::fromLocal8Bit(data);
  output = filterShellStartupNoise(output);
  if (output.isEmpty()) {
    return;
  }

  appendOutput(output, true);
  emit outputReceived(output);
}

void Terminal::onProcessError(QProcess::ProcessError error) {
  QString errorMsg;
  bool shouldRestart = false;

  switch (error) {
  case QProcess::FailedToStart:
    errorMsg = "Failed to start shell process";
    shouldRestart = true;
    break;
  case QProcess::Crashed:
    errorMsg = "Shell process crashed";
    shouldRestart = true;
    break;
  case QProcess::Timedout:
    errorMsg = "Shell process timed out";
    shouldRestart = true;
    break;
  case QProcess::WriteError:
    errorMsg = "Error writing to shell process";

    break;
  case QProcess::ReadError:
    errorMsg = "Error reading from shell process";

    break;
  default:
    errorMsg = "Unknown shell error";
    break;
  }

  appendOutput(QString("Error: %1\n").arg(errorMsg), true);
  m_processRunning = false;

  cleanupProcess();

  emit errorOccurred(errorMsg);

  if (shouldRestart && m_autoRestartEnabled) {
    scheduleAutoRestart();
  }
}

void Terminal::onProcessFinished(int exitCode,
                                 QProcess::ExitStatus exitStatus) {
  m_processRunning = false;

  if (exitStatus == QProcess::CrashExit) {
    appendOutput(QString("\nShell crashed (exit code: %1)\n").arg(exitCode),
                 true);
    cleanupProcess();

    if (m_autoRestartEnabled) {
      scheduleAutoRestart();
    }
  } else {
    appendOutput(QString("\nShell exited with code: %1\n").arg(exitCode));
    cleanupProcess();
  }

  emit shellFinished(exitCode);
}

void Terminal::onInputSubmitted() {
  if (!m_currentInput.isEmpty()) {
    executeCommand(m_currentInput);
    m_currentInput.clear();
  }
}

bool Terminal::eventFilter(QObject *obj, QEvent *event) {
  if (obj == ui->textEdit && event->type() == QEvent::KeyPress) {
    if (m_runProcess && m_runProcess->state() != QProcess::NotRunning) {
      return QWidget::eventFilter(obj, event);
    }

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    switch (keyEvent->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter: {

      QTextCursor cursor = ui->textEdit->textCursor();
      cursor.movePosition(QTextCursor::End);
      int endPos = cursor.position();

      cursor.setPosition(m_inputStartPosition);
      cursor.setPosition(endPos, QTextCursor::KeepAnchor);
      QString userInput = cursor.selectedText();

      ui->textEdit->moveCursor(QTextCursor::End);
      ui->textEdit->insertPlainText("\n");

      if (!userInput.trimmed().isEmpty()) {
        QStringList segments = userInput.split(
            QRegularExpression("\\s*(?:&&|\\|\\||;)\\s*"), Qt::SkipEmptyParts);
        QString firstSegment =
            segments.isEmpty() ? QString() : segments.first().trimmed();
        QRegularExpression cdRegex(R"(^cd(?:\s+(.*))?$)");
        QRegularExpressionMatch cdMatch = cdRegex.match(firstSegment);
        if (cdMatch.hasMatch()) {
          QString target = cdMatch.captured(1).trimmed();
          if (target.isEmpty()) {
            target = QDir::homePath();
          } else {
            if ((target.startsWith('"') && target.endsWith('"')) ||
                (target.startsWith('\'') && target.endsWith('\''))) {
              target = target.mid(1, target.size() - 2);
            }
            if (target == "~") {
              target = QDir::homePath();
            } else if (target.startsWith("~/")) {
              target = QDir::homePath() + target.mid(1);
            }
          }

          if (!target.isEmpty() && target != "-") {
            QString resolved = target;
            if (QDir(resolved).isRelative()) {
              resolved = QDir(m_workingDirectory).filePath(resolved);
            }
            resolved = QDir::cleanPath(resolved);
            if (QDir(resolved).exists()) {
              m_workingDirectory = resolved;
            }
          }
        }
        executeCommand(userInput);
      } else {

        if (m_process && m_process->state() == QProcess::Running) {
          m_process->write("\n");
        }
      }
      return true;
    }

    case Qt::Key_Up:
      handleHistoryNavigation(true);
      return true;

    case Qt::Key_Down:
      handleHistoryNavigation(false);
      return true;

    case Qt::Key_Tab:
      handleTabCompletion();
      return true;

    case Qt::Key_C:
      if ((keyEvent->modifiers() & Qt::ControlModifier) &&
          (keyEvent->modifiers() & Qt::ShiftModifier)) {

        if (ui->textEdit->textCursor().hasSelection()) {
          ui->textEdit->copy();
        }
        return true;
      }
      if (keyEvent->modifiers() & Qt::ControlModifier) {

        if (isRunning()) {
          m_process->write("\x03");
        }
        appendOutput("^C\n");
        return true;
      }
      break;

    case Qt::Key_V:
      if ((keyEvent->modifiers() & Qt::ControlModifier) &&
          (keyEvent->modifiers() & Qt::ShiftModifier)) {

        ui->textEdit->paste();
        return true;
      }
      break;

    case Qt::Key_D:
      if (keyEvent->modifiers() & Qt::ControlModifier) {

        if (isRunning()) {
          m_process->write("\x04");
        }
        return true;
      }
      break;

    case Qt::Key_L:
      if (keyEvent->modifiers() & Qt::ControlModifier) {

        clear();
        return true;
      }
      break;

    default:
      break;
    }
  }

  return QWidget::eventFilter(obj, event);
}

void Terminal::appendOutput(const QString &text, bool isError) {

  QString cleanText = stripAnsiEscapeCodes(text);
  if (cleanText.isEmpty()) {
    return;
  }

  QTextCursor cursor = ui->textEdit->textCursor();
  cursor.movePosition(QTextCursor::End);

  if (isError) {

    QTextCharFormat errorFormat;
    errorFormat.setForeground(QColor(m_errorColor));
    cursor.insertText(cleanText, errorFormat);
  } else {

    QTextCharFormat defaultFormat;
    defaultFormat.setForeground(QColor(m_textColor));
    cursor.insertText(cleanText, defaultFormat);
  }

  ui->textEdit->setTextCursor(cursor);
  scrollToBottom();

  enforceScrollbackLimit();

  m_inputStartPosition = ui->textEdit->textCursor().position();
}

void Terminal::appendPrompt() {
  QString prompt = QString("%1 $ ").arg(m_workingDirectory);
  appendOutput(prompt);
}

QString Terminal::getShellCommand() const {

  if (m_shellProfile.isValid()) {
    return m_shellProfile.command;
  }

#ifdef Q_OS_WIN

  QString comspec = qEnvironmentVariable("COMSPEC", "cmd.exe");
  return comspec;
#else

  QString shell = qEnvironmentVariable("SHELL", "/bin/sh");
  return shell;
#endif
}

QStringList Terminal::getShellArguments() const {

  if (m_shellProfile.isValid()) {
    return m_shellProfile.arguments;
  }

  QStringList args;

#ifdef Q_OS_WIN

#else

  args << "-i";
#endif

  return args;
}

void Terminal::scrollToBottom() {
  QScrollBar *vScrollBar = ui->textEdit->verticalScrollBar();
  vScrollBar->setValue(vScrollBar->maximum());
}

void Terminal::handleHistoryNavigation(bool up) {
  if (m_commandHistory.isEmpty()) {
    return;
  }

  if (up) {

    if (m_historyIndex > 0) {
      m_historyIndex--;
    } else if (m_historyIndex == m_commandHistory.size()) {

      m_historyIndex = m_commandHistory.size() - 1;
    }
  } else {

    if (m_historyIndex < m_commandHistory.size()) {
      m_historyIndex++;
      if (m_historyIndex >= m_commandHistory.size()) {

        QTextCursor cursor = ui->textEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.setPosition(m_inputStartPosition, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        return;
      }
    }
  }

  if (m_historyIndex >= 0 && m_historyIndex < m_commandHistory.size()) {
    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.setPosition(m_inputStartPosition, QTextCursor::KeepAnchor);
    cursor.insertText(m_commandHistory[m_historyIndex]);
    ui->textEdit->setTextCursor(cursor);
  }
}

void Terminal::handleTabCompletion() {

  QTextCursor cursor = ui->textEdit->textCursor();
  cursor.movePosition(QTextCursor::End);
  int endPos = cursor.position();

  cursor.setPosition(m_inputStartPosition);
  cursor.setPosition(endPos, QTextCursor::KeepAnchor);
  QString userInput = cursor.selectedText();

  if (userInput.isEmpty()) {
    return;
  }

  int lastSpace = userInput.lastIndexOf(' ');
  QString prefix = (lastSpace >= 0) ? userInput.left(lastSpace + 1) : QString();
  QString toComplete =
      (lastSpace >= 0) ? userInput.mid(lastSpace + 1) : userInput;

  if (toComplete.isEmpty()) {
    return;
  }

  QString searchPath = toComplete;
  QString pathPrefix;
  if (searchPath.startsWith("~/")) {
    pathPrefix = "~/";
    searchPath = QDir::homePath() + searchPath.mid(1);
  } else if (searchPath == "~") {
    pathPrefix = "~";
    searchPath = QDir::homePath();
  }

  QFileInfo fileInfo(searchPath);
  QString dirPath;
  QString filePrefix;

  if (searchPath.endsWith('/') || QFileInfo(searchPath).isDir()) {
    dirPath = searchPath;
    filePrefix = QString();
  } else {
    dirPath = fileInfo.absolutePath();
    filePrefix = fileInfo.fileName();
  }

  if (!QDir(dirPath).isAbsolute() && pathPrefix.isEmpty()) {
    dirPath = m_workingDirectory + "/" + dirPath;
  }

  QDir dir(dirPath);
  if (!dir.exists()) {
    return;
  }

  QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
  QStringList matches;

  for (const QString &entry : entries) {
    if (filePrefix.isEmpty() ||
        entry.startsWith(filePrefix, Qt::CaseSensitive)) {
      QString match = entry;

      if (QFileInfo(dir.absoluteFilePath(entry)).isDir()) {
        match += '/';
      }
      matches.append(match);
    }
  }

  if (matches.isEmpty()) {
    return;
  }

  QString completion;
  if (matches.size() == 1) {

    completion = matches.first();
  } else {

    completion = matches.first();
    for (int i = 1; i < matches.size(); ++i) {
      int j = 0;
      while (j < completion.length() && j < matches[i].length() &&
             completion[j] == matches[i][j]) {
        ++j;
      }
      completion = completion.left(j);
    }

    if (completion.length() <= filePrefix.length()) {
      appendOutput("\n");
      for (const QString &match : matches) {
        appendOutput(match + "  ");
      }
      appendOutput("\n");
      appendPrompt();

      ui->textEdit->moveCursor(QTextCursor::End);
      ui->textEdit->insertPlainText(userInput);
      return;
    }
  }

  QString completedPart;
  if (toComplete.contains('/')) {

    int lastSlash = toComplete.lastIndexOf('/');
    completedPart = toComplete.left(lastSlash + 1) + completion;
  } else {
    completedPart = completion;
  }

  QString newInput = prefix + pathPrefix + completedPart;

  cursor.movePosition(QTextCursor::End);
  cursor.setPosition(m_inputStartPosition, QTextCursor::KeepAnchor);
  cursor.insertText(newInput);
  ui->textEdit->setTextCursor(cursor);
}

void Terminal::applyTheme(const QString &backgroundColor,
                          const QString &textColor, const QString &errorColor) {
  if (!backgroundColor.isEmpty()) {
    m_backgroundColor = backgroundColor;
  }
  if (!textColor.isEmpty()) {
    m_textColor = textColor;
  }
  if (!errorColor.isEmpty()) {
    m_errorColor = errorColor;
  }

  updateStyleSheet();
}

void Terminal::updateStyleSheet() {
  QString styleSheet = QString("QPlainTextEdit {"
                               "  background-color: %1;"
                               "  color: %2;"
                               "  selection-background-color: #1b2a43;"
                               "  selection-color: %2;"
                               "  border: none;"
                               "}")
                           .arg(m_backgroundColor, m_textColor);

  ui->textEdit->setStyleSheet(styleSheet);

  ui->closeButton->setStyleSheet(closeButtonStyle(m_textColor, m_errorColor));
}

QString Terminal::closeButtonStyle(const QString &textColor,
                                   const QString &pressedColor) {
  const QColor baseColor(textColor);
  const QString subduedColor = QString("rgba(%1, %2, %3, 0.4)")
                                   .arg(baseColor.red())
                                   .arg(baseColor.green())
                                   .arg(baseColor.blue());
  const QString fullTextColor = baseColor.name();

  return QString("QToolButton {"
                 "  color: %1;"
                 "  background: transparent;"
                 "  border: none;"
                 "  border-radius: 4px;"
                 "  padding: 2px;"
                 "  font-size: 14px;"
                 "  font-weight: bold;"
                 "}"
                 "QToolButton:hover {"
                 "  color: %2;"
                 "  background: rgba(255, 255, 255, 0.15);"
                 "}"
                 "QToolButton:pressed {"
                 "  color: %2;"
                 "  background: %3;"
                 "}")
      .arg(subduedColor, fullTextColor, pressedColor);
}

QString Terminal::filterShellStartupNoise(const QString &text) const {
  if (text.isEmpty()) {
    return text;
  }

  const QStringList lines = text.split('\n');
  QStringList kept;
  kept.reserve(lines.size());
  bool hasNonEmpty = false;

  for (const QString &line : lines) {
    if (isShellStartupNoiseLine(line)) {
      continue;
    }
    kept.append(line);
    if (!line.isEmpty()) {
      hasNonEmpty = true;
    }
  }

  if (!hasNonEmpty) {
    return QString();
  }

  QString result = kept.join("\n");
  if (text.endsWith('\n') && !result.endsWith('\n')) {
    result.append('\n');
  }

  return result;
}

bool Terminal::isShellStartupNoiseLine(const QString &line) const {
  if (line.startsWith("bash: cannot set terminal process group")) {
    return true;
  }
  if (line.startsWith("bash: no job control in this shell")) {
    return true;
  }
  return false;
}

void Terminal::setShellProfile(const ShellProfile &profile) {
  if (!profile.isValid()) {
    return;
  }

  bool wasRunning = isRunning();
  QString oldName = m_shellProfile.name;

  m_shellProfile = profile;

  if (wasRunning) {
    stopShell();
    startShell(m_workingDirectory);
  }

  if (oldName != profile.name) {
    emit shellProfileChanged(profile.name);
  }
}

ShellProfile Terminal::shellProfile() const { return m_shellProfile; }

QStringList Terminal::availableShellProfiles() const {
  QStringList names;
  for (const ShellProfile &profile :
       ShellProfileManager::instance().availableProfiles()) {
    names.append(profile.name);
  }
  return names;
}

bool Terminal::setShellProfileByName(const QString &profileName) {
  ShellProfile profile =
      ShellProfileManager::instance().profileByName(profileName);
  if (profile.isValid()) {
    setShellProfile(profile);
    return true;
  }
  return false;
}

void Terminal::sendText(const QString &text, bool appendNewline) {
  if (!isRunning()) {
    appendOutput("Error: Shell not running.\n", true);
    return;
  }

  QString textToSend = text;
  if (appendNewline) {
    textToSend += "\n";
  }

  m_process->write(textToSend.toUtf8());
}

void Terminal::setScrollbackLines(int lines) {
  m_scrollbackLines = lines;
  enforceScrollbackLimit();
}

int Terminal::scrollbackLines() const { return m_scrollbackLines; }

void Terminal::setLinkDetectionEnabled(bool enabled) {
  m_linkDetectionEnabled = enabled;
}

bool Terminal::isLinkDetectionEnabled() const { return m_linkDetectionEnabled; }

void Terminal::onLinkActivated(const QString &link) {
  emit linkClicked(link);

  if (link.startsWith("http://") || link.startsWith("https://") ||
      link.startsWith("ftp://") || link.startsWith("file://")) {
    QDesktopServices::openUrl(QUrl(link));
  } else if (QFile::exists(link)) {

    emit linkClicked(link);
  }
}

void Terminal::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton &&
      (event->modifiers() & Qt::ControlModifier)) {
    QString link = getLinkAtPosition(event->pos());
    if (!link.isEmpty()) {
      onLinkActivated(link);
      event->accept();
      return;
    }
  }
  QWidget::mousePressEvent(event);
}

QString Terminal::getLinkAtPosition(const QPoint &pos) {
  if (!m_linkDetectionEnabled) {
    return QString();
  }

  QTextCursor cursor = ui->textEdit->cursorForPosition(pos);
  if (cursor.isNull()) {
    return QString();
  }

  cursor.movePosition(QTextCursor::StartOfBlock);
  cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
  QString lineText = cursor.selectedText();

  int posInLine = ui->textEdit->cursorForPosition(pos).positionInBlock();

  QRegularExpressionMatchIterator urlMatches = m_urlRegex.globalMatch(lineText);
  while (urlMatches.hasNext()) {
    QRegularExpressionMatch match = urlMatches.next();
    if (posInLine >= match.capturedStart() &&
        posInLine <= match.capturedEnd()) {
      return match.captured();
    }
  }

  QRegularExpressionMatchIterator pathMatches =
      m_filePathRegex.globalMatch(lineText);
  while (pathMatches.hasNext()) {
    QRegularExpressionMatch match = pathMatches.next();
    QString path = match.captured(1);
    int start = match.capturedStart(1);
    int end = match.capturedEnd(1);
    if (posInLine >= start && posInLine <= end && QFile::exists(path)) {
      return path;
    }
  }

  return QString();
}

QString Terminal::processTextForLinks(const QString &text) {
  if (!m_linkDetectionEnabled) {
    return text;
  }

  return text;
}

void Terminal::enforceScrollbackLimit() {
  if (m_scrollbackLines <= 0) {
    return;
  }

  QTextDocument *doc = ui->textEdit->document();
  int blockCount = doc->blockCount();

  if (blockCount > m_scrollbackLines) {
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);

    int linesToRemove = blockCount - m_scrollbackLines;
    for (int i = 0; i < linesToRemove; ++i) {
      cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
    }
    int removedLength = cursor.selectedText().length();
    cursor.removeSelectedText();

    m_inputStartPosition = qMax(0, m_inputStartPosition - removedLength);
  }
}

QString Terminal::stripAnsiEscapeCodes(const QString &text) {

  static QRegularExpression ansiRegex(R"(\x1b\[[0-9;?]*[A-Za-z])"
                                      R"(|\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)?)"

                                      R"(|\x1b[()][AB012])"
                                      R"(|\x1b[=>])"
                                      R"(|\x1b[DME78HcNO])"
                                      R"(|\x07)");

  QString result = text;
  result.remove(ansiRegex);

  QString processed;
  processed.reserve(result.size());
  for (const QChar &ch : result) {
    if (ch == '\x08') {
      if (!processed.isEmpty()) {
        processed.chop(1);
      }
    } else {
      processed.append(ch);
    }
  }

  return processed;
}
