#include "terminal.h"
#include "../../run_templates/runtemplatemanager.h"
#include "../../theme/colorcontrast.h"
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
#include <QTextFragment>
#include <QUrl>
#include <QWheelEvent>

#ifndef Q_OS_WIN
#include <signal.h>
#include <unistd.h>
#endif

namespace {
QString rgba(const QColor &color, qreal alpha) {
  if (!color.isValid())
    return QStringLiteral("transparent");
  const QColor &c = color;
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

constexpr char16_t kWideCellPlaceholder = 0x200B;

bool isWideCharacter(char16_t c) {
  return (c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0x303E) ||
         (c >= 0x3041 && c <= 0x33FF) || (c >= 0x3400 && c <= 0x4DBF) ||
         (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0xA000 && c <= 0xA4CF) ||
         (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF) ||
         (c >= 0xFE30 && c <= 0xFE4F) || (c >= 0xFF00 && c <= 0xFF60) ||
         (c >= 0xFFE0 && c <= 0xFFE6);
}

QString normalizeInputText(QString text) {
  text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
  text.replace(QChar::LineSeparator, QLatin1Char('\n'));
  return text;
}

const QStringList &terminalFontFamilies() {
  static const QStringList families = {
      "JetBrains Mono",  "Cascadia Code", "Fira Code",
      "Source Code Pro", "Consolas",      "Menlo",
      "Monaco",          "Courier New",   "Monospace"};
  return families;
}

int findTrailingIncompleteEscapeStart(const QString &text) {
  for (int i = 0; i < text.size(); ++i) {
    if (text.at(i) != QChar('\x1b')) {
      continue;
    }

    if (i + 1 >= text.size()) {
      return i;
    }

    const QChar next = text.at(i + 1);
    if (next == '[') {
      int j = i + 2;
      while (j < text.size() &&
             (text.at(j).unicode() < 0x40 || text.at(j).unicode() > 0x7e)) {
        ++j;
      }
      if (j >= text.size()) {
        return i;
      }
      i = j;
    } else if (next == ']') {
      int j = i + 2;
      bool terminated = false;
      while (j < text.size()) {
        if (text.at(j) == QChar('\x07')) {
          terminated = true;
          break;
        }
        if (text.at(j) == QChar('\x1b') && j + 1 < text.size() &&
            text.at(j + 1) == QChar('\\')) {
          terminated = true;
          ++j;
          break;
        }
        ++j;
      }
      if (!terminated) {
        return i;
      }
      i = j;
    } else if (QStringLiteral(" ()#%*+,-./").contains(next)) {
      if (i + 2 >= text.size()) {
        return i;
      }
      i += 2;
    } else if (next == 'P' || next == '^' || next == '_' || next == 'X') {
      int j = i + 2;
      bool terminated = false;
      while (j < text.size()) {
        if (text.at(j) == QChar('\x07')) {
          terminated = true;
          break;
        }
        if (text.at(j) == QChar('\x1b') && j + 1 < text.size() &&
            text.at(j + 1) == QChar('\\')) {
          terminated = true;
          ++j;
          break;
        }
        ++j;
      }
      if (!terminated) {
        return i;
      }
      i = j;
    } else {
      ++i;
    }
  }

  return -1;
}
} // namespace

Terminal::Terminal(QWidget *parent, const QString &workingDirectory)
    : QWidget(parent), ui(new Ui::Terminal), m_process(nullptr),
#ifndef Q_OS_WIN
      m_shellPty(nullptr),
#endif
      m_runProcess(nullptr), m_restartTimer(nullptr),
      m_workingDirectory(workingDirectory), m_historyIndex(0),
      m_processRunning(false), m_shellStopRequested(false),
      m_restartShellAfterRun(false), m_autoRestartEnabled(true),
      m_restartAttempts(0), m_backgroundColor("#0e1116"),
      m_textColor("#e6edf3"), m_errorColor("#f44336"), m_linkColor("#58a6ff"),
      m_scrollbackLines(kDefaultScrollbackLines), m_linkDetectionEnabled(true),
      m_urlRegex(R"((https?://|ftp://|file://)[^\s<>\"\'\]\)]+)"),
      m_filePathRegex(R"((?:^|[\s:])(/[^\s:]+|[A-Za-z]:\\[^\s:]+))"),
      m_inputStartPosition(0), m_ansiForeground(m_textColor), m_ansiRow(0),
      m_ansiColumn(0), m_ansiBold(false), m_ansiDim(false), m_ansiItalic(false),
      m_ansiUnderline(false), m_ansiInverse(false), m_ansiStrikeOut(false),
      m_ansiHidden(false), m_alternateScreenActive(false),
      m_terminalColumns(80), m_terminalRows(24), m_screenTop(0), m_scrollTop(0),
      m_scrollBottom(-1), m_autoWrap(true), m_insertMode(false),
      m_applicationCursorKeys(false), m_bracketedPaste(false),
      m_cursorShown(true), m_processingPtyOutput(false),
      m_ptyDecoder(QStringDecoder::Utf8), m_savedPrimaryInputStartPosition(0),
      m_savedPrimaryScreenTop(0), m_savedPrimaryAnsiRow(0),
      m_savedPrimaryAnsiColumn(0), m_baseFontSize(kDefaultFontSize),
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

  ui->textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  ui->textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->textEdit->document()->setDocumentMargin(12);

  ui->textEdit->document()->setUndoRedoEnabled(false);

