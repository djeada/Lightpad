#include "vimmode.h"

#include <QTextBlock>
#include <memory>

namespace {

bool isVisualMode(VimEditMode mode) {
  return mode == VimEditMode::Visual || mode == VimEditMode::VisualLine ||
         mode == VimEditMode::VisualBlock;
}

} // namespace

void VimMode::startVisual(VimEditMode mode, int anchor, int pos) {
  m_visualAnchor = qBound(0, anchor, docLength());
  m_visualPos = qBound(0, pos, docLength());
  m_visualToEol = false;
  setMode(mode);
  updateVisualSelection();
}

void VimMode::storeVisualMarks() {
  int s = qMin(m_visualAnchor, m_visualPos);
  int e = qMax(m_visualAnchor, m_visualPos);
  if (m_mode == VimEditMode::VisualLine) {
    s = lineStart(lineOf(s));
    e = lineEndPos(lineOf(e));
  } else if (m_mode == VimEditMode::VisualBlock) {
    Range r = blockRange(m_visualAnchor, m_visualPos, m_visualToEol);
    s = lineStart(r.startLine) + colForVcol(lineText(r.startLine), r.startVcol);
    e = lineStart(r.endLine) + colForVcol(lineText(r.endLine), r.endVcol);
  }
  setMark('<', s);
  setMark('>', e);
  if (isVisualMode(m_mode)) {
    m_lastVisualMode = m_mode;
    m_lastVisualToEol = m_visualToEol;
  }
}

void VimMode::exitVisual(bool keepCursor) {
  if (!isVisualMode(m_mode))
    return;
  storeVisualMarks();
  int pos = m_visualPos;
  setMode(VimEditMode::Normal);
  m_visualSetAnchor = m_visualSetPos = -1;
  QTextCursor c = m_editor->textCursor();
  c.clearSelection();
  if (keepCursor)
    c.setPosition(clampNormal(pos));
  m_editor->setTextCursor(c);
  emit visualSelectionChanged();
}

void VimMode::updateVisualSelection() {
  if (!isVisualMode(m_mode))
    return;
  const int len = docLength();
  int anchorPos = m_visualAnchor;
  int cursor = m_visualPos;
  if (m_mode == VimEditMode::Visual) {
    if (m_visualPos >= m_visualAnchor) {
      anchorPos = m_visualAnchor;
      cursor = qMin(m_visualPos + 1, len);
    } else {
      anchorPos = qMin(m_visualAnchor + 1, len);
      cursor = m_visualPos;
    }
  } else if (m_mode == VimEditMode::VisualLine) {
    const int la = lineOf(m_visualAnchor);
    const int lp = lineOf(m_visualPos);
    const int last = lineCount() - 1;
    if (lp >= la) {
      anchorPos = lineStart(la);
      cursor = lp < last ? lineStart(lp + 1) : lineEndPos(lp);
    } else {
      anchorPos = la < last ? lineStart(la + 1) : lineEndPos(la);
      cursor = lineStart(lp);
    }
  } else {
    anchorPos = cursor = m_visualPos;
  }
  QTextCursor c = m_editor->textCursor();
  c.setPosition(anchorPos);
  c.setPosition(cursor, QTextCursor::KeepAnchor);
  m_editor->setTextCursor(c);
  m_visualSetAnchor = c.anchor();
  m_visualSetPos = c.position();
  emit visualSelectionChanged();
}

QVector<QPair<int, int>> VimMode::visualBlockRanges() const {
  QVector<QPair<int, int>> ranges;
  if (!m_enabled || m_mode != VimEditMode::VisualBlock)
    return ranges;
  Range r = blockRange(m_visualAnchor, m_visualPos, m_visualToEol);
  for (int l = r.startLine; l <= r.endLine; ++l) {
    const QString text = lineText(l);
    int from = colForVcol(text, r.startVcol);
    int to = r.toEol ? text.size() : colForVcol(text, r.endVcol + 1);
    if (to > from)
      ranges.append({lineStart(l) + from, lineStart(l) + to});
  }
  return ranges;
}

