#include "vimmode.h"

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextDocument>

namespace {

bool isWordChar(QChar c) { return c.isLetterOrNumber() || c == '_'; }

bool isBlank(QChar c) { return c == ' ' || c == '\t'; }

} // namespace

const QString &VimMode::cachedLine(int line) const {
  if (m_lineCacheDoc != doc() || m_lineCacheLine != line ||
      m_lineCacheRevision != doc()->revision() ||
      m_lineCacheCount != lineCount()) {
    m_lineCacheDoc = doc();
    m_lineCacheLine = line;
    m_lineCacheRevision = doc()->revision();
    m_lineCacheCount = lineCount();
    m_lineCacheText = lineText(line);
  }
  return m_lineCacheText;
}

QString VimMode::textBetween(int start, int end) const {
  start = qBound(0, start, docLength());
  end = qBound(0, end, docLength());
  if (end <= start)
    return QString();
  QTextCursor c(doc());
  c.setPosition(start);
  c.setPosition(end, QTextCursor::KeepAnchor);
  QString text = c.selectedText();
  text.replace(QChar::ParagraphSeparator, '\n');
  text.replace(QChar::LineSeparator, '\n');
  return text;
}

bool VimMode::inIndent(int pos) const {
  int line = lineOf(pos);
  return firstNonBlankCol(line) >= pos - lineStart(line);
}

QChar VimMode::charAtTextPos(const TextPos &p) const {
  const QString &text = cachedLine(p.line);
  return p.col < text.size() ? text[p.col] : QChar();
}

int VimMode::charClass(const TextPos &p, bool bigword) const {
  QChar c = charAtTextPos(p);
  if (c.isNull() || isBlank(c))
    return 0;
  if (bigword)
    return 1;
  return isWordChar(c) ? 2 : 1;
}

int VimMode::incPos(TextPos &p) const {
  int len = cachedLine(p.line).size();
  if (p.col < len) {
    ++p.col;
    return p.col < len ? 0 : 2;
  }
  if (p.line < lineCount() - 1) {
    ++p.line;
    p.col = 0;
    return 1;
  }
  return -1;
}

int VimMode::decPos(TextPos &p) const {
  if (p.col > 0) {
    --p.col;
    return 0;
  }
  if (p.line > 0) {
    --p.line;
    p.col = cachedLine(p.line).size();
    return 1;
  }
  return -1;
}

bool VimMode::skipChars(TextPos &p, int cls, bool forward, bool bigword) const {
  while (charClass(p, bigword) == cls) {
    if ((forward ? incPos(p) : decPos(p)) == -1)
      return true;
  }
  return false;
}

bool VimMode::fwdWord(TextPos &p, int count, bool bigword, bool eol) const {
  while (--count >= 0) {
    int sclass = charClass(p, bigword);
    bool lastLine = p.line == lineCount() - 1;
    int i = incPos(p);
    if (i == -1 || (i >= 1 && lastLine))
      return false;
    if (i >= 1 && eol && count == 0)
      return true;
    if (sclass != 0) {
      while (charClass(p, bigword) == sclass) {
        i = incPos(p);
        if (i == -1 || (i >= 1 && eol && count == 0))
          return true;
      }
    }
    while (charClass(p, bigword) == 0) {
      if (p.col == 0 && cachedLine(p.line).isEmpty())
        break;
      i = incPos(p);
      if (i == -1 || (i >= 1 && eol && count == 0))
        return true;
    }
  }
  return true;
}

bool VimMode::endWord(TextPos &p, int count, bool bigword, bool stop,
                      bool empty) const {
  while (--count >= 0) {
    int sclass = charClass(p, bigword);
    if (incPos(p) == -1)
      return false;
    bool finished = false;
    if (charClass(p, bigword) == sclass && sclass != 0) {
      if (skipChars(p, sclass, true, bigword))
        return false;
    } else if (!stop || sclass == 0) {
      while (charClass(p, bigword) == 0) {
        if (p.col == 0 && cachedLine(p.line).isEmpty() && empty) {
          finished = true;
          break;
        }
        if (incPos(p) == -1)
          return false;
      }
      if (!finished && skipChars(p, charClass(p, bigword), true, bigword))
        return false;
    }
    if (!finished)
      decPos(p);
    stop = false;
  }
  return true;
}

bool VimMode::bckWord(TextPos &p, int count, bool bigword, bool stop) const {
  while (--count >= 0) {
    int sclass = charClass(p, bigword);
    if (decPos(p) == -1)
      return false;
    bool finished = false;
    if (!stop || sclass == charClass(p, bigword) || sclass == 0) {
      while (charClass(p, bigword) == 0) {
        if (p.col == 0 && cachedLine(p.line).isEmpty()) {
          finished = true;
          break;
        }
        if (decPos(p) == -1)
          return true;
      }
      if (!finished && skipChars(p, charClass(p, bigword), false, bigword))
        return true;
    }
    if (!finished)
      incPos(p);
    stop = false;
  }
  return true;
}

bool VimMode::bckendWord(TextPos &p, int count, bool bigword, bool eol) const {
  while (--count >= 0) {
    int sclass = charClass(p, bigword);
    int i = decPos(p);
    if (i == -1)
      return false;
    if (eol && i == 1)
      return true;
    if (sclass != 0) {
      while (charClass(p, bigword) == sclass) {
        i = decPos(p);
        if (i == -1 || (eol && i == 1))
          return true;
      }
    }
    while (charClass(p, bigword) == 0) {
      if (p.col == 0 && cachedLine(p.line).isEmpty())
        break;
      i = decPos(p);
      if (i == -1 || (eol && i == 1))
        return true;
    }
  }
  return true;
}

