#include "vimmode.h"
#include "../core/logging/logger.h"

#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <algorithm>

static const int VIM_PAGE_SIZE = 20;
static const int VIM_HALF_PAGE_SIZE = 10;

VimMode::VimMode(QPlainTextEdit *editor, QObject *parent)
    : QObject(parent), m_editor(editor), m_enabled(false),
      m_mode(VimEditMode::Normal), m_pendingOperator(VimOperator::None),
      m_count(0), m_searchForward(true), m_searchHighlightActive(false),
      m_pendingRegister(QChar()), m_recording(false), m_recordCount(1),
      m_replaying(false), m_macroRecording(false),
      m_lastInsertPosition(-1), m_lastVisualStart(-1), m_lastVisualEnd(-1),
      m_lastVisualMode(VimEditMode::Normal), m_commandHistoryIndex(-1) {}

VimMode::~VimMode() = default;

void VimMode::setEnabled(bool enabled) {
  if (m_enabled != enabled) {
    m_enabled = enabled;
    if (enabled) {
      m_commandBuffer.clear();
      emit commandBufferChanged(m_commandBuffer);
      setMode(VimEditMode::Normal);
      m_editor->setCursorWidth(m_editor->fontMetrics().horizontalAdvance('M'));
    } else {
      // Close any open undo group before disabling
      if (m_insertUndoOpen) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.endEditBlock();
        m_insertUndoOpen = false;
      }
      m_commandBuffer.clear();
      emit commandBufferChanged(m_commandBuffer);
      m_mode = VimEditMode::Insert;
      m_editor->setCursorWidth(1);
      emit modeChanged(m_mode);
    }
    LOG_INFO(QString("VIM mode %1").arg(enabled ? "enabled" : "disabled"));
  }
}

bool VimMode::isEnabled() const { return m_enabled; }

VimEditMode VimMode::mode() const { return m_mode; }

QString VimMode::modeName() const {
  switch (m_mode) {
  case VimEditMode::Normal:
    return "NORMAL";
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
  default:
    return "";
  }
}

QString VimMode::commandBuffer() const { return m_commandBuffer; }

QString VimMode::pendingKeys() const {
  QString keys;
  if (m_pendingRegister != QChar())
    keys += QString("\"%1").arg(m_pendingRegister);
  if (m_count > 0)
    keys += QString::number(m_count);
  if (m_pendingOperator != VimOperator::None) {
    switch (m_pendingOperator) {
    case VimOperator::Delete: keys += "d"; break;
    case VimOperator::Change: keys += "c"; break;
    case VimOperator::Yank: keys += "y"; break;
    case VimOperator::Indent: keys += ">"; break;
    case VimOperator::Unindent: keys += "<"; break;
    case VimOperator::ToggleCase: keys += "g~"; break;
    case VimOperator::Lowercase: keys += "gu"; break;
    case VimOperator::Uppercase: keys += "gU"; break;
    default: break;
    }
  }
  if (!m_commandBuffer.isEmpty() && m_mode == VimEditMode::Normal)
    keys += m_commandBuffer;
  return keys;
}

bool VimMode::isRecordingMacro() const { return m_macroRecording; }
QChar VimMode::macroRegister() const { return m_macroRegister; }

QString VimMode::searchPattern() const { return m_searchPattern; }

void VimMode::setSearchPattern(const QString &pattern) {
  m_searchPattern = pattern;
  m_searchHighlightActive = !pattern.isEmpty();
}

QString VimMode::registerContent(QChar reg) const {
  return getRegister(reg).content;
}

bool VimMode::processKeyEvent(QKeyEvent *event) {
  if (!m_enabled) {
    return false;
  }

  // Record keys for macro
  if (m_macroRecording && !m_replaying) {
    m_macroKeyCodes.append(event->key());
    m_macroKeyMods.append(event->modifiers());
    m_macroKeyTexts.append(event->text());
  }

  // Record keys for dot-repeat (only in normal/visual entering insert/change)
  if (m_recording && !m_replaying) {
    m_recordKeyCodes.append(event->key());
    m_recordKeyMods.append(event->modifiers());
    m_recordKeyTexts.append(event->text());
  }

  switch (m_mode) {
  case VimEditMode::Normal:
    return handleNormalMode(event);
  case VimEditMode::Insert:
    return handleInsertMode(event);
  case VimEditMode::Visual:
  case VimEditMode::VisualLine:
  case VimEditMode::VisualBlock:
    return handleVisualMode(event);
  case VimEditMode::Command:
    return handleCommandMode(event);
  case VimEditMode::Replace:
    return handleReplaceMode(event);
  default:
    return false;
  }
}

// ============== REGISTER SYSTEM ==============

void VimMode::setRegister(QChar reg, const QString &text, bool linewise) {
  if (reg == '_')
    return; // black hole register
  VimRegister r;
  r.content = text;
  r.linewise = linewise;
  if (reg >= 'A' && reg <= 'Z') {
    // Append to lowercase register
    QChar lower = reg.toLower();
    if (m_registers.contains(lower)) {
      m_registers[lower].content += text;
    } else {
      m_registers[lower] = r;
    }
  } else {
    m_registers[reg] = r;
  }
  if (reg == '+' || reg == '*') {
    QApplication::clipboard()->setText(text);
  }
  emit registerContentsChanged();
}

VimRegister VimMode::getRegister(QChar reg) const {
  if (reg == '+' || reg == '*') {
    VimRegister r;
    r.content = QApplication::clipboard()->text();
    return r;
  }
  QChar key = reg.toLower();
  if (m_registers.contains(key))
    return m_registers[key];
  return VimRegister();
}

void VimMode::pushDeleteHistory(const QString &text, bool linewise) {
  VimRegister r;
  r.content = text;
  r.linewise = linewise;
  m_deleteHistory.prepend(r);
  while (m_deleteHistory.size() > 9)
    m_deleteHistory.removeLast();
  // Update "1-"9 registers
  for (int i = 0; i < m_deleteHistory.size(); ++i) {
    m_registers[QChar('1' + i)] = m_deleteHistory[i];
  }
}

void VimMode::yankToRegister(const QString &text, bool linewise) {
  QChar reg = m_pendingRegister;
  if (reg == QChar()) {
    setRegister('"', text, linewise);
    setRegister('0', text, linewise);
  } else {
    setRegister(reg, text, linewise);
    setRegister('"', text, linewise);
  }
  m_pendingRegister = QChar();
}

void VimMode::deleteToRegister(const QString &text, bool linewise) {
  QChar reg = m_pendingRegister;
  if (reg == QChar()) {
    setRegister('"', text, linewise);
    pushDeleteHistory(text, linewise);
  } else {
    setRegister(reg, text, linewise);
    setRegister('"', text, linewise);
  }
  m_pendingRegister = QChar();
}

void VimMode::pasteFromRegister(QChar reg, bool after) {
  if (reg == QChar())
    reg = '"';
  VimRegister r = getRegister(reg);
  if (r.content.isEmpty())
    return;
  QTextCursor cursor = m_editor->textCursor();
  if (r.linewise) {
    if (after) {
      cursor.movePosition(QTextCursor::EndOfLine);
      cursor.insertText("\n" + r.content);
    } else {
      cursor.movePosition(QTextCursor::StartOfLine);
      cursor.insertText(r.content + "\n");
      cursor.movePosition(QTextCursor::Up);
    }
  } else {
    if (after)
      cursor.movePosition(QTextCursor::Right);
    cursor.insertText(r.content);
  }
  m_editor->setTextCursor(cursor);
}

// ============== MACRO SYSTEM ==============

void VimMode::startMacroRecording(QChar reg) {
  m_macroRecording = true;
  m_macroRegister = reg;
  m_macroKeyCodes.clear();
  m_macroKeyMods.clear();
  m_macroKeyTexts.clear();
  emit macroRecordingChanged(true, reg);
  emit statusMessage(QString("Recording @%1").arg(reg));
}

void VimMode::stopMacroRecording() {
  // Remove the final 'q' from the recording
  if (!m_macroKeyCodes.isEmpty()) {
    m_macroKeyCodes.removeLast();
    m_macroKeyMods.removeLast();
    m_macroKeyTexts.removeLast();
  }
  // Store the macro
  QString macroContent;
  for (int i = 0; i < m_macroKeyTexts.size(); ++i)
    macroContent += m_macroKeyTexts[i];
  setRegister(m_macroRegister, macroContent);

  m_macroRecording = false;
  m_lastMacroRegister = m_macroRegister;
  emit macroRecordingChanged(false, QChar());
  emit statusMessage(QString("Recorded @%1").arg(m_macroRegister));
}

void VimMode::playbackMacro(QChar reg, int count) {
  if (reg == '@' && m_lastMacroRegister != QChar())
    reg = m_lastMacroRegister;
  // Find macro in stored key sequences - simple replay via stored register
  // For now we search by register key code arrays if they were recorded here
  // Otherwise fall back to register content text
  m_lastMacroRegister = reg;
  VimRegister r = getRegister(reg);
  if (r.content.isEmpty()) {
    emit statusMessage(QString("Empty register @%1").arg(reg));
    return;
  }

  m_replaying = true;
  for (int c = 0; c < count; ++c) {
    for (int i = 0; i < r.content.length(); ++i) {
      QChar ch = r.content[i];
      int key = ch.toUpper().unicode();
      Qt::KeyboardModifiers mods = Qt::NoModifier;
      if (ch.isUpper())
        mods = Qt::ShiftModifier;
      QKeyEvent ev(QEvent::KeyPress, key, mods, QString(ch));
      processKeyEvent(&ev);
    }
  }
  m_replaying = false;
}

// ============== DOT-REPEAT SYSTEM ==============

void VimMode::beginChangeRecording(int count) {
  m_recording = true;
  m_recordKeyCodes.clear();
  m_recordKeyMods.clear();
  m_recordKeyTexts.clear();
  m_recordCount = count;
}

void VimMode::endChangeRecording() {
  if (!m_recording) return;
  m_recording = false;
  m_lastReplayable.keyCodes = m_recordKeyCodes;
  m_lastReplayable.keyMods = m_recordKeyMods;
  m_lastReplayable.keyTexts = m_recordKeyTexts;
  m_lastReplayable.count = m_recordCount;
}

void VimMode::repeatLastChange() {
  if (m_lastReplayable.keyCodes.isEmpty()) {
    emit statusMessage("No change to repeat");
    return;
  }
  m_replaying = true;
  for (int i = 0; i < m_lastReplayable.keyCodes.size(); ++i) {
    QKeyEvent ev(QEvent::KeyPress, m_lastReplayable.keyCodes[i],
                 m_lastReplayable.keyMods[i], m_lastReplayable.keyTexts[i]);
    processKeyEvent(&ev);
  }
  m_replaying = false;
}

// ============== INCREMENT/DECREMENT ==============