QStringList VimMode::visualDotPrefix() const {
  QString prefix;
  Range r = visualRange();
  int lines = r.endLine - r.startLine;
  switch (m_mode) {
  case VimEditMode::VisualLine:
    prefix = "V";
    if (lines > 0)
      prefix += QString("%1j").arg(lines);
    break;
  case VimEditMode::VisualBlock:
    prefix = "<C-v>";
    if (lines > 0)
      prefix += QString("%1j").arg(lines);
    if (r.toEol)
      prefix += "$";
    else if (r.endVcol > r.startVcol)
      prefix += QString("%1l").arg(r.endVcol - r.startVcol);
    break;
  default: {
    prefix = "v";
    int s = qMin(m_visualAnchor, m_visualPos);
    int e = qMax(m_visualAnchor, m_visualPos);
    if (lines > 0) {
      prefix += QString("%1j0").arg(lines);
      int col = colOf(e);
      if (col > 0)
        prefix += QString("%1l").arg(col);
    } else if (e > s) {
      prefix += QString("%1l").arg(e - s);
    }
    break;
  }
  }
  return parseKeyNotation(prefix);
}

bool VimMode::handleVisualKey(const QString &token) {
  QTextCursor editorCursor = m_editor->textCursor();
  if (m_visualSetPos >= 0 && (editorCursor.anchor() != m_visualSetAnchor ||
                              editorCursor.position() != m_visualSetPos)) {
    if (editorCursor.hasSelection() && m_mode != VimEditMode::VisualBlock) {
      int a = editorCursor.anchor();
      int p = editorCursor.position();
      m_visualAnchor = a > p ? a - 1 : a;
      m_visualPos = p > a ? p - 1 : p;
      if (m_mode == VimEditMode::VisualLine)
        setMode(VimEditMode::Visual);
      updateVisualSelection();
    } else if (!editorCursor.hasSelection()) {
      int p = editorCursor.position();
      m_visualPos = p;
      exitVisual();
      return handleNormalKey(token);
    }
  }

  if (token == "<Esc>" || token == "<C-c>") {
    if (!m_pending.isEmpty())
      resetPending();
    else
      exitVisual();
    return true;
  }

  m_pending << token;
  NormalCmd cmd;
  Parse p = parseCommand(m_pending, cmd, true);
  if (p == Parse::Incomplete) {
    updatePendingKeys();
    return true;
  }
  const QStringList keys = m_pending;
  resetPending();
  if (p == Parse::Invalid) {
    if (keys.size() == 1 && token.startsWith("<") && token.size() > 3)
      return false;
    return true;
  }
  m_undoCursors[doc()->availableUndoSteps()] =
      qMin(m_visualAnchor, m_visualPos);
  executeVisual(cmd, keys);
  return true;
}

