#include "vimmode.h"
#include "../core/logging/logger.h"

#include <QApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QTextBlock>
#include <algorithm>
#include <memory>

VimMode::VimMode(QPlainTextEdit *editor, QObject *parent)
    : QObject(parent), m_editor(editor) {
  connect(this, &VimMode::statusMessage, this, [this](const QString &message) {
    static const QRegularExpression error("^E\\d+:");
    if (error.match(message).hasMatch())
      failCommand();
  });
}

VimMode::~VimMode() = default;

std::unique_ptr<QKeyEvent> VimMode::tokenToKeyEvent(const QString &token) {
  static const QMap<QString, QPair<int, QString>> specials = {
      {"<CR>", {Qt::Key_Return, "\r"}},
      {"<BS>", {Qt::Key_Backspace, "\b"}},
      {"<Del>", {Qt::Key_Delete, QString(QChar(0x7f))}},
      {"<Tab>", {Qt::Key_Tab, "\t"}},
      {"<S-Tab>", {Qt::Key_Backtab, QString()}},
      {"<Esc>", {Qt::Key_Escape, QString(QChar(0x1b))}},
      {"<Left>", {Qt::Key_Left, QString()}},
      {"<Right>", {Qt::Key_Right, QString()}},
      {"<Up>", {Qt::Key_Up, QString()}},
      {"<Down>", {Qt::Key_Down, QString()}},
      {"<Home>", {Qt::Key_Home, QString()}},
      {"<End>", {Qt::Key_End, QString()}},
      {"<PageUp>", {Qt::Key_PageUp, QString()}},
      {"<PageDown>", {Qt::Key_PageDown, QString()}},
      {"<Insert>", {Qt::Key_Insert, QString()}},
  };
  auto it = specials.constFind(token);
  if (it != specials.constEnd()) {
    Qt::KeyboardModifiers mods =
        token == "<S-Tab>" ? Qt::ShiftModifier : Qt::NoModifier;
    return std::make_unique<QKeyEvent>(QEvent::KeyPress, it->first, mods,
                                       it->second);
  }
  if (token.size() == 1) {
    QChar c = token[0];
    int key = 0;
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (c.unicode() < 128 && c.isLetter()) {
      key = Qt::Key_A + (c.toUpper().unicode() - 'A');
      if (c.isUpper())
        mods = Qt::ShiftModifier;
    } else if (c.unicode() < 128) {
      key = c.unicode();
    }
    return std::make_unique<QKeyEvent>(QEvent::KeyPress, key, mods, token);
  }
  if (token.size() < 4 || !token.startsWith('<') || !token.endsWith('>'))
    return nullptr;
  QString rest = token.mid(1, token.size() - 2);
  Qt::KeyboardModifiers mods = Qt::NoModifier;
  while (rest.size() >= 3 && rest[1] == '-' &&
         QString("CAS").contains(rest[0])) {
    if (rest[0] == 'C')
      mods |= Qt::ControlModifier;
    else if (rest[0] == 'A')
      mods |= Qt::AltModifier;
    else
      mods |= Qt::ShiftModifier;
    rest = rest.mid(2);
  }
  if (!mods)
    return nullptr;
  int key = 0;
  auto special = specials.constFind("<" + rest + ">");
  if (special != specials.constEnd()) {
    key = special->first;
  } else if (rest == "Space") {
    key = Qt::Key_Space;
  } else if (rest.size() >= 2 && rest[0] == 'F' && rest.mid(1).toInt() > 0) {
    key = Qt::Key_F1 + rest.mid(1).toInt() - 1;
  } else if (rest.startsWith("key") && rest.mid(3).toInt() > 0) {
    key = rest.mid(3).toInt();
  } else if (rest.size() == 1) {
    const QChar c = rest[0];
    key = c.unicode() < 128 && c.isLetter()
              ? Qt::Key_A + (c.toUpper().unicode() - 'A')
              : c.unicode();
  } else {
    return nullptr;
  }
  return std::make_unique<QKeyEvent>(QEvent::KeyPress, key, mods, QString());
}

void VimMode::setEnabled(bool enabled) {
  if (m_enabled == enabled)
    return;
  m_enabled = enabled;
  m_pending.clear();
  m_insertOneCommand = false;
  m_insertEditOpen = false;
  m_blockInsertActive = false;
  m_cmdText.clear();
  emit commandBufferChanged(QString());
  if (enabled) {
    m_mode = VimEditMode::Normal;
    QTextCursor c = m_editor->textCursor();
    c.clearSelection();
    c.setPosition(clampNormal(c.position()));
    m_editor->setTextCursor(c);
    updateCursorShape();
    emit modeChanged(m_mode);
  } else {
    if (m_mode == VimEditMode::VisualBlock)
      emit visualSelectionChanged();
    m_mode = VimEditMode::Insert;
    m_editor->setCursorWidth(1);
    emit modeChanged(m_mode);
  }
  updatePendingKeys();
  LOG_INFO(QString("VIM mode %1").arg(enabled ? "enabled" : "disabled"));
}

