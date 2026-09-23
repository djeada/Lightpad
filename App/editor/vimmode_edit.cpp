#include "vimmode.h"

#include <QRegularExpression>
#include <QTextBlock>
#include <limits>

namespace {

QString transformCase(const QString &op, const QString &text) {
  if (op == "gu")
    return text.toLower();
  if (op == "gU")
    return text.toUpper();
  QString out = text;
  for (int i = 0; i < out.size(); ++i) {
    QChar c = out[i];
    if (op == "g?") {
      ushort u = c.unicode();
      if (u >= 'a' && u <= 'z')
        out[i] = QChar('a' + (u - 'a' + 13) % 26);
      else if (u >= 'A' && u <= 'Z')
        out[i] = QChar('A' + (u - 'A' + 13) % 26);
    } else if (c.isLower()) {
      out[i] = c.toUpper();
    } else if (c.isUpper()) {
      out[i] = c.toLower();
    }
  }
  return out;
}

} // namespace

void VimMode::setAutoIndent(bool enabled) { m_autoIndent = enabled; }

void VimMode::applyOperator(const QString &op, const Range &range, QChar reg,
                            int count, bool fromVisual) {
  if (op != "y") {
    int undoPos = range.start;
    if (range.type == VimRegisterType::Linewise)
      undoPos =
          -2 - colOf(qMax(0, m_undoCursors.value(doc()->availableUndoSteps(),
                                                 cursorPos())));
    else if (range.type == VimRegisterType::Blockwise)
      undoPos = lineStart(range.startLine) +
                colForVcol(lineText(range.startLine), range.startVcol);
    m_undoCursors[doc()->availableUndoSteps()] = undoPos;
  }
  QTextCursor block(doc());
  block.beginEditBlock();
  if (op == "d") {
    operatorDelete(range, reg, false, m_opForceNumbered);
  } else if (op == "c") {
    operatorDelete(range, reg, true, m_opForceNumbered);
  } else if (op == "y") {
    operatorYank(range, reg);
  } else if (op == ">" || op == "<") {
    operatorShift(range, op == ">", fromVisual ? qMax(1, count) : 1);
  } else if (op == "=") {
    operatorReindent(range);
  } else if (op == "g~" || op == "gu" || op == "gU" || op == "g?") {
    operatorCase(op, range);
  } else if (op == "gq" || op == "gw") {
    operatorFormat(range, op == "gw");
  }
  block.endEditBlock();

  const int startPos = range.type == VimRegisterType::Linewise
                           ? lineStart(range.startLine)
                           : range.start;
  setMark('[', startPos);
  if (op == "y") {
    setMark(']', qMax(startPos, range.end - 1));
  } else {
    setMark(']', cursorPos());
    if (op != "c")
      recordChangePosition(cursorPos());
  }
}

void VimMode::operatorDelete(const Range &range, QChar reg, bool change,
                             bool forceNumbered) {
  const QString text = rangeText(range);
  const int last = lineCount() - 1;
  switch (range.type) {
  case VimRegisterType::Linewise: {
    storeDeleted(reg, text, VimRegisterType::Linewise, true);
    const int sl = range.startLine;
    const int el = range.endLine;
    forgetLines(change ? sl + 1 : sl, el, true);
    if (change) {
      QString indent = indentForNewLine(sl);
      replaceRange(lineStart(sl), lineEndPos(el), indent);
      QTextCursor c = m_editor->textCursor();
      c.setPosition(lineStart(sl) + indent.size());
      m_editor->setTextCursor(c);
      return;
    }
    if (el < last)
      replaceRange(lineStart(sl), lineStart(el + 1), QString());
    else if (sl > 0)
      replaceRange(lineEndPos(sl - 1), docLength(), QString());
    else
      replaceRange(0, docLength(), QString());
    const int lines = el - sl + 1;
    if (lines > 2)
      emit statusMessage(QString("%1 fewer lines").arg(lines));
    setCursorPos(firstNonBlankPos(qMin(sl, lineCount() - 1)));
    return;
  }
  case VimRegisterType::Charwise: {
    storeDeleted(reg, text, VimRegisterType::Charwise, forceNumbered);
    const int firstLine = lineOf(range.start);
    const int lastLine = lineOf(range.end);
    if (lastLine > firstLine) {
      forgetLines(firstLine + 1, lastLine - 1, true);
      forgetLines(lastLine, lastLine, false);
    }
    replaceRange(range.start, range.end, QString());
    if (change) {
      QTextCursor c = m_editor->textCursor();
      c.setPosition(range.start);
      m_editor->setTextCursor(c);
    } else {
      setCursorPos(clampNormal(range.start));
    }
    return;
  }
  case VimRegisterType::Blockwise: {
    storeDeleted(reg, text, VimRegisterType::Blockwise, true);
    for (int l = range.endLine; l >= range.startLine; --l) {
      const QString lt = lineText(l);
      int from = colForVcol(lt, range.startVcol);
      int to = range.toEol ? lt.size() : colForVcol(lt, range.endVcol + 1);
      if (from < lt.size() && to > from)
        replaceRange(lineStart(l) + from, lineStart(l) + qMin(to, lt.size()),
                     QString());
    }
    int pos = lineStart(range.startLine) +
              colForVcol(lineText(range.startLine), range.startVcol);
    if (change) {
      QTextCursor c = m_editor->textCursor();
      c.setPosition(pos);
      m_editor->setTextCursor(c);
    } else {
      setCursorPos(clampNormal(pos));
    }
    return;
  }
  }
}