int VimMode::findParagraph(int line, int count, bool forward,
                           bool *inclusive) const {
  int dir = forward ? 1 : -1;
  int curr = line;
  int last = lineCount() - 1;
  *inclusive = false;
  while (count--) {
    bool didSkip = false;
    for (bool first = true;; first = false) {
      if (lineLength(curr) != 0)
        didSkip = true;
      if (!first && didSkip && lineLength(curr) == 0)
        break;
      curr += dir;
      if (curr < 0 || curr > last) {
        if (count)
          return -1;
        curr -= dir;
        break;
      }
    }
  }
  if (curr == last && forward) {
    int len = lineLength(curr);
    if (len != 0) {
      *inclusive = true;
      return lineStart(curr) + len - 1;
    }
  }
  return lineStart(curr);
}

QVector<int> VimMode::sentenceStarts() const {
  const QString text = doc()->toPlainText();
  const int n = text.size();
  QVector<int> starts;
  bool atStart = true;
  for (int i = 0; i < n; ++i) {
    QChar c = text[i];
    if (atStart) {
      if (c == '\n' && (i == 0 || text[i - 1] == '\n') &&
          (i < 2 || text[i - 2] != '\n')) {
        starts.append(i);
      } else if (!c.isSpace()) {
        starts.append(i);
        atStart = false;
      }
      continue;
    }
    if (c == '\n' && i + 1 < n && text[i + 1] == '\n') {
      atStart = true;
      continue;
    }
    if (c == '.' || c == '!' || c == '?') {
      int j = i + 1;
      while (j < n && QString(")]\"'").contains(text[j]))
        ++j;
      if (j >= n || text[j] == ' ' || text[j] == '\t' || text[j] == '\n') {
        atStart = true;
        i = j - 1;
      }
    }
  }
  return starts;
}

int VimMode::findSentence(int pos, int count, bool forward) const {
  const QVector<int> starts = sentenceStarts();
  const int n = docLength();
  int result = pos;
  for (int c = 0; c < count; ++c) {
    int next = -1;
    if (forward) {
      for (int s : starts) {
        if (s > result) {
          next = s;
          break;
        }
      }
      if (next < 0) {
        result = qMax(0, n - 1);
        break;
      }
    } else {
      for (int k = starts.size() - 1; k >= 0; --k) {
        if (starts[k] < result) {
          next = starts[k];
          break;
        }
      }
      if (next < 0) {
        result = 0;
        break;
      }
    }
    result = next;
  }
  return result;
}

bool VimMode::findMatchingBracket(int pos, int &result) const {
  const QString pairs = "(){}[]";
  int line = lineOf(pos);
  const QString text = lineText(line);
  int col = pos - lineStart(line);
  int found = -1;
  for (int i = col; i < text.size(); ++i) {
    if (pairs.contains(text[i])) {
      found = i;
      break;
    }
  }
  if (found < 0)
    return false;
  int start = lineStart(line) + found;
  QChar ch = text[found];
  int idx = pairs.indexOf(ch);
  bool forward = idx % 2 == 0;
  QChar match = pairs[forward ? idx + 1 : idx - 1];
  int depth = 0;
  int end = docLength();
  for (int p = start; p >= 0 && p < end; p += forward ? 1 : -1) {
    QChar c = charAt(p);
    if (c == ch) {
      ++depth;
    } else if (c == match) {
      if (--depth == 0) {
        result = p;
        return true;
      }
    }
  }
  return false;
}

bool VimMode::findUnmatched(int pos, QChar open, QChar close, bool forward,
                            int count, int &result) const {
  int p = pos;
  int end = docLength();
  for (int c = 0; c < count; ++c) {
    int depth = 0;
    bool found = false;
    for (p += forward ? 1 : -1; p >= 0 && p < end; p += forward ? 1 : -1) {
      QChar ch = charAt(p);
      if (ch == (forward ? open : close)) {
        ++depth;
      } else if (ch == (forward ? close : open)) {
        if (depth == 0) {
          found = true;
          break;
        }
        --depth;
      }
    }
    if (!found)
      return c > 0 ? (result = pos, false) : false;
    result = p;
  }
  return true;
}

bool VimMode::findCharInLine(int pos, const QString &cmd, QChar ch, int count,
                             bool repeat, int &result) const {
  int line = lineOf(pos);
  const QString text = lineText(line);
  int col = pos - lineStart(line);
  bool forward = cmd == "f" || cmd == "t";
  bool till = cmd == "t" || cmd == "T";
  if (till && repeat && count == 1) {
    if (forward && col + 1 < text.size() && text[col + 1] == ch)
      ++col;
    else if (!forward && col - 1 >= 0 && text[col - 1] == ch)
      --col;
  }
  for (int i = 0; i < count; ++i) {
    if (forward) {
      col = text.indexOf(ch, col + 1);
    } else {
      col = col - 1 >= 0 ? text.lastIndexOf(ch, col - 1) : -1;
    }
    if (col < 0)
      return false;
  }
  if (till)
    col += forward ? -1 : 1;
  result = lineStart(line) + col;
  return true;
}