bool VimMode::isEnabled() const { return m_enabled; }

VimEditMode VimMode::mode() const { return m_mode; }

QString VimMode::modeName() const {
  switch (m_mode) {
  case VimEditMode::Normal:
    return m_insertOneCommand ? "(insert)" : "NORMAL";
  case VimEditMode::Insert:
    return "INSERT";
  case VimEditMode::Visual:
    return "VISUAL";
  case VimEditMode::VisualLine:
    return "V-LINE";
  case VimEditMode::VisualBlock:
    return "V-BLOCK";
  case VimEditMode::Command:
    return "COMMAND";
  case VimEditMode::Replace:
    return "REPLACE";
  }
  return QString();
}

QString VimMode::commandBuffer() const {
  if (m_mode != VimEditMode::Command)
    return QString();
  return m_cmdType == ':' ? m_cmdText : QString(m_cmdType) + m_cmdText;
}

QString VimMode::commandText() const { return m_cmdText; }

QChar VimMode::commandType() const { return m_cmdType; }

int VimMode::commandCursorPosition() const { return m_cmdCursor; }

void VimMode::setCommandText(const QString &text) {
  if (m_mode != VimEditMode::Command)
    return;
  setCommandBufferInternal(text, text.size());
}

QString VimMode::pendingKeys() const {
  QString keys = tokensToNotation(m_pending);
  if (m_insertRegisterPending || m_cmdRegisterPending)
    keys += "\"";
  return keys;
}

bool VimMode::isRecordingMacro() const { return m_macroRecording; }

QChar VimMode::macroRegister() const { return m_macroRegister; }

QString VimMode::registerContent(QChar reg) const {
  return getRegister(reg).content;
}

VimRegister VimMode::registerValue(QChar reg) const { return getRegister(reg); }

QString VimMode::searchPattern() const { return m_searchPattern; }

void VimMode::setSearchPattern(const QString &pattern) {
  m_searchPattern = pattern;
  m_searchOffset.clear();
  m_searchForward = true;
  m_searchHighlightActive = !pattern.isEmpty();
}

QString VimMode::escapePattern(const QString &literal) {
  QString out;
  for (QChar c : literal) {
    if (QString("\\/.*$^~[]").contains(c))
      out += '\\';
    out += c;
  }
  return out;
}

void VimMode::setTabWidth(int width) {
  if (width <= 0 || width == m_appliedTabWidth)
    return;
  m_appliedTabWidth = width;
  m_tabStop = width;
  m_shiftWidth = width;
}

bool VimMode::processKeyEvent(QKeyEvent *event) {
  if (!m_enabled || m_passthrough || !event)
    return false;
  QString token = keyEventToToken(event);
  if (token.isEmpty())
    return false;
  return handleKey(token, event);
}

bool VimMode::shouldOverrideShortcut(QKeyEvent *event) const {
  if (!m_enabled || !event)
    return false;
  const QString token = keyEventToToken(event);
  if (!token.startsWith("<C-") || token.size() != 5)
    return false;
  const QChar c = token[3];
  switch (m_mode) {
  case VimEditMode::Normal:
  case VimEditMode::Visual:
  case VimEditMode::VisualLine:
  case VimEditMode::VisualBlock:
    return QString("abdefgiloqruvwxy[]").contains(c);
  case VimEditMode::Insert:
  case VimEditMode::Replace:
    return QString("adehortuvwy[").contains(c);
  case VimEditMode::Command:
    return QString("chruvw[").contains(c);
  }
  return false;
}

void VimMode::feedKeys(const QString &keys) {
  if (!m_enabled)
    return;
  for (const QString &token : parseKeyNotation(keys))
    handleKey(token, nullptr);
}