void VimMode::executeVisual(const NormalCmd &cmdIn, const QStringList &keys) {
  NormalCmd cmd = cmdIn;
  if (m_dotCountOverride > 0) {
    m_dotCountOverride = 0;
  }
  const QString &key = cmd.key;
  const int count = cmd.count;
  const int c1 = qMax(1, count);

  if (!cmd.textObject.isEmpty()) {
    int s = 0, e = 0;
    MotionType type = MotionType::Inclusive;
    if (!evalTextObject(cmd.textObject, count, s, e, type, true))
      return;
    if (type == MotionType::Linewise) {
      if (m_mode != VimEditMode::VisualLine)
        setMode(VimEditMode::VisualLine);
      int anchorLine = lineOf(s);
      if (m_visualAnchor != m_visualPos &&
          lineOf(qMin(m_visualAnchor, m_visualPos)) < anchorLine)
        anchorLine = lineOf(qMin(m_visualAnchor, m_visualPos));
      m_visualAnchor = lineStart(anchorLine);
      m_visualPos = lineStart(lineOf(e));
    } else {
      if (m_mode != VimEditMode::Visual)
        setMode(VimEditMode::Visual);
      m_visualAnchor = s;
      int last = type == MotionType::Inclusive ? e : qMax(s, e - 1);
      if (type == MotionType::Exclusive && last > s && charAt(last) == '\n' &&
          cmd.textObject.endsWith('s'))
        --last;
      m_visualPos = last;
    }
    updateVisualSelection();
    return;
  }

  if (isMotionCommand(key)) {
    if (key == "/" || key == "?") {
      m_cmdOperatorKeys.clear();
      m_cmdCount = count;
      m_cmdFromVisual = true;
      enterCommandLine(key[0], QString());
      return;
    }
    MotionResult r = evalMotion(key, cmd.arg, count, false, m_visualPos, true);
    if (!r.ok)
      return;
    if (r.jump)
      pushJump(m_visualPos);
    int target = r.pos;
    m_visualPos = target;
    if (m_mode == VimEditMode::VisualBlock)
      m_visualToEol = r.wantEol;
    if (!r.keepWantCol) {
      int line = lineOf(target);
      m_wantCol = vcolOf(lineText(line), target - lineStart(line));
      m_wantEol = r.wantEol;
    }
    updateVisualSelection();
    return;
  }

  if (key == "o" || key == "O") {
    if (key == "O" && m_mode == VimEditMode::VisualBlock) {
      int la = lineOf(m_visualAnchor), lp = lineOf(m_visualPos);
      int ca = colOf(m_visualAnchor), cp = colOf(m_visualPos);
      m_visualAnchor = posOf(la, cp);
      m_visualPos = posOf(lp, ca);
    } else {
      std::swap(m_visualAnchor, m_visualPos);
    }
    updateVisualSelection();
    return;
  }
  if (key == "v" || key == "V" || key == "<C-v>" || key == "<C-q>") {
    VimEditMode target = key == "v"   ? VimEditMode::Visual
                         : key == "V" ? VimEditMode::VisualLine
                                      : VimEditMode::VisualBlock;
    if (target == m_mode) {
      exitVisual();
    } else {
      setMode(target);
      updateVisualSelection();
    }
    return;
  }
  if (key == ":") {
    const int startPos =
        m_mode == VimEditMode::VisualLine
            ? lineStart(lineOf(qMin(m_visualAnchor, m_visualPos)))
            : qMin(m_visualAnchor, m_visualPos);
    exitVisual();
    setCursorPos(clampNormal(startPos));
    m_cmdFromVisual = false;
    enterCommandLine(':', "'<,'>");
    return;
  }
  if (key == "gv") {
    int s = 0, e = 0;
    if (!markPosition('<', s) || !markPosition('>', e))
      return;
    VimEditMode lastMode = m_lastVisualMode;
    bool lastEol = m_lastVisualToEol;
    storeVisualMarks();
    setMode(lastMode);
    m_visualAnchor = s;
    m_visualPos =
        lastMode == VimEditMode::VisualLine ? lineStart(lineOf(e)) : e;
    m_visualToEol = lastEol;
    updateVisualSelection();
    return;
  }
  if (key == "gn" || key == "gN") {
    int s = 0, e = 0;
    if (selectSearchMatch(key == "gn", s, e)) {
      m_visualPos = key == "gn" ? e : s;
      updateVisualSelection();
    }
    return;
  }
  if (key == "<C-e>" || key == "<C-y>") {
    scrollLines(key == "<C-e>" ? c1 : -c1, false);
    return;
  }
  if (key == "zz" || key == "zt" || key == "zb") {
    scrollCursorTo(key == "zz" ? 1 : key == "zt" ? 0 : 2, false);
    return;
  }
  if (key == "<C-d>" || key == "<C-u>" || key == "<C-f>" || key == "<C-b>") {
    int line = lineOf(m_visualPos);
    int amount = (key == "<C-d>" || key == "<C-u>")
                     ? (count > 0 ? count : qMax(1, visibleLineCount() / 2))
                     : c1 * qMax(1, visibleLineCount() - 2);
    bool down = key == "<C-d>" || key == "<C-f>";
    m_visualPos = posForWantCol(line + (down ? amount : -amount));
    updateVisualSelection();
    return;
  }

  const QStringList dotKeys = visualDotPrefix() + keys;
  VimEditMode mode = m_mode;
  Range range = visualRange();
  const QChar reg = cmd.reg;

  auto finishChange = [&]() { setDotCommand(dotKeys); };

  if (key == "d" || key == "x" || key == "<Del>" || key == "X" || key == "D") {
    if ((key == "X" || key == "D") && mode != VimEditMode::VisualBlock)
      range = lineRange(range.startLine, range.endLine);
    if (key == "D" && mode == VimEditMode::VisualBlock)
      range.toEol = true;
    exitVisual(false);
    applyOperator("d", range, reg, count, true);
    finishChange();
  } else if (key == "y" || key == "Y") {
    if (key == "Y" && mode != VimEditMode::VisualBlock)
      range = lineRange(range.startLine, range.endLine);
    if (key == "Y" && mode == VimEditMode::VisualBlock)
      range.toEol = true;
    exitVisual(false);
    applyOperator("y", range, reg, count, true);
  } else if (key == "c" || key == "s" || key == "C" || key == "S" ||
             key == "R") {
    if (mode == VimEditMode::VisualBlock) {
      if (key == "C")
        range.toEol = true;
      exitVisual(false);
      const int revision = doc()->revision();
      applyOperator("d", range, reg, count, true);
      m_insertEditOpen = doc()->revision() != revision;
      m_blockInsertActive = range.endLine > range.startLine;
      m_blockInsertAppend = false;
      m_blockInsertToEol = false;
      m_blockInsertFirstLine = range.startLine;
      m_blockInsertLastLine = range.endLine;
      m_blockInsertVcol = range.startVcol;
      m_blockInsertStartVcol = range.startVcol;
      const QString text = lineText(range.startLine);
      setCursorPos(lineStart(range.startLine) +
                   colForVcol(text, range.startVcol));
      m_blockInsertLineLength = lineLength(range.startLine);
      startInsertSession(1, "c", dotKeys);
      return;
    }
    if (key != "c" && key != "s")
      range = lineRange(range.startLine, range.endLine);
    exitVisual(false);
    const int revision = doc()->revision();
    applyOperator("c", range, reg, count, true);
    m_insertEditOpen = doc()->revision() != revision;
    startInsertSession(1, "c", dotKeys);
  } else if (key == "r") {
    exitVisual(false);
    QTextCursor block(doc());
    block.beginEditBlock();
    if (range.type == VimRegisterType::Blockwise) {
      for (int l = range.startLine; l <= range.endLine; ++l) {
        const QString text = lineText(l);
        int from = colForVcol(text, range.startVcol);
        int to =
            range.toEol ? text.size() : colForVcol(text, range.endVcol + 1);
        if (to > from)
          replaceRange(lineStart(l) + from, lineStart(l) + to,
                       QString(to - from, cmd.arg));
      }
      setCursorPos(lineStart(range.startLine) +
                   colForVcol(lineText(range.startLine), range.startVcol));
    } else {
      QString text = rangeText(range);
      if (range.type == VimRegisterType::Linewise)
        text.chop(1);
      int start = range.type == VimRegisterType::Linewise
                      ? lineStart(range.startLine)
                      : range.start;
      QString replaced;
      for (QChar ch : text)
        replaced += ch == '\n' ? ch : cmd.arg;
      replaceRange(start, start + text.size(), replaced);
      setCursorPos(clampNormal(start));
    }
    block.endEditBlock();
    recordChangePosition(range.start);
    finishChange();
  } else if (key == "J" || key == "gJ") {
    exitVisual(false);
    joinLines(range.startLine, qMax(2, range.endLine - range.startLine + 1),
              key == "J");
    finishChange();
  } else if (key == ">" || key == "<" || key == "=" || key == "~" ||
             key == "u" || key == "U" || key == "g~" || key == "gu" ||
             key == "gU" || key == "g?" || key == "gq" || key == "gw") {
    QString op = key;
    if (key == "~")
      op = "g~";
    else if (key == "u")
      op = "gu";
    else if (key == "U")
      op = "gU";
    exitVisual(false);
    applyOperator(op, range, reg, count, true);
    finishChange();
  } else if (key == "p" || key == "P") {
    visualPut(reg, c1, key == "P");
    finishChange();
  } else if (key == "I" || key == "A") {
    if (mode == VimEditMode::VisualBlock) {
      blockInsert(key == "A");
      return;
    }
    exitVisual(false);
    if (key == "I") {
      int start = mode == VimEditMode::VisualLine
                      ? firstNonBlankPos(range.startLine)
                      : range.start;
      setCursorPos(start);
    } else {
      int end = mode == VimEditMode::VisualLine ? lineEndPos(range.endLine)
                                                : range.end;
      QTextCursor c = m_editor->textCursor();
      c.setPosition(qBound(0, end, docLength()));
      m_editor->setTextCursor(c);
    }
    m_insertEditOpen = false;
    startInsertSession(1, "i", QStringList() << "i");
  } else if (key == "<C-a>" || key == "<C-x>" || key == "g<C-a>" ||
             key == "g<C-x>") {
    exitVisual(false);
    bool progressive = key.startsWith('g');
    int delta = (key.endsWith("a>") ? 1 : -1) * c1;
    QTextCursor block(doc());
    block.beginEditBlock();
    int step = 1;
    for (int l = range.startLine; l <= range.endLine; ++l) {
      int col = 0;
      if (range.type == VimRegisterType::Charwise && l == range.startLine)
        col = colOf(range.start);
      else if (range.type == VimRegisterType::Blockwise)
        col = colForVcol(lineText(l), range.startVcol);
      int dummy = 0;
      int endCol = -1;
      if (range.type == VimRegisterType::Charwise && l == range.endLine &&
          lineOf(range.end) == l)
        endCol = colOf(range.end);
      else if (range.type == VimRegisterType::Blockwise)
        endCol = range.toEol ? -1 : colForVcol(lineText(l), range.endVcol + 1);
      if (incrementNumber(l, col, progressive ? delta * step : delta, endCol,
                          &dummy))
        ++step;
    }
    block.endEditBlock();
    setCursorPos(clampNormal(range.type == VimRegisterType::Linewise
                                 ? lineStart(range.startLine)
                                 : range.start));
    finishChange();
  } else if (key == "zf") {
    exitVisual();
    emit commandExecuted("fold");
  }
}