  QFont monoFont;
  monoFont.setFamilies(terminalFontFamilies());
  monoFont.setStyleHint(QFont::Monospace);
  monoFont.setPointSize(m_baseFontSize);
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

void Terminal::resetAnsiState() {
  m_ansiForeground = QColor(m_textColor);
  m_ansiBackground = QColor();
  m_ansiRow = 0;
  m_ansiColumn = 0;
  m_screenTop = 0;
  m_scrollTop = 0;
  m_scrollBottom = -1;
  m_savedCursor = SavedCursor();
  m_ansiBold = false;
  m_ansiDim = false;
  m_ansiItalic = false;
  m_ansiUnderline = false;
  m_ansiInverse = false;
  m_ansiStrikeOut = false;
  m_ansiHidden = false;
  m_autoWrap = true;
  m_insertMode = false;
}

QTextCharFormat Terminal::currentAnsiFormat() const {
  QTextCharFormat fmt;
  QColor fg = m_ansiForeground;
  QColor bg = m_ansiBackground;
  if (m_ansiDim) {
    fg = fg.darker(135);
  }

  if (m_ansiInverse) {
    const QColor defaultFg(m_textColor);
    const QColor defaultBg(m_backgroundColor);
    qSwap(fg, bg);
    if (!fg.isValid()) {
      fg = defaultBg;
    }
    if (!bg.isValid()) {
      bg = defaultFg;
    }
  }

  fmt.setForeground(fg.isValid() ? fg : QColor(m_textColor));
  if (bg.isValid()) {
    fmt.setBackground(bg);
  }
  if (m_ansiBold) {
    fmt.setFontWeight(QFont::Bold);
  }
  if (m_ansiItalic) {
    fmt.setFontItalic(true);
  }
  if (m_ansiUnderline) {
    fmt.setFontUnderline(true);
  }
  if (m_ansiStrikeOut) {
    fmt.setFontStrikeOut(true);
  }
  if (m_ansiHidden) {
    fmt.setForeground(bg.isValid() ? bg : QColor(m_backgroundColor));
  }
  return fmt;
}

void Terminal::ensureAnsiLineExists(int row) {
  row = qMax(0, row);
  QTextDocument *doc = ui->textEdit->document();
  QTextCursor cursor(doc);
  cursor.movePosition(QTextCursor::End);
  while (doc->blockCount() <= row) {
    cursor.insertBlock();
  }
}

QTextCursor Terminal::ansiCursor(bool padToColumn) {
  m_ansiRow = qMax(0, m_ansiRow);
  m_ansiColumn = qMax(0, m_ansiColumn);
  ensureAnsiLineExists(m_ansiRow);

  QTextDocument *doc = ui->textEdit->document();
  QTextBlock block = doc->findBlockByNumber(m_ansiRow);
  QTextCursor cursor(block);

  const int blockLength = block.length() - 1;
  if (padToColumn && blockLength < m_ansiColumn) {
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertText(QString(m_ansiColumn - blockLength, ' '),
                      QTextCharFormat());
    block = cursor.block();
  }

  cursor.setPosition(block.position() + qMin(m_ansiColumn, block.length() - 1));
  return cursor;
}

void Terminal::syncAnsiCursor(const QTextCursor &cursor) {
  QTextBlock block = cursor.block();
  m_ansiRow = qMax(0, block.blockNumber());
  m_ansiColumn = qMax(0, cursor.position() - block.position());
}

void Terminal::syncAnsiCursorToDocumentEnd() {
  QTextCursor endCursor(ui->textEdit->document());
  endCursor.movePosition(QTextCursor::End);
  syncAnsiCursor(endCursor);
  keepCursorOnScreen();
}

void Terminal::clearDocument() {

  QTextCursor cursor(ui->textEdit->document());
  cursor.select(QTextCursor::Document);
  cursor.removeSelectedText();
  cursor.setBlockFormat(QTextBlockFormat());
  cursor.setCharFormat(QTextCharFormat());
}

void Terminal::enterAlternateScreen() {
  if (m_alternateScreenActive) {
    return;
  }

  QTextCursor all(ui->textEdit->document());
  all.select(QTextCursor::Document);
  m_savedPrimaryScreen = all.selection();
  m_savedPrimaryInputStartPosition = m_inputStartPosition;
  m_savedPrimaryScreenTop = m_screenTop;
  m_savedPrimaryAnsiRow = m_ansiRow;
  m_savedPrimaryAnsiColumn = m_ansiColumn;
  m_alternateScreenActive = true;

  const int screenRow = m_ansiRow - m_screenTop;
  clearDocument();
  ui->textEdit->setDecorationsEnabled(false);
  m_screenTop = 0;
  m_ansiRow = qBound(0, screenRow, screenRows() - 1);
}

void Terminal::leaveAlternateScreen() {
  if (!m_alternateScreenActive) {
    return;
  }

  clearDocument();
  ui->textEdit->setDecorationsEnabled(true);
  if (!m_savedPrimaryScreen.isEmpty()) {
    QTextCursor cursor(ui->textEdit->document());
    cursor.insertFragment(m_savedPrimaryScreen);
  }
  m_savedPrimaryScreen = QTextDocumentFragment();
  m_alternateScreenActive = false;
  m_scrollTop = 0;
  m_scrollBottom = -1;
  m_screenTop = m_savedPrimaryScreenTop;
  m_ansiRow = m_savedPrimaryAnsiRow;
  m_ansiColumn = m_savedPrimaryAnsiColumn;
  keepCursorOnScreen();

  m_inputStartPosition = qMin(m_savedPrimaryInputStartPosition,
                              ui->textEdit->document()->characterCount() - 1);
}

bool Terminal::hasProtectedInputSurface() const {
  return isPtyShellActive() ||
         (m_runProcess && m_runProcess->state() != QProcess::NotRunning);
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

void Terminal::copySelectionToClipboard() const {
  QTextCursor cursor = ui->textEdit->textCursor();
  if (!cursor.hasSelection()) {
    return;
  }

  QString text = normalizeInputText(cursor.selectedText());
  text.remove(QChar(kWideCellPlaceholder));
  QApplication::clipboard()->setText(text);
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

void Terminal::pasteClipboardText() {
  const QString text = QApplication::clipboard()->text();
  if (text.isEmpty()) {
    return;
  }

  if (isPtyShellActive() &&
      !(m_runProcess && m_runProcess->state() != QProcess::NotRunning)) {
    placeCaretAtAnsiCursor();
    scrollToBottom();
    writeToShell(ptyPasteData(text, m_bracketedPaste));
    return;
  }

  insertInputText(text);
}

QByteArray Terminal::ptyPasteData(const QString &text, bool bracketed) {

  QString pasted = normalizeInputText(text);
  pasted.remove(QChar(kWideCellPlaceholder));
  pasted.replace(QStringLiteral("\r\n"), QStringLiteral("\r"));
  pasted.replace(QLatin1Char('\n'), QLatin1Char('\r'));
  if (bracketed) {

    pasted.remove(QStringLiteral("\x1b[201~"));
    pasted = QStringLiteral("\x1b[200~") + pasted + QStringLiteral("\x1b[201~");
  }
  return pasted.toUtf8();
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
  QString pending = normalizeInputText(cursor.selectedText());
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
      copySelectionToClipboard();
      return true;
    }
    if (ctrl)
      return false;
    break;

  case Qt::Key_V:
    if ((ctrl && shift) || keyEvent->matches(QKeySequence::Paste)) {
      pasteClipboardText();
      return true;
    }
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

bool Terminal::isShellInForeground() const {
#ifndef Q_OS_WIN
  return !m_shellPty || m_shellPty->isShellInForeground();
#else
  return true;
#endif
}

QString Terminal::shellCurrentDirectory() const {
#ifndef Q_OS_WIN
  if (m_shellPty) {
    const QString directory = m_shellPty->currentWorkingDirectory();
    if (!directory.isEmpty()) {
      return directory;
    }
  }
#endif
  return m_workingDirectory;
}

void Terminal::placeCaretAtAnsiCursor() {
  ui->textEdit->setTextCursor(ansiCursor(false));
}

bool Terminal::shouldTerminalConsumeShortcut(QKeyEvent *keyEvent) const {
  const Qt::KeyboardModifiers mods =
      keyEvent->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier |
                               Qt::AltModifier | Qt::MetaModifier);
  const int key = keyEvent->key();

  if (mods == Qt::ControlModifier) {

    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
      return true;
    }
    switch (key) {
    case Qt::Key_BracketLeft:
    case Qt::Key_BracketRight:
    case Qt::Key_Backslash:
    case Qt::Key_Underscore:
    case Qt::Key_Slash:
    case Qt::Key_Space:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_Delete:
    case Qt::Key_Backspace:

    case Qt::Key_Plus:
    case Qt::Key_Equal:
    case Qt::Key_Minus:
    case Qt::Key_0:
      return true;
    default:
      return false;
    }
  }

  if (mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
    return key == Qt::Key_C || key == Qt::Key_V;
  }

  if (mods == Qt::AltModifier ||
      mods == (Qt::AltModifier | Qt::ShiftModifier)) {
    return !keyEvent->text().isEmpty() || key == Qt::Key_Left ||
           key == Qt::Key_Right || key == Qt::Key_Backspace;
  }

  if (mods == Qt::NoModifier || mods == Qt::ShiftModifier) {
    return key < Qt::Key_F1 || key > Qt::Key_F35;
  }

  return false;
}

QByteArray Terminal::ptyKeySequence(QKeyEvent *keyEvent) const {
  const Qt::KeyboardModifiers mods = keyEvent->modifiers();
  const bool ctrl = mods & Qt::ControlModifier;
  const bool shift = mods & Qt::ShiftModifier;
  const bool alt = mods & Qt::AltModifier;
  const int key = keyEvent->key();
  const int modifierCode = 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (ctrl ? 4 : 0);

  auto cursorKey = [&](char final) {
    if (modifierCode > 1) {
      return QStringLiteral("\x1b[1;%1%2")
          .arg(modifierCode)
          .arg(QLatin1Char(final))
          .toLatin1();
    }
    return QByteArray(m_applicationCursorKeys ? "\x1bO" : "\x1b[") + final;
  };
  auto tildeKey = [&](int number) {
    if (modifierCode > 1) {
      return QStringLiteral("\x1b[%1;%2~")
          .arg(number)
          .arg(modifierCode)
          .toLatin1();
    }
    return QStringLiteral("\x1b[%1~").arg(number).toLatin1();
  };
  auto functionKey = [&](char final) {
    if (modifierCode > 1) {
      return QStringLiteral("\x1b[1;%1%2")
          .arg(modifierCode)
          .arg(QLatin1Char(final))
          .toLatin1();
    }
    return QByteArray("\x1bO") + final;
  };

  switch (key) {
  case Qt::Key_Up:
    return cursorKey('A');
  case Qt::Key_Down:
    return cursorKey('B');
  case Qt::Key_Right:
    return cursorKey('C');
  case Qt::Key_Left:
    return cursorKey('D');
  case Qt::Key_Home:
    return cursorKey('H');
  case Qt::Key_End:
    return cursorKey('F');
  case Qt::Key_Insert:
    return tildeKey(2);
  case Qt::Key_Delete:
    return tildeKey(3);
  case Qt::Key_PageUp:
    return tildeKey(5);
  case Qt::Key_PageDown:
    return tildeKey(6);
  case Qt::Key_F1:
    return functionKey('P');
  case Qt::Key_F2:
    return functionKey('Q');
  case Qt::Key_F3:
    return functionKey('R');
  case Qt::Key_F4:
    return functionKey('S');
  case Qt::Key_F5:
    return tildeKey(15);
  case Qt::Key_F6:
    return tildeKey(17);
  case Qt::Key_F7:
    return tildeKey(18);
  case Qt::Key_F8:
    return tildeKey(19);
  case Qt::Key_F9:
    return tildeKey(20);
  case Qt::Key_F10:
    return tildeKey(21);
  case Qt::Key_F11:
    return tildeKey(23);
  case Qt::Key_F12:
    return tildeKey(24);
  case Qt::Key_Return:
  case Qt::Key_Enter:
    return alt ? QByteArray("\x1b\r") : QByteArray("\r");
  case Qt::Key_Backspace:
    if (ctrl) {
      return QByteArray("\x08");
    }
    return alt ? QByteArray("\x1b\x7f") : QByteArray("\x7f");
  case Qt::Key_Tab:
    return QByteArray("\t");
  case Qt::Key_Backtab:
    return QByteArray("\x1b[Z");
  case Qt::Key_Escape:
    return QByteArray("\x1b");
  default:
    break;
  }

  const QByteArray prefix = alt ? QByteArray("\x1b") : QByteArray();
  if (ctrl) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
      return prefix + QByteArray(1, char(key - Qt::Key_A + 1));
    }
    switch (key) {
    case Qt::Key_Space:
    case Qt::Key_At:
    case Qt::Key_2:
      return prefix + QByteArray(1, '\0');
    case Qt::Key_BracketLeft:
    case Qt::Key_3:
      return prefix + QByteArray("\x1b");
    case Qt::Key_Backslash:
    case Qt::Key_4:
      return prefix + QByteArray("\x1c");
    case Qt::Key_BracketRight:
    case Qt::Key_5:
      return prefix + QByteArray("\x1d");
    case Qt::Key_AsciiCircum:
    case Qt::Key_6:
      return prefix + QByteArray("\x1e");
    case Qt::Key_Underscore:
    case Qt::Key_Slash:
    case Qt::Key_7:
      return prefix + QByteArray("\x1f");
    default:
      break;
    }
  }

  const QString text = keyEvent->text();
  if (text.isEmpty()) {
    return QByteArray();
  }
  if (alt && !text.startsWith(QChar('\x1b'))) {
    return prefix + text.toUtf8();
  }
  return text.toUtf8();
}