QString VimMode::keyEventToToken(QKeyEvent *event) {
  const int key = event->key();
  switch (key) {
  case Qt::Key_Shift:
  case Qt::Key_Control:
  case Qt::Key_Alt:
  case Qt::Key_AltGr:
  case Qt::Key_Meta:
  case Qt::Key_CapsLock:
  case Qt::Key_NumLock:
  case Qt::Key_ScrollLock:
  case Qt::Key_Super_L:
  case Qt::Key_Super_R:
    return QString();
  default:
    break;
  }
  const Qt::KeyboardModifiers mods = event->modifiers();
  const bool ctrl = mods & Qt::ControlModifier;
  const bool alt = mods & (Qt::AltModifier | Qt::MetaModifier);
  const bool shift = mods & Qt::ShiftModifier;
  const QString text = event->text();

  QString special;
  switch (key) {
  case Qt::Key_Escape:
    special = "Esc";
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    special = "CR";
    break;
  case Qt::Key_Backspace:
    special = "BS";
    break;
  case Qt::Key_Delete:
    special = "Del";
    break;
  case Qt::Key_Tab:
    special = shift ? "S-Tab" : "Tab";
    break;
  case Qt::Key_Backtab:
    special = "S-Tab";
    break;
  case Qt::Key_Left:
    special = "Left";
    break;
  case Qt::Key_Right:
    special = "Right";
    break;
  case Qt::Key_Up:
    special = "Up";
    break;
  case Qt::Key_Down:
    special = "Down";
    break;
  case Qt::Key_Home:
    special = "Home";
    break;
  case Qt::Key_End:
    special = "End";
    break;
  case Qt::Key_PageUp:
    special = "PageUp";
    break;
  case Qt::Key_PageDown:
    special = "PageDown";
    break;
  case Qt::Key_Insert:
    special = "Insert";
    break;
  default:
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
      special = QString("F%1").arg(key - Qt::Key_F1 + 1);
    break;
  }
  if (!special.isEmpty()) {
    QString prefix;
    if (ctrl)
      prefix += "C-";
    if (alt)
      prefix += "A-";
    if (shift && !special.contains("Tab"))
      prefix += "S-";
    return "<" + prefix + special + ">";
  }

  if (ctrl || alt) {
    QChar c;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
      c = QChar('a' + (key - Qt::Key_A));
    else if (key > 0x20 && key < 0x7f)
      c = QChar(key);
    else if (key == Qt::Key_Space)
      return ctrl ? "<C-Space>" : "<A-Space>";
    else
      return QString("<%1%2key%3>")
          .arg(ctrl ? "C-" : "", alt ? "A-" : "")
          .arg(key);
    QString prefix;
    if (ctrl)
      prefix += "C-";
    if (alt)
      prefix += "A-";
    if (shift && c.isLetter())
      prefix += "S-";
    return "<" + prefix + c + ">";
  }

  if (!text.isEmpty()) {
    const QChar c = text[0];
    if (c.unicode() >= 0x20 && c.unicode() != 0x7f)
      return QString(c);
    switch (c.unicode()) {
    case '\r':
    case '\n':
      return "<CR>";
    case '\t':
      return "<Tab>";
    case 0x1b:
      return "<Esc>";
    case 0x08:
      return "<BS>";
    default:
      break;
    }
  }
  if (key == Qt::Key_Space)
    return " ";
  if (key >= Qt::Key_A && key <= Qt::Key_Z)
    return QString(QChar((shift ? 'A' : 'a') + (key - Qt::Key_A)));
  if (key > 0x20 && key < 0x7f)
    return QString(QChar(key));
  return QString();
}