void VimMode::incrementNumber(int delta) {
  QTextCursor cursor = m_editor->textCursor();
  QString line = cursor.block().text();
  int col = cursor.positionInBlock();

  // Find number at or after cursor on current line
  QRegularExpression numRegex("(-?\\d+)");
  QRegularExpressionMatchIterator it = numRegex.globalMatch(line);
  while (it.hasNext()) {
    QRegularExpressionMatch match = it.next();
    int start = match.capturedStart();
    int end = match.capturedEnd();
    if (end > col || (start <= col && end > col)) {
      bool ok;
      long long val = match.captured(1).toLongLong(&ok);
      if (ok) {
        val += delta;
        QString newNum = QString::number(val);
        int blockPos = cursor.block().position();
        cursor.beginEditBlock();
        cursor.setPosition(blockPos + start);
        cursor.setPosition(blockPos + end, QTextCursor::KeepAnchor);
        cursor.insertText(newNum);
        cursor.endEditBlock();
        m_editor->setTextCursor(cursor);
        emit statusMessage(QString::number(val));
        return;
      }
    }
  }
  emit statusMessage("No number found");
}

// ============== SEARCH ==============

void VimMode::clearSearchHighlight() {
  m_searchHighlightActive = false;
  emit searchHighlightRequested("", false);
}

// ============== LAST INSERT / VISUAL TRACKING ==============

void VimMode::trackInsertPosition() {
  m_lastInsertPosition = m_editor->textCursor().position();
}

// ============== PENDING KEYS DISPLAY ==============

void VimMode::updatePendingKeys() {
  emit pendingKeysChanged(pendingKeys());
}

// ============== g-PREFIX HANDLER ==============

bool VimMode::handleGPrefix(QKeyEvent *event, int count) {
  int key = event->key();
  Qt::KeyboardModifiers mods = event->modifiers();
  QString text = event->text();

  if (key == Qt::Key_G) {
    // gg -> go to file start (or line N with count)
    if (count > 1) {
      QTextCursor cursor = m_editor->textCursor();
      QTextBlock block = m_editor->document()->findBlockByNumber(count - 1);
      if (block.isValid()) {
        cursor.setPosition(block.position());
        m_editor->setTextCursor(cursor);
      }
    } else {
      executeMotion(VimMotion::FileStart);
    }
    return true;
  }
  if (key == Qt::Key_I && !(mods & Qt::ShiftModifier)) {
    // gi -> go to last insert position
    if (m_lastInsertPosition >= 0) {
      QTextCursor cursor = m_editor->textCursor();
      cursor.setPosition(m_lastInsertPosition);
      m_editor->setTextCursor(cursor);
    }
    setMode(VimEditMode::Insert);
    return true;
  }
  if (key == Qt::Key_V && !(mods & Qt::ShiftModifier)) {
    // gv -> reselect last visual
    if (m_lastVisualStart >= 0 && m_lastVisualEnd >= 0) {
      QTextCursor cursor = m_editor->textCursor();
      cursor.setPosition(m_lastVisualStart);
      cursor.setPosition(m_lastVisualEnd, QTextCursor::KeepAnchor);
      m_editor->setTextCursor(cursor);
      setMode(m_lastVisualMode != VimEditMode::Normal ? m_lastVisualMode
                                                       : VimEditMode::Visual);
    }
    return true;
  }
  if (key == Qt::Key_AsciiTilde) {
    // g~ -> toggle case operator
    m_pendingOperator = VimOperator::ToggleCase;
    m_commandBuffer.clear();
    updatePendingKeys();
    return true;
  }
  if (key == Qt::Key_U && !(mods & Qt::ShiftModifier)) {
    // gu -> lowercase operator
    m_pendingOperator = VimOperator::Lowercase;
    m_commandBuffer.clear();
    updatePendingKeys();
    return true;
  }
  if (key == Qt::Key_U && (mods & Qt::ShiftModifier)) {
    // gU -> uppercase operator
    m_pendingOperator = VimOperator::Uppercase;
    m_commandBuffer.clear();
    updatePendingKeys();
    return true;
  }

  return false;
}

// ============== NORMAL MODE ==============