bool Terminal::handlePtyKeyPress(QKeyEvent *keyEvent) {
  const Qt::KeyboardModifiers mods = keyEvent->modifiers();
  const bool ctrl = mods & Qt::ControlModifier;
  const bool shift = mods & Qt::ShiftModifier;
  const int key = keyEvent->key();

  if ((ctrl && shift && key == Qt::Key_C) ||
      (ctrl && !shift && key == Qt::Key_Insert)) {
    copySelectionToClipboard();
    return true;
  }

  if ((ctrl && shift && key == Qt::Key_V) ||
      (shift && !ctrl && key == Qt::Key_Insert) ||
      keyEvent->matches(QKeySequence::Paste)) {
    pasteClipboardText();
    return true;
  }

  if (ctrl && !shift) {
    switch (key) {
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
    default:
      break;
    }
  }

  if (shift && !ctrl && !(mods & Qt::AltModifier)) {
    QScrollBar *bar = ui->textEdit->verticalScrollBar();
    switch (key) {
    case Qt::Key_PageUp:
      bar->setValue(bar->value() - bar->pageStep());
      return true;
    case Qt::Key_PageDown:
      bar->setValue(bar->value() + bar->pageStep());
      return true;
    case Qt::Key_Home:
      bar->setValue(bar->minimum());
      return true;
    case Qt::Key_End:
      bar->setValue(bar->maximum());
      return true;
    default:
      break;
    }
  }

  const QByteArray sequence = ptyKeySequence(keyEvent);
  if (sequence.isEmpty()) {
    return true;
  }

  placeCaretAtAnsiCursor();
  scrollToBottom();
  writeToShell(sequence);
  return true;
}