void VimMode::operatorYank(const Range &range, QChar reg, bool moveCursor) {
  const QString text = rangeText(range);
  storeYanked(reg, text, range.type);
  const int lines = range.endLine - range.startLine + 1;
  if (lines > 2 && range.type != VimRegisterType::Charwise)
    emit statusMessage(QString("%1 lines yanked").arg(lines));
  if (!moveCursor)
    return;
  const int target = m_opCursor >= 0 ? m_opCursor : range.start;
  switch (range.type) {
  case VimRegisterType::Charwise:
  case VimRegisterType::Linewise:
    setCursorPos(clampNormal(target));
    break;
  case VimRegisterType::Blockwise:
    setCursorPos(
        clampNormal(lineStart(range.startLine) +
                    colForVcol(lineText(range.startLine), range.startVcol)));
    break;
  }
}

void VimMode::operatorCase(const QString &op, const Range &range) {
  if (range.type == VimRegisterType::Blockwise) {
    for (int l = range.startLine; l <= range.endLine; ++l) {
      const QString text = lineText(l);
      int from = colForVcol(text, range.startVcol);
      int to = range.toEol ? text.size() : colForVcol(text, range.endVcol + 1);
      if (to <= from)
        continue;
      const QString part = text.mid(from, to - from);
      const QString changed = transformCase(op, part);
      if (changed != part)
        replaceRange(lineStart(l) + from, lineStart(l) + to, changed);
    }
    setCursorPos(
        clampNormal(lineStart(range.startLine) +
                    colForVcol(lineText(range.startLine), range.startVcol)));
    return;
  }
  int start = range.start;
  int end = range.end;
  if (range.type == VimRegisterType::Linewise) {
    start = lineStart(range.startLine);
    end = lineEndPos(range.endLine);
  }
  const QString text = textBetween(start, end);
  const QString changed = transformCase(op, text);
  if (changed != text)
    replaceRange(start, end, changed);
  setCursorPos(clampNormal(m_opCursor >= 0 ? m_opCursor : start));
}

void VimMode::operatorShift(const Range &range, bool right, int amount) {
  for (int l = range.startLine; l <= range.endLine; ++l) {
    const QString text = lineText(l);
    if (text.isEmpty())
      continue;
    int fnb = firstNonBlankCol(l);
    int width = indentWidth(text);
    int newWidth = right ? width + m_shiftWidth * amount
                         : qMax(0, width - m_shiftWidth * amount);
    const QString indent = indentString(newWidth);
    if (indent != text.left(fnb))
      replaceRange(lineStart(l), lineStart(l) + fnb, indent);
  }
  const int lines = range.endLine - range.startLine + 1;
  if (lines > 2)
    emit statusMessage(QString("%1 lines %2ed %3 time%4")
                           .arg(lines)
                           .arg(right ? ">" : "<")
                           .arg(amount)
                           .arg(amount == 1 ? "" : "s"));
  setCursorPos(firstNonBlankPos(range.startLine));
}