QStringList VimMode::parseKeyNotation(const QString &keys) {
  static const QMap<QString, QString> names = {
      {"lt", "<"},
      {"bar", "|"},
      {"bslash", "\\"},
  };
  static const QMap<QString, QString> specials = {
      {"esc", "Esc"},           {"cr", "CR"},         {"enter", "CR"},
      {"return", "CR"},         {"nl", "CR"},         {"bs", "BS"},
      {"del", "Del"},           {"tab", "Tab"},       {"left", "Left"},
      {"right", "Right"},       {"up", "Up"},         {"down", "Down"},
      {"home", "Home"},         {"end", "End"},       {"pageup", "PageUp"},
      {"pagedown", "PageDown"}, {"insert", "Insert"}, {"space", "Space"},
  };
  auto parseToken = [](const QString &inner) -> QString {
    const QString lowerInner = inner.toLower();
    if (names.contains(lowerInner))
      return names.value(lowerInner);
    bool ctrl = false, alt = false, shift = false;
    QString rest = inner;
    while (rest.size() >= 3 && rest[1] == '-' &&
           QString("CcAaSsMm").contains(rest[0])) {
      const QChar m = rest[0].toLower();
      if (m == 'c')
        ctrl = true;
      else if (m == 's')
        shift = true;
      else
        alt = true;
      rest = rest.mid(2);
    }
    const QString lower = rest.toLower();
    QString prefix;
    if (ctrl)
      prefix += "C-";
    if (alt)
      prefix += "A-";
    if (rest.size() == 1) {
      if (!ctrl && !alt && !shift)
        return QString();
      const QChar c = rest[0];
      if (!ctrl && !alt)
        return QString(c.toUpper());
      if (shift && c.isLetter())
        prefix += "S-";
      return "<" + prefix + (c.isLetter() ? c.toLower() : c) + ">";
    }
    QString special;
    if (specials.contains(lower)) {
      special = specials.value(lower);
    } else if (lower.size() >= 2 && lower[0] == 'f' &&
               lower.mid(1).toInt() > 0) {
      special = "F" + lower.mid(1);
    } else if (lower.startsWith("key") && lower.mid(3).toInt() > 0 &&
               (ctrl || alt)) {
      special = "key" + lower.mid(3);
    } else {
      return QString();
    }
    if (special == "Space" && !ctrl && !alt)
      return " ";
    if (shift) {
      if (special == "Tab")
        special = "S-Tab";
      else
        prefix += "S-";
    }
    return "<" + prefix + special + ">";
  };
  QStringList tokens;
  for (int i = 0; i < keys.size(); ++i) {
    const QChar c = keys[i];
    if (c == '<') {
      int close = keys.indexOf('>', i + 1);
      if (close > i + 1 && close - i <= 24) {
        const QString token = parseToken(keys.mid(i + 1, close - i - 1));
        if (!token.isEmpty()) {
          tokens << token;
          i = close;
          continue;
        }
      }
      tokens << "<";
      continue;
    }
    switch (c.unicode()) {
    case '\n':
    case '\r':
      tokens << "<CR>";
      break;
    case '\t':
      tokens << "<Tab>";
      break;
    case 0x1b:
      tokens << "<Esc>";
      break;
    case 0x08:
      tokens << "<BS>";
      break;
    default:
      tokens << QString(c);
      break;
    }
  }
  return tokens;
}

QString VimMode::tokensToNotation(const QStringList &tokens) {
  QString out;
  for (const QString &t : tokens)
    out += (t == "<") ? QString("<lt>") : t;
  return out;
}

bool VimMode::handleKey(const QString &tokenIn, QKeyEvent *event) {
  QString token = tokenIn;
  if (token == "<C-[>")
    token = "<Esc>";

  const bool recordMacro = m_macroRecording && m_replayDepth == 0;
  if (m_replayDepth == 0)
    m_abortReplay = false;
  if (recordMacro)
    m_macroKeys << token;

  if (m_mode == VimEditMode::Normal || m_mode == VimEditMode::Insert) {
    const int pos = m_editor->textCursor().position();
    if (pos != m_lastSyncedPos) {
      const int line = lineOf(pos);
      m_wantCol = vcolOf(lineText(line), pos - lineStart(line));
      m_wantEol = false;
    }
  }

  bool handled = false;
  switch (m_mode) {
  case VimEditMode::Normal:
    handled = handleNormalKey(token);
    break;
  case VimEditMode::Insert:
    handled = handleInsertKey(token, event);
    break;
  case VimEditMode::Visual:
  case VimEditMode::VisualLine:
  case VimEditMode::VisualBlock:
    handled = handleVisualKey(token);
    break;
  case VimEditMode::Command:
    handled = handleCommandKey(token);
    break;
  case VimEditMode::Replace:
    handled = handleReplaceKey(token, event);
    break;
  }

  m_lastSyncedPos = m_editor->textCursor().position();
  if (!handled && recordMacro && m_macroRecording && !m_macroKeys.isEmpty())
    m_macroKeys.removeLast();
  return handled;
}

QTextDocument *VimMode::doc() const { return m_editor->document(); }

int VimMode::lineCount() const { return doc()->blockCount(); }

QString VimMode::lineText(int line) const {
  QTextBlock b = doc()->findBlockByNumber(line);
  return b.isValid() ? b.text() : QString();
}

int VimMode::lineLength(int line) const {
  QTextBlock b = doc()->findBlockByNumber(line);
  return b.isValid() ? b.length() - 1 : 0;
}

int VimMode::lineStart(int line) const {
  if (line < 0)
    return 0;
  QTextBlock b = doc()->findBlockByNumber(line);
  return b.isValid() ? b.position() : docLength();
}

int VimMode::lineEndPos(int line) const {
  return lineStart(line) + lineLength(line);
}

int VimMode::lineOf(int pos) const {
  return doc()->findBlock(qBound(0, pos, docLength())).blockNumber();
}

int VimMode::colOf(int pos) const {
  pos = qBound(0, pos, docLength());
  return pos - doc()->findBlock(pos).position();
}

int VimMode::posOf(int line, int col) const {
  line = qBound(0, line, lineCount() - 1);
  return lineStart(line) + qBound(0, col, lineLength(line));
}