VimMode::MotionResult VimMode::evalMotion(const QString &key, QChar arg,
                                          int count, bool hasOperator,
                                          int fromPos, bool visual) {
  MotionResult r;
  const int c1 = qMax(1, count);
  const int line = lineOf(fromPos);
  const int start = lineStart(line);
  const int col = fromPos - start;
  const int len = lineLength(line);
  const int last = lineCount() - 1;
  r.pos = fromPos;
  r.ok = true;

  auto toLine = [&](int target, bool firstNonBlank) {
    target = qBound(0, target, last);
    r.type = MotionType::Linewise;
    if (firstNonBlank) {
      r.pos = lineStart(target) + firstNonBlankCol(target);
    } else {
      r.pos = posForWantCol(target);
      r.keepWantCol = true;
    }
  };

  if (key == "h" || key == "<Left>" || key == "<C-h>") {
    if (col == 0 && !hasOperator)
      r.ok = false;
    r.pos = start + qMax(0, col - c1);
  } else if (key == "<BS>") {
    TextPos p{line, col};
    for (int i = 0; i < c1; ++i) {
      if (p.col > 0) {
        --p.col;
      } else if (p.line > 0) {
        --p.line;
        p.col = qMax(0, lineLength(p.line) - 1);
      } else {
        if (i == 0)
          r.ok = false;
        break;
      }
    }
    r.pos = posOf(p.line, p.col);
  } else if (key == "l" || key == "<Right>") {
    int maxCol = (hasOperator || visual) ? len : qMax(0, len - 1);
    if (col >= maxCol && !hasOperator)
      r.ok = false;
    r.pos = start + qMin(col + c1, maxCol);
  } else if (key == " ") {
    TextPos p{line, col};
    for (int i = 0; i < c1; ++i) {
      int l = lineLength(p.line);
      if (p.col < l - 1) {
        ++p.col;
      } else if (p.line < last) {
        ++p.line;
        p.col = 0;
      } else {
        if (hasOperator)
          p.col = l;
        else if (i == 0)
          r.ok = false;
        break;
      }
    }
    r.pos = lineStart(p.line) + p.col;
  } else if (key == "j" || key == "<Down>" || key == "<C-j>" ||
             key == "<C-n>" || key == "gj") {
    if (line >= last)
      r.ok = false;
    toLine(line + c1, false);
  } else if (key == "k" || key == "<Up>" || key == "<C-p>" || key == "gk") {
    if (line <= 0)
      r.ok = false;
    toLine(line - c1, false);
  } else if (key == "+" || key == "<CR>" || key == "<C-m>") {
    if (line >= last)
      r.ok = false;
    toLine(line + c1, true);
  } else if (key == "-") {
    if (line <= 0)
      r.ok = false;
    toLine(line - c1, true);
  } else if (key == "_") {
    toLine(line + c1 - 1, true);
  } else if (key == "0" || key == "<Home>" || key == "g0") {
    r.pos = start;
  } else if (key == "^" || key == "g^") {
    r.pos = start + firstNonBlankCol(line);
  } else if (key == "$" || key == "<End>" || key == "g$") {
    int target = line + c1 - 1;
    if (target > last) {
      r.ok = false;
      target = last;
    }
    int l = lineLength(target);
    r.wantEol = true;
    r.type = MotionType::Inclusive;
    if (visual) {
      r.pos = lineStart(target) + l;
    } else if (l == 0) {
      r.pos = lineStart(target);
    } else {
      r.pos = lineStart(target) + l - 1;
    }
  } else if (key == "g_") {
    int target = qMin(last, line + c1 - 1);
    const QString text = lineText(target);
    int i = text.size() - 1;
    while (i > 0 && isBlank(text[i]))
      --i;
    r.pos = lineStart(target) + qMax(0, i);
    r.type = MotionType::Inclusive;
  } else if (key == "gm") {
    r.pos = start + qMin(len / 2, qMax(0, len - 1));
  } else if (key == "|") {
    const QString text = lineText(line);
    r.pos = start + qMin(colForVcol(text, c1 - 1), qMax(0, len - 1));
    r.keepWantCol = true;
    m_wantCol = c1 - 1;
    m_wantEol = false;
  } else if (key == "go") {
    r.pos = qBound(0, c1 - 1, qMax(0, docLength() - 1));
    r.jump = true;
  } else if (key == "w" || key == "W" || key == "<S-Right>" ||
             key == "<C-Right>") {
    TextPos p{line, col};
    fwdWord(p, c1, key == "W" || key == "<C-Right>", hasOperator);
    r.pos = lineStart(p.line) + p.col;
  } else if (key == "b" || key == "B" || key == "<S-Left>" ||
             key == "<C-Left>") {
    TextPos p{line, col};
    if (!bckWord(p, c1, key == "B" || key == "<C-Left>", false) && fromPos == 0)
      r.ok = false;
    r.pos = lineStart(p.line) + p.col;
  } else if (key == "e" || key == "E") {
    TextPos p{line, col};
    endWord(p, c1, key == "E", false, false);
    r.pos = lineStart(p.line) + qMin(p.col, lineLength(p.line));
    r.type = MotionType::Inclusive;
  } else if (key == "ge" || key == "gE") {
    TextPos p{line, col};
    if (!bckendWord(p, c1, key == "gE", false))
      r.ok = false;
    r.pos = lineStart(p.line) + p.col;
    r.type = MotionType::Inclusive;
  } else if (key == "G" || key == "gg" || key == "<C-End>" ||
             key == "<C-Home>") {
    int target =
        count > 0 ? count - 1 : ((key == "G" || key == "<C-End>") ? last : 0);
    toLine(target, true);
    r.jump = true;
  } else if (key == "%") {
    r.jump = true;
    if (count > 0) {
      if (count > 100) {
        r.ok = false;
      } else {
        toLine((count * lineCount() + 99) / 100 - 1, true);
      }
    } else {
      int result = 0;
      r.ok = findMatchingBracket(fromPos, result);
      r.pos = r.ok ? result : fromPos;
      r.type = MotionType::Inclusive;
    }
  } else if (key == "}" || key == "{") {
    bool inclusive = false;
    int result = findParagraph(line, c1, key == "}", &inclusive);
    r.jump = true;
    if (result < 0) {
      r.ok = false;
    } else {
      r.pos = result;
      if (inclusive)
        r.type = MotionType::Inclusive;
    }
  } else if (key == ")" || key == "(") {
    r.pos = findSentence(fromPos, c1, key == ")");
    r.jump = true;
    if (key == ")" && r.pos >= docLength() - 1 && docLength() > 0 &&
        charAt(r.pos) != '\n')
      r.type = MotionType::Inclusive;
  } else if (key == "f" || key == "F" || key == "t" || key == "T" ||
             key == ";" || key == ",") {
    QString cmd = key;
    QChar ch = arg;
    bool repeat = false;
    if (key == ";" || key == ",") {
      if (m_lastFindCmd.isEmpty()) {
        r.ok = false;
        return r;
      }
      repeat = true;
      ch = m_lastFindChar;
      cmd = m_lastFindCmd;
      if (key == ",") {
        static const QMap<QString, QString> reverse = {
            {"f", "F"}, {"F", "f"}, {"t", "T"}, {"T", "t"}};
        cmd = reverse.value(cmd);
      }
    } else {
      m_lastFindCmd = key;
      m_lastFindChar = arg;
    }
    int result = 0;
    r.ok = findCharInLine(fromPos, cmd, ch, c1, repeat, result);
    r.pos = r.ok ? result : fromPos;
    r.type = (cmd == "f" || cmd == "t") ? MotionType::Inclusive
                                        : MotionType::Exclusive;
  } else if (key == "H" || key == "L" || key == "M") {
    int first = firstVisibleLine();
    int lastVisible = qMin(last, first + visibleLineCount() - 1);
    int target = first;
    if (key == "H")
      target = qMin(lastVisible, first + c1 - 1);
    else if (key == "L")
      target = qMax(first, lastVisible - (c1 - 1));
    else
      target = first + (lastVisible - first) / 2;
    toLine(target, true);
    r.jump = true;
  } else if (key == "n" || key == "N") {
    return searchMotion(key == "N", c1, fromPos);
  } else if (key == "*" || key == "#" || key == "g*" || key == "g#") {
    bool forward = key == "*" || key == "g*";
    int searchFrom = fromPos;
    if (!prepareWordSearch(forward, key.size() == 1, fromPos, searchFrom)) {
      r.ok = false;
      return r;
    }
    return searchMotion(false, c1, searchFrom);
  } else if (key == "'" || key == "`" || key == "g'" || key == "g`") {
    int pos = 0;
    if (!markPosition(arg, pos)) {
      emit statusMessage("E20: Mark not set");
      r.ok = false;
      return r;
    }
    r.jump = true;
    if (key.endsWith("'")) {
      toLine(lineOf(pos), true);
    } else {
      r.pos = pos;
    }
  } else if (key == "[(" || key == "[{" || key == "])" || key == "]}") {
    QChar open = key[1] == '(' || key[1] == ')' ? '(' : '{';
    QChar close = open == '(' ? ')' : '}';
    int result = 0;
    r.ok = findUnmatched(fromPos, open, close, key[0] == ']', c1, result);
    r.pos = r.ok ? result : fromPos;
    r.jump = true;
    if (key[0] == ']' && hasOperator)
      r.type = MotionType::Exclusive;
  } else if (key == "[[" || key == "]]" || key == "[]" || key == "][") {
    bool forward = key[0] == ']';
    QChar want = (key == "[[" || key == "]]") ? '{' : '}';
    int l = line;
    for (int i = 0; i < c1; ++i) {
      do {
        l += forward ? 1 : -1;
      } while (l >= 0 && l <= last && !lineText(l).startsWith(want));
      if (l < 0 || l > last) {
        l = qBound(0, l, last);
        break;
      }
    }
    r.pos = lineStart(l);
    r.jump = true;
  } else {
    r.ok = false;
  }
  return r;
}