void VimMode::operatorReindent(const Range &range) {
  int prevIndent = 0;
  QString prevText;
  for (int l = range.startLine - 1; l >= 0; --l) {
    if (!lineText(l).trimmed().isEmpty()) {
      prevText = lineText(l);
      prevIndent = indentWidth(prevText);
      break;
    }
  }
  for (int l = range.startLine; l <= range.endLine; ++l) {
    const QString text = lineText(l);
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
      if (!text.isEmpty())
        replaceRange(lineStart(l), lineEndPos(l), QString());
      continue;
    }
    int width = prevIndent;
    const QString prevTrimmed = prevText.trimmed();
    if (!prevTrimmed.isEmpty()) {
      QChar lastChar = prevTrimmed[prevTrimmed.size() - 1];
      if (lastChar == '{' || lastChar == '(' || lastChar == '[')
        width += m_shiftWidth;
    }
    QChar firstChar = trimmed[0];
    if (firstChar == '}' || firstChar == ')' || firstChar == ']')
      width = qMax(0, width - m_shiftWidth);
    const QString indent = indentString(width);
    int fnb = firstNonBlankCol(l);
    if (text.left(fnb) != indent)
      replaceRange(lineStart(l), lineStart(l) + fnb, indent);
    prevText = lineText(l);
    prevIndent = width;
  }
  setCursorPos(firstNonBlankPos(range.startLine));
}

void VimMode::operatorFormat(const Range &range, bool keepCursor) {
  const int savedPos = cursorPos();
  const int savedLine = lineOf(savedPos);
  int savedNonBlank = -1;
  if (keepCursor && savedLine >= range.startLine &&
      savedLine <= range.endLine) {
    savedNonBlank = 0;
    for (int p = lineStart(range.startLine); p < savedPos; ++p) {
      QChar c = charAt(p);
      if (!c.isSpace())
        ++savedNonBlank;
    }
  }
  const int width = m_textWidth > 0 ? m_textWidth : 79;
  int l = range.startLine;
  int lastLine = range.endLine;
  int lastFormatted = range.endLine;
  while (l <= lastLine) {
    if (lineText(l).trimmed().isEmpty()) {
      ++l;
      continue;
    }
    int pEnd = l;
    while (pEnd + 1 <= lastLine && !lineText(pEnd + 1).trimmed().isEmpty())
      ++pEnd;
    const QString first = lineText(l);
    const int indentLen = firstNonBlankCol(l);
    const QString indent = first.left(indentLen);
    const QString nextIndent = m_autoIndent ? indent : QString();
    QString joined = first;
    for (int k = l + 1; k <= pEnd; ++k) {
      const QString next = lineText(k);
      int lead = 0;
      while (lead < next.size() && (next[lead] == ' ' || next[lead] == '\t'))
        ++lead;
      if (lead >= next.size())
        continue;
      if (!(joined.endsWith(' ') || joined.endsWith('\t')) && next[lead] != ')')
        joined += ' ';
      joined += next.mid(lead);
    }
    auto blank = [](QChar c) { return c == ' ' || c == '\t'; };
    QStringList out;
    QString current = joined;
    int currentIndent = indentLen;
    while (vcolOf(current, current.size()) > width) {
      int breakAt = -1;
      for (int i = current.size() - 1; i > currentIndent; --i) {
        if (blank(current[i]) && vcolOf(current, i) <= width) {
          breakAt = i;
          break;
        }
      }
      if (breakAt < 0) {
        for (int i = currentIndent; i < current.size(); ++i) {
          if (blank(current[i]) && i > currentIndent) {
            breakAt = i;
            break;
          }
        }
      }
      if (breakAt < 0)
        break;
      int s = breakAt;
      while (s > currentIndent && blank(current[s - 1]))
        --s;
      int e = breakAt;
      while (e < current.size() && blank(current[e]))
        ++e;
      if (s <= currentIndent || e >= current.size())
        break;
      out << current.left(s);
      current = nextIndent + current.mid(e);
      currentIndent = nextIndent.size();
    }
    out << current;
    const QString replacement = out.join('\n');
    if (replacement != textBetween(lineStart(l), lineEndPos(pEnd)))
      replaceRange(lineStart(l), lineEndPos(pEnd), replacement);
    const int delta = out.size() - (pEnd - l + 1);
    lastLine += delta;
    l = l + out.size();
  }
  if (keepCursor && savedNonBlank >= 0) {
    int p = lineStart(range.startLine);
    int seen = 0;
    const int end = docLength();
    while (p < end && seen < savedNonBlank) {
      if (!charAt(p).isSpace())
        ++seen;
      ++p;
    }
    setCursorPos(clampNormal(p));
  } else if (keepCursor)
    setCursorPos(clampNormal(savedPos));
  else
    setCursorPos(firstNonBlankPos(qMin(lastLine, lineCount() - 1)));
}