bool VimMode::handleNormalMode(QKeyEvent *event) {
  int key = event->key();
  Qt::KeyboardModifiers mods = event->modifiers();
  QString text = event->text();

  // Handle register prefix: "{reg}
  if (!text.isEmpty() && m_commandBuffer == "\"") {
    m_pendingRegister = text[0];
    m_commandBuffer.clear();
    updatePendingKeys();
    return true;
  }

  // Count accumulator
  if (!text.isEmpty() && text[0].isDigit() && (m_count > 0 || text[0] != '0')) {
    m_count = m_count * 10 + text[0].digitValue();
    updatePendingKeys();
    return true;
  }

  int count = qMax(1, m_count);
  m_count = 0;

  // Pending operator awaiting motion/text-object
  if (m_pendingOperator != VimOperator::None) {
    VimMotion motion = VimMotion::None;

    if (m_commandBuffer == "i" || m_commandBuffer == "a") {
      bool inner = (m_commandBuffer == "i");
      VimTextObject textObj = VimTextObject::None;

      switch (key) {
      case Qt::Key_W:
        if (mods & Qt::ShiftModifier)
          textObj = inner ? VimTextObject::InnerWORD : VimTextObject::AroundWORD;
        else
          textObj = inner ? VimTextObject::InnerWord : VimTextObject::AroundWord;
        break;
      case Qt::Key_ParenLeft:
      case Qt::Key_ParenRight:
      case Qt::Key_B:
        if (m_commandBuffer == "i" || m_commandBuffer == "a")
          textObj =
              inner ? VimTextObject::InnerParen : VimTextObject::AroundParen;
        break;
      case Qt::Key_BracketLeft:
      case Qt::Key_BracketRight:
        textObj =
            inner ? VimTextObject::InnerBracket : VimTextObject::AroundBracket;
        break;
      case Qt::Key_BraceLeft:
      case Qt::Key_BraceRight:
        textObj =
            inner ? VimTextObject::InnerBrace : VimTextObject::AroundBrace;
        break;
      case Qt::Key_Less:
      case Qt::Key_Greater:
        textObj =
            inner ? VimTextObject::InnerAngle : VimTextObject::AroundAngle;
        break;
      case Qt::Key_QuoteDbl:
        textObj =
            inner ? VimTextObject::InnerQuote : VimTextObject::AroundQuote;
        break;
      case Qt::Key_Apostrophe:
        textObj = inner ? VimTextObject::InnerSingleQuote
                        : VimTextObject::AroundSingleQuote;
        break;
      case Qt::Key_QuoteLeft:
        textObj = inner ? VimTextObject::InnerBacktick
                        : VimTextObject::AroundBacktick;
        break;
      case Qt::Key_P:
        textObj = inner ? VimTextObject::InnerParagraph
                        : VimTextObject::AroundParagraph;
        break;
      case Qt::Key_S:
        textObj = inner ? VimTextObject::InnerSentence
                        : VimTextObject::AroundSentence;
        break;
      case Qt::Key_T:
        textObj =
            inner ? VimTextObject::InnerTag : VimTextObject::AroundTag;
        break;
      default:
        break;
      }

      if (textObj != VimTextObject::None) {
        beginChangeRecording(count);
        executeOperatorOnTextObject(m_pendingOperator, textObj);
        m_pendingOperator = VimOperator::None;
        m_commandBuffer.clear();
        if (m_mode != VimEditMode::Insert)
          endChangeRecording();
        updatePendingKeys();
        return true;
      }
      m_commandBuffer.clear();
    }

    if (key == Qt::Key_I && !(mods & Qt::ShiftModifier)) {
      m_commandBuffer = "i";
      updatePendingKeys();
      return true;
    }
    if (key == Qt::Key_A && !(mods & Qt::ShiftModifier)) {
      m_commandBuffer = "a";
      updatePendingKeys();
      return true;
    }

    // Double operator -> operate on line
    if ((key == Qt::Key_D && m_pendingOperator == VimOperator::Delete) ||
        (key == Qt::Key_Y && m_pendingOperator == VimOperator::Yank) ||
        (key == Qt::Key_C && m_pendingOperator == VimOperator::Change) ||
        (key == Qt::Key_Greater && m_pendingOperator == VimOperator::Indent) ||
        (key == Qt::Key_Less && m_pendingOperator == VimOperator::Unindent)) {

      QTextCursor cursor = m_editor->textCursor();
      cursor.movePosition(QTextCursor::StartOfLine);
      cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);

      if (m_pendingOperator == VimOperator::Indent ||
          m_pendingOperator == VimOperator::Unindent) {
        QString line = cursor.selectedText();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        if (m_pendingOperator == VimOperator::Indent) {
          cursor.insertText("    " + line);
        } else {
          if (line.startsWith("    ")) {
            cursor.insertText(line.mid(4));
          } else if (line.startsWith("\t")) {
            cursor.insertText(line.mid(1));
          } else {
            int spaces = 0;
            while (spaces < line.length() && line[spaces] == ' ' && spaces < 4)
              spaces++;
            cursor.insertText(line.mid(spaces));
          }
        }
        m_editor->setTextCursor(cursor);
      } else {
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        QString selected = cursor.selectedText();
        if (m_pendingOperator == VimOperator::Yank) {
          yankToRegister(selected, true);
        } else if (m_pendingOperator == VimOperator::Change) {
          cursor.beginEditBlock();
          m_editor->setTextCursor(cursor);
          m_insertUndoOpen = true;
          deleteToRegister(selected, true);
          cursor = m_editor->textCursor();
          cursor.removeSelectedText();
          m_editor->setTextCursor(cursor);
          beginChangeRecording(count);
          trackInsertPosition();
          m_mode = VimEditMode::Insert;
          m_editor->setCursorWidth(1);
          emit modeChanged(m_mode);
          updatePendingKeys();
        } else {
          deleteToRegister(selected, true);
          cursor.removeSelectedText();
          m_editor->setTextCursor(cursor);
        }
      }
      m_pendingOperator = VimOperator::None;
      updatePendingKeys();
      return true;
    }

    // g~/gu/gU doubled -> line
    if (m_pendingOperator == VimOperator::ToggleCase ||
        m_pendingOperator == VimOperator::Lowercase ||
        m_pendingOperator == VimOperator::Uppercase) {
      // Check for doubled: g~g~, gugu, gUgU (simplified: accept motion keys)
    }

    switch (key) {
    case Qt::Key_W:
      motion = (mods & Qt::ShiftModifier) ? VimMotion::WORDForward
                                           : VimMotion::WordForward;
      break;
    case Qt::Key_B:
      motion = (mods & Qt::ShiftModifier) ? VimMotion::WORDBack
                                           : VimMotion::WordBack;
      break;
    case Qt::Key_E:
      motion = (mods & Qt::ShiftModifier) ? VimMotion::WORDEnd
                                           : VimMotion::WordEnd;
      break;
    case Qt::Key_H:
      motion = VimMotion::Left;
      break;
    case Qt::Key_L:
      motion = VimMotion::Right;
      break;
    case Qt::Key_J:
      motion = VimMotion::Down;
      break;
    case Qt::Key_K:
      motion = VimMotion::Up;
      break;
    case Qt::Key_0:
      motion = VimMotion::LineStart;
      break;
    case Qt::Key_Dollar:
      motion = VimMotion::LineEnd;
      break;
    case Qt::Key_AsciiCircum:
      motion = VimMotion::FirstNonSpace;
      break;
    case Qt::Key_Percent:
      motion = VimMotion::MatchingBrace;
      break;
    case Qt::Key_BraceLeft:
      motion = VimMotion::PrevParagraph;
      break;
    case Qt::Key_BraceRight:
      motion = VimMotion::NextParagraph;
      break;
    case Qt::Key_G:
      if (mods & Qt::ShiftModifier)
        motion = VimMotion::FileEnd;
      else
        motion = VimMotion::FileStart;
      break;
    default:
      m_pendingOperator = VimOperator::None;
      updatePendingKeys();
      return false;
    }

    executeOperator(m_pendingOperator, motion, count);
    m_pendingOperator = VimOperator::None;
    updatePendingKeys();
    return true;
  }

  // g-prefix commands
  if (!m_commandBuffer.isEmpty()) {
    if (m_commandBuffer == "g") {
      bool handled = handleGPrefix(event, count);
      m_commandBuffer.clear();
      updatePendingKeys();
      return handled;
    }
    if (m_commandBuffer == "r" && !text.isEmpty()) {
      replaceChar(text[0]);
      m_commandBuffer.clear();
      updatePendingKeys();
      return true;
    }

    if ((m_commandBuffer == "f" || m_commandBuffer == "F" ||
         m_commandBuffer == "t" || m_commandBuffer == "T") &&
        !text.isEmpty()) {
      bool backward = (m_commandBuffer == "F" || m_commandBuffer == "T");
      bool before = (m_commandBuffer == "t" || m_commandBuffer == "T");
      m_findChar = text[0];
      m_findCharBefore = before;
      m_findCharBackward = backward;
      moveCursorToChar(m_findChar, before, backward);
      m_commandBuffer.clear();
      updatePendingKeys();
      return true;
    }

    if (m_commandBuffer == "m" && !text.isEmpty()) {
      QChar mark = text[0];
      if ((mark >= 'a' && mark <= 'z') || (mark >= 'A' && mark <= 'Z')) {
        setMark(mark);
      }
      m_commandBuffer.clear();
      updatePendingKeys();
      return true;
    }

    if (m_commandBuffer == "'" && !text.isEmpty()) {
      QChar mark = text[0];
      if ((mark >= 'a' && mark <= 'z') || (mark >= 'A' && mark <= 'Z')) {
        jumpToMark(mark);
      }
      m_commandBuffer.clear();
      updatePendingKeys();
      return true;
    }

    if (m_commandBuffer == "z") {
      if (key == Qt::Key_Z || text == "z") {
        m_editor->centerCursor();
      } else if (key == Qt::Key_T || text == "t") {
        m_editor->centerCursor();
        for (int i = 0; i < VIM_HALF_PAGE_SIZE; ++i) {
          m_editor->verticalScrollBar()->setValue(
              m_editor->verticalScrollBar()->value() -
              m_editor->fontMetrics().height());
        }
      } else if (key == Qt::Key_B || text == "b") {
        m_editor->centerCursor();
        for (int i = 0; i < VIM_HALF_PAGE_SIZE; ++i) {
          m_editor->verticalScrollBar()->setValue(
              m_editor->verticalScrollBar()->value() +
              m_editor->fontMetrics().height());
        }
      }
      m_commandBuffer.clear();
      updatePendingKeys();
      return true;
    }

    // Macro playback: @{reg}
    if (m_commandBuffer == "@" && !text.isEmpty()) {
      QChar reg = text[0];
      playbackMacro(reg, count);
      m_commandBuffer.clear();
      updatePendingKeys();
      return true;
    }

    m_commandBuffer.clear();
    updatePendingKeys();
  }

  switch (key) {

  case Qt::Key_I:
    if (mods & Qt::ShiftModifier) {
      // I -> insert at first non-space
      moveCursor(QTextCursor::StartOfLine);
      QTextCursor cursor = m_editor->textCursor();
      QString line = cursor.block().text();
      int pos = 0;
      while (pos < line.length() && line[pos].isSpace())
        pos++;
      cursor.movePosition(QTextCursor::StartOfLine);
      cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, pos);
      m_editor->setTextCursor(cursor);
    }
    beginChangeRecording(count);
    trackInsertPosition();
    setMode(VimEditMode::Insert);
    return true;

  case Qt::Key_A:
    if (mods & Qt::ShiftModifier) {
      moveCursor(QTextCursor::EndOfLine);
    } else {
      moveCursor(QTextCursor::Right);
    }
    beginChangeRecording(count);
    trackInsertPosition();
    setMode(VimEditMode::Insert);
    return true;

  case Qt::Key_O: {
    // Open edit block first so newline is part of the insert undo group
    QTextCursor cursor = m_editor->textCursor();
    cursor.beginEditBlock();
    m_editor->setTextCursor(cursor);
    m_insertUndoOpen = true;
    insertNewLine(mods & Qt::ShiftModifier);
    beginChangeRecording(count);
    trackInsertPosition();
    // setMode won't re-open edit block since m_insertUndoOpen is already true
    m_mode = VimEditMode::Insert;
    m_editor->setCursorWidth(1);
    emit modeChanged(m_mode);
    updatePendingKeys();
    return true;
  }

  case Qt::Key_V:
    if (mods & Qt::ShiftModifier) {
      setMode(VimEditMode::VisualLine);
    } else if (mods & Qt::ControlModifier) {
      setMode(VimEditMode::VisualBlock);
    } else {
      setMode(VimEditMode::Visual);
    }
    return true;

  case Qt::Key_Colon:
    setMode(VimEditMode::Command);
    m_commandBuffer = "";
    emit commandBufferChanged(m_commandBuffer);
    return true;

  case Qt::Key_Slash:
    setMode(VimEditMode::Command);
    m_commandBuffer = "/";
    emit commandBufferChanged(m_commandBuffer);
    m_searchForward = true;
    return true;

  case Qt::Key_Question:
    setMode(VimEditMode::Command);
    m_commandBuffer = "?";
    emit commandBufferChanged(m_commandBuffer);
    m_searchForward = false;
    return true;

  case Qt::Key_H:
    if (mods & Qt::ShiftModifier) {
      executeMotion(VimMotion::ScreenTop, count);
    } else {
      executeMotion(VimMotion::Left, count);
    }
    return true;

  case Qt::Key_Left:
    executeMotion(VimMotion::Left, count);
    return true;

  case Qt::Key_L:
    if (mods & Qt::ShiftModifier) {
      executeMotion(VimMotion::ScreenBottom, count);
    } else {
      executeMotion(VimMotion::Right, count);
    }
    return true;

  case Qt::Key_Right:
    executeMotion(VimMotion::Right, count);
    return true;

  case Qt::Key_J:
    if (mods & Qt::ShiftModifier) {
      joinLines(count);
    } else {
      executeMotion(VimMotion::Down, count);
    }
    return true;

  case Qt::Key_Down:
    executeMotion(VimMotion::Down, count);
    return true;

  case Qt::Key_K:
  case Qt::Key_Up:
    executeMotion(VimMotion::Up, count);
    return true;

  case Qt::Key_M:
    if (mods & Qt::ShiftModifier) {
      executeMotion(VimMotion::ScreenMiddle);
    } else {
      m_commandBuffer = "m";
      updatePendingKeys();
    }
    return true;

  case Qt::Key_W:
    if (mods & Qt::ShiftModifier)
      executeMotion(VimMotion::WORDForward, count);
    else
      executeMotion(VimMotion::WordForward, count);
    return true;

  case Qt::Key_B:
    if (mods & Qt::ControlModifier) {
      executeMotion(VimMotion::FullPageUp, count);
    } else if (mods & Qt::ShiftModifier) {
      executeMotion(VimMotion::WORDBack, count);
    } else {
      executeMotion(VimMotion::WordBack, count);
    }
    return true;

  case Qt::Key_E:
    if (mods & Qt::ControlModifier) {
      scrollLines(count);
    } else if (mods & Qt::ShiftModifier) {
      executeMotion(VimMotion::WORDEnd, count);
    } else {
      executeMotion(VimMotion::WordEnd, count);
    }
    return true;

  case Qt::Key_0:
    executeMotion(VimMotion::LineStart);
    return true;

  case Qt::Key_Dollar:
    executeMotion(VimMotion::LineEnd);
    return true;

  case Qt::Key_AsciiCircum:
    executeMotion(VimMotion::FirstNonSpace);
    return true;

  case Qt::Key_G:
    if (mods & Qt::ShiftModifier) {
      executeMotion(VimMotion::FileEnd);
    } else {
      m_commandBuffer = "g";
      updatePendingKeys();
    }
    return true;

  case Qt::Key_Percent:
    executeMotion(VimMotion::MatchingBrace);
    return true;

  case Qt::Key_BraceLeft:
    executeMotion(VimMotion::PrevParagraph, count);
    return true;

  case Qt::Key_BraceRight:
    executeMotion(VimMotion::NextParagraph, count);
    return true;

  case Qt::Key_ParenLeft:
    executeMotion(VimMotion::PrevSentence, count);
    return true;

  case Qt::Key_ParenRight:
    executeMotion(VimMotion::NextSentence, count);
    return true;

  case Qt::Key_Asterisk:
    searchWord(true);
    return true;

  case Qt::Key_NumberSign:
    searchWord(false);
    return true;

  case Qt::Key_N:
    if (mods & Qt::ShiftModifier) {
      searchNext(false);
    } else {
      searchNext(true);
    }
    return true;

  case Qt::Key_F:
    if (mods & Qt::ControlModifier) {
      executeMotion(VimMotion::FullPageDown, count);
    } else if (mods & Qt::ShiftModifier) {
      m_commandBuffer = "F";
      updatePendingKeys();
    } else {
      m_commandBuffer = "f";
      updatePendingKeys();
    }
    return true;

  case Qt::Key_T:
    if (mods & Qt::ShiftModifier) {
      m_commandBuffer = "T";
      updatePendingKeys();
    } else {
      m_commandBuffer = "t";
      updatePendingKeys();
    }
    return true;

  case Qt::Key_Semicolon:
    if (!m_findChar.isNull()) {
      moveCursorToChar(m_findChar, m_findCharBefore, m_findCharBackward);
    }
    return true;

  case Qt::Key_Comma:
    if (!m_findChar.isNull()) {
      moveCursorToChar(m_findChar, m_findCharBefore, !m_findCharBackward);
    }
    return true;

  case Qt::Key_D:
    if (mods & Qt::ControlModifier) {
      executeMotion(VimMotion::PageDown, count);
    } else if (mods & Qt::ShiftModifier) {
      deleteText(VimMotion::LineEnd);
    } else {
      m_pendingOperator = VimOperator::Delete;
      updatePendingKeys();
    }
    return true;

  case Qt::Key_C:
    if (mods & Qt::ShiftModifier) {
      changeText(VimMotion::LineEnd);
    } else {
      m_pendingOperator = VimOperator::Change;
      updatePendingKeys();
    }
    return true;

  case Qt::Key_Greater:
    m_pendingOperator = VimOperator::Indent;
    updatePendingKeys();
    return true;

  case Qt::Key_Less:
    m_pendingOperator = VimOperator::Unindent;
    updatePendingKeys();
    return true;

  case Qt::Key_AsciiTilde:
  {
    QTextCursor cursor = m_editor->textCursor();
    cursor.beginEditBlock();
    for (int i = 0; i < count; ++i) {
      if (!cursor.atEnd()) {
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        QString ch = cursor.selectedText();
        if (!ch.isEmpty()) {
          QChar c = ch[0];
          if (c.isLower())
            cursor.insertText(c.toUpper());
          else if (c.isUpper())
            cursor.insertText(c.toLower());
          else {
            cursor.clearSelection();
            cursor.movePosition(QTextCursor::Right);
          }
        }
      }
    }
    cursor.endEditBlock();
    m_editor->setTextCursor(cursor);
  }
    return true;

  case Qt::Key_X:
    deleteText(VimMotion::Right, count);
    return true;

  case Qt::Key_S:
    if (mods & Qt::ShiftModifier) {
      QTextCursor cursor = m_editor->textCursor();
      cursor.beginEditBlock();
      m_editor->setTextCursor(cursor);
      m_insertUndoOpen = true;
      cursor.movePosition(QTextCursor::StartOfLine);
      cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
      deleteToRegister(cursor.selectedText());
      cursor.removeSelectedText();
      m_editor->setTextCursor(cursor);
      beginChangeRecording(count);
      trackInsertPosition();
      m_mode = VimEditMode::Insert;
      m_editor->setCursorWidth(1);
      emit modeChanged(m_mode);
      updatePendingKeys();
    } else {
      QTextCursor cursor = m_editor->textCursor();
      cursor.beginEditBlock();
      m_editor->setTextCursor(cursor);
      m_insertUndoOpen = true;
      deleteText(VimMotion::Right);
      beginChangeRecording(count);
      trackInsertPosition();
      m_mode = VimEditMode::Insert;
      m_editor->setCursorWidth(1);
      emit modeChanged(m_mode);
      updatePendingKeys();
    }
    return true;

  case Qt::Key_P:
    if (mods & Qt::ShiftModifier) {
      pasteFromRegister(m_pendingRegister != QChar() ? m_pendingRegister : '"',
                        false);
    } else {
      pasteFromRegister(m_pendingRegister != QChar() ? m_pendingRegister : '"',
                        true);
    }
    m_pendingRegister = QChar();
    return true;

  case Qt::Key_U:
    if (mods & Qt::ControlModifier) {
      executeMotion(VimMotion::PageUp, count);
    } else {
      m_editor->undo();
    }
    return true;

  case Qt::Key_R:
    if (mods & Qt::ControlModifier) {
      m_editor->redo();
    } else if (mods & Qt::ShiftModifier) {
      setMode(VimEditMode::Replace);
    } else {
      m_commandBuffer = "r";
      updatePendingKeys();
    }
    return true;

  case Qt::Key_Z:
    if (mods & Qt::ControlModifier) {
      m_editor->undo();
      return true;
    }
    m_commandBuffer = "z";
    updatePendingKeys();
    return true;

  case Qt::Key_Y:
    if (mods & Qt::ControlModifier) {
      if (mods & Qt::ShiftModifier) {
        m_editor->redo();
      } else {
        scrollLines(-count);
      }
    } else if (mods & Qt::ShiftModifier) {
      yankText(VimMotion::LineEnd);
    } else {
      m_pendingOperator = VimOperator::Yank;
      updatePendingKeys();
    }
    return true;

  case Qt::Key_Apostrophe:
  case Qt::Key_QuoteLeft:
    m_commandBuffer = "'";
    updatePendingKeys();
    return true;

  case Qt::Key_Period:
    repeatLastChange();
    return true;

  case Qt::Key_QuoteDbl:
    // Start register prefix
    m_commandBuffer = "\"";
    updatePendingKeys();
    return true;

  case Qt::Key_Q:
    // Macro recording toggle
    if (m_macroRecording) {
      stopMacroRecording();
    } else {
      // Wait for register key
      m_commandBuffer = "q";
      updatePendingKeys();
    }
    return true;

  case Qt::Key_At:
    // Macro playback
    m_commandBuffer = "@";
    updatePendingKeys();
    return true;

  default:
    break;
  }

  // Ctrl+A / Ctrl+X for increment/decrement (outside switch to avoid duplicate)
  if (key == Qt::Key_A && (mods & Qt::ControlModifier)) {
    incrementNumber(count);
    return true;
  }
  if (key == Qt::Key_X && (mods & Qt::ControlModifier)) {
    incrementNumber(-count);
    return true;
  }

  // Handle 'q' pending for macro register
  if (!m_commandBuffer.isEmpty() && m_commandBuffer == "q" && !text.isEmpty()) {
    QChar reg = text[0];
    if ((reg >= 'a' && reg <= 'z') || (reg >= 'A' && reg <= 'Z')) {
      startMacroRecording(reg);
    }
    m_commandBuffer.clear();
    updatePendingKeys();
    return true;
  }

  updatePendingKeys();
  return false;
}