void VimMode::blockInsert(bool append) {
  Range r = blockRange(m_visualAnchor, m_visualPos, m_visualToEol);
  QStringList dotKeys = visualDotPrefix();
  dotKeys << (append ? "A" : "I");
  exitVisual(false);
  m_blockInsertActive = true;
  m_blockInsertAppend = append;
  m_blockInsertToEol = append && r.toEol;
  m_blockInsertFirstLine = r.startLine;
  m_blockInsertLastLine = r.endLine;
  m_blockInsertVcol = append ? r.endVcol + 1 : r.startVcol;
  m_blockInsertStartVcol = r.startVcol;

  const QString text = lineText(r.startLine);
  int col;
  m_insertEditOpen = false;
  if (m_blockInsertToEol) {
    col = text.size();
  } else {
    col = colForVcol(text, m_blockInsertVcol);
    int lineVcol = vcolOf(text, text.size());
    if (append && lineVcol < m_blockInsertVcol) {
      QTextCursor b(doc());
      b.beginEditBlock();
      replaceRange(lineEndPos(r.startLine), lineEndPos(r.startLine),
                   QString(m_blockInsertVcol - lineVcol, ' '));
      b.endEditBlock();
      m_insertEditOpen = true;
      col = lineLength(r.startLine);
    }
  }
  QTextCursor c = m_editor->textCursor();
  c.setPosition(lineStart(r.startLine) + col);
  m_editor->setTextCursor(c);
  m_blockInsertLineLength = lineLength(r.startLine);
  startInsertSession(1, append ? "blockA" : "blockI", dotKeys);
}