VimMode::Range VimMode::lineRange(int firstLine, int lastLine) const {
  Range r;
  r.type = VimRegisterType::Linewise;
  r.startLine = qBound(0, qMin(firstLine, lastLine), lineCount() - 1);
  r.endLine = qBound(0, qMax(firstLine, lastLine), lineCount() - 1);
  r.start = lineStart(r.startLine);
  r.end = lineEndPos(r.endLine);
  return r;
}

VimMode::Range VimMode::charRange(int start, int endExclusive) const {
  Range r;
  r.type = VimRegisterType::Charwise;
  r.start = qBound(0, qMin(start, endExclusive), docLength());
  r.end = qBound(0, qMax(start, endExclusive), docLength());
  r.startLine = lineOf(r.start);
  r.endLine = lineOf(r.end);
  return r;
}

VimMode::Range VimMode::blockRange(int anchorPos, int cursorPos,
                                   bool toEol) const {
  Range r;
  r.type = VimRegisterType::Blockwise;
  int la = lineOf(anchorPos), lc = lineOf(cursorPos);
  r.startLine = qMin(la, lc);
  r.endLine = qMax(la, lc);
  auto vcolSpan = [this](int pos, int &left, int &right) {
    int line = lineOf(pos);
    const QString text = lineText(line);
    int col = pos - lineStart(line);
    left = vcolOf(text, col);
    int width = (col < text.size() && text[col] == '\t')
                    ? m_tabStop - (left % m_tabStop)
                    : 1;
    right = left + width - 1;
  };
  int al, ar, cl, cr;
  vcolSpan(anchorPos, al, ar);
  vcolSpan(cursorPos, cl, cr);
  r.startVcol = qMin(al, cl);
  r.endVcol = qMax(ar, cr);
  r.toEol = toEol;
  r.start = lineStart(r.startLine);
  r.end = lineEndPos(r.endLine);
  return r;
}