void Terminal::updatePtySize() {
#ifndef Q_OS_WIN
  if (!m_shellPty || !ui || !ui->textEdit) {
    return;
  }

  const bool laidOut = ui->textEdit->isVisible() &&
                       ui->textEdit->viewport()->width() > 0 &&
                       ui->textEdit->viewport()->height() > 0;
  if (laidOut) {

    const QFontMetricsF metrics(ui->textEdit->font());
    const qreal charWidth =
        qMax<qreal>(1.0, metrics.horizontalAdvance(QLatin1Char('M')));
    const qreal lineHeight = qMax<qreal>(1.0, metrics.lineSpacing());
    const QRect viewport = ui->textEdit->viewport()->rect();
    const qreal documentMargin =
        qMax<qreal>(0.0, ui->textEdit->document()->documentMargin() * 2);
    const int caretWidth = 2;
    const qreal availableWidth =
        qMax(charWidth, viewport.width() - documentMargin - caretWidth);
    const qreal availableHeight =
        qMax(lineHeight, viewport.height() - documentMargin);
    m_terminalColumns = qMax(20, int(availableWidth / charWidth));
    m_terminalRows = qMax(4, int(availableHeight / lineHeight));
  }

  if (m_shellPty->columns() == m_terminalColumns &&
      m_shellPty->rows() == m_terminalRows) {
    return;
  }
  handleScreenResize();
  m_shellPty->resize(m_terminalColumns, m_terminalRows);
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

QColor Terminal::ansi256Color(int index) const {
  const ThemeDefinition &td = ThemeEngine::instance().activeTheme();
  const ThemeColors &colors = td.colors;

  const QColor standard[16] = {
      colors.ansiBlack.isValid() ? colors.ansiBlack : QColor(m_backgroundColor),
      colors.ansiRed.isValid() ? colors.ansiRed : QColor("#f85149"),
      colors.ansiGreen.isValid() ? colors.ansiGreen : QColor("#7dffb2"),
      colors.ansiYellow.isValid() ? colors.ansiYellow : QColor("#e6b450"),
      colors.ansiBlue.isValid() ? colors.ansiBlue : QColor("#7bb7ff"),
      colors.ansiMagenta.isValid() ? colors.ansiMagenta : QColor("#c7a7ff"),
      colors.ansiCyan.isValid() ? colors.ansiCyan : QColor("#65d6c4"),
      colors.ansiWhite.isValid() ? colors.ansiWhite : QColor(m_textColor),
      colors.ansiBrightBlack.isValid() ? colors.ansiBrightBlack
                                       : QColor(m_textColor).darker(160),
      colors.ansiBrightRed.isValid() ? colors.ansiBrightRed : QColor("#ff7b72"),
      colors.ansiBrightGreen.isValid() ? colors.ansiBrightGreen
                                       : QColor("#9dffc7"),
      colors.ansiBrightYellow.isValid() ? colors.ansiBrightYellow
                                        : QColor("#ffee99"),
      colors.ansiBrightBlue.isValid() ? colors.ansiBrightBlue
                                      : QColor("#9dccff"),
      colors.ansiBrightMagenta.isValid() ? colors.ansiBrightMagenta
                                         : QColor("#d8c2ff"),
      colors.ansiBrightCyan.isValid() ? colors.ansiBrightCyan
                                      : QColor("#9ce7da"),
      colors.ansiBrightWhite.isValid() ? colors.ansiBrightWhite
                                       : QColor("#e4f3ed"),
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

  m_pendingAnsiText.clear();
  m_ptyDecoder.resetState();
  m_applicationCursorKeys = false;
  m_bracketedPaste = false;
  setCursorShown(true);

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
  ptyEnv.insert("TERM_PROGRAM", "Lightpad");

  QString shell = getShellCommand();
  QStringList args = getShellArguments();
  if (!m_shellPty->start(shell, args, m_workingDirectory, ptyEnv)) {
    appendOutput("Error: Failed to start PTY shell process.\n", true);
    emit errorOccurred("Failed to start shell");
    return false;
  }

  m_processRunning = true;
  m_restartAttempts = 0;
  ui->textEdit->setReadOnly(true);
  ui->textEdit->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                        Qt::TextSelectableByKeyboard);
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
    m_pendingAnsiText.clear();
    ui->textEdit->setReadOnly(false);
    ui->textEdit->setTextInteractionFlags(Qt::TextEditorInteraction);
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
  m_pendingAnsiText.clear();
  ui->textEdit->setReadOnly(false);
  ui->textEdit->setTextInteractionFlags(Qt::TextEditorInteraction);
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

#ifndef Q_OS_WIN
  if (isPtyShellActive()) {

    writeToShell(QByteArray(1, '\x03'));
    return true;
  }
#endif

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
  const QString target = QDir::cleanPath(directory);
  const bool alreadyThere =
      !target.isEmpty() && QDir::cleanPath(shellCurrentDirectory()) == target;
  m_workingDirectory = target;
  updateCwdLabel();
  if (isRunning() && !alreadyThere) {
    QString quoted = target;
    quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    executeCommand(QStringLiteral("cd -- '%1'").arg(quoted));
  }
}

void Terminal::clear() {
  const bool runActive =
      m_runProcess && m_runProcess->state() != QProcess::NotRunning;
  if (isPtyShellActive() && !runActive && m_alternateScreenActive) {

    writeToShell(QByteArray(1, '\x0c'));
    return;
  }

  clearDocument();
  ui->textEdit->setDecorationsEnabled(true);
  m_alternateScreenActive = false;
  m_savedPrimaryScreen = QTextDocumentFragment();
  m_pendingAnsiText.clear();
  resetAnsiState();
  m_inputStartPosition = 0;
  if (isRunning() && !runActive) {
    if (isPtyShellActive()) {

      if (isShellInForeground()) {
        writeToShell(QByteArray(1, '\x0c'));
      }
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

  if (m_alternateScreenActive) {
    leaveAlternateScreen();
  }
  m_pendingAnsiText.clear();
  resetAnsiState();
  syncAnsiCursorToDocumentEnd();
  m_inputStartPosition = ui->textEdit->document()->characterCount() - 1;
  m_runTranscript.clear();
  setRunInputIndicatorActive(false);
  if (!ui->textEdit->document()->isEmpty()) {
    appendOutput("\n");
  }
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
  ui->textEdit->setTextInteractionFlags(Qt::TextEditorInteraction);

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

void Terminal::appendNotice(const QString &text, bool isError) {
  if (text.isEmpty()) {
    return;
  }
  appendOutput(text.endsWith(QLatin1Char('\n')) ? text
                                                : text + QLatin1Char('\n'),
               isError);
}

void Terminal::recordRunOutput(const QString &text) {
  constexpr int kMaxTranscriptChars = 512 * 1024;
  if (m_runTranscript.size() >= kMaxTranscriptChars) {
    return;
  }
  m_runTranscript.append(text);
}

void Terminal::onRunProcessReadyReadStdout() {
  if (m_runProcess) {
    QString output = QString::fromUtf8(m_runProcess->readAllStandardOutput());
    recordRunOutput(output);
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
    recordRunOutput(output);
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

  cursor.insertText(cursor.block().text().isEmpty() ? "\n" : "\n\n");

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
  syncAnsiCursorToDocumentEnd();
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

  const QString output = m_ptyDecoder.decode(data);
  m_processingPtyOutput = true;
  appendOutput(output);
  m_processingPtyOutput = false;
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
    appendOutput(QString("\r\nShell exited with code %1. Press Enter to "
                         "start a new shell.\r\n")
                     .arg(exitCode));
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
  const bool ptyInput =
      isPtyShellActive() &&
      !(m_runProcess && m_runProcess->state() != QProcess::NotRunning);

  if (terminalTextObject && event->type() == QEvent::ShortcutOverride) {

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if (ptyInput && shouldTerminalConsumeShortcut(keyEvent)) {
      event->accept();
      return true;
    }
  }

  if (terminalTextObject && event->type() == QEvent::MouseButtonRelease) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::MiddleButton && ptyInput) {
      const QString selection =
          QApplication::clipboard()->text(QClipboard::Selection);
      if (!selection.isEmpty()) {
        const QString saved = QApplication::clipboard()->text();
        QApplication::clipboard()->setText(selection);
        pasteClipboardText();
        QApplication::clipboard()->setText(saved);
      }
      return true;
    }
    const bool plainLeftClick = mouseEvent->button() == Qt::LeftButton &&
                                mouseEvent->modifiers() == Qt::NoModifier;
    if (plainLeftClick && hasProtectedInputSurface() &&
        !ui->textEdit->textCursor().hasSelection()) {
      ui->textEdit->setFocus(Qt::MouseFocusReason);
      if (ptyInput) {

        const int scroll = ui->textEdit->verticalScrollBar()->value();
        placeCaretAtAnsiCursor();
        ui->textEdit->verticalScrollBar()->setValue(scroll);
        return true;
      }
      QTextCursor endCursor(ui->textEdit->document());
      endCursor.movePosition(QTextCursor::End);
      ui->textEdit->setTextCursor(endCursor);
      if (QScrollBar *hScrollBar = ui->textEdit->horizontalScrollBar()) {
        hScrollBar->setValue(hScrollBar->minimum());
      }
      scrollToBottom();
      return true;
    }
  }

  if (terminalTextObject && event->type() == QEvent::Wheel && ptyInput &&
      m_alternateScreenActive) {

    QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
    const int steps = wheelEvent->angleDelta().y() / 40;
    if (steps != 0) {
      const QByteArray key = m_applicationCursorKeys
                                 ? QByteArray(steps > 0 ? "\x1bOA" : "\x1bOB")
                                 : QByteArray(steps > 0 ? "\x1b[A" : "\x1b[B");
      writeToShell(key.repeated(qAbs(steps)));
    }
    return true;
  }

  if (terminalTextObject && event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    if (ptyInput) {
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
        QString userInput = normalizeInputText(cursor.selectedText());

        ui->textEdit->moveCursor(QTextCursor::End);
        ui->textEdit->insertPlainText("\n");
        m_inputStartPosition = ui->textEdit->textCursor().position();

        syncAnsiCursorToDocumentEnd();

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
      QString userInput = normalizeInputText(cursor.selectedText());

      ui->textEdit->moveCursor(QTextCursor::End);
      ui->textEdit->insertPlainText("\n");
      syncAnsiCursorToDocumentEnd();

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
      } else if (!isRunning()) {
        startShell(m_workingDirectory);
      } else if (m_process && m_process->state() == QProcess::Running) {
        writeToShell("\n");
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

  QString output = text;
  if (output.size() > kMaxOutputChunkCharacters) {
    const int dropped = output.size() - kMaxOutputChunkCharacters;
    output =
        QString("\n[Lightpad truncated %1 characters of terminal output]\n")
            .arg(dropped) +
        output.right(kMaxOutputChunkCharacters);
  }

  if (!isError) {
    if (!m_pendingAnsiText.isEmpty()) {
      output.prepend(m_pendingAnsiText);
      m_pendingAnsiText.clear();
    }

    const int incompleteEscapeStart = findTrailingIncompleteEscapeStart(output);
    if (incompleteEscapeStart >= 0) {
      m_pendingAnsiText = output.mid(incompleteEscapeStart);
      output.truncate(incompleteEscapeStart);
    }
  }

  if (output.isEmpty()) {
    return;
  }

  QScrollBar *vScrollBar = ui->textEdit->verticalScrollBar();
  const int previousScroll = vScrollBar->value();
  const bool followOutput = previousScroll >= vScrollBar->maximum() - 1;
  const bool keepSelection = ui->textEdit->textCursor().hasSelection();

  QTextCursor cursor(ui->textEdit->document());
  cursor.movePosition(QTextCursor::End);

  if (isError) {
    QString cleanText = stripAnsiEscapeCodes(output);
    if (cleanText.isEmpty()) {
      return;
    }
    QTextCharFormat errorFormat;
    errorFormat.setForeground(QColor(m_errorColor));
    cursor.insertText(cleanText, errorFormat);
    syncAnsiCursor(cursor);
    keepCursorOnScreen();
  } else {

    QTextCursor editBlock(ui->textEdit->document());
    editBlock.beginEditBlock();
    cursor = ansiCursor(false);
    appendAnsiText(output, cursor);
    editBlock.endEditBlock();
  }

  enforceScrollbackLimit();

  if (!keepSelection) {
    placeCaretAtAnsiCursor();
  }
  if (followOutput) {
    scrollToBottom();
  } else {
    vScrollBar->setValue(previousScroll);
  }

  QTextCursor endCursor(ui->textEdit->document());
  endCursor.movePosition(QTextCursor::End);
  m_inputStartPosition = endCursor.position();
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

  syncAnsiCursor(cursor);
  m_inputStartPosition = cursor.position();
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
  const QColor previousForeground(m_textColor);
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

  if (m_ansiForeground == previousForeground)
    m_ansiForeground = QColor(m_textColor);
  QTextCursor edit(ui->textEdit->document());
  edit.beginEditBlock();
  for (QTextBlock block = ui->textEdit->document()->begin(); block.isValid();
       block = block.next()) {
    QList<QPair<int, int>> ranges;
    for (auto it = block.begin(); !it.atEnd(); ++it) {
      const auto fragment = it.fragment();
      if (fragment.charFormat().foreground().color() == previousForeground)
        ranges.append(qMakePair(fragment.position(), fragment.length()));
    }
    for (const auto &range : ranges) {
      edit.setPosition(range.first);
      edit.setPosition(range.first + range.second, QTextCursor::KeepAnchor);
      QTextCharFormat format;
      format.setForeground(QColor(m_textColor));
      edit.mergeCharFormat(format);
    }
  }
  edit.endEditBlock();
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

  if (m_ansiForeground == QColor(m_textColor)) {
    m_ansiForeground = fg;
  }
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

  const bool themedSelection =
      colors.termSelection.isValid() || colors.inputSelection.isValid();
  const QString selectionBackground =
      themedSelection ? selection.name() : rgba(selection, 0.34);

  QStringList quotedFamilies;
  for (const QString &family : terminalFontFamilies()) {
    quotedFamilies << QStringLiteral("\"%1\"").arg(family);
  }
  quotedFamilies << QStringLiteral("monospace");
  const QString fontRule = QStringLiteral("  font-family: %1;"
                                          "  font-size: %2pt;")
                               .arg(quotedFamilies.join(", "))
                               .arg(m_baseFontSize);

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
               scrollThumbHover.name(), selectionBackground,
               scrollTrack.name()) +
      QStringLiteral("QPlainTextEdit {%1}").arg(fontRule);

  ui->textEdit->setStyleSheet(styleSheet);

  const QColor cwdSurface =
      colors.surfaceRaised.isValid() ? colors.surfaceRaised : bg.lighter(104);
  QColor indicatorFill = selection;
  indicatorFill.setAlphaF(0.28);
  indicatorFill = ColorContrast::flatten(indicatorFill, bg);

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
          .arg(cwdSurface.name(),
               ColorContrast::ensure(accent, cwdSurface).name(),
               rgba(border, 0.44));
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
            .arg(ColorContrast::ensure(accent, indicatorFill).name(),
                 indicatorFill.name(), rgba(accent, 0.32));
    m_runInputIndicator->setStyleSheet(indicatorStyle);
  }
}

QString Terminal::closeButtonStyle(const QString &textColor,
                                   const QString &pressedColor) {
  const QColor baseColor(textColor);
  const QString subduedColor = rgba(baseColor, 0.72);
  const QString hoverColor = rgba(baseColor, 0.14);
  const QString fullTextColor = baseColor.name();
  const QString pressedTextColor =
      ColorContrast::ensure(baseColor, QColor(pressedColor)).name();

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
                 "  background: %4;"
                 "}"
                 "QToolButton:pressed {"
                 "  color: %5;"
                 "  background: %3;"
                 "}")
      .arg(subduedColor, fullTextColor, pressedColor, hoverColor,
           pressedTextColor);
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

void Terminal::refreshTerminalSize() { updatePtySize(); }

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

void Terminal::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);

  QTimer::singleShot(0, this, [this]() { updatePtySize(); });
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

int Terminal::screenRows() const { return qMax(1, m_terminalRows); }

int Terminal::screenColumns() const { return qMax(1, m_terminalColumns); }

int Terminal::scrollRegionBottom() const {
  const int last = screenRows() - 1;
  return (m_scrollBottom < 0 || m_scrollBottom > last) ? last : m_scrollBottom;
}

int Terminal::scrollRegionTop() const {
  return qBound(0, m_scrollTop, scrollRegionBottom());
}

void Terminal::keepCursorOnScreen() {
  const int rows = screenRows();
  m_screenTop = qMax(0, m_screenTop);
  m_ansiRow = qMax(0, m_ansiRow);
  if (m_ansiRow < m_screenTop) {
    m_screenTop = m_ansiRow;
  } else if (m_ansiRow >= m_screenTop + rows) {
    m_screenTop = m_ansiRow - rows + 1;
  }
  const int blocks = ui->textEdit->document()->blockCount();
  if (blocks > m_screenTop + rows) {
    m_screenTop = blocks - rows;
    m_ansiRow = qMax(m_ansiRow, m_screenTop);
  }
}

void Terminal::handleScreenResize() {
  m_scrollTop = 0;
  m_scrollBottom = -1;
  m_ansiColumn = qMin(m_ansiColumn, screenColumns());
  if (m_alternateScreenActive) {

    m_screenTop = 0;
    removeLines(screenRows(), ui->textEdit->document()->blockCount());
    m_ansiRow = qMin(m_ansiRow, screenRows() - 1);
    return;
  }

  QTextDocument *doc = ui->textEdit->document();
  while (doc->blockCount() - 1 > m_ansiRow &&
         doc->lastBlock().text().isEmpty()) {
    removeLines(doc->blockCount() - 1, 1);
  }
  keepCursorOnScreen();
}

void Terminal::removeLines(int first, int count) {
  QTextDocument *doc = ui->textEdit->document();
  const int total = doc->blockCount();
  if (first < 0 || count <= 0 || first >= total) {
    return;
  }
  const int end = count >= total - first ? total : first + count;

  QTextCursor cursor(doc);
  if (end < total) {
    cursor.setPosition(doc->findBlockByNumber(first).position());
    cursor.setPosition(doc->findBlockByNumber(end).position(),
                       QTextCursor::KeepAnchor);
  } else if (first > 0) {
    const QTextBlock previous = doc->findBlockByNumber(first - 1);
    cursor.setPosition(previous.position() + previous.length() - 1);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
  } else {
    cursor.select(QTextCursor::Document);
  }
  cursor.removeSelectedText();
}

void Terminal::insertBlankLines(int at, int count) {
  QTextDocument *doc = ui->textEdit->document();
  if (at < 0 || count <= 0 || at >= doc->blockCount()) {
    return;
  }
  QTextCursor cursor(doc->findBlockByNumber(at));
  cursor.setCharFormat(QTextCharFormat());
  for (int i = 0; i < count; ++i) {
    cursor.insertBlock();
  }
}

void Terminal::scrollUp(int count, int top, int bottom, bool allowScrollback) {
  if (bottom < top) {
    return;
  }
  count = qBound(1, count, bottom - top + 1);
  if (allowScrollback && !m_alternateScreenActive && top == 0 &&
      bottom == screenRows() - 1) {

    m_screenTop += count;
    m_ansiRow += count;
    return;
  }

  const int first = m_screenTop + top;
  const int last = m_screenTop + bottom;
  removeLines(first, count);
  insertBlankLines(last - count + 1, count);
}

void Terminal::scrollDown(int count, int top, int bottom) {
  if (bottom < top) {
    return;
  }
  count = qBound(1, count, bottom - top + 1);
  QTextDocument *doc = ui->textEdit->document();
  const int first = m_screenTop + top;
  const int last = m_screenTop + bottom;
  if (first >= doc->blockCount()) {
    return;
  }
  const bool contentBelowRegion = doc->blockCount() > last + 1;
  removeLines(last - count + 1, count);
  insertBlankLines(first, count);
  if (!contentBelowRegion) {
    removeLines(last + 1, doc->blockCount());
  }
}

void Terminal::lineFeed() {
  const int screenRow = m_ansiRow - m_screenTop;
  const int bottom = scrollRegionBottom();
  if (screenRow == bottom) {
    scrollUp(1, scrollRegionTop(), bottom, true);
  } else if (screenRow < screenRows() - 1) {
    ++m_ansiRow;
  }
}

void Terminal::reverseIndex() {
  const int screenRow = m_ansiRow - m_screenTop;
  const int top = scrollRegionTop();
  if (screenRow == top) {
    scrollDown(1, top, scrollRegionBottom());
  } else if (screenRow > 0) {
    --m_ansiRow;
  }
}

void Terminal::writePrintable(const QString &input) {
  QString text;
  text.reserve(input.size());
  for (const QChar ch : input) {
    text.append(ch);
    if (isWideCharacter(ch.unicode())) {
      text.append(QChar(kWideCellPlaceholder));
    }
  }

  const int columns = screenColumns();
  const QTextCharFormat format = currentAnsiFormat();
  int offset = 0;
  while (offset < text.size()) {
    if (m_ansiColumn >= columns) {
      if (m_autoWrap) {
        m_ansiColumn = 0;
        lineFeed();
      } else {
        m_ansiColumn = columns - 1;
      }
    }

    int take = qMin(text.size() - offset, columns - m_ansiColumn);
    if (take < text.size() - offset &&
        text.at(offset + take) == QChar(kWideCellPlaceholder) && columns > 1) {

      --take;
      if (take == 0) {
        m_ansiColumn = columns;
        continue;
      }
    }
    QTextCursor cursor = ansiCursor(true);
    if (!m_insertMode) {
      const int available = cursor.block().length() - 1 - m_ansiColumn;
      if (available > 0) {
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor,
                            qMin(take, available));
      }
    }
    cursor.insertText(text.mid(offset, take), format);
    m_ansiColumn += take;
    offset += take;
  }
}

void Terminal::eraseInLine(int mode) {
  QTextCursor cursor = ansiCursor(false);
  const QTextBlock block = cursor.block();
  const int length = block.length() - 1;
  const int column = qMin(m_ansiColumn, screenColumns() - 1);

  if (mode == 0) {
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    if (m_ansiBackground.isValid() || m_ansiInverse) {
      const int fill = screenColumns() - m_ansiColumn;
      if (fill > 0) {
        cursor.insertText(QString(fill, ' '), currentAnsiFormat());
      }
    }
  } else if (mode == 1) {
    const int end = qMin(length, column + 1);
    if (end > 0) {
      cursor.setPosition(block.position());
      cursor.setPosition(block.position() + end, QTextCursor::KeepAnchor);
      cursor.insertText(QString(end, ' '), currentAnsiFormat());
    }
  } else if (mode == 2) {
    cursor.setPosition(block.position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
  }
}

void Terminal::eraseInDisplay(int mode) {
  QTextDocument *doc = ui->textEdit->document();
  if (mode == 0) {
    eraseInLine(0);
    removeLines(m_ansiRow + 1, doc->blockCount());
    return;
  }

  if (mode == 1) {
    for (int row = m_screenTop; row < m_ansiRow && row < doc->blockCount();
         ++row) {
      QTextCursor cursor(doc->findBlockByNumber(row));
      cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
      cursor.removeSelectedText();
    }
    eraseInLine(1);
    return;
  }

  if (mode == 3) {
    if (!m_alternateScreenActive && m_screenTop > 0) {
      removeLines(0, m_screenTop);
      m_ansiRow -= m_screenTop;
      m_inputStartPosition = 0;
      m_screenTop = 0;
    }
    return;
  }

  if (mode != 2) {
    return;
  }

  int lastContent = -1;
  for (int row = doc->blockCount() - 1; row >= m_screenTop; --row) {
    if (!doc->findBlockByNumber(row).text().trimmed().isEmpty()) {
      lastContent = row;
      break;
    }
  }
  if (m_alternateScreenActive || lastContent < 0) {
    removeLines(m_screenTop, doc->blockCount());
    return;
  }

  removeLines(lastContent + 1, doc->blockCount());
  const int newTop = lastContent + 1;
  m_ansiRow = newTop + (m_ansiRow - m_screenTop);
  m_screenTop = newTop;
  ensureAnsiLineExists(m_screenTop + screenRows() - 1);
}

void Terminal::trimTrailingBlanksAfterCursor() {
  QTextDocument *doc = ui->textEdit->document();
  if (m_ansiRow >= doc->blockCount()) {
    return;
  }
  const QTextBlock block = doc->findBlockByNumber(m_ansiRow);
  const QString text = block.text();
  if (m_ansiColumn >= text.size() ||
      !text.mid(m_ansiColumn).trimmed().isEmpty()) {
    return;
  }
  for (auto it = block.begin(); !it.atEnd(); ++it) {
    const QTextFragment fragment = it.fragment();
    const int fragmentEnd =
        fragment.position() - block.position() + fragment.length();
    if (fragmentEnd > m_ansiColumn &&
        fragment.charFormat().hasProperty(QTextFormat::BackgroundBrush)) {
      return;
    }
  }
  QTextCursor cursor(doc);
  cursor.setPosition(block.position() + m_ansiColumn);
  cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
}

void Terminal::saveCursorState() {
  m_savedCursor.row = m_ansiRow - m_screenTop;
  m_savedCursor.column = m_ansiColumn;
  m_savedCursor.foreground = m_ansiForeground;
  m_savedCursor.background = m_ansiBackground;
  m_savedCursor.bold = m_ansiBold;
  m_savedCursor.dim = m_ansiDim;
  m_savedCursor.italic = m_ansiItalic;
  m_savedCursor.underline = m_ansiUnderline;
  m_savedCursor.inverse = m_ansiInverse;
  m_savedCursor.strikeOut = m_ansiStrikeOut;
  m_savedCursor.hidden = m_ansiHidden;
}

void Terminal::restoreCursorState() {
  m_ansiRow = m_screenTop + qBound(0, m_savedCursor.row, screenRows() - 1);
  m_ansiColumn = qBound(0, m_savedCursor.column, screenColumns() - 1);
  m_ansiForeground = m_savedCursor.foreground.isValid()
                         ? m_savedCursor.foreground
                         : QColor(m_textColor);
  m_ansiBackground = m_savedCursor.background;
  m_ansiBold = m_savedCursor.bold;
  m_ansiDim = m_savedCursor.dim;
  m_ansiItalic = m_savedCursor.italic;
  m_ansiUnderline = m_savedCursor.underline;
  m_ansiInverse = m_savedCursor.inverse;
  m_ansiStrikeOut = m_savedCursor.strikeOut;
  m_ansiHidden = m_savedCursor.hidden;
}

void Terminal::setCursorShown(bool shown) {
  m_cursorShown = shown;
  ui->textEdit->setCursorWidth(shown ? 2 : 0);
}

void Terminal::setPrivateMode(int mode, bool enable) {
  switch (mode) {
  case 1:
    m_applicationCursorKeys = enable;
    break;
  case 7:
    m_autoWrap = enable;
    break;
  case 25:
    setCursorShown(enable);
    break;
  case 47:
  case 1047:
  case 1049:
    if (enable) {
      if (mode == 1049) {
        saveCursorState();
      }
      enterAlternateScreen();
    } else {
      leaveAlternateScreen();
      if (mode == 1049) {
        restoreCursorState();
      }
    }
    break;
  case 1048:
    if (enable) {
      saveCursorState();
    } else {
      restoreCursorState();
    }
    break;
  case 2004:
    m_bracketedPaste = enable;
    break;
  default:
    break;
  }
}

void Terminal::sendTerminalReply(const QByteArray &reply) {

  if (m_processingPtyOutput) {
    writeToShell(reply);
  }
}

void Terminal::applySgr(const QString &params) {
  auto parseNumber = [](const QString &value, int defaultValue) {
    bool ok = false;
    const int parsed = value.toInt(&ok);
    return ok ? parsed : defaultValue;
  };

  auto resetAttributes = [this]() {
    m_ansiForeground = QColor(m_textColor);
    m_ansiBackground = QColor();
    m_ansiBold = m_ansiDim = m_ansiItalic = m_ansiUnderline = false;
    m_ansiInverse = m_ansiStrikeOut = m_ansiHidden = false;
  };

  auto extendedColor = [&](const QStringList &values, int &consumed) {
    consumed = 0;
    if (values.isEmpty()) {
      return QColor();
    }
    const int kind = parseNumber(values.at(0), -1);
    if (kind == 5 && values.size() >= 2) {
      consumed = 2;
      return ansi256Color(parseNumber(values.at(1), 0));
    }
    if (kind == 2 && values.size() >= 5) {

      consumed = 5;
      return QColor(qBound(0, parseNumber(values.at(2), 0), 255),
                    qBound(0, parseNumber(values.at(3), 0), 255),
                    qBound(0, parseNumber(values.at(4), 0), 255));
    }
    if (kind == 2 && values.size() == 4) {

      consumed = 4;
      return QColor(qBound(0, parseNumber(values.at(1), 0), 255),
                    qBound(0, parseNumber(values.at(2), 0), 255),
                    qBound(0, parseNumber(values.at(3), 0), 255));
    }
    return QColor();
  };

  const QStringList codes = params.split(';', Qt::KeepEmptyParts);
  for (int i = 0; i < codes.size(); ++i) {
    const QString &code = codes.at(i);
    if (code.contains(':')) {
      const QStringList parts = code.split(':');
      const int n = parseNumber(parts.first(), 0);
      int consumed = 0;
      if (n == 38 || n == 48) {
        const QColor color = extendedColor(parts.mid(1), consumed);
        if (color.isValid()) {
          (n == 38 ? m_ansiForeground : m_ansiBackground) = color;
        }
      } else if (n == 4) {
        m_ansiUnderline = parseNumber(parts.value(1), 1) != 0;
      }
      continue;
    }

    const int n = parseNumber(code, 0);
    if (n == 0) {
      resetAttributes();
    } else if (n == 1) {
      m_ansiBold = true;
    } else if (n == 2) {
      m_ansiDim = true;
    } else if (n == 3) {
      m_ansiItalic = true;
    } else if (n == 4 || n == 21) {
      m_ansiUnderline = true;
    } else if (n == 7) {
      m_ansiInverse = true;
    } else if (n == 8) {
      m_ansiHidden = true;
    } else if (n == 9) {
      m_ansiStrikeOut = true;
    } else if (n == 22) {
      m_ansiBold = false;
      m_ansiDim = false;
    } else if (n == 23) {
      m_ansiItalic = false;
    } else if (n == 24) {
      m_ansiUnderline = false;
    } else if (n == 27) {
      m_ansiInverse = false;
    } else if (n == 28) {
      m_ansiHidden = false;
    } else if (n == 29) {
      m_ansiStrikeOut = false;
    } else if (n >= 30 && n <= 37) {
      m_ansiForeground = ansi256Color(n - 30);
    } else if (n == 38 || n == 48) {
      QStringList values = codes.mid(i + 1, 4);
      if (parseNumber(values.value(0), -1) == 2) {

        values.insert(1, QString());
      }
      int consumed = 0;
      const QColor color = extendedColor(values, consumed);
      if (color.isValid()) {
        (n == 38 ? m_ansiForeground : m_ansiBackground) = color;
      }
      i += qMin(consumed, 4);
    } else if (n == 39) {
      m_ansiForeground = QColor(m_textColor);
    } else if (n >= 40 && n <= 47) {
      m_ansiBackground = ansi256Color(n - 40);
    } else if (n == 49) {
      m_ansiBackground = QColor();
    } else if (n >= 90 && n <= 97) {
      m_ansiForeground = ansi256Color(n - 90 + 8);
    } else if (n >= 100 && n <= 107) {
      m_ansiBackground = ansi256Color(n - 100 + 8);
    }
  }
}

void Terminal::handleCsi(const QString &body, QChar finalByte) {
  QString params = body;
  QChar prefix;
  if (!params.isEmpty() && QStringLiteral("<=>?").contains(params.at(0))) {
    prefix = params.at(0);
    params.remove(0, 1);
  }
  QString intermediates;
  while (!params.isEmpty() && params.back().unicode() >= 0x20 &&
         params.back().unicode() <= 0x2f) {
    intermediates.prepend(params.back());
    params.chop(1);
  }

  const QStringList parts = params.split(';', Qt::KeepEmptyParts);
  auto param = [&](int index, int defaultValue) {
    bool ok = false;
    const int value = parts.value(index).toInt(&ok);
    return ok ? value : defaultValue;
  };

  auto count = [&](int index) { return qMax(1, param(index, 1)); };

  const char op = finalByte.toLatin1();

  if (!intermediates.isEmpty()) {
    if (intermediates == QLatin1String("!") && op == 'p') {

      applySgr(QStringLiteral("0"));
      m_scrollTop = 0;
      m_scrollBottom = -1;
      m_autoWrap = true;
      m_insertMode = false;
      m_applicationCursorKeys = false;
      setCursorShown(true);
    }
    return;
  }

  if (prefix == '?') {
    if (op == 'h' || op == 'l') {
      for (int i = 0; i < parts.size(); ++i) {
        setPrivateMode(param(i, 0), op == 'h');
      }
    }
    return;
  }
  if (!prefix.isNull()) {

    return;
  }

  const int rows = screenRows();
  const int columns = screenColumns();
  const int screenRow = m_ansiRow - m_screenTop;

  switch (op) {
  case 'm':
    applySgr(params);
    break;
  case 'A':
    m_ansiRow = m_screenTop + qMax(0, screenRow - count(0));
    m_ansiColumn = qMin(m_ansiColumn, columns - 1);
    break;
  case 'B':
  case 'e':
    m_ansiRow = m_screenTop + qMin(rows - 1, screenRow + count(0));
    m_ansiColumn = qMin(m_ansiColumn, columns - 1);
    break;
  case 'C':
  case 'a':
    m_ansiColumn = qMin(columns - 1, m_ansiColumn + count(0));
    break;
  case 'D':
    m_ansiColumn = qMax(0, qMin(m_ansiColumn, columns - 1) - count(0));
    break;
  case 'E':
    m_ansiRow = m_screenTop + qMin(rows - 1, screenRow + count(0));
    m_ansiColumn = 0;
    break;
  case 'F':
    m_ansiRow = m_screenTop + qMax(0, screenRow - count(0));
    m_ansiColumn = 0;
    break;
  case 'G':
  case '`':
    m_ansiColumn = qBound(0, count(0) - 1, columns - 1);
    break;
  case 'H':
  case 'f':
    m_ansiRow = m_screenTop + qBound(0, count(0) - 1, rows - 1);
    m_ansiColumn = qBound(0, count(1) - 1, columns - 1);
    break;
  case 'd':
    m_ansiRow = m_screenTop + qBound(0, count(0) - 1, rows - 1);
    break;
  case 'I':
    for (int i = 0; i < count(0); ++i) {
      m_ansiColumn = qMin(columns - 1, (m_ansiColumn / 8 + 1) * 8);
    }
    break;
  case 'Z':
    for (int i = 0; i < count(0); ++i) {
      m_ansiColumn = qMax(0, (qMin(m_ansiColumn, columns - 1) - 1) / 8 * 8);
    }
    break;
  case 'J':
    eraseInDisplay(param(0, 0));
    break;
  case 'K':
    eraseInLine(param(0, 0));
    break;
  case 'L':
    if (screenRow >= scrollRegionTop() && screenRow <= scrollRegionBottom()) {
      scrollDown(count(0), screenRow, scrollRegionBottom());
      m_ansiColumn = 0;
    }
    break;
  case 'M':
    if (screenRow >= scrollRegionTop() && screenRow <= scrollRegionBottom()) {
      scrollUp(count(0), screenRow, scrollRegionBottom(), false);
      m_ansiColumn = 0;
    }
    break;
  case 'S':
    scrollUp(count(0), scrollRegionTop(), scrollRegionBottom(), true);
    break;
  case 'T':
    scrollDown(count(0), scrollRegionTop(), scrollRegionBottom());
    break;
  case 'P': {
    QTextCursor cursor = ansiCursor(false);
    const int available = cursor.block().length() - 1 - m_ansiColumn;
    if (available > 0) {
      cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor,
                          qMin(count(0), available));
      cursor.removeSelectedText();
    }
    break;
  }
  case 'X': {
    const int n = qMin(count(0), columns - qMin(m_ansiColumn, columns - 1));
    QTextCursor cursor = ansiCursor(true);
    const int available = cursor.block().length() - 1 - m_ansiColumn;
    if (available > 0) {
      cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor,
                          qMin(n, available));
    }
    cursor.insertText(QString(n, ' '), currentAnsiFormat());
    break;
  }
  case '@': {
    QTextCursor cursor = ansiCursor(true);
    cursor.insertText(QString(count(0), ' '), currentAnsiFormat());

    const QTextBlock block = cursor.block();
    if (block.length() - 1 > columns) {
      cursor.setPosition(block.position() + columns);
      cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
      cursor.removeSelectedText();
    }
    break;
  }
  case 'r': {
    const int top = qBound(0, count(0) - 1, rows - 1);
    const int bottom = qBound(0, param(1, rows) - 1, rows - 1);
    if (top < bottom) {
      m_scrollTop = top;
      m_scrollBottom = bottom;
      m_ansiRow = m_screenTop;
      m_ansiColumn = 0;
    }
    break;
  }
  case 's':
    saveCursorState();
    break;
  case 'u':
    restoreCursorState();
    break;
  case 'h':
  case 'l':
    if (param(0, 0) == 4) {
      m_insertMode = op == 'h';
    }
    break;
  case 'n':
    if (param(0, 0) == 6) {
      sendTerminalReply(QStringLiteral("\x1b[%1;%2R")
                            .arg(screenRow + 1)
                            .arg(qMin(m_ansiColumn, columns - 1) + 1)
                            .toLatin1());
    } else if (param(0, 0) == 5) {
      sendTerminalReply(QByteArrayLiteral("\x1b[0n"));
    }
    break;
  case 'c':
    if (param(0, 0) == 0) {
      sendTerminalReply(QByteArrayLiteral("\x1b[?1;2c"));
    }
    break;
  default:
    break;
  }
}