bool VimMode::handleInsertMode(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape ||
      (event->key() == Qt::Key_BracketLeft &&
       event->modifiers() & Qt::ControlModifier)) {
    trackInsertPosition();
    endChangeRecording();
    setMode(VimEditMode::Normal);
    moveCursor(QTextCursor::Left);
    return true;
  }

  return false;
}

bool VimMode::handleReplaceMode(QKeyEvent *event) {
  int key = event->key();
  QString text = event->text();

  if (key == Qt::Key_Escape || (key == Qt::Key_BracketLeft &&
                                event->modifiers() & Qt::ControlModifier)) {
    setMode(VimEditMode::Normal);
    moveCursor(QTextCursor::Left);
    return true;
  }

  if (key == Qt::Key_Backspace) {
    moveCursor(QTextCursor::Left);
    return true;
  }

  if (!text.isEmpty() && text[0].isPrint()) {
    QTextCursor cursor = m_editor->textCursor();
    if (!cursor.atEnd()) {
      cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
      cursor.insertText(text);
    } else {
      cursor.insertText(text);
    }
    m_editor->setTextCursor(cursor);
    return true;
  }

  return false;
}

// ============== VISUAL MODE ==============

bool VimMode::handleVisualMode(QKeyEvent *event) {
  int key = event->key();
  Qt::KeyboardModifiers mods = event->modifiers();

  if (key == Qt::Key_Escape) {
    // Save visual selection for gv
    QTextCursor cursor = m_editor->textCursor();
    m_lastVisualStart = cursor.anchor();
    m_lastVisualEnd = cursor.position();
    m_lastVisualMode = m_mode;
    setMode(VimEditMode::Normal);
    return true;
  }

  int count = qMax(1, m_count);
  m_count = 0;

  QTextCursor cursor = m_editor->textCursor();

  switch (key) {

  case Qt::Key_H:
  case Qt::Key_Left:
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, count);
    m_editor->setTextCursor(cursor);
    return true;

  case Qt::Key_L:
  case Qt::Key_Right:
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, count);
    m_editor->setTextCursor(cursor);
    return true;

  case Qt::Key_J:
  case Qt::Key_Down:
    if (key == Qt::Key_J && (mods & Qt::ShiftModifier)) {
      visualJoinLines();
      setMode(VimEditMode::Normal);
      return true;
    }
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, count);
    m_editor->setTextCursor(cursor);
    return true;

  case Qt::Key_K:
  case Qt::Key_Up:
    cursor.movePosition(QTextCursor::Up, QTextCursor::KeepAnchor, count);
    m_editor->setTextCursor(cursor);
    return true;

  case Qt::Key_W:
    if (mods & Qt::ShiftModifier) {
      // WORD motion in visual
      cursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor, count);
    } else {
      cursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor, count);
    }
    m_editor->setTextCursor(cursor);
    return true;

  case Qt::Key_B:
    cursor.movePosition(QTextCursor::PreviousWord, QTextCursor::KeepAnchor,
                        count);
    m_editor->setTextCursor(cursor);
    return true;

  case Qt::Key_E:
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor, count);
    m_editor->setTextCursor(cursor);
    return true;

  case Qt::Key_0:
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cursor);
    return true;

  case Qt::Key_Dollar:
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cursor);
    return true;

  case Qt::Key_G:
    if (mods & Qt::ShiftModifier) {
      cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
      m_editor->setTextCursor(cursor);
    }
    return true;

  case Qt::Key_D:
  case Qt::Key_X:
    m_lastVisualStart = cursor.anchor();
    m_lastVisualEnd = cursor.position();
    m_lastVisualMode = m_mode;
    deleteToRegister(cursor.selectedText());
    cursor.removeSelectedText();
    setMode(VimEditMode::Normal);
    return true;

  case Qt::Key_Y:
    m_lastVisualStart = cursor.anchor();
    m_lastVisualEnd = cursor.position();
    m_lastVisualMode = m_mode;
    yankToRegister(cursor.selectedText());
    cursor.clearSelection();
    m_editor->setTextCursor(cursor);
    setMode(VimEditMode::Normal);
    emit statusMessage("Yanked");
    return true;

  case Qt::Key_C:
    m_lastVisualStart = cursor.anchor();
    m_lastVisualEnd = cursor.position();
    m_lastVisualMode = m_mode;
    cursor.beginEditBlock();
    m_editor->setTextCursor(cursor);
    m_insertUndoOpen = true;
    deleteToRegister(cursor.selectedText());
    cursor = m_editor->textCursor();
    cursor.removeSelectedText();
    m_editor->setTextCursor(cursor);
    beginChangeRecording(count);
    trackInsertPosition();
    m_mode = VimEditMode::Insert;
    m_editor->setCursorWidth(1);
    emit modeChanged(m_mode);
    updatePendingKeys();
    return true;

  case Qt::Key_Greater:
    visualIndent(true);
    setMode(VimEditMode::Normal);
    return true;

  case Qt::Key_Less:
    visualIndent(false);
    setMode(VimEditMode::Normal);
    return true;

  case Qt::Key_AsciiTilde:
    visualToggleCase();
    setMode(VimEditMode::Normal);
    return true;

  case Qt::Key_U:
    if (mods & Qt::ShiftModifier) {
      visualUppercase();
    } else {
      visualLowercase();
    }
    setMode(VimEditMode::Normal);
    return true;

  case Qt::Key_V:
    if (mods & Qt::ShiftModifier) {
      if (m_mode == VimEditMode::VisualLine)
        setMode(VimEditMode::Normal);
      else
        setMode(VimEditMode::VisualLine);
    } else {
      if (m_mode == VimEditMode::Visual)
        setMode(VimEditMode::Normal);
      else
        setMode(VimEditMode::Visual);
    }
    return true;

  default:
    break;
  }

  return false;
}