void VimMode::put(QChar reg, int count, bool after, bool moveAfter,
                  bool adjustIndent) {
  VimRegister r = getRegister(reg);
  if (r.content.isEmpty()) {
    emit statusMessage(QString("E353: Nothing in register %1")
                           .arg(reg.isNull() ? QChar('"') : reg));
    return;
  }
  const int c1 = qMax(1, count);
  if (exceedsRepeatLimit(r.content.size() + 1, c1))
    return;
  QTextCursor block(doc());
  block.beginEditBlock();
  if (r.blockwise) {
    putBlock(r, c1, after, moveAfter);
    block.endEditBlock();
    recordChangePosition(cursorPos());
    return;
  }
  const int pos = cursorPos();
  const int line = lineOf(pos);
  if (r.linewise) {
    QString content = r.content;
    if (!content.endsWith('\n'))
      content += '\n';
    if (adjustIndent) {
      QStringList lines = content.split('\n');
      int base = -1;
      for (const QString &l : lines) {
        if (!l.trimmed().isEmpty()) {
          base = indentWidth(l);
          break;
        }
      }
      int target = indentWidth(lineText(line));
      if (base >= 0 && base != target) {
        for (QString &l : lines) {
          if (l.trimmed().isEmpty())
            continue;
          int w = indentWidth(l) - base + target;
          int fnb = 0;
          while (fnb < l.size() && (l[fnb] == ' ' || l[fnb] == '\t'))
            ++fnb;
          l = indentString(qMax(0, w)) + l.mid(fnb);
        }
        content = lines.join('\n');
      }
    }
    const QString text = content.repeated(c1);
    const int insertedLines = text.count('\n');
    int firstLine;
    if (after) {
      if (line == lineCount() - 1) {
        replaceRange(lineEndPos(line), lineEndPos(line),
                     "\n" + text.left(text.size() - 1));
      } else {
        replaceRange(lineStart(line + 1), lineStart(line + 1), text);
      }
      firstLine = line + 1;
    } else {
      replaceRange(lineStart(line), lineStart(line), text);
      firstLine = line;
    }
    const int lastLine = firstLine + insertedLines - 1;
    setMark('[', lineStart(firstLine));
    setMark(']', lineStart(lastLine));
    if (moveAfter) {
      int target = lastLine + 1;
      setCursorPos(target < lineCount() ? lineStart(target)
                                        : lineStart(lineCount() - 1));
    } else {
      setCursorPos(firstNonBlankPos(firstLine));
    }
    if (insertedLines > 2)
      emit statusMessage(QString("%1 more lines").arg(insertedLines));
  } else {
    const QString text = r.content.repeated(c1);
    int insertPos = pos;
    if (after && lineLength(line) > 0)
      insertPos = qMin(pos + 1, lineEndPos(line));
    replaceRange(insertPos, insertPos, text);
    setMark('[', insertPos);
    setMark(']', insertPos + text.size() - 1);
    int target;
    if (moveAfter)
      target = insertPos + text.size();
    else if (text.contains('\n'))
      target = insertPos;
    else
      target = insertPos + text.size() - 1;
    if (moveAfter) {
      QTextCursor c = m_editor->textCursor();
      c.setPosition(qBound(0, target, docLength()));
      m_editor->setTextCursor(c);
      setCursorPos(target, true);
    } else {
      setCursorPos(clampNormal(target));
    }
  }
  block.endEditBlock();
  recordChangePosition(cursorPos());
}

