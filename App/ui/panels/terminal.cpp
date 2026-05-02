#include "terminal.h"
#include "../../run_templates/runtemplatemanager.h"
#include "../../theme/themeengine.h"
#ifndef Q_OS_WIN
#include "terminalpty.h"
#endif
#include "terminalview.h"
#include "ui_terminal.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QUrl>

#ifndef Q_OS_WIN
#include <signal.h>
#include <unistd.h>
#endif

namespace {
QString rgba(const QColor &color, qreal alpha) {
  QColor c = color.isValid() ? color : QColor("#ffffff");
  return QString("rgba(%1, %2, %3, %4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(qBound(0.0, alpha, 1.0), 0, 'f', 3);
}

const QRegularExpression
    kYesNoPromptPattern(R"(\[y/n\]|\[Y/n\]|\[n/Y\]|\(yes/no\)|\(y/n\))",
                        QRegularExpression::CaseInsensitiveOption);

const QLatin1String kPromptSuffixes[] = {
    QLatin1String("? "),  QLatin1String(": "), QLatin1String("> "),
    QLatin1String("$ "),  QLatin1String("# "), QLatin1String(">>> "),
    QLatin1String("... ")};
} // namespace

Terminal::Terminal(QWidget *parent)
    : QWidget(parent), ui(new Ui::Terminal), m_process(nullptr),
#ifndef Q_OS_WIN
      m_shellPty(nullptr),
#endif
      m_runProcess(nullptr), m_restartTimer(nullptr), m_historyIndex(0),
      m_processRunning(false), m_shellStopRequested(false),
      m_restartShellAfterRun(false), m_autoRestartEnabled(true),
      m_restartAttempts(0), m_backgroundColor("#0e1116"),
      m_textColor("#e6edf3"), m_errorColor("#f44336"), m_linkColor("#58a6ff"),
      m_scrollbackLines(kDefaultScrollbackLines), m_linkDetectionEnabled(true),
      m_urlRegex(R"((https?://|ftp://|file://)[^\s<>\"\'\]\)]+)"),
      m_filePathRegex(R"((?:^|[\s:])(/[^\s:]+|[A-Za-z]:\\[^\s:]+))"),
      m_inputStartPosition(0), m_baseFontSize(kDefaultFontSize),
      m_contextMenu(nullptr), m_copyAction(nullptr), m_stopAction(nullptr),
      m_runInputHistoryIndex(0), m_runInputIndicator(nullptr),
      m_runInputIndicatorTimer(nullptr), m_runInputIndicatorActive(false),
      m_runInputCursorVisible(false), m_inputIndicatorDebounceTimer(nullptr) {
  ui->setupUi(this);

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
  ui->horizontalLayout_3->setContentsMargins(0, 0, 0, 0);
  ui->horizontalLayout_3->setSpacing(0);
  ui->cwdLabel->setMaximumHeight(0);
  ui->cwdLabel->hide();

  ui->textEdit->setReadOnly(false);
  ui->textEdit->setTextInteractionFlags(Qt::TextEditorInteraction);
  ui->textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
  ui->textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QFont monoFont;
  QStringList fontFamilies = {"JetBrains Mono",  "Cascadia Code", "Fira Code",
                              "Source Code Pro", "Consolas",      "Menlo",
                              "Monaco",          "Courier New",   "Monospace"};
  monoFont.setFamilies(fontFamilies);
  monoFont.setStyleHint(QFont::Monospace);
  monoFont.setPointSize(11);
  ui->textEdit->setFont(monoFont);
  ui->textEdit->setCursorWidth(2);

  ui->textEdit->installEventFilter(this);
  ui->textEdit->viewport()->installEventFilter(this);

  setupContextMenu();

  ui->cwdLabel->setFont(monoFont);
  updateCwdLabel();
  setupInputIndicator();

  updateStyleSheet();

  startShell();
}

QTextCursor Terminal::clampedInputCursor(bool moveToEndWhenOutsideInput) const {
  QTextCursor currentCursor = ui->textEdit->textCursor();
  QTextCursor endCursor = currentCursor;
  endCursor.movePosition(QTextCursor::End);
  int endPosition = endCursor.position();

  int anchor = currentCursor.anchor();
  int position = currentCursor.position();
  int selectionEnd = qMax(anchor, position);

  if (moveToEndWhenOutsideInput && selectionEnd <= m_inputStartPosition) {
    anchor = endPosition;
    position = endPosition;
  }

  anchor = qBound(m_inputStartPosition, anchor, endPosition);
  position = qBound(m_inputStartPosition, position, endPosition);

  QTextCursor clampedCursor(ui->textEdit->document());
  clampedCursor.setPosition(anchor);
  clampedCursor.setPosition(position, QTextCursor::KeepAnchor);
  return clampedCursor;
}

void Terminal::insertInputText(const QString &text) {
  if (text.isEmpty()) {
    return;
  }

  QTextCursor cursor = clampedInputCursor(true);
  cursor.insertText(text);
  ui->textEdit->setTextCursor(cursor);
  scrollToBottom();
}

void Terminal::removeInputText(bool backwards) {
  QTextCursor originalCursor = ui->textEdit->textCursor();
  int selectionEnd = qMax(originalCursor.anchor(), originalCursor.position());
  if (!originalCursor.hasSelection() &&
      originalCursor.position() < m_inputStartPosition) {
    return;
  }
  if (originalCursor.hasSelection() && selectionEnd <= m_inputStartPosition) {
    return;
  }

  QTextCursor cursor = clampedInputCursor(false);
  if (cursor.hasSelection()) {
    cursor.removeSelectedText();
    ui->textEdit->setTextCursor(cursor);
    return;
  }

  if (backwards) {
    if (cursor.position() <= m_inputStartPosition) {
      return;
    }
    cursor.deletePreviousChar();
  } else {
    cursor.deleteChar();
  }

  ui->textEdit->setTextCursor(cursor);
}

QString Terminal::takePendingInput() {
  QTextCursor cursor(ui->textEdit->document());
  cursor.movePosition(QTextCursor::End);
  int endPos = cursor.position();
  if (endPos <= m_inputStartPosition) {
    return QString();
  }
  cursor.setPosition(m_inputStartPosition);
  cursor.setPosition(endPos, QTextCursor::KeepAnchor);
  QString pending = cursor.selectedText();
  cursor.removeSelectedText();
  return pending;
}

bool Terminal::handleCommonInputKey(QKeyEvent *keyEvent) {
  const Qt::KeyboardModifiers mods = keyEvent->modifiers();
  const bool ctrl = mods & Qt::ControlModifier;
  const bool shift = mods & Qt::ShiftModifier;
  const QTextCursor::MoveMode sel =
      shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor;

  switch (keyEvent->key()) {
  case Qt::Key_Home: {
    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.setPosition(m_inputStartPosition, sel);
    ui->textEdit->setTextCursor(cursor);
    return true;
  }
  case Qt::Key_End: {
    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.movePosition(QTextCursor::End, sel);
    ui->textEdit->setTextCursor(cursor);
    return true;
  }
  case Qt::Key_Left: {
    QTextCursor cursor = ui->textEdit->textCursor();
    if (!shift && cursor.hasSelection()) {
      int pos = qMin(cursor.position(), cursor.anchor());
      cursor.setPosition(qMax(pos, m_inputStartPosition));
      ui->textEdit->setTextCursor(cursor);
      return true;
    }
    if (cursor.position() <= m_inputStartPosition && !shift) {
      return true;
    }
    if (ctrl) {
      cursor.movePosition(QTextCursor::WordLeft, sel);
    } else {
      cursor.movePosition(QTextCursor::Left, sel);
    }
    if (cursor.position() < m_inputStartPosition && !shift) {
      cursor.setPosition(m_inputStartPosition);
    }
    ui->textEdit->setTextCursor(cursor);
    return true;
  }
  case Qt::Key_Right: {
    QTextCursor cursor = ui->textEdit->textCursor();
    if (!shift && cursor.hasSelection()) {
      int pos = qMax(cursor.position(), cursor.anchor());
      cursor.setPosition(pos);
      ui->textEdit->setTextCursor(cursor);
      return true;
    }
    if (ctrl) {
      cursor.movePosition(QTextCursor::WordRight, sel);
    } else {
      cursor.movePosition(QTextCursor::Right, sel);
    }
    ui->textEdit->setTextCursor(cursor);
    return true;
  }

  case Qt::Key_C:
    if (ctrl && shift) {
      if (ui->textEdit->textCursor().hasSelection()) {
        ui->textEdit->copy();
      }
      return true;
    }
    if (ctrl)
      return false;
    break;

  case Qt::Key_V:
    if (ctrl && shift) {
      insertInputText(QApplication::clipboard()->text());
      return true;
    }
    if (ctrl)
      return false;
    break;

  case Qt::Key_X:
    if (ctrl)
      return true;
    break;

  case Qt::Key_L:
    if (ctrl) {
      clear();
      return true;
    }
    break;

  case Qt::Key_A:
    if (ctrl) {
      QTextCursor cursor = ui->textEdit->textCursor();
      cursor.setPosition(m_inputStartPosition);
      ui->textEdit->setTextCursor(cursor);
      return true;
    }
    break;

  case Qt::Key_E:
    if (ctrl) {
      ui->textEdit->moveCursor(QTextCursor::End);
      return true;
    }
    break;

  case Qt::Key_W:
    if (ctrl) {
      QTextCursor cursor = ui->textEdit->textCursor();
      if (cursor.position() <= m_inputStartPosition) {
        return true;
      }
      cursor.movePosition(QTextCursor::WordLeft, QTextCursor::KeepAnchor);
      if (cursor.position() < m_inputStartPosition) {
        cursor.setPosition(cursor.anchor());
        cursor.setPosition(m_inputStartPosition, QTextCursor::KeepAnchor);

        int a = cursor.anchor(), p = cursor.position();
        cursor.setPosition(p);
        cursor.setPosition(a, QTextCursor::KeepAnchor);
      }
      cursor.removeSelectedText();
      ui->textEdit->setTextCursor(cursor);
      return true;
    }
    break;

  case Qt::Key_U:
    if (ctrl) {
      QTextCursor cursor = ui->textEdit->textCursor();
      int pos = cursor.position();
      if (pos <= m_inputStartPosition) {
        return true;
      }
      cursor.setPosition(m_inputStartPosition);
      cursor.setPosition(pos, QTextCursor::KeepAnchor);
      cursor.removeSelectedText();
      ui->textEdit->setTextCursor(cursor);
      return true;
    }
    break;

  case Qt::Key_K:
    if (ctrl) {
      QTextCursor cursor = ui->textEdit->textCursor();
      cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
      if (cursor.hasSelection()) {
        cursor.removeSelectedText();
      }
      ui->textEdit->setTextCursor(cursor);
      return true;
    }
    break;

  case Qt::Key_Plus:
  case Qt::Key_Equal:
    if (ctrl) {
      zoomIn();
      return true;
    }
    break;

  case Qt::Key_Minus:
    if (ctrl) {
      zoomOut();
      return true;
    }
    break;

  case Qt::Key_0:
    if (ctrl) {
      zoomReset();
      return true;
    }
    break;

  case Qt::Key_Backspace:
    removeInputText(true);
    return true;

  case Qt::Key_Delete:
    removeInputText(false);
    return true;

  default:
    break;
  }

  const QString keyText = keyEvent->text();
  bool hasEditableText = false;
  for (const QChar &ch : keyText) {
    if (!ch.isNull() && ch != '\n' && ch != '\r' && ch >= QChar(' ')) {
      hasEditableText = true;
      break;
    }
  }
  if (hasEditableText) {
    insertInputText(keyText);
    return true;
  }

  return false;
}

bool Terminal::isPtyShellActive() const {
#ifndef Q_OS_WIN
  return m_shellPty && m_shellPty->isRunning();
#else
  return false;
#endif
}

void Terminal::writeToShell(const QByteArray &data) {
  if (data.isEmpty()) {
    return;
  }
#ifndef Q_OS_WIN
  if (m_shellPty && m_shellPty->isRunning()) {
    m_shellPty->writeData(data);
    return;
  }
#endif
  if (m_process && m_process->state() == QProcess::Running) {
    m_process->write(data);
  }
}

bool Terminal::handlePtyKeyPress(QKeyEvent *keyEvent) {
  const Qt::KeyboardModifiers mods = keyEvent->modifiers();
  const bool ctrl = mods & Qt::ControlModifier;
  const bool shift = mods & Qt::ShiftModifier;

  if (ctrl && shift && keyEvent->key() == Qt::Key_C) {
    if (ui->textEdit->textCursor().hasSelection()) {
      ui->textEdit->copy();
    }
    return true;
  }

  if (ctrl && shift && keyEvent->key() == Qt::Key_V) {
    writeToShell(QApplication::clipboard()->text().toUtf8());
    return true;
  }

  if (ctrl) {
    switch (keyEvent->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
      zoomIn();
      return true;
    case Qt::Key_Minus:
      zoomOut();
      return true;
    case Qt::Key_0:
      zoomReset();
      return true;
    case Qt::Key_C:
      interruptActiveProcess();
      return true;
    case Qt::Key_D:
      writeToShell(QByteArray(1, '\x04'));
      return true;
    case Qt::Key_L:
      writeToShell(QByteArray(1, '\x0c'));
      return true;
    case Qt::Key_A:
      writeToShell(QByteArray(1, '\x01'));
      return true;
    case Qt::Key_E:
      writeToShell(QByteArray(1, '\x05'));
      return true;
    case Qt::Key_K:
      writeToShell(QByteArray(1, '\x0b'));
      return true;
    case Qt::Key_U:
      writeToShell(QByteArray(1, '\x15'));
      return true;
    case Qt::Key_W:
      writeToShell(QByteArray(1, '\x17'));
      return true;
    default:
      break;
    }
  }

  QByteArray sequence;
  switch (keyEvent->key()) {
  case Qt::Key_Return:
  case Qt::Key_Enter:
    sequence = "\r";
    break;
  case Qt::Key_Backspace:
    sequence = "\x7f";
    break;
  case Qt::Key_Tab:
    sequence = "\t";
    break;
  case Qt::Key_Escape:
    sequence = "\x1b";
    break;
  case Qt::Key_Up:
    sequence = "\x1b[A";
    break;
  case Qt::Key_Down:
    sequence = "\x1b[B";
    break;
  case Qt::Key_Right:
    sequence = "\x1b[C";
    break;
  case Qt::Key_Left:
    sequence = "\x1b[D";
    break;
  case Qt::Key_Home:
    sequence = "\x1b[H";
    break;
  case Qt::Key_End:
    sequence = "\x1b[F";
    break;
  case Qt::Key_Delete:
    sequence = "\x1b[3~";
    break;
  case Qt::Key_PageUp:
    sequence = "\x1b[5~";
    break;
  case Qt::Key_PageDown:
    sequence = "\x1b[6~";
    break;
  default:
    break;
  }

  if (!sequence.isEmpty()) {
    writeToShell(sequence);
    return true;
  }

  const QString text = keyEvent->text();
  if (!text.isEmpty()) {
    writeToShell(text.toUtf8());
    return true;
  }

  return true;
}

void Terminal::updatePtySize() {
#ifndef Q_OS_WIN
  if (!m_shellPty || !ui || !ui->textEdit) {
    return;
  }

  QFontMetrics metrics(ui->textEdit->font());
  int charWidth = qMax(1, metrics.horizontalAdvance(QLatin1Char('M')));
  int lineHeight = qMax(1, metrics.lineSpacing());
  const QRect viewport = ui->textEdit->viewport()->rect();
  int columns = qMax(20, viewport.width() / charWidth);
  int rows = qMax(4, viewport.height() / lineHeight);
  m_shellPty->resize(columns, rows);
#endif
}

void Terminal::handleRunInputHistoryNavigation(bool up) {
  if (m_runInputHistory.isEmpty()) {
    return;
  }

  if (up) {
    if (m_runInputHistoryIndex > 0) {
      m_runInputHistoryIndex--;
    } else {
      return;
    }
  } else {
    if (m_runInputHistoryIndex < m_runInputHistory.size()) {
      m_runInputHistoryIndex++;
    } else {
      return;
    }
  }

  QTextCursor cursor = ui->textEdit->textCursor();
  cursor.movePosition(QTextCursor::End);
  cursor.setPosition(m_inputStartPosition, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();

  if (m_runInputHistoryIndex < m_runInputHistory.size()) {
    cursor.insertText(m_runInputHistory.at(m_runInputHistoryIndex));
  }
  ui->textEdit->setTextCursor(cursor);
}

QColor Terminal::ansi256Color(int index) {
  static const QColor standard[16] = {
      QColor("#282c34"), QColor("#e06c75"), QColor("#98c379"),
      QColor("#e5c07b"), QColor("#61afef"), QColor("#c678dd"),
      QColor("#56b6c2"), QColor("#abb2bf"), QColor("#5c6370"),
      QColor("#be5046"), QColor("#7ec16e"), QColor("#d19a66"),
      QColor("#4d78cc"), QColor("#b070cf"), QColor("#49a5b0"),
      QColor("#ffffff"),
  };
  if (index >= 0 && index < 16) {
    return standard[index];
  }
  if (index < 232) {
    int n = index - 16;
    int b = n % 6;
    int g = (n / 6) % 6;
    int r = n / 36;
    auto toVal = [](int c) { return c == 0 ? 0 : 55 + c * 40; };
    return QColor(toVal(r), toVal(g), toVal(b));
  }
  if (index <= 255) {
    int gray = 8 + (index - 232) * 10;
    return QColor(gray, gray, gray);
  }
  return QColor();
}

bool Terminal::startShell(const QString &workingDirectory) {

  if (m_restartTimer && m_restartTimer->isActive()) {
    m_restartTimer->stop();
  }

#ifndef Q_OS_WIN
  if (m_shellPty && m_shellPty->isRunning()) {
    return true;
  }
#endif

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

#ifndef Q_OS_WIN
  if (!m_shellPty) {
    m_shellPty = new TerminalPty(this);
    connect(m_shellPty, &TerminalPty::readyRead, this,
            &Terminal::onPtyReadyRead);
    connect(m_shellPty, &TerminalPty::finished, this, &Terminal::onPtyFinished);
    connect(m_shellPty, &TerminalPty::errorOccurred, this,
            &Terminal::onPtyError);
  }

  QMap<QString, QString> ptyEnv = m_shellProfile.environment;
  ptyEnv.insert("TERM", "xterm-256color");
  ptyEnv.insert("COLORTERM", "truecolor");
  ptyEnv.insert("LIGHTPAD_TERMINAL", "1");

  QString shell = getShellCommand();
  QStringList args = getShellArguments();
  if (!m_shellPty->start(shell, args, m_workingDirectory, ptyEnv)) {
    appendOutput("Error: Failed to start PTY shell process.\n", true);
    emit errorOccurred("Failed to start shell");
    return false;
  }

  m_processRunning = true;
  m_restartAttempts = 0;
  ui->textEdit->setReadOnly(false);
  updatePtySize();
  emit shellStarted();
  return true;
#else
  m_process = new QProcess(this);

  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &Terminal::onReadyReadStandardOutput);
  connect(m_process, &QProcess::readyReadStandardError, this,
          &Terminal::onReadyReadStandardError);
  connect(m_process, &QProcess::errorOccurred, this, &Terminal::onProcessError);
  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &Terminal::onProcessFinished);