// ============== VISUAL MODE HELPERS ==============

void VimMode::visualIndent(bool indent) {
  QTextCursor cursor = m_editor->textCursor();
  int startPos = qMin(cursor.anchor(), cursor.position());
  int endPos = qMax(cursor.anchor(), cursor.position());
  int startBlock = m_editor->document()->findBlock(startPos).blockNumber();
  int endBlock = m_editor->document()->findBlock(endPos).blockNumber();

  cursor.beginEditBlock();
  for (int i = startBlock; i <= endBlock; ++i) {
    QTextBlock block = m_editor->document()->findBlockByNumber(i);
    QTextCursor lineCursor(block);
    lineCursor.movePosition(QTextCursor::StartOfLine);
    if (indent) {
      lineCursor.insertText("    ");
    } else {
      QString line = block.text();
      if (line.startsWith("    ")) {
        lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 4);
        lineCursor.removeSelectedText();
      } else if (line.startsWith("\t")) {
        lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
        lineCursor.removeSelectedText();
      }
    }
  }
  cursor.endEditBlock();
}

void VimMode::visualToggleCase() {
  QTextCursor cursor = m_editor->textCursor();
  QString text = cursor.selectedText();
  QString result;
  for (const QChar &c : text) {
    if (c.isLower())
      result += c.toUpper();
    else if (c.isUpper())
      result += c.toLower();
    else
      result += c;
  }
  cursor.insertText(result);
}

void VimMode::visualLowercase() {
  QTextCursor cursor = m_editor->textCursor();
  cursor.insertText(cursor.selectedText().toLower());
}

void VimMode::visualUppercase() {
  QTextCursor cursor = m_editor->textCursor();
  cursor.insertText(cursor.selectedText().toUpper());
}

void VimMode::visualJoinLines() {
  QTextCursor cursor = m_editor->textCursor();
  int startPos = qMin(cursor.anchor(), cursor.position());
  int endPos = qMax(cursor.anchor(), cursor.position());
  int startBlock = m_editor->document()->findBlock(startPos).blockNumber();
  int endBlock = m_editor->document()->findBlock(endPos).blockNumber();

  cursor.setPosition(m_editor->document()->findBlockByNumber(startBlock).position());
  cursor.beginEditBlock();
  for (int i = startBlock; i < endBlock; ++i) {
    cursor.movePosition(QTextCursor::EndOfLine);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    // Skip leading whitespace on next line
    while (!cursor.atEnd()) {
      QChar ch = m_editor->document()->characterAt(cursor.position());
      if (ch.isSpace() && ch != '\n')
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
      else
        break;
    }
    cursor.insertText(" ");
  }
  cursor.endEditBlock();
  m_editor->setTextCursor(cursor);
}

// ============== COMMAND MODE ==============

bool VimMode::handleCommandMode(QKeyEvent *event) {
  int key = event->key();
  QString text = event->text();

  if (key == Qt::Key_Escape) {
    m_commandBuffer.clear();
    emit commandBufferChanged(m_commandBuffer);
    m_commandHistoryIndex = -1;
    m_commandDraft.clear();
    setMode(VimEditMode::Normal);
    return true;
  }

  if (key == Qt::Key_Up || key == Qt::Key_Down) {
    if (!m_commandBuffer.startsWith("/") && !m_commandBuffer.startsWith("?") &&
        !m_commandHistory.isEmpty()) {
      if (m_commandHistoryIndex < 0) {
        m_commandDraft = m_commandBuffer;
      }
      if (key == Qt::Key_Up) {
        if (m_commandHistoryIndex < m_commandHistory.size() - 1)
          m_commandHistoryIndex++;
      } else {
        if (m_commandHistoryIndex >= 0)
          m_commandHistoryIndex--;
      }
      if (m_commandHistoryIndex >= 0)
        m_commandBuffer = m_commandHistory[m_commandHistoryIndex];
      else
        m_commandBuffer = m_commandDraft;
      emit commandBufferChanged(m_commandBuffer);
    }
    return true;
  }

  if (key == Qt::Key_Return || key == Qt::Key_Enter) {
    if (!m_commandBuffer.isEmpty() && !m_commandBuffer.startsWith("/") &&
        !m_commandBuffer.startsWith("?")) {
      m_commandHistory.removeAll(m_commandBuffer);
      m_commandHistory.prepend(m_commandBuffer);
      while (m_commandHistory.size() > kMaxCommandHistory)
        m_commandHistory.removeLast();
    }
    executeCommand(m_commandBuffer);
    m_commandBuffer.clear();
    m_commandHistoryIndex = -1;
    m_commandDraft.clear();
    emit commandBufferChanged(m_commandBuffer);
    setMode(VimEditMode::Normal);
    return true;
  }

  if (key == Qt::Key_Backspace) {
    if (!m_commandBuffer.isEmpty()) {
      m_commandBuffer.chop(1);
      emit commandBufferChanged(m_commandBuffer);
      if (m_commandHistoryIndex < 0)
        m_commandDraft = m_commandBuffer;
    } else {
      m_commandHistoryIndex = -1;
      m_commandDraft.clear();
      setMode(VimEditMode::Normal);
    }
    return true;
  }

  if (!text.isEmpty()) {
    m_commandBuffer += text;
    emit commandBufferChanged(m_commandBuffer);
    if (m_commandHistoryIndex < 0)
      m_commandDraft = m_commandBuffer;
  }

  return true;
}

void VimMode::setMode(VimEditMode mode) {
  if (m_mode != mode) {
    // Close the insert/replace undo group when leaving insert or replace mode
    if (m_insertUndoOpen &&
        (m_mode == VimEditMode::Insert || m_mode == VimEditMode::Replace) &&
        mode != VimEditMode::Insert && mode != VimEditMode::Replace) {
      QTextCursor cursor = m_editor->textCursor();
      cursor.endEditBlock();
      m_insertUndoOpen = false;
    }

    m_mode = mode;
    if (m_mode != VimEditMode::Command && !m_commandBuffer.isEmpty()) {
      m_commandBuffer.clear();
      emit commandBufferChanged(m_commandBuffer);
    }

    if (mode == VimEditMode::Insert || mode == VimEditMode::Replace) {
      m_editor->setCursorWidth(
          mode == VimEditMode::Replace
              ? m_editor->fontMetrics().horizontalAdvance('M') / 2
              : 1);
      // Open an undo group so the entire insert/replace session is one undo step
      if (!m_insertUndoOpen) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.beginEditBlock();
        m_editor->setTextCursor(cursor);
        m_insertUndoOpen = true;
      }
    } else {
      m_editor->setCursorWidth(m_editor->fontMetrics().horizontalAdvance('M'));
    }

    emit modeChanged(mode);
    updatePendingKeys();
    LOG_DEBUG(QString("VIM mode changed to: %1").arg(modeName()));
  }
}

void VimMode::executeMotion(VimMotion motion, int count,
                            QTextCursor::MoveMode moveMode) {
  QTextCursor cursor = m_editor->textCursor();

  for (int i = 0; i < count; ++i) {
    switch (motion) {
    case VimMotion::Left:
      cursor.movePosition(QTextCursor::Left, moveMode);
      break;
    case VimMotion::Right:
      cursor.movePosition(QTextCursor::Right, moveMode);
      break;
    case VimMotion::Up:
      cursor.movePosition(QTextCursor::Up, moveMode);
      break;
    case VimMotion::Down:
      cursor.movePosition(QTextCursor::Down, moveMode);
      break;
    case VimMotion::WordForward:
      cursor.movePosition(QTextCursor::NextWord, moveMode);
      break;
    case VimMotion::WordBack:
      cursor.movePosition(QTextCursor::PreviousWord, moveMode);
      break;
    case VimMotion::WordEnd:
      cursor.movePosition(QTextCursor::EndOfWord, moveMode);
      break;
    case VimMotion::WORDForward:
      m_editor->setTextCursor(cursor);
      moveCursorWORD(true);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::WORDBack:
      m_editor->setTextCursor(cursor);
      moveCursorWORD(false);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::WORDEnd:
      m_editor->setTextCursor(cursor);
      moveCursorWORDEnd();
      cursor = m_editor->textCursor();
      break;
    case VimMotion::LineStart:
      cursor.movePosition(QTextCursor::StartOfLine, moveMode);
      break;
    case VimMotion::LineEnd:
      cursor.movePosition(QTextCursor::EndOfLine, moveMode);
      break;
    case VimMotion::FirstNonSpace:
      cursor.movePosition(QTextCursor::StartOfLine, moveMode);
      {
        QString line = cursor.block().text();
        int pos = 0;
        while (pos < line.length() && line[pos].isSpace())
          pos++;
        cursor.movePosition(QTextCursor::Right, moveMode, pos);
      }
      break;
    case VimMotion::FileStart:
      cursor.movePosition(QTextCursor::Start, moveMode);
      break;
    case VimMotion::FileEnd:
      cursor.movePosition(QTextCursor::End, moveMode);
      break;
    case VimMotion::PageUp:
    case VimMotion::HalfPageUp:
      cursor.movePosition(QTextCursor::Up, moveMode, VIM_HALF_PAGE_SIZE);
      break;
    case VimMotion::PageDown:
    case VimMotion::HalfPageDown:
      cursor.movePosition(QTextCursor::Down, moveMode, VIM_HALF_PAGE_SIZE);
      break;
    case VimMotion::FullPageUp:
      cursor.movePosition(QTextCursor::Up, moveMode, VIM_PAGE_SIZE);
      break;
    case VimMotion::FullPageDown:
      cursor.movePosition(QTextCursor::Down, moveMode, VIM_PAGE_SIZE);
      break;
    case VimMotion::MatchingBrace:
      m_editor->setTextCursor(cursor);
      moveCursorToMatchingBrace();
      cursor = m_editor->textCursor();
      break;
    case VimMotion::NextParagraph:
      m_editor->setTextCursor(cursor);
      moveCursorToParagraph(true);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::PrevParagraph:
      m_editor->setTextCursor(cursor);
      moveCursorToParagraph(false);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::NextSentence:
      m_editor->setTextCursor(cursor);
      moveCursorToSentence(true);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::PrevSentence:
      m_editor->setTextCursor(cursor);
      moveCursorToSentence(false);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::SearchNext:
      m_editor->setTextCursor(cursor);
      searchNext(true);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::SearchPrev:
      m_editor->setTextCursor(cursor);
      searchNext(false);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::WordUnderCursor:
      m_editor->setTextCursor(cursor);
      searchWord(true);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::WordUnderCursorBack:
      m_editor->setTextCursor(cursor);
      searchWord(false);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::ScreenTop:
    case VimMotion::ScreenMiddle:
    case VimMotion::ScreenBottom:
      m_editor->setTextCursor(cursor);
      moveCursorToScreenLine(motion == VimMotion::ScreenTop ? 0
                             : motion == VimMotion::ScreenMiddle ? 1
                                                                  : 2);
      cursor = m_editor->textCursor();
      break;
    case VimMotion::ColumnZero:
      cursor.movePosition(QTextCursor::StartOfLine, moveMode);
      break;
    default:
      break;
    }
  }

  m_editor->setTextCursor(cursor);
}