int VimMode::docLength() const { return doc()->characterCount() - 1; }

QChar VimMode::charAt(int pos) const {
  if (pos < 0 || pos >= docLength())
    return QChar();
  QChar c = doc()->characterAt(pos);
  if (c == QChar::ParagraphSeparator || c == QChar::LineSeparator)
    return '\n';
  return c;
}

int VimMode::firstNonBlankCol(int line) const {
  const QString text = lineText(line);
  int i = 0;
  while (i < text.size() && (text[i] == ' ' || text[i] == '\t'))
    ++i;
  return i;
}

int VimMode::firstNonBlankPos(int line) const {
  line = qBound(0, line, lineCount() - 1);
  return clampNormal(lineStart(line) + firstNonBlankCol(line));
}

int VimMode::vcolOf(const QString &text, int col) const {
  int v = 0;
  for (int i = 0; i < col; ++i) {
    if (i < text.size() && text[i] == '\t')
      v += m_tabStop - (v % m_tabStop);
    else
      ++v;
  }
  return v;
}

int VimMode::colForVcol(const QString &text, int vcol) const {
  int v = 0;
  for (int i = 0; i < text.size(); ++i) {
    int w = text[i] == '\t' ? m_tabStop - (v % m_tabStop) : 1;
    if (v + w > vcol)
      return i;
    v += w;
  }
  return text.size();
}

int VimMode::cursorPos() const {
  if (m_mode == VimEditMode::Visual || m_mode == VimEditMode::VisualLine ||
      m_mode == VimEditMode::VisualBlock)
    return m_visualPos;
  return m_editor->textCursor().position();
}

void VimMode::setCursorPos(int pos, bool updateWantCol) {
  pos = qBound(0, pos, docLength());
  if (m_mode == VimEditMode::Visual || m_mode == VimEditMode::VisualLine ||
      m_mode == VimEditMode::VisualBlock) {
    m_visualPos = pos;
    updateVisualSelection();
  } else {
    QTextCursor c = m_editor->textCursor();
    c.setPosition(pos);
    m_editor->setTextCursor(c);
  }
  if (updateWantCol) {
    int line = lineOf(pos);
    m_wantCol = vcolOf(lineText(line), pos - lineStart(line));
    m_wantEol = false;
  }
}

int VimMode::clampNormal(int pos) const {
  pos = qBound(0, pos, docLength());
  QTextBlock b = doc()->findBlock(pos);
  int len = b.length() - 1;
  int col = pos - b.position();
  if (len <= 0)
    return b.position();
  return b.position() + qMin(col, len - 1);
}

int VimMode::posForWantCol(int line) const {
  line = qBound(0, line, lineCount() - 1);
  const QString text = lineText(line);
  int len = text.size();
  int col = m_wantEol ? len : colForVcol(text, m_wantCol);
  bool allowEol = m_mode == VimEditMode::Insert ||
                  m_mode == VimEditMode::Replace ||
                  (m_mode == VimEditMode::VisualBlock && m_wantEol);
  if (!allowEol)
    col = qMin(col, qMax(0, len - 1));
  return lineStart(line) + qMin(col, len);
}

void VimMode::moveToLineWithWantCol(int line) {
  setCursorPos(posForWantCol(line), false);
}

QString VimMode::indentString(int width) const {
  if (width <= 0)
    return QString();
  if (m_expandTab)
    return QString(width, ' ');
  return QString(width / m_tabStop, '\t') + QString(width % m_tabStop, ' ');
}

int VimMode::indentWidth(const QString &text) const {
  int i = 0;
  while (i < text.size() && (text[i] == ' ' || text[i] == '\t'))
    ++i;
  return vcolOf(text, i);
}

void VimMode::replaceRange(int start, int end, const QString &text) {
  QTextCursor c(doc());
  c.setPosition(qBound(0, start, docLength()));
  c.setPosition(qBound(0, end, docLength()), QTextCursor::KeepAnchor);
  c.insertText(text);
}

bool VimMode::isValidRegister(QChar reg) {
  if (reg.unicode() < 128 && reg.isLetterOrNumber())
    return true;
  return QString("\"-_+*/:.%").contains(reg);
}