VimMode::Range VimMode::visualRange() const {
  int s = qMin(m_visualAnchor, m_visualPos);
  int e = qMax(m_visualAnchor, m_visualPos);
  switch (m_mode) {
  case VimEditMode::VisualLine:
    return lineRange(lineOf(s), lineOf(e));
  case VimEditMode::VisualBlock:
    return blockRange(m_visualAnchor, m_visualPos, m_visualToEol);
  default: {
    Range r = charRange(s, qMin(e + 1, docLength()));
    r.endLine = lineOf(e);
    return r;
  }
  }
}

VimMode::Range VimMode::motionRange(int a, int b, MotionType type) const {
  int s = qMin(a, b);
  int e = qMax(a, b);
  if (type == MotionType::Linewise)
    return lineRange(lineOf(s), lineOf(e));
  if (type == MotionType::Inclusive) {
    if (e < docLength() && charAt(e) != '\n')
      ++e;
    return charRange(s, e);
  }
  int sl = lineOf(s), el = lineOf(e);
  if (el > sl && e == lineStart(el)) {
    if (inIndent(s))
      return lineRange(sl, el - 1);
    e = lineEndPos(el - 1);
  }
  return charRange(s, e);
}

QString VimMode::rangeText(const Range &range) const {
  switch (range.type) {
  case VimRegisterType::Linewise:
    return textBetween(lineStart(range.startLine), lineEndPos(range.endLine)) +
           "\n";
  case VimRegisterType::Blockwise: {
    QStringList parts;
    for (int l = range.startLine; l <= range.endLine; ++l) {
      const QString text = lineText(l);
      int from = colForVcol(text, range.startVcol);
      int to = range.toEol ? text.size() : colForVcol(text, range.endVcol + 1);
      QString part = text.mid(from, qMax(0, to - from));
      if (!range.toEol && vcolOf(text, text.size()) < range.startVcol)
        part = QString(range.endVcol - range.startVcol + 1, ' ');
      parts << part;
    }
    return parts.join('\n');
  }
  default:
    return textBetween(range.start, range.end);
  }
}

bool VimMode::evalTextObject(const QString &obj, int count, int &start,
                             int &end, MotionType &type, bool visual) {
  if (obj == "gn" || obj == "gN") {
    type = MotionType::Inclusive;
    return selectSearchMatch(obj == "gn", start, end);
  }
  if (obj.size() != 2)
    return false;
  bool around = obj[0] == 'a';
  QChar c = obj[1];
  int c1 = qMax(1, count);
  switch (c.unicode()) {
  case 'w':
    return selectWordObject(c1, around, false, start, end, type, visual);
  case 'W':
    return selectWordObject(c1, around, true, start, end, type, visual);
  case '(':
  case ')':
  case 'b':
    return selectBracketObject('(', ')', c1, around, start, end, type, visual);
  case '{':
  case '}':
  case 'B':
    return selectBracketObject('{', '}', c1, around, start, end, type, visual);
  case '[':
  case ']':
    return selectBracketObject('[', ']', c1, around, start, end, type, visual);
  case '<':
  case '>':
    return selectBracketObject('<', '>', c1, around, start, end, type, visual);
  case '"':
  case '\'':
  case '`':
    return selectQuoteObject(c, around, start, end, type);
  case 'p':
    return selectParagraphObject(c1, around, start, end, type, visual);
  case 's':
    return selectSentenceObject(c1, around, start, end, type);
  case 't':
    return selectTagObject(c1, around, start, end, type);
  default:
    return false;
  }
}