void VimMode::executeOperator(VimOperator op, VimMotion motion, int count) {
  switch (op) {
  case VimOperator::Delete:
    deleteText(motion, count);
    break;
  case VimOperator::Change:
    changeText(motion, count);
    break;
  case VimOperator::Yank:
    yankText(motion, count);
    break;
  case VimOperator::Indent:
    indentText(motion, count, true);
    break;
  case VimOperator::Unindent:
    indentText(motion, count, false);
    break;
  case VimOperator::ToggleCase:
    toggleCase(motion, count);
    break;
  case VimOperator::Lowercase:
    lowercaseText(motion, count);
    break;
  case VimOperator::Uppercase:
    uppercaseText(motion, count);
    break;
  default:
    break;
  }
}

void VimMode::executeCommand(const QString &command) {
  LOG_DEBUG(QString("Executing VIM command: %1").arg(command));

  if (command == "w") {
    emit commandExecuted("save");
    emit statusMessage("File saved");
  } else if (command == "q") {
    emit commandExecuted("quit");
  } else if (command == "wq" || command == "x") {
    emit commandExecuted("save");
    emit commandExecuted("quit");
  } else if (command == "q!") {
    emit commandExecuted("forceQuit");
  } else if (command == "noh" || command == "nohlsearch") {
    clearSearchHighlight();
    emit statusMessage("Search highlight cleared");
  } else if (command == "bn" || command == "bnext") {
    emit commandExecuted("nextTab");
    emit statusMessage("Next buffer");
  } else if (command == "bp" || command == "bprev" || command == "bprevious") {
    emit commandExecuted("prevTab");
    emit statusMessage("Previous buffer");
  } else if (command == "sp" || command == "split") {
    emit commandExecuted("splitHorizontal");
  } else if (command == "vsp" || command == "vsplit") {
    emit commandExecuted("splitVertical");
  } else if (command == "sort") {
    // Sort lines in document
    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) {
      int startPos = qMin(cursor.anchor(), cursor.position());
      int endPos = qMax(cursor.anchor(), cursor.position());
      int startBlock = m_editor->document()->findBlock(startPos).blockNumber();
      int endBlock = m_editor->document()->findBlock(endPos).blockNumber();
      QStringList lines;
      for (int i = startBlock; i <= endBlock; ++i)
        lines << m_editor->document()->findBlockByNumber(i).text();
      lines.sort();
      cursor.beginEditBlock();
      cursor.setPosition(
          m_editor->document()->findBlockByNumber(startBlock).position());
      cursor.setPosition(
          m_editor->document()->findBlockByNumber(endBlock).position() +
              m_editor->document()->findBlockByNumber(endBlock).length() - 1,
          QTextCursor::KeepAnchor);
      cursor.insertText(lines.join("\n"));
      cursor.endEditBlock();
    } else {
      emit statusMessage("Select lines to sort (use visual mode)");
    }
  } else if (command == "registers" || command == "reg") {
    emit commandExecuted("showRegisters");
  } else if (command == "marks") {
    emit commandExecuted("showMarks");
  } else if (command.startsWith("set ")) {
    QString option = command.mid(4).trimmed();
    if (option == "novim" || option == "no-vim") {
      emit commandExecuted("vim:off");
      emit statusMessage("Vim mode disabled");
    } else if (option == "vim") {
      emit commandExecuted("vim:on");
      emit statusMessage("Vim mode enabled");
    } else {
      emit statusMessage(QString("Set: %1").arg(option));
    }
  } else if (command.startsWith("/") || command.startsWith("?")) {
    bool forward = command.startsWith("/");
    QString pattern = command.mid(1);
    if (!pattern.isEmpty()) {
      m_searchPattern = QRegularExpression::escape(pattern);
      m_searchForward = forward;
      m_searchHighlightActive = true;
      emit searchHighlightRequested(m_searchPattern, true);
      searchNext(true);
    }
  } else if (command.startsWith("s/") || command.startsWith("%s/")) {
    QString cmd = command;
    bool global = cmd.startsWith("%");
    if (global)
      cmd = cmd.mid(1);

    QStringList parts = cmd.mid(2).split('/');
    if (parts.size() >= 2) {
      QString pattern = parts[0];
      QString replacement = parts[1];
      bool replaceAll = (parts.size() > 2 && parts[2].contains('g'));

      QTextCursor cursor = m_editor->textCursor();
      QString text;
      int startPos = 0;

      if (global) {
        text = m_editor->toPlainText();
      } else {
        text = cursor.block().text();
        startPos = cursor.block().position();
      }

      QRegularExpression regex(pattern);
      QString newText;
      if (replaceAll) {
        newText = text;
        newText.replace(regex, replacement);
      } else {
        QRegularExpressionMatch match = regex.match(text);
        if (match.hasMatch()) {
          newText = text.left(match.capturedStart()) + replacement +
                    text.mid(match.capturedEnd());
        } else {
          newText = text;
        }
      }

      cursor.beginEditBlock();
      if (global) {
        cursor.select(QTextCursor::Document);
        cursor.insertText(newText);
      } else {
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        cursor.insertText(newText);
      }
      cursor.endEditBlock();
      m_editor->setTextCursor(cursor);
      emit statusMessage("Substitution complete");
    }
  } else if (command.startsWith("e ")) {
    QString filename = command.mid(2).trimmed();
    emit commandExecuted(QString("edit:%1").arg(filename));
  } else {
    bool ok;
    int lineNum = command.toInt(&ok);
    if (ok && lineNum > 0) {
      QTextCursor cursor = m_editor->textCursor();
      QTextBlock block = m_editor->document()->findBlockByNumber(lineNum - 1);
      if (block.isValid()) {
        cursor.setPosition(block.position());
        m_editor->setTextCursor(cursor);
        emit statusMessage(QString("Line %1").arg(lineNum));
      } else {
        emit statusMessage("Invalid line number");
      }
    } else {
      emit statusMessage(QString("Unknown command: %1").arg(command));
    }
  }
}

void VimMode::moveCursor(QTextCursor::MoveOperation op, int count) {
  QTextCursor cursor = m_editor->textCursor();
  for (int i = 0; i < count; ++i)
    cursor.movePosition(op);
  m_editor->setTextCursor(cursor);
}

// ============== WORD/WORD MOTIONS ==============

void VimMode::moveCursorWORD(bool forward) {
  QTextCursor cursor = m_editor->textCursor();
  QString text = m_editor->toPlainText();
  int pos = cursor.position();
  int len = text.length();

  if (forward) {
    // Skip non-whitespace
    while (pos < len && !text[pos].isSpace())
      pos++;
    // Skip whitespace
    while (pos < len && text[pos].isSpace())
      pos++;
  } else {
    // Skip whitespace backwards
    if (pos > 0) pos--;
    while (pos > 0 && text[pos].isSpace())
      pos--;
    // Skip non-whitespace backwards
    while (pos > 0 && !text[pos - 1].isSpace())
      pos--;
  }

  cursor.setPosition(pos);
  m_editor->setTextCursor(cursor);
}

void VimMode::moveCursorWORDEnd() {
  QTextCursor cursor = m_editor->textCursor();
  QString text = m_editor->toPlainText();
  int pos = cursor.position();
  int len = text.length();

  if (pos < len) pos++;
  // Skip whitespace
  while (pos < len && text[pos].isSpace())
    pos++;
  // Skip non-whitespace
  while (pos < len && !text[pos].isSpace())
    pos++;
  if (pos > 0) pos--;

  cursor.setPosition(pos);
  m_editor->setTextCursor(cursor);
}

// ============== DELETE/YANK/CHANGE ==============

void VimMode::deleteText(VimMotion motion, int count) {
  QTextCursor cursor = m_editor->textCursor();
  int startPos = cursor.position();

  executeMotion(motion, count);

  cursor = m_editor->textCursor();
  int endPos = cursor.position();

  cursor.setPosition(qMin(startPos, endPos));
  cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);

  deleteToRegister(cursor.selectedText());
  cursor.removeSelectedText();
  m_editor->setTextCursor(cursor);
}

void VimMode::yankText(VimMotion motion, int count) {
  QTextCursor cursor = m_editor->textCursor();
  int startPos = cursor.position();

  executeMotion(motion, count);

  cursor = m_editor->textCursor();
  int endPos = cursor.position();

  cursor.setPosition(qMin(startPos, endPos));
  cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);

  yankToRegister(cursor.selectedText());
  cursor.setPosition(startPos);
  m_editor->setTextCursor(cursor);

  emit statusMessage("Yanked");
}

void VimMode::changeText(VimMotion motion, int count) {
  // Open the edit block before deleting so the delete + insert are one undo step
  QTextCursor cursor = m_editor->textCursor();
  cursor.beginEditBlock();
  m_editor->setTextCursor(cursor);
  m_insertUndoOpen = true;
  deleteText(motion, count);
  beginChangeRecording(count);
  trackInsertPosition();
  m_mode = VimEditMode::Insert;
  m_editor->setCursorWidth(1);
  emit modeChanged(m_mode);
  updatePendingKeys();
}

void VimMode::insertNewLine(bool above) {
  QTextCursor cursor = m_editor->textCursor();

  if (above) {
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.insertText("\n");
    cursor.movePosition(QTextCursor::Up);
  } else {
    cursor.movePosition(QTextCursor::EndOfLine);
    cursor.insertText("\n");
  }

  m_editor->setTextCursor(cursor);
}