void VimMode::putBlock(const VimRegister &r, int count, bool after,
                       bool moveAfter) {
  const QStringList parts = r.content.split('\n');
  int width = 0;
  for (const QString &p : parts)
    width = qMax(width, vcolOf(p, p.size()));
  if (exceedsRepeatLimit(qint64(width + 1) * parts.size(), count))
    return;
  const int pos = cursorPos();
  const int line = lineOf(pos);
  const QString curText = lineText(line);
  int col = pos - lineStart(line);
  int vcol = vcolOf(curText, col);
  if (after && col < curText.size())
    vcol += curText[col] == '\t' ? m_tabStop - (vcol % m_tabStop) : 1;

  int endCol = 0;
  for (int i = 0; i < parts.size(); ++i) {
    int target = line + i;
    if (target >= lineCount())
      replaceRange(docLength(), docLength(), "\n");
    QString text = lineText(target);
    int lineVcol = vcolOf(text, text.size());
    QString piece;
    const bool atEnd = colForVcol(text, vcol) >= text.size();
    for (int k = 0; k < count; ++k) {
      piece += parts[i];
      if (k + 1 < count || !atEnd)
        piece += QString(width - vcolOf(parts[i], parts[i].size()), ' ');
    }
    int insertCol;
    if (lineVcol < vcol) {
      replaceRange(lineEndPos(target), lineEndPos(target),
                   QString(vcol - lineVcol, ' '));
      insertCol = lineLength(target);
    } else {
      insertCol = colForVcol(text, vcol);
    }
    replaceRange(lineStart(target) + insertCol, lineStart(target) + insertCol,
                 piece);
    if (i == parts.size() - 1)
      endCol = insertCol + piece.size();
  }
  if (moveAfter) {
    int target = line + parts.size() - 1;
    setCursorPos(clampNormal(lineStart(target) + endCol));
  } else {
    setCursorPos(
        clampNormal(lineStart(line) + colForVcol(lineText(line), vcol)));
  }
}

void VimMode::visualPut(QChar reg, int count, bool keepRegister) {
  VimRegister r = getRegister(reg);
  const int c1 = qMax(1, count);
  if (exceedsRepeatLimit(r.content.size() + 1, c1))
    return;
  const VimEditMode mode = m_mode;
  const Range range = visualRange();
  exitVisual(false);
  const QString deleted = rangeText(range);
  QTextCursor block(doc());
  block.beginEditBlock();
  if (mode == VimEditMode::VisualBlock) {
    operatorDelete(range, '_', false);
    QTextCursor c = m_editor->textCursor();
    c.setPosition(lineStart(range.startLine) +
                  colForVcol(lineText(range.startLine), range.startVcol));
    m_editor->setTextCursor(c);
    if (r.blockwise) {
      putBlock(r, c1, false, false);
    } else {
      const QString text = r.content.repeated(c1);
      replaceRange(c.position(), c.position(), text);
      setCursorPos(clampNormal(c.position() + text.size() - 1));
    }
  } else if (range.type == VimRegisterType::Linewise) {
    QString text = r.content;
    if (r.linewise && text.endsWith('\n'))
      text.chop(1);
    QStringList copies;
    for (int i = 0; i < c1; ++i)
      copies << text;
    replaceRange(lineStart(range.startLine), lineEndPos(range.endLine),
                 copies.join('\n'));
    setCursorPos(firstNonBlankPos(range.startLine));
  } else if (r.linewise) {
    QString text = r.content;
    if (text.endsWith('\n'))
      text.chop(1);
    QStringList copies;
    for (int i = 0; i < c1; ++i)
      copies << text;
    replaceRange(range.start, range.end, "\n" + copies.join('\n') + "\n");
    setCursorPos(firstNonBlankPos(range.startLine + 1));
  } else {
    const QString text = r.content.repeated(c1);
    replaceRange(range.start, range.end, text);
    int target = (text.contains('\n') || text.isEmpty())
                     ? range.start
                     : range.start + text.size() - 1;
    setCursorPos(clampNormal(target));
  }
  block.endEditBlock();
  if (!keepRegister)
    storeDeleted(QChar(), deleted, range.type, false);
  recordChangePosition(cursorPos());
}