  m_process->setWorkingDirectory(m_workingDirectory);

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("TERM", "xterm-256color");
  env.insert("COLORTERM", "truecolor");
  env.insert("LIGHTPAD_TERMINAL", "1");
  for (auto it = m_shellProfile.environment.constBegin();
       it != m_shellProfile.environment.constEnd(); ++it) {
    env.insert(it.key(), it.value());
  }
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
#endif
}

void Terminal::stopShell() {

  if (m_restartTimer) {
    m_restartTimer->stop();
  }

  if (!m_process) {
#ifndef Q_OS_WIN
    if (m_shellPty) {
      m_shellStopRequested = true;
      m_shellPty->stop();
      m_shellStopRequested = false;
    }
#endif
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
#ifndef Q_OS_WIN
  if (m_shellPty && m_shellPty->isRunning()) {
    return true;
  }
#endif
  return m_processRunning && m_process &&
         m_process->state() == QProcess::Running;
}

bool Terminal::hasActiveRunProcess() const {
  return m_runProcess && m_runProcess->state() != QProcess::NotRunning;
}

bool Terminal::canInterruptActiveProcess() const {
  return hasActiveRunProcess() || isRunning();
}

bool Terminal::interruptActiveProcess() {
  if (hasActiveRunProcess()) {
    stopProcess();
    return true;
  }

  if (!isRunning() || !m_process) {
    return false;
  }

  bool interrupted = false;

#ifndef Q_OS_WIN
  if (m_shellPty && m_shellPty->isRunning()) {
    interrupted = m_shellPty->interruptProcessGroup();
  } else {
    const qint64 processId = m_process->processId();
    if (processId > 0) {
      interrupted = (::kill(-static_cast<pid_t>(processId), SIGINT) == 0);
    }
  }
#endif

  if (!interrupted) {
    writeToShell(QByteArray(1, '\x03'));
    interrupted = true;
  }

  if (interrupted) {
    appendOutput("^C\n");
  }

  return interrupted;
}

qint64 Terminal::runProcessId() const {
  return m_runProcess ? m_runProcess->processId() : 0;
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
  writeToShell(cmdWithNewline.toUtf8());
}

void Terminal::setWorkingDirectory(const QString &directory) {
  m_workingDirectory = directory;
  updateCwdLabel();
  if (isRunning()) {

    executeCommand(QString("cd \"%1\"").arg(directory));
  }
}

void Terminal::clear() {
  ui->textEdit->clear();
  m_inputStartPosition = 0;
  if (isRunning() &&
      !(m_runProcess && m_runProcess->state() != QProcess::NotRunning)) {
    if (isPtyShellActive()) {
      writeToShell(QByteArray(1, '\x0c'));
    } else {
      appendPrompt();
    }
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
  connect(m_runProcess, &QProcess::started, this, [this]() {
    ui->textEdit->setFocus(Qt::OtherFocusReason);
    emit processStarted();
  });
  connect(m_runProcess,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &Terminal::onRunProcessFinished);
  connect(m_runProcess, &QProcess::errorOccurred, this,
          &Terminal::onRunProcessError);

  m_runProcess->setWorkingDirectory(workingDirectory);
  m_runInputHistory.clear();
  m_runInputHistoryIndex = 0;
  m_runProcessTimer.start();

  QProcessEnvironment processEnv = QProcessEnvironment::systemEnvironment();
  for (auto it = env.begin(); it != env.end(); ++it) {
    processEnv.insert(it.key(), it.value());
  }
  m_runProcess->setProcessEnvironment(processEnv);

  clear();
  setRunInputIndicatorActive(false);
  appendOutput(QString("$ %1 %2\n").arg(command, args.join(" ")));
  if (!workingDirectory.isEmpty()) {
    appendOutput(QString("Working directory: %1\n").arg(workingDirectory));
  }
  if (!m_pythonEnvironmentBanner.isEmpty()) {
    appendOutput(m_pythonEnvironmentBanner);
    m_pythonEnvironmentBanner.clear();
  }
  appendOutput("\n");

  m_runProcess->start(command, args);
  if (m_runProcess->state() == QProcess::NotRunning) {
    appendOutput(QString("\nError: Failed to start process '%1': %2\n")
                     .arg(command, m_runProcess->errorString()),
                 true);
    emit processError(m_runProcess->errorString());
    cleanupRunProcess(true);
  }
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

  const QString ext = QFileInfo(filePath).suffix().toLower();
  if (ext == "py" || ext == "pyw" || ext == "pyi") {
    FileTemplateAssignment assignment = manager.getAssignmentForFile(filePath);
    PythonEnvironmentPreference preference;
    preference.mode = assignment.pythonMode;
    preference.customInterpreter = assignment.pythonInterpreter;
    preference.venvPath = assignment.pythonVenvPath;
    preference.requirementsFile = assignment.pythonRequirementsFile;

    const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
        preference, manager.workspaceFolder(), filePath, workingDir);
    m_pythonEnvironmentBanner = formatPythonBanner(info);

    env.insert("PYTHONUNBUFFERED", "1");
  } else {
    m_pythonEnvironmentBanner.clear();
  }

  executeCommand(command.first, command.second, workingDir, env);
  return true;
}

void Terminal::stopProcess() {
  if (!hasActiveRunProcess()) {
    return;
  }

  appendOutput("\nProcess stopped by user.\n");
  cleanupRunProcess(true);
  emit processFinished(130);
}

void Terminal::cleanupRunProcess(bool restartShell) {
  setRunInputIndicatorActive(false);

  if (m_inputIndicatorDebounceTimer) {
    m_inputIndicatorDebounceTimer->stop();
  }
  m_lastRunProcessOutput.clear();

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

void Terminal::setupInputIndicator() {
  if (m_runInputIndicator) {
    return;
  }

  m_runInputIndicator = new QLabel(this);
  m_runInputIndicator->setObjectName("runInputIndicator");
  m_runInputIndicator->setAlignment(Qt::AlignCenter);
  m_runInputIndicator->setSizePolicy(QSizePolicy::Fixed,
                                     QSizePolicy::Preferred);
  m_runInputIndicator->setText(tr("Input ready |"));
  m_runInputIndicator->setMinimumWidth(
      m_runInputIndicator->fontMetrics().horizontalAdvance(
          tr("Input ready |")) +
      18);
  m_runInputIndicator->setVisible(false);
  ui->horizontalLayout_3->addWidget(m_runInputIndicator);

  m_runInputIndicatorTimer = new QTimer(this);
  m_runInputIndicatorTimer->setInterval(kInputIndicatorBlinkMs);
  connect(m_runInputIndicatorTimer, &QTimer::timeout, this, [this]() {
    m_runInputCursorVisible = !m_runInputCursorVisible;
    updateRunInputIndicator();
  });

  m_inputIndicatorDebounceTimer = new QTimer(this);
  m_inputIndicatorDebounceTimer->setSingleShot(true);
  m_inputIndicatorDebounceTimer->setInterval(kInputIndicatorDebounceMs);
  connect(m_inputIndicatorDebounceTimer, &QTimer::timeout, this, [this]() {
    if (m_runProcess && m_runProcess->state() != QProcess::NotRunning) {
      setRunInputIndicatorActive(looksLikeInputPrompt(m_lastRunProcessOutput));
    }
  });
}

void Terminal::setRunInputIndicatorActive(bool active) {
  if (!m_runInputIndicator || !m_runInputIndicatorTimer) {
    return;
  }

  m_runInputIndicatorActive = active;
  m_runInputCursorVisible = true;
  updateRunInputIndicator();

  if (active) {
    if (!m_runInputIndicatorTimer->isActive()) {
      m_runInputIndicatorTimer->start();
    }
  } else {
    m_runInputIndicatorTimer->stop();
  }
}

void Terminal::updateRunInputIndicator() {
  if (!m_runInputIndicator) {
    return;
  }

  m_runInputIndicator->setVisible(m_runInputIndicatorActive);
  if (!m_runInputIndicatorActive) {
    return;
  }

  const QString cursor = m_runInputCursorVisible ? "|" : " ";
  m_runInputIndicator->setText(tr("Input ready %1").arg(cursor));
}

bool Terminal::looksLikeInputPrompt(const QString &text) {
  if (text.isEmpty()) {
    return false;
  }

  const QString stripped = stripAnsiEscapeCodes(text);
  if (stripped.isEmpty()) {
    return false;
  }

  if (!stripped.endsWith('\n') && !stripped.endsWith('\r')) {
    return true;
  }

  const QStringList lines = stripped.split('\n');
  QString lastLine;
  for (int i = lines.size() - 1; i >= 0; --i) {
    const QString &candidate = lines.at(i);
    if (!candidate.trimmed().isEmpty()) {
      lastLine = candidate;
      break;
    }
  }

  if (lastLine.isEmpty()) {
    return false;
  }

  if (kYesNoPromptPattern.match(lastLine).hasMatch()) {
    return true;
  }

  for (const QLatin1String &suffix : kPromptSuffixes) {
    if (lastLine.endsWith(suffix)) {
      return true;
    }
  }

  return false;
}

void Terminal::onRunProcessReadyReadStdout() {
  if (m_runProcess) {
    QString output = QString::fromUtf8(m_runProcess->readAllStandardOutput());
    QString pending = takePendingInput();

    if (m_runProcess->state() != QProcess::NotRunning &&
        m_inputIndicatorDebounceTimer) {
      m_lastRunProcessOutput = output;
      m_inputIndicatorDebounceTimer->start();
    }
    appendOutput(output);
    if (!pending.isEmpty()) {
      insertInputText(pending);
    }
  }
}

void Terminal::onRunProcessReadyReadStderr() {
  if (m_runProcess) {
    QString output = QString::fromUtf8(m_runProcess->readAllStandardError());
    QString pending = takePendingInput();

    appendOutput(output, true);
    if (!pending.isEmpty()) {
      insertInputText(pending);
    }
  }
}

void Terminal::onRunProcessFinished(int exitCode,
                                    QProcess::ExitStatus exitStatus) {
  qint64 elapsedMs = m_runProcessTimer.elapsed();
  QString elapsed;
  if (elapsedMs < 1000) {
    elapsed = QString("%1ms").arg(elapsedMs);
  } else {
    double secs = elapsedMs / 1000.0;
    elapsed = QString("%1s").arg(secs, 0, 'f', secs < 10.0 ? 2 : 1);
  }

  QTextCursor cursor = ui->textEdit->textCursor();
  cursor.movePosition(QTextCursor::End);
  cursor.insertText("\n\n");

  QTextCharFormat codeFmt;
  if (exitStatus == QProcess::CrashExit) {
    codeFmt.setForeground(QColor(m_errorColor));
    cursor.insertText(QString("Process crashed (exit code: %1)").arg(exitCode),
                      codeFmt);
  } else {
    QColor successColor = QColor(m_textColor).lighter(110);
    codeFmt.setForeground(exitCode == 0 ? successColor : QColor(m_errorColor));
    cursor.insertText(
        QString("Process finished with exit code %1").arg(exitCode), codeFmt);
  }

  QTextCharFormat dimFmt;
  dimFmt.setForeground(QColor(m_textColor).darker(160));
  cursor.insertText(QString("  [%1]\n").arg(elapsed), dimFmt);

  m_inputStartPosition = cursor.position();
  ui->textEdit->setTextCursor(cursor);
  scrollToBottom();

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

  if (error == QProcess::FailedToStart) {
    cleanupRunProcess(true);
  }
}

void Terminal::onReadyReadStandardOutput() {
  if (!m_process) {
    return;
  }

  QByteArray data = m_process->readAllStandardOutput();
  QString output = QString::fromLocal8Bit(data);

  appendOutput(output);
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

#ifndef Q_OS_WIN
void Terminal::onPtyReadyRead(const QByteArray &data) {
  QString output = QString::fromLocal8Bit(data);
  appendOutput(output);
}

void Terminal::onPtyFinished(int exitCode, bool crashed) {
  m_processRunning = false;
  if (m_shellStopRequested) {
    emit shellFinished(exitCode);
    return;
  }
  if (crashed) {
    appendOutput(QString("\nShell crashed (exit code: %1)\n").arg(exitCode),
                 true);
    if (m_autoRestartEnabled) {
      scheduleAutoRestart();
    }
  } else {
    appendOutput(QString("\nShell exited with code: %1\n").arg(exitCode));
  }
  emit shellFinished(exitCode);
}

void Terminal::onPtyError(const QString &message) {
  m_processRunning = false;
  appendOutput(QString("Error: %1\n").arg(message), true);
  emit errorOccurred(message);
}
#endif

void Terminal::onInputSubmitted() {
  if (!m_currentInput.isEmpty()) {
    executeCommand(m_currentInput);
    m_currentInput.clear();
  }
}

bool Terminal::eventFilter(QObject *obj, QEvent *event) {
  const bool terminalTextObject =
      obj == ui->textEdit || obj == ui->textEdit->viewport();

  if (terminalTextObject && event->type() == QEvent::MouseButtonRelease) {
    const bool protectedInputSurface =
        isPtyShellActive() ||
        (m_runProcess && m_runProcess->state() != QProcess::NotRunning);
    if (protectedInputSurface && !ui->textEdit->textCursor().hasSelection()) {
      ui->textEdit->moveCursor(QTextCursor::End);
      return true;
    }
  }

  if (terminalTextObject && event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    if (isPtyShellActive() &&
        !(m_runProcess && m_runProcess->state() != QProcess::NotRunning)) {
      return handlePtyKeyPress(keyEvent);
    }

    if (handleCommonInputKey(keyEvent)) {
      return true;
    }

    if (m_runProcess && m_runProcess->state() != QProcess::NotRunning) {

      switch (keyEvent->key()) {
      case Qt::Key_Return:
      case Qt::Key_Enter: {
        QTextCursor cursor = ui->textEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.setPosition(m_inputStartPosition, QTextCursor::KeepAnchor);
        QString userInput = cursor.selectedText();

        ui->textEdit->moveCursor(QTextCursor::End);
        ui->textEdit->insertPlainText("\n");
        m_inputStartPosition = ui->textEdit->textCursor().position();

        if (!userInput.isEmpty()) {
          m_runInputHistory.append(userInput);
        }
        m_runInputHistoryIndex = m_runInputHistory.size();

        m_runProcess->write((userInput + "\n").toUtf8());
        return true;
      }
      case Qt::Key_C:
        if (keyEvent->modifiers() & Qt::ControlModifier) {
          stopProcess();
          return true;
        }
        break;
      case Qt::Key_D:
        if (keyEvent->modifiers() & Qt::ControlModifier) {
          m_runProcess->closeWriteChannel();
          setRunInputIndicatorActive(false);
          return true;
        }
        break;
      case Qt::Key_Up:
        handleRunInputHistoryNavigation(true);
        return true;
      case Qt::Key_Down:
        handleRunInputHistoryNavigation(false);
        return true;
      default:
        break;
      }
      return QWidget::eventFilter(obj, event);
    }

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
          writeToShell("\n");
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
      if (keyEvent->modifiers() & Qt::ControlModifier) {
        interruptActiveProcess();
        return true;
      }
      break;

    case Qt::Key_D:
      if (keyEvent->modifiers() & Qt::ControlModifier) {
        if (isRunning()) {
          writeToShell(QByteArray(1, '\x04'));
        }
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

  if (text.isEmpty()) {
    return;
  }

  QTextCursor cursor = ui->textEdit->textCursor();
  cursor.movePosition(QTextCursor::End);

  if (isError) {

    QString cleanText = stripAnsiEscapeCodes(text);
    if (cleanText.isEmpty()) {
      return;
    }
    QTextCharFormat errorFormat;
    errorFormat.setForeground(QColor(m_errorColor));
    cursor.insertText(cleanText, errorFormat);
  } else {

    appendAnsiText(text, cursor);
  }

  ui->textEdit->setTextCursor(cursor);
  scrollToBottom();

  enforceScrollbackLimit();

  m_inputStartPosition = ui->textEdit->textCursor().position();
}

void Terminal::appendPrompt() {
  QTextCursor cursor = ui->textEdit->textCursor();
  cursor.movePosition(QTextCursor::End);

  const ThemeDefinition &td = ThemeEngine::instance().activeTheme();
  QColor accent = td.colors.accentPrimary;
  QColor muted = td.colors.textMuted;
  QColor path = td.colors.syntaxString.isValid() ? td.colors.syntaxString
                                                 : td.colors.accentPrimary;
  QColor git = td.colors.statusWarning;

  QString host = qEnvironmentVariable("HOSTNAME", QString());
  if (host.isEmpty()) {
    QProcess hp;
    hp.start("hostname", {});
    if (hp.waitForFinished(80))
      host = QString::fromUtf8(hp.readAllStandardOutput()).trimmed();
  }
  if (host.isEmpty())
    host = "localhost";
  QString userHost = QString("%1@%2").arg(
      qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME", "user")),
      host.split('.').first());

  QString home = QDir::homePath();
  QString shownPath = m_workingDirectory;
  if (shownPath.startsWith(home))
    shownPath.replace(0, home.length(), "~");

  QString gitBranch;
  {
    QProcess proc;
    proc.setWorkingDirectory(m_workingDirectory);
    proc.start("git", {"symbolic-ref", "--short", "HEAD"});
    if (proc.waitForFinished(120) && proc.exitCode() == 0) {
      gitBranch = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    }
  }

  auto seg = [&](const QString &text, const QColor &fg, bool bold = false) {
    QTextCharFormat f;
    f.setForeground(fg);
    if (bold) {
      QFont ft = f.font();
      ft.setBold(true);
      f.setFont(ft);
    }
    cursor.insertText(text, f);
  };

  QTextCharFormat plain;
  plain.setForeground(muted);

  if (cursor.position() > 0) {
    QTextCursor pc = cursor;
    pc.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
    if (pc.selectedText() != QChar(0x2029) && pc.selectedText() != "\n")
      cursor.insertText("\n", plain);
  }

  seg("╭─", muted);
  seg(" ", muted);
  seg(userHost, accent, true);
  seg(" ", muted);
  seg("", muted);
  seg(" ", muted);
  seg(shownPath, path, true);
  if (!gitBranch.isEmpty()) {
    seg(" ", muted);
    seg("", muted);
    seg(" ", muted);
    seg("⎇ " + gitBranch, git, true);
  }
  seg("\n", muted);
  seg("╰─", muted);
  seg("❯ ", accent, true);

  ui->textEdit->setTextCursor(cursor);
  scrollToBottom();

  m_inputStartPosition = ui->textEdit->textCursor().position();
  updateCwdLabel();
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
  const ThemeDefinition &td = ThemeEngine::instance().activeTheme();
  if (!backgroundColor.isEmpty()) {
    m_backgroundColor =
        td.colors.termBg.isValid() ? td.colors.termBg.name() : backgroundColor;
  }
  if (!textColor.isEmpty()) {
    m_textColor =
        td.colors.termFg.isValid() ? td.colors.termFg.name() : textColor;
  }
  if (!errorColor.isEmpty()) {
    m_errorColor = errorColor;
  } else if (td.colors.statusError.isValid()) {
    m_errorColor = td.colors.statusError.name();
  }
  if (td.colors.textLink.isValid()) {
    m_linkColor = td.colors.textLink.name();
  } else if (td.colors.accentPrimary.isValid()) {
    m_linkColor = td.colors.accentPrimary.name();
  }

  updateStyleSheet();
}

void Terminal::updateStyleSheet() {
  const ThemeDefinition &td = ThemeEngine::instance().activeTheme();
  const ThemeColors &colors = td.colors;
  QColor bg = colors.termBg.isValid()
                  ? colors.termBg
                  : (colors.surfaceBase.isValid() ? colors.surfaceBase
                                                  : QColor(m_backgroundColor));
  QColor fg = colors.termFg.isValid() ? colors.termFg : QColor(m_textColor);
  QColor accent = colors.termCursor.isValid()
                      ? colors.termCursor
                      : (colors.accentPrimary.isValid() ? colors.accentPrimary
                                                        : QColor(m_linkColor));
  QColor selection =
      colors.termSelection.isValid()
          ? colors.termSelection
          : (colors.inputSelection.isValid() ? colors.inputSelection
                                             : QColor(m_linkColor));
  QColor border = colors.borderSubtle.isValid()
                      ? colors.borderSubtle
                      : (colors.borderDefault.isValid() ? colors.borderDefault
                                                        : bg.lighter(160));
  QColor glow = colors.accentGlow.isValid() ? colors.accentGlow : accent;
  QColor scrollTrack = colors.scrollTrack.isValid() ? colors.scrollTrack : bg;
  QColor scrollThumb =
      colors.scrollThumb.isValid() ? colors.scrollThumb : border;
  QColor scrollThumbHover =
      colors.scrollThumbHover.isValid() ? colors.scrollThumbHover : accent;

  m_backgroundColor = bg.name();
  m_textColor = fg.name();
  m_linkColor = accent.name();

  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QString("QWidget#Terminal {"
                        "  background-color: %1;"
                        "  border: none;"
                        "  margin: 0;"
                        "  padding: 0;"
                        "}")
                    .arg(bg.name()));

  ui->textEdit->setVisualTheme(bg, fg, accent, selection, border, glow,
                               td.ui.scanlineEffect,
                               ThemeEngine::instance().glowIntensity());

  QString styleSheet =
      QString("QPlainTextEdit {"
              "  background-color: %1;"
              "  color: %2;"
              "  selection-background-color: %5;"
              "  selection-color: %2;"
              "  border: none;"
              "  padding: 0;"
              "}"
              "QPlainTextEdit QScrollBar:vertical {"
              "  background: %6;"
              "  width: 10px;"
              "  margin: 0;"
              "}"
              "QPlainTextEdit QScrollBar::handle:vertical {"
              "  background: %3;"
              "  min-height: 20px;"
              "  border-radius: 5px;"
              "}"
              "QPlainTextEdit QScrollBar::handle:vertical:hover {"
              "  background: %4;"
              "}"
              "QPlainTextEdit QScrollBar::add-line:vertical,"
              "QPlainTextEdit QScrollBar::sub-line:vertical {"
              "  height: 0;"
              "  background: none;"
              "}"
              "QPlainTextEdit QScrollBar:horizontal {"
              "  background: %6;"
              "  height: 10px;"
              "  margin: 0;"
              "}"
              "QPlainTextEdit QScrollBar::handle:horizontal {"
              "  background: %3;"
              "  min-width: 20px;"
              "  border-radius: 5px;"
              "}"
              "QPlainTextEdit QScrollBar::handle:horizontal:hover {"
              "  background: %4;"
              "}"
              "QPlainTextEdit QScrollBar::add-line:horizontal,"
              "QPlainTextEdit QScrollBar::sub-line:horizontal {"
              "  width: 0;"
              "  background: none;"
              "}")
          .arg(bg.name(), fg.name(), scrollThumb.name(),
               scrollThumbHover.name(), rgba(selection, 0.34),
               scrollTrack.name());

  ui->textEdit->setStyleSheet(styleSheet);

  QString cwdLabelStyle =
      QString("QLabel {"
              "  color: %2;"
              "  font-size: 11px;"
              "  font-weight: 600;"
              "  padding: 5px 12px;"
              "  background: %1;"
              "  border-top: 1px solid %3;"
              "  border-left: 1px solid %3;"
              "  border-right: 1px solid %3;"
              "}")
          .arg(colors.surfaceRaised.isValid() ? colors.surfaceRaised.name()
                                              : bg.lighter(104).name(),
               accent.name(), rgba(border, 0.44));
  ui->cwdLabel->setStyleSheet(cwdLabelStyle);

  if (m_runInputIndicator) {
    QString indicatorStyle =
        QString("QLabel#runInputIndicator {"
                "  color: %1;"
                "  background: %2;"
                "  border: 1px solid %3;"
                "  border-radius: 4px;"
                "  padding: 1px 6px;"
                "  font-size: 11px;"
                "}")
            .arg(accent.name(), rgba(selection, 0.28), rgba(accent, 0.32));
    m_runInputIndicator->setStyleSheet(indicatorStyle);
  }
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

void Terminal::setPythonEnvironmentBanner(const PythonEnvironmentInfo &info) {
  m_pythonEnvironmentBanner = formatPythonBanner(info);
}

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

  writeToShell(textToSend.toUtf8());
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

void Terminal::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updatePtySize();
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

void Terminal::updateCwdLabel() {
  if (!ui->cwdLabel) {
    return;
  }

  QString displayPath = m_workingDirectory;
  QString home = QDir::homePath();
  if (displayPath.startsWith(home)) {
    displayPath = "~" + displayPath.mid(home.length());
  }

  QString shellName = m_shellProfile.name;
  if (shellName.isEmpty()) {
    shellName = QFileInfo(getShellCommand()).fileName();
  }

  ui->cwdLabel->setText(QString("%1: %2").arg(shellName, displayPath));
}

QString Terminal::formatPythonBanner(const PythonEnvironmentInfo &info) const {

  const QString reset = "\x1b[0m";
  const QString bold = "\x1b[1m";
  const QString green = "\x1b[32m";
  const QString yellow = "\x1b[33m";
  const QString red = "\x1b[31m";
  const QString dim = "\x1b[2m";
  const QString cyan = "\x1b[36m";

  QString banner;
  if (info.found && info.isVirtualEnvironment()) {
    const QString venvName = QFileInfo(info.venvPath).fileName();
    banner = QString("%1%2%3 Python Environment%4\n"
                     "  %5Interpreter:%4 %6\n"
                     "  %5Venv:%4       %7 %8(%9)%4\n")
                 .arg(bold, green, QString::fromUtf8("\xF0\x9F\x90\x8D"), reset,
                      cyan, info.interpreter, venvName, dim, info.venvPath);
  } else if (info.found) {
    banner = QString("%1%2%3 Python Environment%4\n"
                     "  %5Interpreter:%4 %6\n"
                     "  %7%8No virtual environment active%4\n")
                 .arg(bold, yellow, QString::fromUtf8("\xF0\x9F\x90\x8D"),
                      reset, cyan, info.interpreter, dim, yellow);
  } else {
    const QString msg = info.statusMessage.isEmpty()
                            ? QString("Python interpreter not found")
                            : info.statusMessage;
    banner = QString("%1%2%3 Python Environment%4\n"
                     "  %5%6%4\n")
                 .arg(bold, red, QString::fromUtf8("\xF0\x9F\x90\x8D"), reset,
                      red, msg);
  }
  return banner;
}

void Terminal::appendAnsiText(const QString &text, QTextCursor &cursor) {
  QColor currentFg = QColor(m_textColor);
  QColor currentBg;
  bool bold = false;
  bool dim = false;
  bool italic = false;
  bool underline = false;

  auto applyFormat = [&]() -> QTextCharFormat {
    QTextCharFormat fmt;
    QColor fg = currentFg;
    if (dim) {
      fg = fg.darker(150);
    }
    fmt.setForeground(fg);
    if (currentBg.isValid()) {
      fmt.setBackground(currentBg);
    }
    if (bold) {
      fmt.setFontWeight(QFont::Bold);
    }
    if (italic) {
      fmt.setFontItalic(true);
    }
    if (underline) {
      fmt.setFontUnderline(true);
    }
    return fmt;
  };

  auto applySgrCodes = [&](const QString &params) {
    QStringList codes = params.split(';', Qt::SkipEmptyParts);

    if (codes.isEmpty() || params.isEmpty()) {
      currentFg = QColor(m_textColor);
      currentBg = QColor();
      bold = dim = italic = underline = false;
    }

    for (int i = 0; i < codes.size(); ++i) {
      int n = codes[i].toInt();
      if (n == 0) {
        currentFg = QColor(m_textColor);
        currentBg = QColor();
        bold = dim = italic = underline = false;
      } else if (n == 1) {
        bold = true;
      } else if (n == 2) {
        dim = true;
      } else if (n == 3) {
        italic = true;
      } else if (n == 4) {
        underline = true;
      } else if (n == 22) {
        bold = false;
        dim = false;
      } else if (n == 23) {
        italic = false;
      } else if (n == 24) {
        underline = false;
      } else if (n >= 30 && n <= 37) {
        currentFg = ansi256Color(n - 30);
      } else if (n == 38 && i + 1 < codes.size()) {
        int mode = codes[i + 1].toInt();
        if (mode == 5 && i + 2 < codes.size()) {
          currentFg = ansi256Color(codes[i + 2].toInt());
          i += 2;
        } else if (mode == 2 && i + 4 < codes.size()) {
          currentFg = QColor(codes[i + 2].toInt(), codes[i + 3].toInt(),
                             codes[i + 4].toInt());
          i += 4;
        }
      } else if (n == 39) {
        currentFg = QColor(m_textColor);
      } else if (n >= 40 && n <= 47) {
        currentBg = ansi256Color(n - 40);
      } else if (n == 48 && i + 1 < codes.size()) {
        int mode = codes[i + 1].toInt();
        if (mode == 5 && i + 2 < codes.size()) {
          currentBg = ansi256Color(codes[i + 2].toInt());
          i += 2;
        } else if (mode == 2 && i + 4 < codes.size()) {
          currentBg = QColor(codes[i + 2].toInt(), codes[i + 3].toInt(),
                             codes[i + 4].toInt());
          i += 4;
        }
      } else if (n == 49) {
        currentBg = QColor();
      } else if (n >= 90 && n <= 97) {
        currentFg = ansi256Color(n - 90 + 8);
      } else if (n >= 100 && n <= 107) {
        currentBg = ansi256Color(n - 100 + 8);
      }
    }
  };

  auto deletePreviousCharacter = [&]() {
    if (cursor.position() <= 0) {
      return;
    }
    QTextCursor probe = cursor;
    probe.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
    if (probe.selectedText() == QChar(0x2029)) {
      return;
    }
    probe.removeSelectedText();
    cursor = probe;
  };

  auto eraseToEndOfLine = [&]() {
    QTextCursor erase = cursor;
    erase.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    erase.removeSelectedText();
    cursor = erase;
  };

  for (int i = 0; i < text.length(); ++i) {
    const QChar ch = text.at(i);

    if (ch == QChar('\x1b')) {
      if (i + 1 >= text.length()) {
        continue;
      }

      const QChar next = text.at(i + 1);
      if (next == '[') {
        int j = i + 2;
        while (j < text.length() && !text.at(j).isLetter() &&
               text.at(j) != QChar('~') && text.at(j) != QChar('@')) {
          ++j;
        }
        if (j >= text.length()) {
          break;
        }

        const QString params = text.mid(i + 2, j - i - 2);
        const QChar final = text.at(j);
        QStringList parts = params.split(';', Qt::SkipEmptyParts);
        int count = parts.isEmpty() ? 1 : qMax(1, parts.first().toInt());

        if (final == 'm') {
          applySgrCodes(params);
        } else if (final == 'D') {
          for (int n = 0; n < count; ++n) {
            cursor.movePosition(QTextCursor::PreviousCharacter);
          }
        } else if (final == 'C') {
          for (int n = 0; n < count; ++n) {
            cursor.movePosition(QTextCursor::NextCharacter);
          }
        } else if (final == 'K') {
          const int mode = parts.isEmpty() ? 0 : parts.first().toInt();
          if (mode == 0) {
            eraseToEndOfLine();
          } else if (mode == 1) {
            QTextCursor erase = cursor;
            erase.movePosition(QTextCursor::StartOfBlock,
                               QTextCursor::KeepAnchor);
            erase.removeSelectedText();
            cursor = erase;
          } else if (mode == 2) {
            cursor.movePosition(QTextCursor::StartOfBlock);
            eraseToEndOfLine();
          }
        } else if (final == 'G') {
          cursor.movePosition(QTextCursor::StartOfBlock);
          for (int n = 1; n < count; ++n) {
            cursor.movePosition(QTextCursor::NextCharacter);
          }
        }
        i = j;
      } else if (next == ']') {
        int j = i + 2;
        while (j < text.length()) {
          if (text.at(j) == QChar('\x07')) {
            break;
          }
          if (text.at(j) == QChar('\x1b') && j + 1 < text.length() &&
              text.at(j + 1) == '\\') {
            ++j;
            break;
          }
          ++j;
        }
        i = j;
      } else {
        ++i;
      }
      continue;
    }

    if (ch == QChar('\x08') || ch == QChar('\x7f')) {
      deletePreviousCharacter();
      continue;
    }

    if (ch == QChar('\r')) {
      if (i + 1 < text.length() && text.at(i + 1) == QChar('\n')) {
        continue;
      }
      cursor.movePosition(QTextCursor::StartOfBlock);
      continue;
    }

    if (ch == QChar('\n')) {
      cursor.movePosition(QTextCursor::EndOfBlock);
      cursor.insertBlock();
      continue;
    }

    if (ch == QChar('\x07')) {
      continue;
    }

    cursor.insertText(QString(ch), applyFormat());
  }
}

void Terminal::setupContextMenu() {
  m_contextMenu = new QMenu(this);

  m_copyAction = m_contextMenu->addAction(tr("Copy"));
  m_copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
  connect(m_copyAction, &QAction::triggered, this,
          [this]() { ui->textEdit->copy(); });

  QAction *pasteAction = m_contextMenu->addAction(tr("Paste"));
  pasteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
  connect(pasteAction, &QAction::triggered, this,
          [this]() { insertInputText(QApplication::clipboard()->text()); });

  m_contextMenu->addSeparator();

  QAction *selectAllAction = m_contextMenu->addAction(tr("Select All"));
  selectAllAction->setShortcut(QKeySequence::SelectAll);
  connect(selectAllAction, &QAction::triggered, this,
          [this]() { ui->textEdit->selectAll(); });

  m_contextMenu->addSeparator();

  m_stopAction = m_contextMenu->addAction(tr("Stop Running Program"));
  connect(m_stopAction, &QAction::triggered, this,
          &Terminal::interruptActiveProcess);

  m_contextMenu->addSeparator();

  QAction *clearAction = m_contextMenu->addAction(tr("Clear"));
  connect(clearAction, &QAction::triggered, this, &Terminal::clear);

  ui->textEdit->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->textEdit, &QPlainTextEdit::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            m_copyAction->setEnabled(ui->textEdit->textCursor().hasSelection());
            m_stopAction->setEnabled(canInterruptActiveProcess());
            m_contextMenu->exec(ui->textEdit->mapToGlobal(pos));
          });
}

void Terminal::zoomIn() {
  QFont font = ui->textEdit->font();
  int newSize = font.pointSize() + 1;
  if (newSize <= kMaxFontSize) {
    font.setPointSize(newSize);
    ui->textEdit->setFont(font);
    m_baseFontSize = newSize;
    emit fontSizeChanged(newSize);
  }
}

void Terminal::zoomOut() {
  QFont font = ui->textEdit->font();
  int newSize = font.pointSize() - 1;
  if (newSize >= kMinFontSize) {
    font.setPointSize(newSize);
    ui->textEdit->setFont(font);
    m_baseFontSize = newSize;
    emit fontSizeChanged(newSize);
  }
}

void Terminal::zoomReset() {
  QFont font = ui->textEdit->font();
  font.setPointSize(kDefaultFontSize);
  ui->textEdit->setFont(font);
  m_baseFontSize = kDefaultFontSize;
  emit fontSizeChanged(kDefaultFontSize);
}

int Terminal::currentFontSize() const { return m_baseFontSize; }

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