void VimMode::enterInsert(const QString &how, int count,
                          const QStringList &keys) {
  int pos = cursorPos();
  int line = lineOf(pos);
  m_insertEditOpen = false;
  if (how == "a") {
    if (lineLength(line) > 0)
      pos = qMin(pos + 1, lineEndPos(line));
  } else if (how == "A") {
    pos = lineEndPos(line);
  } else if (how == "I") {
    pos = lineStart(line) + firstNonBlankCol(line);
  } else if (how == "gI") {
    pos = lineStart(line);
  } else if (how == "gi") {
    int mark = 0;
    if (markPosition('^', mark))
      pos = mark;
  } else if (how == "o" || how == "O") {
    QTextCursor block(doc());
    block.beginEditBlock();
    QTextCursor c = m_editor->textCursor();
    c.setPosition(pos);
    m_editor->setTextCursor(c);
    openLine(how == "O");
    block.endEditBlock();
    m_insertEditOpen = true;
    pos = cursorPos();
  }
  QTextCursor c = m_editor->textCursor();
  c.setPosition(qBound(0, pos, docLength()));
  m_editor->setTextCursor(c);
  startInsertSession(count, how, keys);
}

QString VimMode::indentForNewLine(int line) const {
  if (!m_autoIndent)
    return QString();
  const QString text = lineText(line);
  return text.left(firstNonBlankCol(line));
}

void VimMode::openLine(bool above) {
  int line = lineOf(cursorPos());
  QString indent = indentForNewLine(line);
  QTextCursor c(doc());
  if (above) {
    c.setPosition(lineStart(line));
    c.insertText(indent + "\n");
    c.setPosition(lineStart(line) + indent.size());
  } else {
    c.setPosition(lineEndPos(line));
    c.insertText("\n" + indent);
  }
  m_editor->setTextCursor(c);
}

void VimMode::startInsertSession(int count, const QString &kind,
                                 const QStringList &dotKeys) {
  if (!m_insertEditOpen && !m_inDotRepeat)
    m_undoCursors[doc()->availableUndoSteps()] =
        m_editor->textCursor().position();
  m_insertKind = kind;
  m_insertCount = qMax(1, count);
  m_insertKeys.clear();
  m_insertStartPos = m_editor->textCursor().position();
  m_insertLiteralNext = false;
  m_insertRegisterPending = false;
  m_dotRecordingActive = !m_inDotRepeat;
  m_dotRecording = dotKeys;
  if (!kind.startsWith("block") && kind != "c")
    m_blockInsertActive = false;
  if (kind == "R") {
    m_replacedChars.clear();
    setMode(VimEditMode::Replace);
  } else {
    setMode(VimEditMode::Insert);
  }
}