int Terminal::handleEscapeSequence(const QString &text, int index) {

  if (index + 1 >= text.length()) {
    return index;
  }

  auto skipString = [&](int from) -> int {
    for (int j = from; j < text.length(); ++j) {
      if (text.at(j) == QChar('\x07')) {
        return j;
      }
      if (text.at(j) == QChar('\x1b') && j + 1 < text.length() &&
          text.at(j + 1) == QChar('\\')) {
        return j + 1;
      }
    }
    return text.length() - 1;
  };

  const QChar next = text.at(index + 1);
  switch (next.toLatin1()) {
  case '[': {
    int j = index + 2;
    while (j < text.length() &&
           (text.at(j).unicode() < 0x40 || text.at(j).unicode() > 0x7e)) {
      ++j;
    }
    if (j >= text.length()) {
      return text.length() - 1;
    }
    handleCsi(text.mid(index + 2, j - index - 2), text.at(j));
    return j;
  }
  case ']':
  case 'P':
  case '^':
  case '_':
  case 'X':
    return skipString(index + 2);
  case '7':
    saveCursorState();
    return index + 1;
  case '8':
    restoreCursorState();
    return index + 1;
  case 'D':
    lineFeed();
    return index + 1;
  case 'E':
    lineFeed();
    m_ansiColumn = 0;
    return index + 1;
  case 'M':
    reverseIndex();
    return index + 1;
  case 'c':
    clearDocument();
    ui->textEdit->setDecorationsEnabled(true);
    m_alternateScreenActive = false;
    m_savedPrimaryScreen = QTextDocumentFragment();
    resetAnsiState();
    setCursorShown(true);
    return index + 1;
  default:
    break;
  }

  if (QStringLiteral(" ()#%*+,-./").contains(next)) {
    return qMin(index + 2, text.length() - 1);
  }
  return index + 1;
}