void VimMode::joinLines(int count) {
  QTextCursor cursor = m_editor->textCursor();
  cursor.beginEditBlock();
  for (int c = 0; c < count; ++c) {
    cursor.movePosition(QTextCursor::EndOfLine);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);

    while (!cursor.atEnd()) {
      QChar ch = m_editor->document()->characterAt(cursor.position());
      if (ch.isSpace() && ch != '\n')
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
      else
        break;
    }

    cursor.insertText(" ");
  }
  cursor.endEditBlock();
  m_editor->setTextCursor(cursor);
}

void VimMode::replaceChar(QChar ch) {
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
  cursor.insertText(QString(ch));
  cursor.movePosition(QTextCursor::Left);
  m_editor->setTextCursor(cursor);
}

void VimMode::moveCursorToChar(QChar ch, bool before, bool backward) {
  QTextCursor cursor = m_editor->textCursor();
  QString line = cursor.block().text();
  int col = cursor.positionInBlock();

  if (backward) {
    for (int i = col - 1; i >= 0; --i) {
      if (line[i] == ch) {
        int newPos = before ? i + 1 : i;
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, newPos);
        m_editor->setTextCursor(cursor);
        return;
      }
    }
  } else {
    for (int i = col + 1; i < line.length(); ++i) {
      if (line[i] == ch) {
        int newPos = before ? i - 1 : i;
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, newPos);
        m_editor->setTextCursor(cursor);
        return;
      }
    }
  }
}

bool VimMode::moveCursorToMatchingBrace() {
  QTextCursor cursor = m_editor->textCursor();
  QChar ch = m_editor->document()->characterAt(cursor.position());

  QChar match;
  bool forward = true;

  if (ch == '(') { match = ')'; forward = true; }
  else if (ch == ')') { match = '('; forward = false; }
  else if (ch == '[') { match = ']'; forward = true; }
  else if (ch == ']') { match = '['; forward = false; }
  else if (ch == '{') { match = '}'; forward = true; }
  else if (ch == '}') { match = '{'; forward = false; }
  else if (ch == '<') { match = '>'; forward = true; }
  else if (ch == '>') { match = '<'; forward = false; }
  else { return false; }

  int depth = 1;
  int pos = cursor.position();
  int len = m_editor->document()->characterCount();

  while (depth > 0) {
    if (forward) { pos++; if (pos >= len) return false; }
    else { pos--; if (pos < 0) return false; }
    QChar c = m_editor->document()->characterAt(pos);
    if (c == ch) depth++;
    else if (c == match) depth--;
  }

  cursor.setPosition(pos);
  m_editor->setTextCursor(cursor);
  return true;
}

void VimMode::moveCursorToParagraph(bool forward) {
  QTextCursor cursor = m_editor->textCursor();
  if (forward) {
    while (!cursor.atEnd()) {
      cursor.movePosition(QTextCursor::NextBlock);
      if (cursor.block().text().trimmed().isEmpty()) break;
    }
  } else {
    while (!cursor.atStart()) {
      cursor.movePosition(QTextCursor::PreviousBlock);
      if (cursor.block().text().trimmed().isEmpty()) break;
    }
  }
  m_editor->setTextCursor(cursor);
}

void VimMode::moveCursorToSentence(bool forward) {
  QTextCursor cursor = m_editor->textCursor();
  QString text = m_editor->toPlainText();
  int pos = cursor.position();
  QRegularExpression sentenceEnd("[.!?][\\s\\n]");

  if (forward) {
    QRegularExpressionMatch match = sentenceEnd.match(text, pos);
    if (match.hasMatch()) {
      cursor.setPosition(match.capturedEnd());
      while (cursor.position() < text.length() && text[cursor.position()].isSpace())
        cursor.movePosition(QTextCursor::Right);
    } else {
      cursor.movePosition(QTextCursor::End);
    }
  } else {
    int searchPos = qMax(0, pos - 2);
    int lastMatch = 0;
    QRegularExpressionMatchIterator it = sentenceEnd.globalMatch(text.left(searchPos));
    while (it.hasNext()) {
      QRegularExpressionMatch m = it.next();
      lastMatch = m.capturedEnd();
    }
    cursor.setPosition(lastMatch);
    while (cursor.position() < text.length() && text[cursor.position()].isSpace())
      cursor.movePosition(QTextCursor::Right);
  }
  m_editor->setTextCursor(cursor);
}

void VimMode::moveCursorToScreenLine(int which) {
  QTextCursor cursor = m_editor->textCursor();
  QRect rect = m_editor->viewport()->rect();
  int lineHeight = m_editor->fontMetrics().height();
  if (lineHeight <= 0) lineHeight = 16;
  int visibleLines = rect.height() / lineHeight;

  QTextCursor firstVisible = m_editor->cursorForPosition(QPoint(0, 0));
  int firstLine = firstVisible.blockNumber();

  int targetLine;
  if (which == 0) // Top
    targetLine = firstLine;
  else if (which == 1) // Middle
    targetLine = firstLine + visibleLines / 2;
  else // Bottom
    targetLine = firstLine + visibleLines - 1;

  targetLine = qBound(0, targetLine, m_editor->document()->blockCount() - 1);
  QTextBlock block = m_editor->document()->findBlockByNumber(targetLine);
  if (block.isValid()) {
    cursor.setPosition(block.position());
    // Move to first non-space
    QString line = block.text();
    int p = 0;
    while (p < line.length() && line[p].isSpace()) p++;
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, p);
    m_editor->setTextCursor(cursor);
  }
}

void VimMode::setMark(QChar mark) {
  m_marks[mark] = m_editor->textCursor().position();
  emit statusMessage(QString("Mark '%1' set").arg(mark));
}

bool VimMode::jumpToMark(QChar mark) {
  if (!m_marks.contains(mark)) {
    emit statusMessage(QString("Mark '%1' not set").arg(mark));
    return false;
  }
  QTextCursor cursor = m_editor->textCursor();
  cursor.setPosition(m_marks[mark]);
  m_editor->setTextCursor(cursor);
  return true;
}

void VimMode::searchWord(bool forward) {
  QTextCursor cursor = m_editor->textCursor();
  cursor.select(QTextCursor::WordUnderCursor);
  QString word = cursor.selectedText();
  if (word.isEmpty()) {
    emit statusMessage("No word under cursor");
    return;
  }
  m_searchPattern = "\\b" + QRegularExpression::escape(word) + "\\b";
  m_searchForward = forward;
  m_searchHighlightActive = true;
  emit searchHighlightRequested(m_searchPattern, true);
  searchNext(forward);
}

void VimMode::searchNext(bool forward) {
  if (m_searchPattern.isEmpty()) {
    emit statusMessage("No previous search");
    return;
  }

  QTextCursor cursor = m_editor->textCursor();
  QString text = m_editor->toPlainText();
  QRegularExpression regex(m_searchPattern);
  bool actualForward = (forward == m_searchForward);

  // Collect all match positions for count display
  QVector<int> matchPositions;
  QRegularExpressionMatchIterator allMatches = regex.globalMatch(text);
  while (allMatches.hasNext()) {
    QRegularExpressionMatch m = allMatches.next();
    matchPositions.append(m.capturedStart());
  }
  int totalMatches = matchPositions.size();

  if (totalMatches == 0) {
    emit statusMessage("Pattern not found");
    return;
  }

  int targetPos = -1;
  bool wrapped = false;

  if (actualForward) {
    QRegularExpressionMatch match = regex.match(text, cursor.position() + 1);
    if (match.hasMatch()) {
      targetPos = match.capturedStart();
    } else {
      match = regex.match(text, 0);
      if (match.hasMatch()) {
        targetPos = match.capturedStart();
        wrapped = true;
      }
    }
  } else {
    int lastMatch = -1;
    QRegularExpressionMatchIterator it =
        regex.globalMatch(text.left(cursor.position()));
    while (it.hasNext()) {
      QRegularExpressionMatch m = it.next();
      lastMatch = m.capturedStart();
    }
    if (lastMatch >= 0) {
      targetPos = lastMatch;
    } else {
      it = regex.globalMatch(text);
      while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        lastMatch = m.capturedStart();
      }
      if (lastMatch >= 0) {
        targetPos = lastMatch;
        wrapped = true;
      }
    }
  }

  if (targetPos >= 0) {
    cursor.setPosition(targetPos);
    m_editor->setTextCursor(cursor);

    // Find the 1-based index of the current match
    int matchIndex = 0;
    for (int i = 0; i < matchPositions.size(); ++i) {
      if (matchPositions[i] == targetPos) {
        matchIndex = i + 1;
        break;
      }
    }

    QString msg = QString("[%1/%2]").arg(matchIndex).arg(totalMatches);
    if (wrapped)
      msg += " search wrapped";
    emit statusMessage(msg);
  } else {
    emit statusMessage("Pattern not found");
  }
}

void VimMode::scrollLines(int lines) {
  QScrollBar *vbar = m_editor->verticalScrollBar();
  int lineHeight = m_editor->cursorRect().height();
  if (lineHeight <= 0)
    lineHeight = m_editor->fontMetrics().height();
  vbar->setValue(vbar->value() + lines * lineHeight);
}

void VimMode::indentText(VimMotion motion, int count, bool indent) {
  QTextCursor cursor = m_editor->textCursor();
  int startPos = cursor.position();
  executeMotion(motion, count);
  cursor = m_editor->textCursor();
  int endPos = cursor.position();

  int startBlock = m_editor->document()->findBlock(qMin(startPos, endPos)).blockNumber();
  int endBlock = m_editor->document()->findBlock(qMax(startPos, endPos)).blockNumber();

  cursor.beginEditBlock();
  for (int i = startBlock; i <= endBlock; ++i) {
    QTextBlock block = m_editor->document()->findBlockByNumber(i);
    QTextCursor lineCursor(block);
    lineCursor.movePosition(QTextCursor::StartOfLine);
    if (indent) {
      lineCursor.insertText("    ");
    } else {
      QString line = block.text();
      if (line.startsWith("    ")) {
        lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 4);
        lineCursor.removeSelectedText();
      } else if (line.startsWith("\t")) {
        lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
        lineCursor.removeSelectedText();
      }
    }
  }
  cursor.endEditBlock();
  cursor.setPosition(startPos);
  m_editor->setTextCursor(cursor);
}

void VimMode::toggleCase(VimMotion motion, int count) {
  QTextCursor cursor = m_editor->textCursor();
  int startPos = cursor.position();
  executeMotion(motion, count);
  cursor = m_editor->textCursor();
  int endPos = cursor.position();

  cursor.setPosition(qMin(startPos, endPos));
  cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);

  QString text = cursor.selectedText();
  QString result;
  for (const QChar &c : text) {
    if (c.isLower()) result += c.toUpper();
    else if (c.isUpper()) result += c.toLower();
    else result += c;
  }
  cursor.insertText(result);
  cursor.setPosition(qMin(startPos, endPos));
  m_editor->setTextCursor(cursor);
}