void VimMode::setRegister(QChar reg, const QString &text,
                          VimRegisterType type) {
  if (reg == '_' || reg.isNull())
    return;
  VimRegister value;
  value.content = text;
  value.linewise = type == VimRegisterType::Linewise;
  value.blockwise = type == VimRegisterType::Blockwise;
  if (reg.unicode() < 128 && reg.isUpper()) {
    QChar lower = reg.toLower();
    if (m_registers.contains(lower)) {
      VimRegister &existing = m_registers[lower];
      if (value.linewise && !existing.linewise) {
        existing.content += "\n" + text;
        existing.linewise = true;
      } else if (existing.linewise && !value.linewise) {
        existing.content += text + "\n";
      } else {
        existing.content += text;
      }
    } else {
      m_registers[lower] = value;
    }
  } else {
    m_registers[reg] = value;
    if (reg == '+' || reg == '*')
      QApplication::clipboard()->setText(text);
  }
  emit registerContentsChanged();
}

VimRegister VimMode::getRegister(QChar reg) const {
  if (reg.isNull())
    reg = '"';
  if (reg == '"' && m_clipboardUnnamed)
    reg = '+';
  if (reg == '+' || reg == '*') {
    const QString clip = QApplication::clipboard()->text();
    if (m_registers.contains(reg) && m_registers[reg].content == clip)
      return m_registers[reg];
    VimRegister r;
    r.content = clip;
    r.linewise = clip.endsWith('\n');
    return r;
  }
  VimRegister r;
  if (reg == '/') {
    r.content = m_searchPattern;
    return r;
  }
  if (reg == ':') {
    r.content = m_lastExCommand;
    return r;
  }
  if (reg == '.') {
    r.content = m_lastInsertedText;
    return r;
  }
  QChar key = reg.unicode() < 128 ? reg.toLower() : reg;
  return m_registers.value(key);
}

void VimMode::storeDeleted(QChar reg, const QString &text, VimRegisterType type,
                           bool forceNumbered) {
  if (reg == '_')
    return;
  if (reg.isNull() || reg == '"') {
    if (type == VimRegisterType::Linewise || text.contains('\n') ||
        forceNumbered) {
      for (int i = 9; i > 1; --i) {
        QChar from('0' + i - 1);
        if (m_registers.contains(from))
          m_registers[QChar('0' + i)] = m_registers[from];
      }
      setRegister('1', text, type);
    } else {
      setRegister('-', text, type);
    }
    setRegister('"', text, type);
    if (m_clipboardUnnamed)
      setRegister('+', text, type);
    return;
  }
  setRegister(reg, text, type);
  m_registers['"'] = getRegister(reg);
}

void VimMode::storeYanked(QChar reg, const QString &text,
                          VimRegisterType type) {
  if (reg == '_')
    return;
  if (reg.isNull() || reg == '"') {
    setRegister('0', text, type);
    setRegister('"', text, type);
    if (m_clipboardUnnamed)
      setRegister('+', text, type);
    return;
  }
  setRegister(reg, text, type);
  m_registers['"'] = getRegister(reg);
}

void VimMode::setMark(QChar mark, int pos) {
  QTextCursor c(doc());
  c.setPosition(qBound(0, pos, docLength()));
  m_marks[mark == '`' ? QChar('\'') : mark] = c;
}

void VimMode::forgetLines(int first, int last, bool namedMarks) {
  if (first > last)
    return;
  if (namedMarks) {
    const int steps = doc()->availableUndoSteps();
    for (auto it = m_marks.begin(); it != m_marks.end();) {
      const ushort m = it.key().unicode();
      if (m >= 'a' && m <= 'z' && !it->isNull() && it->document() == doc()) {
        const int line = it->blockNumber();
        if (line >= first && line <= last) {
          m_deletedMarks.append({it.key(), line, it->positionInBlock(), steps});
          it = m_marks.erase(it);
          continue;
        }
      }
      ++it;
    }
    while (m_deletedMarks.size() > 200)
      m_deletedMarks.removeFirst();
  }
  for (QTextCursor &c : m_globalMarks) {
    if (c.isNull())
      continue;
    const int line = c.blockNumber();
    if (line >= first && line <= last)
      c = QTextCursor();
  }
}

void VimMode::restoreDeletedMarks() {
  const int steps = doc()->availableUndoSteps();
  for (int k = m_deletedMarks.size() - 1; k >= 0; --k) {
    const DeletedMark d = m_deletedMarks[k];
    if (d.undoSteps < steps)
      continue;
    m_deletedMarks.removeAt(k);
    if (d.line < lineCount())
      setMark(d.mark, posOf(d.line, d.col));
  }
}

bool VimMode::markPosition(QChar mark, int &pos) const {
  if (mark == '`')
    mark = '\'';
  auto it = m_marks.constFind(mark);
  if (it == m_marks.constEnd() || it->isNull() || it->document() != doc())
    return false;
  pos = qBound(0, it->position(), docLength());
  return true;
}