void VimMode::joinLines(int line, int count, bool insertSpace) {
  const int last = lineCount() - 1;
  int joins = qMax(2, count) - 1;
  if (line >= last)
    return;
  if (joins > last - line)
    joins = last - line;
  forgetLines(line + 1, line + joins, false);
  QTextCursor block(doc());
  block.beginEditBlock();
  int cursorCol = 0;
  for (int k = 0; k < joins; ++k) {
    const QString cur = lineText(line);
    const QString next = lineText(line + 1);
    const int curLen = cur.size();
    if (!insertSpace) {
      replaceRange(lineEndPos(line), lineStart(line + 1), QString());
      cursorCol = curLen;
      continue;
    }
    int lead = 0;
    while (lead < next.size() && (next[lead] == ' ' || next[lead] == '\t'))
      ++lead;
    QString sep = " ";
    if (lead >= next.size())
      sep.clear();
    else if (curLen == 0)
      sep.clear();
    else if (cur.endsWith(' ') || cur.endsWith('\t'))
      sep.clear();
    else if (next[lead] == ')')
      sep.clear();
    else if (m_joinSpaces &&
             (cur.endsWith('.') || cur.endsWith('!') || cur.endsWith('?')))
      sep = "  ";
    replaceRange(lineEndPos(line), lineStart(line + 1) + lead, sep);
    cursorCol = (sep.isEmpty() && curLen > 0 && lead >= next.size())
                    ? curLen - 1
                    : curLen;
  }
  block.endEditBlock();
  setCursorPos(clampNormal(lineStart(line) + cursorCol));
  recordChangePosition(cursorPos());
}

void VimMode::replaceChars(QChar ch, int count) {
  const int pos = cursorPos();
  const int line = lineOf(pos);
  const int col = pos - lineStart(line);
  if (col + count > lineLength(line))
    return;
  QTextCursor block(doc());
  block.beginEditBlock();
  if (ch == '\n') {
    replaceRange(pos, pos + count, "\n");
    block.endEditBlock();
    setCursorPos(lineStart(line + 1));
  } else if (ch == '\t' && m_expandTab) {
    const QString spaces(m_tabStop - (vcolOf(lineText(line), col) % m_tabStop),
                         ' ');
    replaceRange(pos, pos + count, spaces);
    block.endEditBlock();
    setCursorPos(pos + spaces.size() - 1);
  } else {
    replaceRange(pos, pos + count, QString(count, ch));
    block.endEditBlock();
    setCursorPos(pos + count - 1);
  }
  recordChangePosition(cursorPos());
}

void VimMode::toggleCaseChars(int count) {
  const int pos = cursorPos();
  const int line = lineOf(pos);
  const int len = lineLength(line);
  const int col = pos - lineStart(line);
  if (len == 0)
    return;
  const int n = qMin(count, len - col);
  const QString text = textBetween(pos, pos + n);
  const QString changed = transformCase("g~", text);
  QTextCursor block(doc());
  block.beginEditBlock();
  if (changed != text)
    replaceRange(pos, pos + n, changed);
  block.endEditBlock();
  recordChangePosition(pos);
  setCursorPos(clampNormal(pos + n));
}

bool VimMode::exceedsRepeatLimit(qint64 size, qint64 count) {
  if (count <= 1 || size * count <= kMaxRepeatChars)
    return false;
  emit statusMessage("E1240: Resulting text too long");
  return true;
}