void Terminal::appendAnsiText(const QString &text, QTextCursor &cursor) {
  QString printable;
  auto flush = [&]() {
    if (!printable.isEmpty()) {
      writePrintable(printable);
      printable.clear();
    }
  };

  for (int i = 0; i < text.length(); ++i) {
    const QChar ch = text.at(i);
    const ushort code = ch.unicode();
    if (code >= 0x20 && code != 0x7f && (code < 0x80 || code > 0x9f)) {
      printable.append(ch);
      continue;
    }

    flush();
    switch (code) {
    case 0x1b:
      i = handleEscapeSequence(text, i);
      break;
    case '\r':
      m_ansiColumn = 0;
      break;
    case '\n':
    case '\v':
      lineFeed();

      if (!m_processingPtyOutput) {
        m_ansiColumn = 0;
      }
      break;
    case '\f':
      clearDocument();
      m_screenTop = 0;
      m_ansiRow = 0;
      m_ansiColumn = 0;
      break;
    case '\b':
      m_ansiColumn = qMax(0, qMin(m_ansiColumn, screenColumns() - 1) - 1);
      break;
    case '\t':
      m_ansiColumn = qMin(screenColumns() - 1,
                          (qMin(m_ansiColumn, screenColumns()) / 8 + 1) * 8);
      break;
    default:
      break;
    }
  }
  flush();

  keepCursorOnScreen();
  trimTrailingBlanksAfterCursor();
  cursor = ansiCursor(false);
}