void VimMode::pushJump(int pos) {
  setMark('\'', pos);
  int line = lineOf(pos);
  for (int i = m_jumpList.size() - 1; i >= 0; --i) {
    if (m_jumpList[i].isNull() || m_jumpList[i].document() != doc() ||
        m_jumpList[i].blockNumber() == line)
      m_jumpList.removeAt(i);
  }
  QTextCursor c(doc());
  c.setPosition(pos);
  m_jumpList.append(c);
  while (m_jumpList.size() > 100)
    m_jumpList.removeFirst();
  m_jumpIndex = m_jumpList.size();
}

void VimMode::jumpOlder(int count) {
  if (m_jumpIndex >= m_jumpList.size()) {
    int saved = m_jumpList.size();
    pushJump(cursorPos());
    m_jumpIndex = m_jumpList.size() - 1;
    if (m_jumpList.size() < saved + 1 && m_jumpIndex > 0)
      m_jumpIndex = m_jumpList.size() - 1;
  }
  int target = m_jumpIndex - count;
  if (target < 0 || target >= m_jumpList.size())
    return;
  m_jumpIndex = target;
  setMark('\'', cursorPos());
  setCursorPos(clampNormal(m_jumpList[target].position()));
}

void VimMode::jumpNewer(int count) {
  int target = m_jumpIndex + count;
  if (target >= m_jumpList.size())
    return;
  m_jumpIndex = target;
  setMark('\'', cursorPos());
  setCursorPos(clampNormal(m_jumpList[target].position()));
}

void VimMode::recordChangePosition(int pos) {
  setMark('.', pos);
  int line = lineOf(pos);
  if (!m_changeList.isEmpty() && !m_changeList.last().isNull() &&
      m_changeList.last().document() == doc() &&
      m_changeList.last().blockNumber() == line) {
    m_changeList.last().setPosition(qBound(0, pos, docLength()));
  } else {
    QTextCursor c(doc());
    c.setPosition(qBound(0, pos, docLength()));
    m_changeList.append(c);
    while (m_changeList.size() > 100)
      m_changeList.removeFirst();
  }
  m_changeIndex = m_changeList.size();
}

void VimMode::startMacroRecording(QChar reg) {
  m_macroRecording = true;
  m_macroRegister = reg;
  m_macroKeys.clear();
  emit macroRecordingChanged(true, reg);
  emit statusMessage(QString("recording @%1").arg(reg));
}

void VimMode::stopMacroRecording() {
  if (!m_macroKeys.isEmpty())
    m_macroKeys.removeLast();
  m_macroRecording = false;
  setRegister(m_macroRegister, tokensToNotation(m_macroKeys),
              VimRegisterType::Charwise);
  m_lastMacroRegister = m_macroRegister.toLower();
  emit macroRecordingChanged(false, QChar());
  emit statusMessage(QString("Recorded @%1").arg(m_macroRegister));
}

void VimMode::playMacro(QChar reg, int count) {
  if (reg == '@') {
    if (m_lastMacroRegister.isNull()) {
      emit statusMessage("E748: No previously used register");
      return;
    }
    reg = m_lastMacroRegister;
  }
  if (m_replayDepth > 100)
    return;
  m_lastMacroRegister = reg;
  if (reg == ':') {
    if (m_lastExCommand.isEmpty()) {
      emit statusMessage("E30: No previous command line");
      return;
    }
    const QString command = m_lastExCommand;
    m_repeatedCommandLine = true;
    ++m_replayDepth;
    for (int i = 0; i < count && !m_abortReplay; ++i)
      executeEx(command);
    --m_replayDepth;
    return;
  }
  VimRegister r = getRegister(reg);
  if (r.content.isEmpty()) {
    emit statusMessage(QString("Register @%1 is empty").arg(reg));
    return;
  }
  const QStringList tokens = parseKeyNotation(r.content);
  for (int i = 0; i < count && !m_abortReplay; ++i)
    replayTokens(tokens);
}

void VimMode::replayTokens(const QStringList &tokens, bool stopOnFailure) {
  ++m_replayDepth;
  for (const QString &token : tokens) {
    if (stopOnFailure && m_abortReplay)
      break;
    handleKey(token, nullptr);
  }
  --m_replayDepth;
}

void VimMode::failCommand() { m_abortReplay = true; }

void VimMode::setDotCommand(const QStringList &keys) {
  if (m_inDotRepeat || keys.isEmpty())
    return;
  m_dotKeys = keys;
  m_dotCount = 0;
}