void VimMode::dispatchToEditor(const QString &token, QKeyEvent *event) {
  std::unique_ptr<QKeyEvent> synthetic;
  QKeyEvent *ev = event;
  if (!ev || keyEventToToken(ev) != token) {
    synthetic = tokenToKeyEvent(token);
    if (!synthetic)
      return;
    ev = synthetic.get();
  }
  QTextCursor c = m_editor->textCursor();
  if (m_insertEditOpen) {
    c.joinPreviousEditBlock();
  } else {
    c.beginEditBlock();
    m_insertEditOpen = true;
  }
  m_passthrough = true;
  static_cast<QObject *>(m_editor)->event(ev);
  m_passthrough = false;
  c.endEditBlock();
  if (event)
    event->accept();
}

void VimMode::insertTextAtCursor(const QString &text) {
  if (text.isEmpty())
    return;
  QTextCursor c = m_editor->textCursor();
  if (m_insertEditOpen) {
    c.joinPreviousEditBlock();
  } else {
    c.beginEditBlock();
    m_insertEditOpen = true;
  }
  c.insertText(text);
  c.endEditBlock();
  m_editor->setTextCursor(c);
}

bool VimMode::handleInsertKey(const QString &token, QKeyEvent *event) {
  if (m_insertRegisterPending) {
    m_insertRegisterPending = false;
    updatePendingKeys();
    if (token.size() == 1 && isValidRegister(token[0])) {
      m_insertKeys << token;
      if (m_dotRecordingActive)
        m_dotRecording << token;
      VimRegister r = getRegister(token[0]);
      QString text = r.content;
      insertTextAtCursor(text);
    }
    return true;
  }
  if (m_insertLiteralNext) {
    m_insertLiteralNext = false;
    QChar ch;
    if (tokenToArg(token, ch, true) || token == "<Esc>") {
      m_insertKeys << token;
      if (m_dotRecordingActive)
        m_dotRecording << token;
      insertTextAtCursor(token == "<Esc>" ? QString(QChar(0x1b)) : QString(ch));
    }
    return true;
  }

  const bool navigation =
      token == "<Left>" || token == "<Right>" || token == "<Up>" ||
      token == "<Down>" || token == "<Home>" || token == "<End>" ||
      token == "<PageUp>" || token == "<PageDown>" || token.startsWith("<S-") ||
      token.startsWith("<C-Left") || token.startsWith("<C-Right");
  if (navigation && token != "<S-Tab>") {
    m_insertEditOpen = false;
    m_insertKeys.clear();
    m_insertCount = 1;
    if (m_dotRecordingActive)
      m_dotRecording = QStringList() << "i";
    return false;
  }

  static const QStringList handledCtrl = {
      "<C-w>", "<C-u>", "<C-h>", "<C-t>", "<C-d>", "<C-r>", "<C-v>",
      "<C-q>", "<C-o>", "<C-e>", "<C-y>", "<C-a>", "<C-c>"};
  const bool editKey =
      token.size() == 1 || token == "<CR>" || token == "<BS>" ||
      token == "<Del>" || token == "<Tab>" || token == "<S-Tab>" ||
      token == "<Esc>" || token == "<Insert>" || handledCtrl.contains(token);
  if (!editKey)
    return false;

  if (!m_insertRepeating) {
    m_insertKeys << token;
    if (m_dotRecordingActive)
      m_dotRecording << token;
  }

  if (token == "<Esc>" || token == "<C-c>") {
    if (!m_insertRepeating) {
      m_insertKeys.removeLast();
      if (m_dotRecordingActive)
        m_dotRecording.removeLast();
    }
    finishInsertSession();
    return true;
  }
  if (token == "<Insert>") {
    m_replacedChars.clear();
    setMode(VimEditMode::Replace);
    return true;
  }

  const int pos = m_editor->textCursor().position();
  const int line = lineOf(pos);
  const int col = pos - lineStart(line);

  if (token == "<C-o>") {
    m_insertKeys.removeLast();
    m_insertOneCommand = true;
    setMode(VimEditMode::Normal);
    return true;
  }
  if (token == "<C-r>") {
    m_insertRegisterPending = true;
    updatePendingKeys();
    return true;
  }
  if (token == "<C-v>" || token == "<C-q>") {
    m_insertLiteralNext = true;
    return true;
  }
  if (token == "<C-h>") {
    dispatchToEditor("<BS>", nullptr);
    return true;
  }
  if (token == "<C-w>" || token == "<C-u>") {
    if (col == 0) {
      dispatchToEditor("<BS>", nullptr);
      return true;
    }
    const QString text = lineText(line);
    int i = col;
    if (token == "<C-w>") {
      while (i > 0 && (text[i - 1] == ' ' || text[i - 1] == '\t'))
        --i;
      if (i > 0) {
        bool word = text[i - 1].isLetterOrNumber() || text[i - 1] == '_';
        while (i > 0 && text[i - 1] != ' ' && text[i - 1] != '\t' &&
               (text[i - 1].isLetterOrNumber() || text[i - 1] == '_') == word)
          --i;
      }
    } else {
      i = 0;
    }
    const int startCol = m_insertStartPos - lineStart(line);
    if (startCol > 0 && startCol < col && i < startCol &&
        m_insertStartPos >= lineStart(line))
      i = startCol;
    QTextCursor c = m_editor->textCursor();
    if (m_insertEditOpen) {
      c.joinPreviousEditBlock();
    } else {
      c.beginEditBlock();
      m_insertEditOpen = true;
    }
    c.setPosition(lineStart(line) + i);
    c.setPosition(pos, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    c.endEditBlock();
    m_editor->setTextCursor(c);
    return true;
  }
  if (token == "<C-t>" || token == "<C-d>") {
    const QString text = lineText(line);
    int fnb = firstNonBlankCol(line);
    int width = indentWidth(text);
    int newWidth =
        token == "<C-t>"
            ? (width / m_shiftWidth + 1) * m_shiftWidth
            : qMax(0, ((width + m_shiftWidth - 1) / m_shiftWidth - 1) *
                          m_shiftWidth);
    QString indent = indentString(newWidth);
    QTextCursor c = m_editor->textCursor();
    if (m_insertEditOpen) {
      c.joinPreviousEditBlock();
    } else {
      c.beginEditBlock();
      m_insertEditOpen = true;
    }
    c.setPosition(lineStart(line));
    c.setPosition(lineStart(line) + fnb, QTextCursor::KeepAnchor);
    c.insertText(indent);
    c.endEditBlock();
    int newCol = qMax(0, col + int(indent.size()) - fnb);
    c.setPosition(lineStart(line) + newCol);
    m_editor->setTextCursor(c);
    return true;
  }
  if (token == "<C-e>" || token == "<C-y>") {
    int other = line + (token == "<C-e>" ? 1 : -1);
    if (other < 0 || other >= lineCount())
      return true;
    const QString otherText = lineText(other);
    int vcol = vcolOf(lineText(line), col);
    int otherCol = colForVcol(otherText, vcol);
    if (otherCol < otherText.size())
      insertTextAtCursor(QString(otherText[otherCol]));
    return true;
  }
  if (token == "<C-a>") {
    insertTextAtCursor(m_lastInsertedText);
    return true;
  }
  if (token == "<Tab>" && m_expandTab) {
    const int vcol = vcolOf(lineText(line), col);
    insertTextAtCursor(QString(m_tabStop - (vcol % m_tabStop), ' '));
    if (event)
      event->accept();
    return true;
  }

  dispatchToEditor(token, event);
  return true;
}

void VimMode::finishInsertSession() {
  if (m_insertCount > 1 && !m_insertKeys.isEmpty()) {
    const QStringList keys = m_insertKeys;
    int repeats = m_insertCount - 1;
    m_insertCount = 1;
    m_insertRepeating = true;
    for (int i = 0; i < repeats; ++i) {
      if (m_insertKind == "o" || m_insertKind == "O") {
        QTextCursor c = m_editor->textCursor();
        c.joinPreviousEditBlock();
        openLine(false);
        c.endEditBlock();
      }
      for (const QString &k : keys) {
        if (m_mode == VimEditMode::Replace)
          handleReplaceKey(k, nullptr);
        else
          handleInsertKey(k, nullptr);
      }
    }
    m_insertRepeating = false;
  }

  if (m_blockInsertActive) {
    m_blockInsertActive = false;
    const int first = m_blockInsertFirstLine;
    const int cur = m_editor->textCursor().position();
    const int grown = lineLength(first) - m_blockInsertLineLength;
    if (lineOf(cur) == first && grown > 0 && m_blockInsertLastLine > first) {
      const QString firstText = lineText(first);
      int insertCol = m_blockInsertToEol
                          ? m_blockInsertLineLength
                          : colForVcol(firstText, m_blockInsertVcol);
      if (m_blockInsertToEol)
        insertCol = m_blockInsertLineLength;
      const QString inserted = firstText.mid(insertCol, grown);
      QTextCursor c = m_editor->textCursor();
      c.joinPreviousEditBlock();
      for (int l = first + 1; l <= m_blockInsertLastLine; ++l) {
        const QString text = lineText(l);
        const int lineVcol = vcolOf(text, text.size());
        if (m_blockInsertToEol) {
          replaceRange(lineEndPos(l), lineEndPos(l), inserted);
          continue;
        }
        if (lineVcol < m_blockInsertVcol) {
          if (!m_blockInsertAppend)
            continue;
          replaceRange(lineEndPos(l), lineEndPos(l),
                       QString(m_blockInsertVcol - lineVcol, ' ') + inserted);
          continue;
        }
        int col = colForVcol(text, m_blockInsertVcol);
        replaceRange(lineStart(l) + col, lineStart(l) + col, inserted);
      }
      c.endEditBlock();
      QTextCursor restore = m_editor->textCursor();
      restore.setPosition(lineStart(first) +
                          colForVcol(lineText(first), m_blockInsertStartVcol) +
                          1);
      m_editor->setTextCursor(restore);
    }
  }

  QString inserted;
  for (const QString &k : m_insertKeys) {
    if (k.size() == 1)
      inserted += k;
    else if (k == "<CR>")
      inserted += '\n';
    else if (k == "<Tab>")
      inserted += '\t';
    else if (k == "<BS>" && !inserted.isEmpty())
      inserted.chop(1);
  }
  if (!m_insertKeys.isEmpty())
    m_lastInsertedText = inserted;

  int pos = m_editor->textCursor().position();
  setMark('^', pos);
  if (doc()->revision() != 0)
    recordChangePosition(qMax(0, pos - 1));

  if (m_dotRecordingActive) {
    m_dotRecording << "<Esc>";
    setDotCommand(m_dotRecording);
    m_dotRecordingActive = false;
  }

  setMode(VimEditMode::Normal);
  int line = lineOf(pos);
  if (pos > lineStart(line))
    --pos;
  setCursorPos(clampNormal(pos));
}

bool VimMode::handleReplaceKey(const QString &token, QKeyEvent *event) {
  const bool navigation = token == "<Left>" || token == "<Right>" ||
                          token == "<Up>" || token == "<Down>" ||
                          token == "<Home>" || token == "<End>";
  if (navigation) {
    m_insertEditOpen = false;
    m_replacedChars.clear();
    m_insertKeys.clear();
    m_insertCount = 1;
    return false;
  }
  const bool editKey = token.size() == 1 || token == "<CR>" ||
                       token == "<BS>" || token == "<Esc>" ||
                       token == "<C-c>" || token == "<Insert>" ||
                       token == "<Tab>";
  if (!editKey)
    return false;

  if (!m_insertRepeating) {
    m_insertKeys << token;
    if (m_dotRecordingActive)
      m_dotRecording << token;
  }

  if (token == "<Esc>" || token == "<C-c>") {
    if (!m_insertRepeating) {
      m_insertKeys.removeLast();
      if (m_dotRecordingActive)
        m_dotRecording.removeLast();
    }
    finishInsertSession();
    return true;
  }
  if (token == "<Insert>") {
    setMode(VimEditMode::Insert);
    return true;
  }

  int pos = m_editor->textCursor().position();
  if (token == "<BS>") {
    if (pos <= m_insertStartPos || m_replacedChars.isEmpty()) {
      if (pos > m_insertStartPos) {
        QTextCursor c = m_editor->textCursor();
        c.setPosition(pos - 1);
        m_editor->setTextCursor(c);
      }
      return true;
    }
    QChar original = m_replacedChars.takeLast();
    QTextCursor c = m_editor->textCursor();
    c.joinPreviousEditBlock();
    if (original.isNull() || original == QChar(0xFFFE))
      replaceRange(pos - 1, pos, QString());
    else
      replaceRange(pos - 1, pos, QString(original));
    c.endEditBlock();
    c.setPosition(pos - 1);
    m_editor->setTextCursor(c);
    return true;
  }
  if (token == "<CR>") {
    const int before = docLength();
    dispatchToEditor(token, event);
    const int inserted = docLength() - before;
    for (int k = 0; k < inserted; ++k)
      m_replacedChars.append(k == 0 ? QChar(0xFFFE) : QChar());
    return true;
  }

  QString text = token == "<Tab>" ? QString("\t") : token;
  QTextCursor c = m_editor->textCursor();
  if (m_insertEditOpen) {
    c.joinPreviousEditBlock();
  } else {
    c.beginEditBlock();
    m_insertEditOpen = true;
  }
  QChar under = charAt(pos);
  if (!under.isNull() && under != '\n') {
    m_replacedChars.append(under);
    c.setPosition(pos);
    c.setPosition(pos + 1, QTextCursor::KeepAnchor);
  } else {
    m_replacedChars.append(QChar());
    c.setPosition(pos);
  }
  c.insertText(text);
  c.endEditBlock();
  m_editor->setTextCursor(c);
  if (event)
    event->accept();
  return true;
}