void Terminal::setupContextMenu() {
  m_contextMenu = new QMenu(this);

  m_copyAction = m_contextMenu->addAction(tr("Copy"));
  m_copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
  connect(m_copyAction, &QAction::triggered, this,
          [this]() { copySelectionToClipboard(); });

  QAction *pasteAction = m_contextMenu->addAction(tr("Paste"));
  pasteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
  connect(pasteAction, &QAction::triggered, this,
          [this]() { pasteClipboardText(); });

  m_contextMenu->addSeparator();

  QAction *selectAllAction = m_contextMenu->addAction(tr("Select All"));
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

void Terminal::zoomIn() { setFontSize(m_baseFontSize + 1); }

void Terminal::zoomOut() { setFontSize(m_baseFontSize - 1); }

void Terminal::zoomReset() { setFontSize(kDefaultFontSize); }

void Terminal::setFontSize(int pointSize) {
  if (pointSize < kMinFontSize || pointSize > kMaxFontSize) {
    return;
  }
  m_baseFontSize = pointSize;
  QFont font = ui->textEdit->font();
  font.setPointSize(pointSize);
  ui->textEdit->setFont(font);
  updateStyleSheet();
  updatePtySize();
  emit fontSizeChanged(pointSize);
}

int Terminal::currentFontSize() const { return m_baseFontSize; }

void Terminal::enforceScrollbackLimit() {
  QTextDocument *doc = ui->textEdit->document();
  if (m_alternateScreenActive) {
    return;
  }

  int linesToRemove = 0;
  if (m_scrollbackLines > 0) {
    linesToRemove = qMax(0, doc->blockCount() - m_scrollbackLines);
  }
  if (doc->characterCount() > kMaxDocumentCharacters) {
    const int target = doc->characterCount() - kMaxDocumentCharacters;
    const QTextBlock block = doc->findBlock(target);
    linesToRemove =
        qMax(linesToRemove,
             block.isValid() ? block.blockNumber() + 1 : doc->blockCount());
  }
  linesToRemove = qMin(linesToRemove, m_screenTop);

  if (linesToRemove <= 0) {
    if (doc->characterCount() > kMaxDocumentCharacters) {

      const int charsToRemove = doc->characterCount() - kMaxDocumentCharacters;
      QTextCursor cursor(doc);
      cursor.setPosition(0);
      cursor.setPosition(charsToRemove, QTextCursor::KeepAnchor);
      cursor.removeSelectedText();
      m_inputStartPosition = qMax(0, m_inputStartPosition - charsToRemove);
      syncAnsiCursorToDocumentEnd();
    }
    return;
  }

  const int removedLength = doc->findBlockByNumber(linesToRemove).position();
  removeLines(0, linesToRemove);
  m_inputStartPosition = qMax(0, m_inputStartPosition - removedLength);
  m_ansiRow = qMax(0, m_ansiRow - linesToRemove);
  m_screenTop -= linesToRemove;
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