void VimMode::repeatLastChange(int count) {
  if (m_dotKeys.isEmpty())
    return;
  if (count > 0)
    m_dotCount = count;
  if (m_dotKeys.size() > 2 && m_dotKeys[0] == "\"" &&
      m_dotKeys[1].size() == 1 && m_dotKeys[1][0] >= '1' &&
      m_dotKeys[1][0] < '9')
    m_dotKeys[1] = QString(QChar(m_dotKeys[1][0].unicode() + 1));
  const QStringList keys = m_dotKeys;
  QTextCursor block(doc());
  block.beginEditBlock();
  const bool outerAbort = m_abortReplay;
  m_inDotRepeat = true;
  m_dotCountOverride = m_dotCount;
  replayTokens(keys, false);
  if (m_mode == VimEditMode::Insert || m_mode == VimEditMode::Replace)
    handleKey("<Esc>", nullptr);
  m_dotCountOverride = 0;
  m_inDotRepeat = false;
  m_abortReplay = outerAbort;
  block.endEditBlock();
  m_dotKeys = keys;
}

void VimMode::setMode(VimEditMode mode) {
  if (m_mode == mode) {
    updateCursorShape();
    return;
  }
  VimEditMode old = m_mode;
  m_mode = mode;
  if ((old == VimEditMode::Insert || old == VimEditMode::Replace) &&
      mode != VimEditMode::Insert && mode != VimEditMode::Replace)
    m_insertEditOpen = false;
  if (old == VimEditMode::VisualBlock || mode == VimEditMode::VisualBlock)
    emit visualSelectionChanged();
  updateCursorShape();
  emit modeChanged(mode);
  updatePendingKeys();
  LOG_DEBUG(QString("VIM mode changed to: %1").arg(modeName()));
}

void VimMode::updateCursorShape() {
  const int charWidth = qMax(2, m_editor->fontMetrics().horizontalAdvance('M'));
  switch (m_mode) {
  case VimEditMode::Insert:
    m_editor->setCursorWidth(1);
    break;
  case VimEditMode::Replace:
    m_editor->setCursorWidth(qMax(2, charWidth / 2));
    break;
  case VimEditMode::Visual:
  case VimEditMode::VisualLine:
    m_editor->setCursorWidth(2);
    break;
  default:
    m_editor->setCursorWidth(charWidth);
    break;
  }
}

void VimMode::updatePendingKeys() { emit pendingKeysChanged(pendingKeys()); }

void VimMode::resetPending() {
  m_pending.clear();
  updatePendingKeys();
}

int VimMode::visibleLineCount() const {
  int lineHeight = m_editor->fontMetrics().lineSpacing();
  if (lineHeight <= 0)
    lineHeight = 16;
  return qMax(1, m_editor->viewport()->height() / lineHeight);
}

int VimMode::firstVisibleLine() const {
  return m_editor->cursorForPosition(QPoint(0, 0)).blockNumber();
}

void VimMode::scrollLines(int lines, bool moveCursorWithView) {
  QScrollBar *bar = m_editor->verticalScrollBar();
  bar->setValue(bar->value() + lines);
  int first = firstVisibleLine();
  int last = qMin(lineCount() - 1, first + visibleLineCount() - 1);
  int line = lineOf(cursorPos());
  if (moveCursorWithView) {
    line = qBound(0, line + lines, lineCount() - 1);
    moveToLineWithWantCol(line);
    return;
  }
  if (line < first)
    moveToLineWithWantCol(first);
  else if (line > last && last >= first)
    moveToLineWithWantCol(last);
}

void VimMode::scrollHalfPage(bool down, int count) {
  int amount = count > 0 ? count : qMax(1, visibleLineCount() / 2);
  int line = lineOf(cursorPos());
  int target = qBound(0, line + (down ? amount : -amount), lineCount() - 1);
  QScrollBar *bar = m_editor->verticalScrollBar();
  bar->setValue(bar->value() + (down ? amount : -amount));
  setCursorPos(firstNonBlankPos(target));
}

void VimMode::scrollPage(bool down, int count) {
  const int amount = int(qMin<qint64>(
      qint64(qMax(1, count)) * qMax(1, visibleLineCount() - 2), lineCount()));
  int line = lineOf(cursorPos());
  int target = qBound(0, line + (down ? amount : -amount), lineCount() - 1);
  QScrollBar *bar = m_editor->verticalScrollBar();
  bar->setValue(bar->value() + (down ? amount : -amount));
  setCursorPos(firstNonBlankPos(target));
}

void VimMode::scrollCursorTo(int where, bool firstNonBlank) {
  int line = lineOf(cursorPos());
  int visible = visibleLineCount();
  int value = line;
  if (where == 1)
    value = line - visible / 2;
  else if (where == 2)
    value = line - visible + 1;
  if (firstNonBlank)
    setCursorPos(firstNonBlankPos(line));
  m_editor->verticalScrollBar()->setValue(qMax(0, value));
}