bool VimMode::selectWordObject(int count, bool include, bool bigword,
                               int &start, int &end, MotionType &type,
                               bool visual) {
  int pos = cursorPos();
  TextPos p{lineOf(pos), colOf(pos)};
  bool inclusive = true;
  bool includeWhite = false;
  TextPos startPos;

  auto backInLine = [&](TextPos &q) {
    int sclass = charClass(q, bigword);
    while (q.col > 0) {
      decPos(q);
      if (charClass(q, bigword) != sclass) {
        incPos(q);
        break;
      }
    }
  };
  auto oneLeft = [&](TextPos &q) {
    if (q.col == 0)
      return false;
    --q.col;
    return true;
  };
  auto decl = [&](TextPos &q) {
    int r = decPos(q);
    if (r == 1 && q.col)
      r = decPos(q);
    return r;
  };
  auto incl = [&](TextPos &q) {
    int r = incPos(q);
    if (r >= 1 && q.col)
      r = incPos(q);
    return r;
  };

  bool extending =
      visual && m_visualAnchor != m_visualPos && m_mode == VimEditMode::Visual;
  if (extending) {
    startPos = TextPos{lineOf(m_visualAnchor), colOf(m_visualAnchor)};
  } else {
    backInLine(p);
    startPos = p;
    if ((charClass(p, bigword) == 0) == include) {
      if (!endWord(p, 1, bigword, true, true))
        return false;
    } else {
      fwdWord(p, 1, bigword, true);
      if (p.col == 0)
        decl(p);
      else
        oneLeft(p);
      if (include)
        includeWhite = true;
    }
    --count;
  }

  while (count > 0) {
    inclusive = true;
    if (incl(p) == -1)
      return false;
    if (include != (charClass(p, bigword) == 0)) {
      if (!fwdWord(p, 1, bigword, true) && count > 1)
        return false;
      if (!oneLeft(p))
        inclusive = false;
    } else {
      if (!endWord(p, 1, bigword, true, true))
        return false;
    }
    --count;
  }

  if (includeWhite &&
      (charClass(p, bigword) != 0 || (p.col == 0 && !inclusive))) {
    TextPos q = startPos;
    if (oneLeft(q)) {
      backInLine(q);
      if (charClass(q, bigword) == 0 && q.col > 0)
        startPos = q;
    }
  }

  start = lineStart(startPos.line) + startPos.col;
  end = lineStart(p.line) + p.col;
  type = inclusive ? MotionType::Inclusive : MotionType::Exclusive;
  return true;
}

bool VimMode::selectBracketObject(QChar open, QChar close, int count,
                                  bool around, int &start, int &end,
                                  MotionType &type, bool visual) {
  int pos = cursorPos();
  if (visual && m_visualAnchor != m_visualPos)
    pos = qMin(m_visualAnchor, m_visualPos);
  if (charAt(pos) == open)
    ++pos;

  int openPos = -1;
  int searchFrom = pos;
  bool searchedForward = false;
  for (int i = 0; i < count; ++i) {
    int depth = 0;
    int found = -1;
    for (int p = searchFrom - 1; p >= 0; --p) {
      QChar ch = charAt(p);
      if (ch == close) {
        ++depth;
      } else if (ch == open) {
        if (depth == 0) {
          found = p;
          break;
        }
        --depth;
      }
    }
    if (found < 0 && openPos < 0 && !searchedForward && count == 1) {
      searchedForward = true;
      int next = -1;
      for (int p = pos; p < docLength(); ++p) {
        if (charAt(p) == open) {
          next = p;
          break;
        }
      }
      if (next >= 0) {
        found = next;
        count = 1;
      }
    }
    if (found < 0)
      return false;
    openPos = found;
    searchFrom = found;
  }

  int closePos = -1;
  int depth = 0;
  for (int p = openPos + 1; p < docLength(); ++p) {
    QChar ch = charAt(p);
    if (ch == open) {
      ++depth;
    } else if (ch == close) {
      if (depth == 0) {
        closePos = p;
        break;
      }
      --depth;
    }
  }
  if (closePos < 0)
    return false;

  if (around) {
    start = openPos;
    end = closePos;
    type = MotionType::Inclusive;
    if (visual && m_visualAnchor != m_visualPos &&
        qMin(m_visualAnchor, m_visualPos) == start &&
        qMax(m_visualAnchor, m_visualPos) == end) {
      const int savedAnchor = m_visualAnchor, savedPos = m_visualPos;
      m_visualAnchor = m_visualPos = openPos;
      if (!selectBracketObject(open, close, 1, around, start, end, type,
                               false)) {
        m_visualAnchor = savedAnchor;
        m_visualPos = savedPos;
        return false;
      }
    }
    return true;
  }

  TextPos s{lineOf(openPos), colOf(openPos)};
  TextPos e{lineOf(closePos), colOf(closePos)};
  auto incl = [&](TextPos &q) {
    int r = incPos(q);
    if (r >= 1 && q.col)
      r = incPos(q);
    return r;
  };
  auto decl = [&](TextPos &q) {
    int r = decPos(q);
    if (r == 1 && q.col)
      r = decPos(q);
    return r;
  };
  incl(s);
  bool sol = e.col == 0;
  decl(e);
  while (firstNonBlankCol(e.line) >= e.col + 1 && e.col < lineLength(e.line)) {
    sol = true;
    if (decl(e) != 0)
      break;
  }
  int sPos = lineStart(s.line) + s.col;
  int ePos = lineStart(e.line) + e.col;
  start = sPos;
  if (sol) {
    incl(e);
    end = lineStart(e.line) + e.col;
    type = MotionType::Exclusive;
  } else if (sPos <= ePos) {
    end = ePos;
    type = MotionType::Inclusive;
  } else {
    end = sPos;
    type = MotionType::Exclusive;
  }
  if (visual && m_visualAnchor != m_visualPos &&
      qMin(m_visualAnchor, m_visualPos) == start &&
      qMax(m_visualAnchor, m_visualPos) ==
          (type == MotionType::Inclusive ? end : end - 1) &&
      count == 1) {
    const int savedAnchor = m_visualAnchor, savedPos = m_visualPos;
    m_visualAnchor = m_visualPos = openPos;
    if (!selectBracketObject(open, close, 2, false, start, end, type, false)) {
      m_visualAnchor = savedAnchor;
      m_visualPos = savedPos;
      return false;
    }
  }
  return true;
}