bool VimMode::incrementNumber(int line, int col, qint64 delta, int endCol,
                              int *resultPos) {
  QString text = lineText(line);
  if (endCol >= 0) {
    text = text.left(endCol);
    for (int i = 0; i < col && i < text.size(); ++i)
      text[i] = ' ';
  }
  static const QRegularExpression numberRe("0[xX][0-9a-fA-F]+|0[bB][01]+|\\d+");
  auto it = numberRe.globalMatch(text);
  while (it.hasNext()) {
    auto m = it.next();
    int start = m.capturedStart();
    int end = m.capturedEnd();
    if (end <= col)
      continue;

    QString number = m.captured();
    QString replacement;
    bool isHex = number.size() > 2 && (number[1] == 'x' || number[1] == 'X');
    bool isBin = number.size() > 2 && (number[1] == 'b' || number[1] == 'B');
    if (isHex && m_nrFormats.contains("hex")) {
      QString digits = number.mid(2);
      bool ok = false;
      quint64 value = digits.toULongLong(&ok, 16);
      if (!ok)
        return false;
      value += static_cast<quint64>(delta);
      bool upper = false;
      for (QChar c : digits) {
        if (c.isLetter()) {
          upper = c.isUpper();
          break;
        }
      }
      QString out = QString::number(value, 16);
      if (upper)
        out = out.toUpper();
      if (out.size() < digits.size())
        out = QString(digits.size() - out.size(), '0') + out;
      replacement = number.left(2) + out;
    } else if (isBin && m_nrFormats.contains("bin")) {
      QString digits = number.mid(2);
      bool ok = false;
      quint64 value = digits.toULongLong(&ok, 2);
      if (!ok)
        return false;
      value += static_cast<quint64>(delta);
      QString out = QString::number(value, 2);
      if (out.size() < digits.size())
        out = QString(digits.size() - out.size(), '0') + out;
      replacement = number.left(2) + out;
    } else {
      QString digits = number;
      if (isHex || isBin) {
        digits = number.left(1);
        end = start + 1;
      }
      bool negative = start > 0 && text[start - 1] == '-';
      if (negative)
        --start;
      bool ok = false;
      quint64 value = digits.toULongLong(&ok);
      const bool overflow = !ok;
      if (overflow)
        value = std::numeric_limits<quint64>::max();
      const quint64 magnitude =
          delta < 0 ? quint64(0) - quint64(delta) : quint64(delta);
      const bool subtract = (delta < 0) != negative;
      const quint64 old = value;
      if (!overflow)
        value = subtract ? value - magnitude : value + magnitude;
      if (subtract && value > old) {
        value = ~value + 1;
        negative = !negative;
      } else if (!subtract && value < old) {
        value = ~value;
        negative = !negative;
      }
      if (value == 0)
        negative = false;
      QString out = QString::number(value);
      if (digits.size() > 1 && digits.startsWith('0') &&
          out.size() < digits.size())
        out = QString(digits.size() - out.size(), '0') + out;
      replacement = (negative ? "-" : "") + out;
    }
    replaceRange(lineStart(line) + start, lineStart(line) + end, replacement);
    if (resultPos)
      *resultPos = lineStart(line) + start + replacement.size() - 1;
    return true;
  }
  return false;
}

void VimMode::undo(int count) {
  int minPos = -1;
  int minAdded = 0;
  auto conn = connect(doc(), &QTextDocument::contentsChange, this,
                      [&minPos, &minAdded](int pos, int, int added) {
                        if (minPos < 0 || pos < minPos) {
                          minPos = pos;
                          minAdded = added;
                        }
                      });
  for (int i = 0; i < count; ++i) {
    if (!doc()->isUndoAvailable()) {
      emit statusMessage("Already at oldest change");
      break;
    }
    m_editor->undo();
  }
  disconnect(conn);
  restoreDeletedMarks();
  int target = m_editor->textCursor().position();
  const int steps = doc()->availableUndoSteps();
  if (minPos >= 0 && minAdded > 0 && charAt(minPos) == '\n' &&
      minPos < docLength() && minPos > lineStart(lineOf(minPos)))
    ++minPos;
  if (minPos >= 0) {
    target = m_undoCursors.value(steps, minPos);
    if (target < -1) {
      const int recorded = -2 - target;
      const int line = lineOf(minPos);
      target = lineStart(line) + qMin(recorded, firstNonBlankCol(line));
    } else if (target < 0) {
      target = firstNonBlankPos(lineOf(minPos));
    }
  }
  QTextCursor c = m_editor->textCursor();
  c.clearSelection();
  c.setPosition(clampNormal(target));
  m_editor->setTextCursor(c);
  setCursorPos(c.position());
}

void VimMode::redo(int count) {
  int minPos = -1;
  auto conn = connect(doc(), &QTextDocument::contentsChange, this,
                      [&minPos](int pos, int, int) {
                        minPos = minPos < 0 ? pos : qMin(minPos, pos);
                      });
  for (int i = 0; i < count; ++i) {
    if (!doc()->isRedoAvailable()) {
      emit statusMessage("Already at newest change");
      break;
    }
    m_editor->redo();
  }
  disconnect(conn);
  int target = m_editor->textCursor().position();
  if (minPos >= 0) {
    target = minPos;
    if (target == lineStart(lineOf(target)))
      target = firstNonBlankPos(lineOf(target));
  }
  QTextCursor c = m_editor->textCursor();
  c.clearSelection();
  c.setPosition(clampNormal(target));
  m_editor->setTextCursor(c);
  setCursorPos(c.position());
}