void VimMode::lowercaseText(VimMotion motion, int count) {
  QTextCursor cursor = m_editor->textCursor();
  int startPos = cursor.position();
  executeMotion(motion, count);
  cursor = m_editor->textCursor();
  int endPos = cursor.position();

  cursor.setPosition(qMin(startPos, endPos));
  cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);
  cursor.insertText(cursor.selectedText().toLower());
  cursor.setPosition(qMin(startPos, endPos));
  m_editor->setTextCursor(cursor);
}

void VimMode::uppercaseText(VimMotion motion, int count) {
  QTextCursor cursor = m_editor->textCursor();
  int startPos = cursor.position();
  executeMotion(motion, count);
  cursor = m_editor->textCursor();
  int endPos = cursor.position();

  cursor.setPosition(qMin(startPos, endPos));
  cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);
  cursor.insertText(cursor.selectedText().toUpper());
  cursor.setPosition(qMin(startPos, endPos));
  m_editor->setTextCursor(cursor);
}

bool VimMode::selectTextObject(VimTextObject textObj) {
  QTextCursor cursor = m_editor->textCursor();
  int pos = cursor.position();
  QString text = m_editor->toPlainText();

  QChar openChar, closeChar;
  bool isQuote = false;
  bool inner = false;

  switch (textObj) {
  case VimTextObject::InnerWord:
  case VimTextObject::AroundWord:
    cursor.select(QTextCursor::WordUnderCursor);
    if (textObj == VimTextObject::AroundWord) {
      int end = cursor.selectionEnd();
      if (end < text.length() && text[end].isSpace()) {
        cursor.setPosition(cursor.selectionStart());
        cursor.setPosition(end + 1, QTextCursor::KeepAnchor);
      }
    }
    m_editor->setTextCursor(cursor);
    return true;

  case VimTextObject::InnerWORD:
  case VimTextObject::AroundWORD: {
    // WORD = non-whitespace sequence
    int start = pos, end = pos;
    while (start > 0 && !text[start - 1].isSpace()) start--;
    while (end < text.length() && !text[end].isSpace()) end++;
    if (textObj == VimTextObject::AroundWORD) {
      while (end < text.length() && text[end].isSpace()) end++;
    }
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cursor);
    return true;
  }

  case VimTextObject::InnerParagraph:
  case VimTextObject::AroundParagraph: {
    inner = (textObj == VimTextObject::InnerParagraph);
    int blockNum = cursor.blockNumber();
    int startBlock = blockNum, endBlock = blockNum;
    bool inParagraph = !cursor.block().text().trimmed().isEmpty();
    if (inParagraph) {
      while (startBlock > 0 &&
             !m_editor->document()->findBlockByNumber(startBlock - 1).text().trimmed().isEmpty())
        startBlock--;
      while (endBlock < m_editor->document()->blockCount() - 1 &&
             !m_editor->document()->findBlockByNumber(endBlock + 1).text().trimmed().isEmpty())
        endBlock++;
      if (!inner) {
        while (endBlock < m_editor->document()->blockCount() - 1 &&
               m_editor->document()->findBlockByNumber(endBlock + 1).text().trimmed().isEmpty())
          endBlock++;
      }
    } else {
      while (startBlock > 0 &&
             m_editor->document()->findBlockByNumber(startBlock - 1).text().trimmed().isEmpty())
        startBlock--;
      while (endBlock < m_editor->document()->blockCount() - 1 &&
             m_editor->document()->findBlockByNumber(endBlock + 1).text().trimmed().isEmpty())
        endBlock++;
    }
    QTextBlock startB = m_editor->document()->findBlockByNumber(startBlock);
    QTextBlock endB = m_editor->document()->findBlockByNumber(endBlock);
    cursor.setPosition(startB.position());
    cursor.setPosition(endB.position() + endB.length() - 1, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cursor);
    return true;
  }

  case VimTextObject::InnerSentence:
  case VimTextObject::AroundSentence: {
    // Simple sentence: from previous sentence-end to next sentence-end
    QRegularExpression sentEnd("[.!?]\\s");
    int start = 0, end = text.length();
    // Find sentence start
    QRegularExpressionMatchIterator it = sentEnd.globalMatch(text.left(pos));
    while (it.hasNext()) {
      QRegularExpressionMatch m = it.next();
      start = m.capturedEnd();
    }
    while (start < text.length() && text[start].isSpace()) start++;
    // Find sentence end
    QRegularExpressionMatch match = sentEnd.match(text, pos);
    if (match.hasMatch()) {
      end = textObj == VimTextObject::InnerSentence ? match.capturedStart() + 1
                                                     : match.capturedEnd();
    }
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cursor);
    return true;
  }

  case VimTextObject::InnerTag:
  case VimTextObject::AroundTag: {
    inner = (textObj == VimTextObject::InnerTag);
    // Find enclosing HTML/XML tags
    // Search backward for <tag>
    int openEnd = -1, closeStart = -1;
    int searchPos = pos;
    // Find opening tag end
    for (int i = searchPos; i >= 0; --i) {
      if (text[i] == '>') {
        // Check if this is an opening tag (not </...>)
        int tagStart = text.lastIndexOf('<', i);
        if (tagStart >= 0 && tagStart < i) {
          QString tag = text.mid(tagStart, i - tagStart + 1);
          if (!tag.startsWith("</") && !tag.endsWith("/>")) {
            openEnd = i + 1;
            // Find matching closing tag
            QRegularExpression closeRegex("</" + QRegularExpression::escape(
                tag.mid(1, tag.indexOf(QRegularExpression("[\\s>]"), 1) - 1)) + ">");
            QRegularExpressionMatch closeMatch = closeRegex.match(text, openEnd);
            if (closeMatch.hasMatch()) {
              closeStart = closeMatch.capturedStart();
              if (inner) {
                cursor.setPosition(openEnd);
                cursor.setPosition(closeStart, QTextCursor::KeepAnchor);
              } else {
                cursor.setPosition(tagStart);
                cursor.setPosition(closeMatch.capturedEnd(), QTextCursor::KeepAnchor);
              }
              m_editor->setTextCursor(cursor);
              return true;
            }
          }
        }
      }
    }
    return false;
  }

  case VimTextObject::InnerParen:
  case VimTextObject::AroundParen:
    openChar = '('; closeChar = ')';
    inner = (textObj == VimTextObject::InnerParen);
    break;
  case VimTextObject::InnerBracket:
  case VimTextObject::AroundBracket:
    openChar = '['; closeChar = ']';
    inner = (textObj == VimTextObject::InnerBracket);
    break;
  case VimTextObject::InnerBrace:
  case VimTextObject::AroundBrace:
    openChar = '{'; closeChar = '}';
    inner = (textObj == VimTextObject::InnerBrace);
    break;
  case VimTextObject::InnerAngle:
  case VimTextObject::AroundAngle:
    openChar = '<'; closeChar = '>';
    inner = (textObj == VimTextObject::InnerAngle);
    break;
  case VimTextObject::InnerQuote:
  case VimTextObject::AroundQuote:
    openChar = closeChar = '"';
    isQuote = true;
    inner = (textObj == VimTextObject::InnerQuote);
    break;
  case VimTextObject::InnerSingleQuote:
  case VimTextObject::AroundSingleQuote:
    openChar = closeChar = '\'';
    isQuote = true;
    inner = (textObj == VimTextObject::InnerSingleQuote);
    break;
  case VimTextObject::InnerBacktick:
  case VimTextObject::AroundBacktick:
    openChar = closeChar = '`';
    isQuote = true;
    inner = (textObj == VimTextObject::InnerBacktick);
    break;
  default:
    return false;
  }

  if (isQuote) {
    QString line = cursor.block().text();
    int col = cursor.positionInBlock();
    int lineStart = cursor.position() - col;
    int openPos = -1, closePos = -1;
    bool foundOpen = false;

    for (int i = 0; i < line.length(); ++i) {
      if (line[i] == openChar) {
        if (!foundOpen) {
          if (i <= col) {
            openPos = i;
            foundOpen = true;
          }
        } else {
          closePos = i;
          if (i >= col) break;
          openPos = -1;
          closePos = -1;
          foundOpen = false;
        }
      }
    }

    if (openPos >= 0 && closePos > openPos) {
      int start = inner ? openPos + 1 : openPos;
      int end = inner ? closePos : closePos + 1;
      cursor.setPosition(lineStart + start);
      cursor.setPosition(lineStart + end, QTextCursor::KeepAnchor);
      m_editor->setTextCursor(cursor);
      return true;
    }
  } else {
    int openPos = -1;
    int depth = 0;

    for (int i = pos; i >= 0; --i) {
      if (text[i] == closeChar) depth++;
      else if (text[i] == openChar) {
        if (depth == 0) { openPos = i; break; }
        depth--;
      }
    }

    if (openPos >= 0) {
      depth = 1;
      for (int i = openPos + 1; i < text.length(); ++i) {
        if (text[i] == openChar) depth++;
        else if (text[i] == closeChar) {
          depth--;
          if (depth == 0) {
            int start = inner ? openPos + 1 : openPos;
            int end = inner ? i : i + 1;
            cursor.setPosition(start);
            cursor.setPosition(end, QTextCursor::KeepAnchor);
            m_editor->setTextCursor(cursor);
            return true;
          }
        }
      }
    }
  }

  return false;
}

void VimMode::executeOperatorOnTextObject(VimOperator op,
                                          VimTextObject textObj) {
  if (!selectTextObject(textObj))
    return;

  QTextCursor cursor = m_editor->textCursor();
  QString selected = cursor.selectedText();

  switch (op) {
  case VimOperator::Delete:
    deleteToRegister(selected);
    cursor.removeSelectedText();
    m_editor->setTextCursor(cursor);
    break;
  case VimOperator::Change:
    cursor.beginEditBlock();
    m_editor->setTextCursor(cursor);
    m_insertUndoOpen = true;
    deleteToRegister(selected);
    cursor = m_editor->textCursor();
    cursor.removeSelectedText();
    m_editor->setTextCursor(cursor);
    m_mode = VimEditMode::Insert;
    m_editor->setCursorWidth(1);
    emit modeChanged(m_mode);
    updatePendingKeys();
    break;
  case VimOperator::Yank:
    yankToRegister(selected);
    cursor.clearSelection();
    m_editor->setTextCursor(cursor);
    emit statusMessage("Yanked");
    break;
  case VimOperator::ToggleCase: {
    QString result;
    for (const QChar &c : selected) {
      if (c.isLower()) result += c.toUpper();
      else if (c.isUpper()) result += c.toLower();
      else result += c;
    }
    cursor.insertText(result);
    break;
  }
  case VimOperator::Lowercase:
    cursor.insertText(selected.toLower());
    break;
  case VimOperator::Uppercase:
    cursor.insertText(selected.toUpper());
    break;
  default:
    break;
  }
}