bool VimMode::selectQuoteObject(QChar quote, bool include, int &start, int &end,
                                MotionType &type) {
  int pos = cursorPos();
  int line = lineOf(pos);
  const QString text = lineText(line);
  int col = pos - lineStart(line);
  if (col >= text.size())
    return false;

  auto findNext = [&](int from, bool escape) {
    for (int i = from; i < text.size(); ++i) {
      if (escape && text[i] == '\\') {
        ++i;
        continue;
      }
      if (text[i] == quote)
        return i;
    }
    return -1;
  };
  auto findPrev = [&](int from) {
    int i = from;
    while (i > 0) {
      --i;
      int n = 0;
      while (i - n > 0 && text[i - n - 1] == '\\')
        ++n;
      if (n & 1)
        i -= n;
      else if (text[i] == quote)
        break;
    }
    return i;
  };

  int colStart = -1, colEnd = -1;
  if (text[col] == quote) {
    int s = 0;
    for (;;) {
      s = findNext(s, false);
      if (s < 0 || s > col)
        return false;
      int e = findNext(s + 1, true);
      if (e < 0)
        return false;
      if (s <= col && col <= e) {
        colStart = s;
        colEnd = e;
        break;
      }
      s = e + 1;
    }
  } else {
    colStart = findPrev(col);
    if (colStart < 0 || text[colStart] != quote) {
      colStart = findNext(col, false);
      if (colStart < 0)
        return false;
    }
    colEnd = findNext(colStart + 1, true);
    if (colEnd < 0)
      return false;
  }

  if (include) {
    if (colEnd + 1 < text.size() && isBlank(text[colEnd + 1])) {
      while (colEnd + 1 < text.size() && isBlank(text[colEnd + 1]))
        ++colEnd;
    } else {
      while (colStart > 0 && isBlank(text[colStart - 1]))
        --colStart;
    }
    start = lineStart(line) + colStart;
    end = lineStart(line) + colEnd;
    type = MotionType::Inclusive;
  } else {
    start = lineStart(line) + colStart + 1;
    end = lineStart(line) + colEnd;
    type = MotionType::Exclusive;
  }
  return true;
}

bool VimMode::selectParagraphObject(int count, bool include, int &start,
                                    int &end, MotionType &type, bool visual) {
  auto lineWhite = [this](int l) { return lineText(l).trimmed().isEmpty(); };
  const int last = lineCount() - 1;
  auto extendVisual = [&]() {
    int endLine = lineOf(m_visualPos);
    for (int c = 0; c < count; ++c) {
      if (endLine >= last)
        return c > 0;
      const bool white = lineWhite(endLine + 1);
      ++endLine;
      while (endLine < last && lineWhite(endLine + 1) == white)
        ++endLine;
      if (include) {
        while (endLine < last && lineWhite(endLine + 1) != white)
          ++endLine;
        if (!white) {
          while (endLine < last && lineWhite(endLine + 1))
            ++endLine;
        }
      }
      start = lineStart(lineOf(m_visualAnchor));
      end = lineStart(endLine);
    }
    type = m_mode == VimEditMode::Visual ? MotionType::Exclusive
                                         : MotionType::Linewise;
    if (type == MotionType::Exclusive) {
      start = m_visualAnchor;
      end = lineStart(endLine) + 1;
    }
    return true;
  };
  if (visual && m_visualPos > m_visualAnchor &&
      lineOf(m_visualAnchor) != lineOf(m_visualPos))
    return extendVisual();
  int startLine = lineOf(cursorPos());
  bool whiteInFront = lineWhite(startLine);
  while (startLine > 0) {
    if (whiteInFront) {
      if (!lineWhite(startLine - 1))
        break;
    } else if (lineWhite(startLine - 1)) {
      break;
    }
    --startLine;
  }
  int endLine = startLine;
  while (endLine <= last && lineWhite(endLine))
    ++endLine;
  --endLine;
  int i = count;
  if (!include && whiteInFront)
    --i;
  bool doWhite = false;
  while (i--) {
    if (endLine == last)
      return false;
    if (!include)
      doWhite = lineWhite(endLine + 1);
    if (include || !doWhite) {
      ++endLine;
      while (endLine < last && !lineWhite(endLine + 1))
        ++endLine;
    }
    if (i == 0 && whiteInFront && include)
      break;
    if (include || doWhite) {
      while (endLine < last && lineWhite(endLine + 1))
        ++endLine;
    }
  }
  if (!whiteInFront && !lineWhite(endLine) && include) {
    while (startLine > 0 && lineWhite(startLine - 1))
      --startLine;
  }
  if (visual && m_mode == VimEditMode::VisualLine &&
      lineOf(m_visualAnchor) == startLine && lineOf(m_visualPos) == endLine &&
      m_visualPos >= m_visualAnchor)
    return extendVisual();
  start = lineStart(startLine);
  end = lineStart(qMax(startLine, endLine));
  type = MotionType::Linewise;
  return true;
}

bool VimMode::selectSentenceObject(int count, bool include, int &start,
                                   int &end, MotionType &type) {
  const int n = docLength();
  const int pos = cursorPos();
  type = MotionType::Exclusive;
  const int line = lineOf(pos);
  if (lineLength(line) == 0) {
    start = lineStart(line);
    end = qMin(n, start + 1);
    return end > start;
  }
  const QVector<int> starts = sentenceStarts();
  if (starts.isEmpty())
    return false;
  auto isEmptyLineStart = [&](int s) {
    return s < n && charAt(s) == '\n' && (s == 0 || charAt(s - 1) == '\n');
  };
  auto nextStart = [&](int idx) {
    return idx + 1 < starts.size() ? starts[idx + 1] : n;
  };
  auto contentEnd = [&](int idx) {
    int e = nextStart(idx);
    if (e >= n || isEmptyLineStart(e))
      return e;
    while (e > starts[idx] && (charAt(e - 1) == ' ' || charAt(e - 1) == '\t'))
      --e;
    return e;
  };
  int k = 0;
  for (int i = 0; i < starts.size(); ++i) {
    if (starts[i] <= pos)
      k = i;
    else
      break;
  }
  const int last = qMin(starts.size() - 1, k + qMax(1, count) - 1);
  if (pos >= contentEnd(k) && pos < nextStart(k)) {
    start = contentEnd(k);
    end = include && k + 1 < starts.size()
              ? contentEnd(qMin(last + 1, int(starts.size()) - 1))
              : nextStart(k);
    return end > start;
  }
  start = starts[k];
  if (!include) {
    end = contentEnd(last);
  } else {
    end = nextStart(last);
    if (end == contentEnd(last)) {
      while (start > 0 &&
             (charAt(start - 1) == ' ' || charAt(start - 1) == '\t'))
        --start;
    }
  }
  return end > start;
}

bool VimMode::selectTagObject(int count, bool include, int &start, int &end,
                              MotionType &type) {
  const QString text = doc()->toPlainText();
  const int pos = cursorPos();
  static const QRegularExpression tagRe(
      "<(/?)([A-Za-z][\\w:.-]*)(?:\\s[^<>]*?)?(/?)>");
  struct Tag {
    QString name;
    int start;
    int end;
  };
  QVector<Tag> stack;
  struct Pair {
    int openStart, openEnd, closeStart, closeEnd;
  };
  QVector<Pair> pairs;
  auto it = tagRe.globalMatch(text);
  while (it.hasNext()) {
    auto m = it.next();
    if (!m.captured(3).isEmpty())
      continue;
    if (m.captured(1).isEmpty()) {
      stack.append(
          {m.captured(2), int(m.capturedStart()), int(m.capturedEnd())});
    } else {
      for (int i = stack.size() - 1; i >= 0; --i) {
        if (stack[i].name == m.captured(2)) {
          pairs.append({stack[i].start, stack[i].end, int(m.capturedStart()),
                        int(m.capturedEnd())});
          stack.resize(i);
          break;
        }
      }
    }
  }
  QVector<Pair> enclosing;
  for (const Pair &p : pairs) {
    if (p.openStart <= pos && pos < p.closeEnd)
      enclosing.append(p);
  }
  if (enclosing.isEmpty())
    return false;
  std::sort(enclosing.begin(), enclosing.end(),
            [](const Pair &a, const Pair &b) {
              return (a.closeEnd - a.openStart) < (b.closeEnd - b.openStart);
            });
  const Pair &p = enclosing[qMin(count, enclosing.size()) - 1];
  if (include) {
    start = p.openStart;
    end = p.closeEnd;
  } else {
    start = p.openEnd;
    end = p.closeStart;
  }
  type = MotionType::Exclusive;
  return true;
}

bool VimMode::selectSearchMatch(bool forward, int &start, int &end) {
  if (m_searchPattern.isEmpty()) {
    emit statusMessage("E35: No previous regular expression");
    return false;
  }
  const QRegularExpression re = compilePattern(m_searchPattern);
  if (!re.isValid())
    return false;
  const QString text = doc()->toPlainText();
  const int pos = cursorPos();
  int firstStart = -1, firstEnd = -1, lastStart = -1, lastEnd = -1;
  int foundStart = -1, foundEnd = -1;
  auto it = re.globalMatch(text);
  while (it.hasNext()) {
    auto m = it.next();
    if (m.capturedLength() == 0)
      continue;
    if (firstStart < 0) {
      firstStart = m.capturedStart();
      firstEnd = m.capturedEnd();
    }
    if (forward && foundStart < 0 && m.capturedEnd() > pos) {
      foundStart = m.capturedStart();
      foundEnd = m.capturedEnd();
    }
    if (!forward && m.capturedStart() <= pos) {
      foundStart = m.capturedStart();
      foundEnd = m.capturedEnd();
    }
    lastStart = m.capturedStart();
    lastEnd = m.capturedEnd();
  }
  if (foundStart < 0) {
    if (firstStart < 0 || !m_wrapScan) {
      emit statusMessage(
          QString("E486: Pattern not found: %1").arg(m_searchPattern));
      return false;
    }
    foundStart = forward ? firstStart : lastStart;
    foundEnd = forward ? firstEnd : lastEnd;
  }
  start = forward ? foundStart : foundEnd - 1;
  end = forward ? foundEnd - 1 : foundStart;
  if (!forward)
    std::swap(start, end);
  highlightSearch();
  return true;
}
