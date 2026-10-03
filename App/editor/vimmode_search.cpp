#include "vimmode.h"

#include <QApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QTextBlock>

namespace {

bool isWordChar(QChar c) { return c.isLetterOrNumber() || c == '_'; }

int findUnescaped(const QString &text, QChar delim, int from) {
  for (int i = from; i < text.size(); ++i) {
    if (text[i] == '\\') {
      ++i;
      continue;
    }
    if (text[i] == delim)
      return i;
  }
  return -1;
}

} // namespace

QString VimMode::toRegularExpression(const QString &vim, bool *caseSensitive,
                                     bool ignoreCase, bool smartCase,
                                     const VimMode *ctx) {
  enum Magic { VeryMagic, MagicOn, NoMagic, VeryNoMagic };
  Magic magic = MagicOn;
  bool cs = !ignoreCase;
  bool forced = false;
  bool hasUpper = false;
  bool zeOpen = false;
  QString out;
  const int n = vim.size();
  bool branchStart = true;
  int atomStart = 0;
  int branchOut = 0;
  int iterStartOut = 0;
  bool iterSpecial = false;
  QVector<QPair<int, int>> groupStack;
  bool visualFilter = false;

  auto quantifier = [&](int &i, bool escapedClose) -> bool {
    int close = vim.indexOf('}', i + 1);
    if (close < 0)
      return false;
    QString inner = vim.mid(i + 1, close - i - 1);
    if (escapedClose && inner.endsWith('\\'))
      inner.chop(1);
    bool lazy = inner.startsWith('-');
    if (lazy)
      inner = inner.mid(1);
    if (inner.isEmpty())
      out += lazy ? "*?" : "*";
    else if (inner.startsWith(','))
      out += "{0" + inner + "}" + (lazy ? "?" : "");
    else
      out += "{" + inner + "}" + (lazy ? "?" : "");
    i = close;
    return true;
  };

  auto charClass = [&](int &i) -> bool {
    int j = i + 1;
    bool negated = false;
    if (j < n && vim[j] == '^') {
      negated = true;
      ++j;
    }
    const int bodyStart = j;
    if (j < n && vim[j] == ']')
      ++j;
    while (j < n && vim[j] != ']') {
      if (vim[j] == '\\')
        ++j;
      else if (vim[j] == '[' && j + 1 < n && vim[j + 1] == ':') {
        int end = vim.indexOf(":]", j + 2);
        if (end > 0)
          j = end + 1;
      }
      ++j;
    }
    if (j >= n)
      return false;
    QString body = vim.mid(bodyStart, j - bodyStart);
    if (body.startsWith(']'))
      body = "\\]" + body.mid(1);
    body.replace("\\e", "\\x1b");
    {
      static const QRegularExpression codeEscape(
          "\\\\(?:d([0-9]+)|o([0-7]{1,4})|x([0-9a-fA-F]{1,2})|u([0-9a-fA-F]{1,"
          "4})|U([0-9a-fA-F]{1,8}))");
      QString converted;
      int last = 0;
      auto it = codeEscape.globalMatch(body);
      while (it.hasNext()) {
        const auto m = it.next();
        uint code = 0;
        if (!m.captured(1).isEmpty())
          code = m.captured(1).toUInt(nullptr, 10);
        else if (!m.captured(2).isEmpty())
          code = m.captured(2).toUInt(nullptr, 8);
        else if (!m.captured(3).isEmpty())
          code = m.captured(3).toUInt(nullptr, 16);
        else if (!m.captured(4).isEmpty())
          code = m.captured(4).toUInt(nullptr, 16);
        else
          code = m.captured(5).toUInt(nullptr, 16);
        converted += body.mid(last, m.capturedStart() - last);
        converted += QString("\\x{%1}").arg(code, 0, 16);
        last = m.capturedEnd();
      }
      converted += body.mid(last);
      body = converted;
    }
    out += QString("[") + (negated ? "^\\n" : "") + body + "]";
    i = j;
    return true;
  };

  auto lineCountNow = [&]() { return ctx ? ctx->lineCount() : 1; };
  auto lineEq = [&](int line) -> QString {
    const int rest = lineCountNow() - line;
    if (rest < 0)
      return "(?!)";
    return QString("(?=[^\\n]*(?:\\n[^\\n]*){%1}\\z)").arg(rest);
  };
  auto lineLt = [&](int line) -> QString {
    const int rest = lineCountNow() - line;
    if (rest < 0)
      return "";
    return QString("(?=[^\\n]*(?:\\n[^\\n]*){%1,}\\z)").arg(rest + 1);
  };
  auto lineGt = [&](int line) -> QString {
    const int rest = lineCountNow() - line - 1;
    if (rest < 0)
      return "(?!)";
    return QString("(?=[^\\n]*(?:\\n[^\\n]*){0,%1}\\z)").arg(rest);
  };
  auto colEq = [&](int col) -> QString {
    if (col < 1)
      return "(?!)";
    return QString("(?<=^[^\\n]{%1})").arg(col - 1);
  };
  auto colLt = [&](int col) -> QString {
    if (col <= 1)
      return "(?!)";
    QStringList alts;
    for (int k = 0; k < qMin(col - 1, 500); ++k)
      alts << QString("(?<=^[^\\n]{%1})").arg(k);
    return "(?:" + alts.join('|') + ")";
  };
  auto colGt = [&](int col) -> QString {
    QString out2;
    for (int k = 0; k < qMin(col, 500); ++k)
      out2 += QString("(?<!^[^\\n]{%1})").arg(k);
    return out2;
  };
  auto positionTest = [&](QChar cmp, int line, int col, bool useCol,
                          bool colOnly = false) -> QString {
    if (colOnly) {
      if (cmp == '<')
        return colLt(col);
      if (cmp == '>')
        return colGt(col);
      return colEq(col);
    }
    if (!useCol) {
      if (cmp == '<')
        return lineLt(line);
      if (cmp == '>')
        return lineGt(line);
      return lineEq(line);
    }
    if (cmp == '<')
      return "(?:" + lineLt(line) + "|" + lineEq(line) + colLt(col) + ")";
    if (cmp == '>')
      return "(?:" + lineGt(line) + "|" + lineEq(line) + colGt(col) + ")";
    return lineEq(line) + colEq(col);
  };
  auto literalChar = [&](uint code) -> QString {
    return QString("\\x{%1}").arg(code, 0, 16);
  };

  auto openGroup = [&](const char *text) {
    groupStack.append({out.size(), branchOut});
    out += text;
    branchOut = out.size();
    iterSpecial = true;
    branchStart = true;
  };
  auto closeGroup = [&]() {
    out += ")";
    if (!groupStack.isEmpty()) {
      atomStart = groupStack.last().first;
      branchOut = groupStack.last().second;
      groupStack.removeLast();
    }
    iterSpecial = true;
  };
  auto alternate = [&]() {
    out += "|";
    branchOut = out.size();
    iterSpecial = true;
    branchStart = true;
  };
  auto concat = [&]() {
    out = out.left(branchOut) + "(?=" + out.mid(branchOut) + ")";
    iterSpecial = true;
  };
  auto lookaround = [&](int &idx) -> bool {
    int j = idx + 1;
    while (j < n && vim[j].isDigit())
      ++j;
    QString prefix;
    if (j < n && vim[j] == '=') {
      prefix = "(?=";
    } else if (j < n && vim[j] == '!') {
      prefix = "(?!";
    } else if (j < n && vim[j] == '>') {
      prefix = "(?>";
    } else if (j + 1 < n && vim[j] == '<' && vim[j + 1] == '=') {
      prefix = "(?<=";
      ++j;
    } else if (j + 1 < n && vim[j] == '<' && vim[j + 1] == '!') {
      prefix = "(?<!";
      ++j;
    } else {
      return false;
    }
    out = out.left(atomStart) + prefix + out.mid(atomStart) + ")";
    idx = j;
    iterSpecial = true;
    return true;
  };

  auto percentAtom = [&](int &i) -> bool {
    if (i + 1 >= n)
      return false;
    const QChar f = vim[i + 1];
    if (f == '(') {
      groupStack.append({out.size(), branchOut});
      out += "(?:";
      branchOut = out.size();
      iterSpecial = true;
      ++i;
      branchStart = true;
      return true;
    }
    if (f == '^') {
      out += "\\A";
      ++i;
      return true;
    }
    if (f == '$') {
      out += "\\z";
      ++i;
      return true;
    }
    if (f == 'V') {
      visualFilter = true;
      iterSpecial = true;
      ++i;
      return true;
    }
    if (f == 'd' || f == 'x' || f == 'X' || f == 'u' || f == 'U' || f == 'o') {
      const int base = f == 'd' ? 10 : f == 'o' ? 8 : 16;
      const int maxDigits = f == 'x'   ? 2
                            : f == 'X' ? 2
                            : f == 'u' ? 4
                            : f == 'U' ? 8
                            : f == 'o' ? 4
                                       : 10;
      int j = i + 2;
      QString digits;
      while (j < n && digits.size() < maxDigits) {
        bool ok = false;
        QString(vim[j]).toUInt(&ok, base);
        if (!ok)
          break;
        digits += vim[j++];
      }
      if (!digits.isEmpty()) {
        out += literalChar(digits.toUInt(nullptr, base));
        i = j - 1;
        return true;
      }
      return false;
    }
    if (f == '[') {
      int j = i + 2;
      QStringList atoms;
      bool closed = false;
      while (j < n) {
        if (vim[j] == ']') {
          closed = true;
          break;
        }
        if (vim[j] == '\\' && j + 1 < n) {
          atoms << toRegularExpression(vim.mid(j, 2));
          j += 2;
        } else {
          atoms << QRegularExpression::escape(QString(vim[j]));
          ++j;
        }
      }
      if (closed && !atoms.isEmpty()) {
        QString seq;
        for (int k = atoms.size() - 1; k >= 0; --k)
          seq = "(?:" + atoms[k] + seq + ")?";
        out += seq;
        i = j;
        return true;
      }
      return false;
    }
    {
      int j = i + 1;
      QChar cmp;
      if (j < n && (vim[j] == '<' || vim[j] == '>')) {
        cmp = vim[j];
        ++j;
      }
      if (j + 1 < n && vim[j] == '\'') {
        int pos = 0;
        if (ctx && ctx->markPosition(vim[j + 1], pos))
          out += positionTest(cmp, ctx->lineOf(pos) + 1, ctx->colOf(pos) + 1,
                              true);
        else
          out += "(?!)";
        i = j + 1;
        return true;
      }
      if (j + 1 < n && vim[j] == '.' &&
          (vim[j + 1] == 'l' || vim[j + 1] == 'c' || vim[j + 1] == 'v')) {
        const int pos = ctx ? ctx->cursorPos() : 0;
        out += positionTest(cmp, ctx ? ctx->lineOf(pos) + 1 : 1,
                            ctx ? ctx->colOf(pos) + 1 : 1, vim[j + 1] != 'l',
                            vim[j + 1] != 'l');
        i = j + 1;
        return true;
      }
      if (j < n && vim[j] == '#' && cmp.isNull()) {
        const int pos = ctx ? ctx->cursorPos() : 0;
        out += positionTest(QChar(), ctx ? ctx->lineOf(pos) + 1 : 1,
                            ctx ? ctx->colOf(pos) + 1 : 1, true);
        i = j;
        return true;
      }
      int k = j;
      int num = 0;
      while (k < n && vim[k].isDigit()) {
        num = qMin(99999999, num * 10 + vim[k].digitValue());
        ++k;
      }
      if (k > j && k < n && (vim[k] == 'l' || vim[k] == 'c' || vim[k] == 'v')) {
        out += positionTest(cmp, num, num, vim[k] != 'l', vim[k] != 'l');
        i = k;
        return true;
      }
    }
    return false;
    return false;
  };

  for (int i = 0; i < n; ++i) {
    if (out.size() != iterStartOut && !iterSpecial)
      atomStart = iterStartOut;
    iterStartOut = out.size();
    iterSpecial = false;
    QChar c = vim[i];
    bool wasBranchStart = branchStart;
    branchStart = false;
    if (c == '\\' && i + 1 < n) {
      QChar d = vim[++i];
      switch (d.unicode()) {
      case 'v':
        magic = VeryMagic;
        branchStart = wasBranchStart;
        continue;
      case 'm':
        magic = MagicOn;
        branchStart = wasBranchStart;
        continue;
      case 'M':
        magic = NoMagic;
        branchStart = wasBranchStart;
        continue;
      case 'V':
        magic = VeryNoMagic;
        branchStart = wasBranchStart;
        continue;
      case 'c':
        cs = false;
        forced = true;
        branchStart = wasBranchStart;
        continue;
      case 'C':
        cs = true;
        forced = true;
        branchStart = wasBranchStart;
        continue;
      case 'n':
        out += "\\n";
        continue;
      case 't':
        out += "\\t";
        continue;
      case 'e':
        out += "\\x1b";
        continue;
      case 'r':
        out += "\\r";
        continue;
      case 's':
        out += "[ \\t]";
        continue;
      case 'S':
        out += "[^ \\t\\n]";
        continue;
      case 'd':
      case 'w':
        out += QString("\\") + d;
        continue;
      case 'D':
        out += "[^\\d\\n]";
        continue;
      case 'W':
        out += "[^\\w\\n]";
        continue;
      case 'a':
        out += "[A-Za-z]";
        continue;
      case 'A':
        out += "[^A-Za-z\\n]";
        continue;
      case 'l':
        out += "[a-z]";
        continue;
      case 'L':
        out += "[^a-z\\n]";
        continue;
      case 'u':
        out += "[A-Z]";
        continue;
      case 'U':
        out += "[^A-Z\\n]";
        continue;
      case 'x':
        out += "[0-9A-Fa-f]";
        continue;
      case 'X':
        out += "[^0-9A-Fa-f\\n]";
        continue;
      case 'o':
        out += "[0-7]";
        continue;
      case 'O':
        out += "[^0-7\\n]";
        continue;
      case 'h':
        out += "[A-Za-z_]";
        continue;
      case 'H':
        out += "[^A-Za-z_\\n]";
        continue;
      case 'i':
      case 'k':
        out += "\\w";
        continue;
      case 'I':
      case 'K':
        out += "[^\\W\\d]";
        continue;
      case 'f':
        out += "[\\p{L}0-9/.\\-_+,#$%~=]";
        continue;
      case 'F':
        out += "[\\p{L}/.\\-_+,#$%~=]";
        continue;
      case 'p':
        out += "[^\\x00-\\x1f\\x7f]";
        continue;
      case 'P':
        out += "[^\\x00-\\x1f\\x7f0-9]";
        continue;
      case '_':
        if (i + 1 < n) {
          QChar e = vim[++i];
          if (e == '.') {
            out += "[\\s\\S]";
          } else if (e == '^') {
            out += "^";
          } else if (e == '$') {
            out += "$";
          } else if (e == '[') {
            const int mark = out.size();
            if (charClass(i)) {
              const QString cls = out.mid(mark);
              out.truncate(mark);
              out += "(?:" + cls + "|\\n)";
            } else {
              out += "\\[";
            }
          } else {
            out += "(?:" + toRegularExpression(QString("\\") + e) + "|\\n)";
          }
        }
        continue;
      case '%':
        if (magic != VeryMagic && percentAtom(i))
          continue;
        break;
      case 'z':
        if (i + 1 < n && vim[i + 1] == 's') {
          out += "\\K";
          ++i;
        } else if (i + 1 < n && vim[i + 1] == 'e') {
          out += "(?=";
          zeOpen = true;
          ++i;
        }
        continue;
      default:
        break;
      }
      if (d.isDigit()) {
        out += QString("\\") + d;
        continue;
      }
      if (d == '<') {
        out += magic == VeryMagic ? "<" : "\\b(?=\\w)";
        continue;
      }
      if (d == '>') {
        out += magic == VeryMagic ? ">" : "\\b(?<=\\w)";
        continue;
      }
      const bool specialWhenEscaped =
          magic == MagicOn || magic == NoMagic || magic == VeryNoMagic;
      if (d == '&' && magic != VeryMagic) {
        concat();
        continue;
      }
      if (specialWhenEscaped) {
        if (d == '(') {
          openGroup("(");
          continue;
        }
        if (d == ')') {
          closeGroup();
          continue;
        }
        if (d == '|') {
          alternate();
          continue;
        }
        if (d == '+' || d == '?' || d == '=') {
          out += d == '+' ? "+" : "?";
          iterSpecial = true;
          continue;
        }
        if (d == '@' && lookaround(i))
          continue;
        if (d == '{' && quantifier(i, true)) {
          iterSpecial = true;
          continue;
        }
      }
      if (magic == NoMagic || magic == VeryNoMagic) {
        if (d == '.' || d == '*') {
          out += d;
          if (d == '*')
            iterSpecial = true;
          continue;
        }
        if (d == '[' && charClass(i))
          continue;
      }
      if (magic == VeryNoMagic && (d == '^' || d == '$')) {
        out += d;
        continue;
      }
      out += QRegularExpression::escape(QString(d));
      continue;
    }

    if (c.isUpper())
      hasUpper = true;

    if (magic == VeryMagic) {
      if (c == '%' && percentAtom(i))
        continue;
      if (c == '(') {
        openGroup("(");
        continue;
      }
      if (c == '|') {
        alternate();
        continue;
      }
      if (c == ')') {
        closeGroup();
        continue;
      }
      if (c == '&') {
        concat();
        continue;
      }
      if (c == '@' && lookaround(i))
        continue;
      if (QString("+?*.{}").contains(c)) {
        if (c == '{' && quantifier(i, false)) {
          iterSpecial = true;
          continue;
        }
        out += c;
        if (c != '.')
          iterSpecial = true;
        continue;
      }
      if (c == '=') {
        out += "?";
        iterSpecial = true;
        continue;
      }
      if (c == '<') {
        out += "\\b(?=\\w)";
        continue;
      }
      if (c == '>') {
        out += "\\b(?<=\\w)";
        continue;
      }
      if (c == '[' && charClass(i))
        continue;
    } else if (magic == MagicOn) {
      if (c == '.' || c == '*') {
        out += c;
        if (c == '*')
          iterSpecial = true;
        continue;
      }
      if (c == '[' && charClass(i))
        continue;
    }
    if (c == '^' && wasBranchStart && magic != VeryNoMagic) {
      out += "^";
      branchStart = true;
      continue;
    }
    if (c == '^' && wasBranchStart && magic == VeryNoMagic && i == 0) {
      out += "^";
      continue;
    }
    if (c == '$') {
      const QString rest = vim.mid(i + 1);
      bool atEnd = rest.isEmpty() || rest.startsWith("\\|") ||
                   rest.startsWith("\\)") || rest.startsWith("\\n") ||
                   (magic == VeryMagic &&
                    (rest.startsWith('|') || rest.startsWith(')')));
      if (atEnd) {
        out += "$";
        continue;
      }
    }
    out += QRegularExpression::escape(QString(c));
  }
  if (zeOpen)
    out += ")";
  if (!forced && ignoreCase && smartCase && hasUpper)
    cs = true;
  if (caseSensitive)
    *caseSensitive = cs;
  return out;
}

QRegularExpression VimMode::compilePattern(const QString &vimPattern) const {
  bool cs = true;
  const QString pcre =
      toRegularExpression(vimPattern, &cs, m_ignoreCase, m_smartCase, this);
  QRegularExpression::PatternOptions options =
      QRegularExpression::MultilineOption |
      QRegularExpression::UseUnicodePropertiesOption;
  if (!cs)
    options |= QRegularExpression::CaseInsensitiveOption;
  return QRegularExpression(pcre, options);
}

void VimMode::highlightSearch() {
  if (!m_hlSearch || m_searchPattern.isEmpty())
    return;
  m_searchHighlightActive = true;
  emit searchHighlightRequested(
      toRegularExpression(m_searchPattern, nullptr, m_ignoreCase, m_smartCase),
      true);
}

void VimMode::clearSearchHighlight() {
  m_searchHighlightActive = false;
  emit searchHighlightRequested(QString(), false);
}

bool VimMode::search(const QString &pattern, bool forward, int count,
                     int fromPos, int &matchStart, int &matchEnd, bool *wrapped,
                     bool quiet) {
  if (wrapped)
    *wrapped = false;
  const QRegularExpression re = compilePattern(pattern);
  if (!re.isValid()) {
    if (!quiet)
      emit statusMessage(
          QString("E383: Invalid search string: %1").arg(pattern));
    return false;
  }
  const QString text = doc()->toPlainText();
  int pos = fromPos;
  int len = 0;
  for (int c = 0; c < qMax(1, count); ++c) {
    if (forward) {
      QRegularExpressionMatch m;
      {
        const int curLine = lineOf(qBound(0, pos, docLength()));
        const int curEol = lineEndPos(curLine);
        const int nextLine = curEol + 1;
        int offset = lineStart(curLine);
        for (int guard = 0; guard < 100000 && offset <= text.size(); ++guard) {
          QRegularExpressionMatch cand = re.match(text, offset);
          if (!cand.hasMatch())
            break;
          const int mStart = cand.capturedStart();
          const int mEnd = cand.capturedEnd();
          if (mStart > curEol) {
            m = cand;
            break;
          }
          const int adjusted = mStart - (mStart == curEol ? 1 : 0);
          if (adjusted > pos) {
            m = cand;
            break;
          }
          if (mEnd > curEol) {
            offset = nextLine;
          } else if (mEnd > mStart) {
            offset = mEnd;
          } else {
            offset = mStart + 1;
          }
          if (offset >= curEol)
            offset = nextLine;
        }
      }
      if (!m.hasMatch()) {
        if (!m_wrapScan) {
          if (!quiet)
            emit statusMessage(
                QString("E385: Search hit BOTTOM without match for: %1")
                    .arg(pattern));
          return false;
        }
        m = re.match(text, 0);
        if (wrapped)
          *wrapped = true;
      }
      if (!m.hasMatch()) {
        if (!quiet)
          emit statusMessage(
              QString("E486: Pattern not found: %1").arg(pattern));
        return false;
      }
      pos = m.capturedStart();
      len = m.capturedLength();
    } else {
      int best = -1, bestLen = 0, lastStart = -1, lastLen = 0;
      auto it = re.globalMatch(text);
      while (it.hasNext()) {
        auto m = it.next();
        if (m.capturedStart() < pos) {
          best = m.capturedStart();
          bestLen = m.capturedLength();
        }
        lastStart = m.capturedStart();
        lastLen = m.capturedLength();
      }
      if (best < 0) {
        if (lastStart < 0) {
          if (!quiet)
            emit statusMessage(
                QString("E486: Pattern not found: %1").arg(pattern));
          return false;
        }
        if (!m_wrapScan) {
          if (!quiet)
            emit statusMessage(
                QString("E384: Search hit TOP without match for: %1")
                    .arg(pattern));
          return false;
        }
        best = lastStart;
        bestLen = lastLen;
        if (wrapped)
          *wrapped = true;
      }
      pos = best;
      len = bestLen;
    }
  }
  matchStart = pos;
  matchEnd = pos + len;
  return true;
}

VimMode::MotionResult VimMode::searchMotion(bool reverse, int count,
                                            int fromPos) {
  MotionResult r;
  r.pos = fromPos;
  r.jump = true;
  if (m_searchPattern.isEmpty()) {
    emit statusMessage("E35: No previous regular expression");
    return r;
  }
  const bool forward = m_searchForward != reverse;
  QChar kind;
  int offset = 0;
  const QString off = m_searchOffset;
  if (!off.isEmpty()) {
    QString num = off;
    if (QString("esb").contains(off[0])) {
      kind = off[0];
      num = off.mid(1);
    } else {
      kind = 'l';
    }
    if (num == "+")
      offset = 1;
    else if (num == "-")
      offset = -1;
    else if (!num.isEmpty())
      offset = num.toInt();
  }
  int searchFrom = fromPos;
  if ((kind == 'e' || kind == 's' || kind == 'b') && offset != 0)
    searchFrom = qBound(-1, fromPos - offset, docLength());
  int ms = 0, me = 0;
  bool wrapped = false;
  if (!search(m_searchPattern, forward, count, searchFrom, ms, me, &wrapped))
    return r;
  r.ok = true;
  if (kind == 'l') {
    int line = qBound(0, lineOf(ms) + offset, lineCount() - 1);
    r.pos = lineStart(line);
    r.type = MotionType::Linewise;
  } else if (kind == 'e') {
    r.pos = qBound(0, qMax(ms, me - 1) + offset, qMax(0, docLength() - 1));
    r.type = MotionType::Inclusive;
  } else if (kind == 's' || kind == 'b') {
    r.pos = qBound(0, ms + offset, docLength());
  } else {
    r.pos = ms;
  }
  if (wrapped)
    emit statusMessage(forward ? "search hit BOTTOM, continuing at TOP"
                               : "search hit TOP, continuing at BOTTOM");
  else
    emit statusMessage(QString(forward ? "/" : "?") + m_searchPattern);
  highlightSearch();
  return r;
}

bool VimMode::doSearchCommand(const QString &input, bool forward, int count,
                              MotionResult &result, int fromPos) {
  QChar delim = forward ? '/' : '?';
  QString pattern = input;
  QString offset;
  QString chained;
  int at = findUnescaped(input, delim, 0);
  if (at >= 0) {
    pattern = input.left(at);
    offset = input.mid(at + 1);
    const int semi = offset.indexOf(';');
    if (semi >= 0 && semi + 1 < offset.size() &&
        (offset[semi + 1] == '/' || offset[semi + 1] == '?')) {
      chained = offset.mid(semi + 1);
      offset = offset.left(semi);
    }
  }
  if (pattern.isEmpty()) {
    if (m_searchPattern.isEmpty()) {
      emit statusMessage("E35: No previous regular expression");
      return false;
    }
    pattern = m_searchPattern;
    if (at < 0)
      offset = m_searchOffset;
  }
  m_searchPattern = pattern;
  m_searchOffset = offset;
  m_searchForward = forward;
  result = searchMotion(false, count, fromPos >= 0 ? fromPos : cursorPos());
  if (result.ok && !chained.isEmpty()) {
    MotionResult second;
    const bool ok = doSearchCommand(chained.mid(1), chained[0] == '/', 1,
                                    second, result.pos);
    if (!ok) {
      result.ok = false;
      return false;
    }
    result = second;
  }
  return result.ok;
}

QString VimMode::wordUnderCursor(int pos, bool keywordOnly, int *start,
                                 int *end) const {
  int line = lineOf(pos);
  const QString text = lineText(line);
  int col = pos - lineStart(line);
  int i = col;
  while (i < text.size() && !isWordChar(text[i]))
    ++i;
  bool keyword = i < text.size();
  if (!keyword) {
    if (keywordOnly)
      return QString();
    i = col;
    while (i < text.size() && text[i].isSpace())
      ++i;
    if (i >= text.size())
      return QString();
  }
  int s = i, e = i;
  auto same = [&](QChar ch) {
    return keyword ? isWordChar(ch) : !ch.isSpace() && !isWordChar(ch);
  };
  if (i == col || keyword) {
    while (s > 0 && same(text[s - 1]) && (s > col || i == col))
      --s;
  }
  while (e < text.size() && same(text[e]))
    ++e;
  if (start)
    *start = lineStart(line) + s;
  if (end)
    *end = lineStart(line) + e;
  return text.mid(s, e - s);
}

bool VimMode::prepareWordSearch(bool forward, bool wholeWord, int fromPos,
                                int &searchFrom) {
  int start = 0, end = 0;
  QString word = wordUnderCursor(fromPos, false, &start, &end);
  if (word.isEmpty()) {
    emit statusMessage("E348: No string under cursor");
    return false;
  }
  bool keyword = isWordChar(word[0]);
  QString pattern = escapePattern(word);
  if (wholeWord && keyword)
    pattern = "\\<" + pattern + "\\>";
  m_searchPattern = pattern;
  m_searchOffset.clear();
  m_searchForward = forward;
  m_searchHistory.removeAll(pattern);
  m_searchHistory.prepend(pattern);
  searchFrom = start;
  return true;
}

void VimMode::enterCommandLine(QChar type, const QString &initial) {
  m_cmdReturnMode = m_mode;
  m_cmdType = type;
  m_historyIndex = -1;
  m_historyDraft.clear();
  m_cmdRegisterPending = false;
  m_incsearchOrigin = (type == ':' || type == '=') ? -1 : cursorPos();
  m_incsearchScroll = m_editor->verticalScrollBar()
                          ? m_editor->verticalScrollBar()->value()
                          : 0;
  m_cmdText = initial;
  m_cmdCursor = initial.size();
  setMode(VimEditMode::Command);
  emit commandBufferChanged(commandBuffer());
}

void VimMode::leaveCommandLine() {
  m_cmdText.clear();
  m_cmdCursor = 0;
  m_cmdRegisterPending = false;
  const bool toVisual = m_cmdFromVisual;
  m_cmdFromVisual = false;
  if (toVisual) {
    setMode(m_cmdReturnMode);
    updateVisualSelection();
  } else {
    setMode(VimEditMode::Normal);
  }
  emit commandBufferChanged(QString());
}

void VimMode::setCommandBufferInternal(const QString &text, int cursor) {
  m_cmdText = text;
  m_cmdCursor = qBound(0, cursor, text.size());
  emit commandBufferChanged(commandBuffer());
  if (m_cmdType == '/' || m_cmdType == '?')
    updateIncrementalSearch();
}

void VimMode::updateIncrementalSearch() {
  if (!m_incSearch || m_incsearchOrigin < 0)
    return;
  QString pattern = m_cmdText;
  int at = findUnescaped(pattern, m_cmdType, 0);
  if (at >= 0)
    pattern = pattern.left(at);
  QTextCursor c = m_editor->textCursor();
  int ms = 0, me = 0;
  if (!pattern.isEmpty() && search(pattern, m_cmdType == '/', 1,
                                   m_incsearchOrigin, ms, me, nullptr, true)) {
    c.setPosition(ms);
    c.setPosition(me, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(c);
    if (m_hlSearch)
      emit searchHighlightRequested(
          toRegularExpression(pattern, nullptr, m_ignoreCase, m_smartCase),
          true);
  } else {
    c.setPosition(qBound(0, m_incsearchOrigin, docLength()));
    m_editor->setTextCursor(c);
  }
}

bool VimMode::handleCommandKey(const QString &token) {
  if (m_cmdRegisterPending) {
    m_cmdRegisterPending = false;
    QString insert;
    if (token == "<C-w>") {
      insert = wordUnderCursor(
          m_incsearchOrigin >= 0 ? m_incsearchOrigin : cursorPos(), true);
    } else if (token.size() == 1 && isValidRegister(token[0])) {
      insert = getRegister(token[0]).content;
      if (insert.endsWith('\n'))
        insert.chop(1);
    }
    updatePendingKeys();
    setCommandBufferInternal(m_cmdText.left(m_cmdCursor) + insert +
                                 m_cmdText.mid(m_cmdCursor),
                             m_cmdCursor + insert.size());
    return true;
  }

  if (token == "<Esc>" || token == "<C-c>") {
    if (m_incsearchOrigin >= 0) {
      if (m_cmdFromVisual) {
        m_visualPos = m_incsearchOrigin;
      } else {
        QTextCursor c = m_editor->textCursor();
        c.setPosition(qBound(0, m_incsearchOrigin, docLength()));
        m_editor->setTextCursor(c);
      }
      if (m_editor->verticalScrollBar())
        m_editor->verticalScrollBar()->setValue(m_incsearchScroll);
      if (m_searchHighlightActive)
        highlightSearch();
      else
        clearSearchHighlight();
    }
    m_incsearchOrigin = -1;
    m_cmdOperatorKeys.clear();
    if (m_cmdType == '=')
      resetPending();
    leaveCommandLine();
    return true;
  }
  if (token == "<CR>") {
    executeCommandLine();
    return true;
  }
  if (token == "<BS>" || token == "<C-h>") {
    if (m_cmdText.isEmpty())
      return handleCommandKey("<Esc>");
    if (m_cmdCursor > 0)
      setCommandBufferInternal(m_cmdText.left(m_cmdCursor - 1) +
                                   m_cmdText.mid(m_cmdCursor),
                               m_cmdCursor - 1);
    return true;
  }
  if (token == "<Del>") {
    if (m_cmdCursor < m_cmdText.size())
      setCommandBufferInternal(m_cmdText.left(m_cmdCursor) +
                                   m_cmdText.mid(m_cmdCursor + 1),
                               m_cmdCursor);
    return true;
  }
  if (token == "<C-u>") {
    setCommandBufferInternal(m_cmdText.mid(m_cmdCursor), 0);
    return true;
  }
  if (token == "<C-w>") {
    int i = m_cmdCursor;
    while (i > 0 && m_cmdText[i - 1].isSpace())
      --i;
    if (i > 0) {
      bool word = isWordChar(m_cmdText[i - 1]);
      while (i > 0 && !m_cmdText[i - 1].isSpace() &&
             isWordChar(m_cmdText[i - 1]) == word)
        --i;
    }
    setCommandBufferInternal(m_cmdText.left(i) + m_cmdText.mid(m_cmdCursor), i);
    return true;
  }
  if (token == "<Left>" || token == "<Right>" || token == "<Home>" ||
      token == "<End>" || token == "<C-b>" || token == "<C-e>") {
    int cursor = m_cmdCursor;
    if (token == "<Left>")
      cursor = qMax(0, cursor - 1);
    else if (token == "<Right>")
      cursor = qMin(m_cmdText.size(), cursor + 1);
    else if (token == "<Home>" || token == "<C-b>")
      cursor = 0;
    else
      cursor = m_cmdText.size();
    m_cmdCursor = cursor;
    emit commandBufferChanged(commandBuffer());
    return true;
  }
  if (token == "<Up>" || token == "<Down>") {
    const QStringList &history =
        m_cmdType == ':' ? m_exHistory : m_searchHistory;
    if (history.isEmpty())
      return true;
    if (m_historyIndex < 0)
      m_historyDraft = m_cmdText;
    int index = m_historyIndex;
    const int step = token == "<Up>" ? 1 : -1;
    for (int i = index + step; i >= -1 && i < history.size(); i += step) {
      if (i == -1 || history[i].startsWith(m_historyDraft)) {
        index = i;
        break;
      }
    }
    m_historyIndex = index;
    const QString text = index >= 0 ? history[index] : m_historyDraft;
    setCommandBufferInternal(text, text.size());
    return true;
  }
  if (token == "<C-r>") {
    m_cmdRegisterPending = true;
    updatePendingKeys();
    return true;
  }
  if (token == "<C-v>") {
    QString clip = QApplication::clipboard()->text();
    clip.replace('\n', ' ');
    setCommandBufferInternal(m_cmdText.left(m_cmdCursor) + clip +
                                 m_cmdText.mid(m_cmdCursor),
                             m_cmdCursor + clip.size());
    return true;
  }
  if (token.size() == 1) {
    setCommandBufferInternal(m_cmdText.left(m_cmdCursor) + token +
                                 m_cmdText.mid(m_cmdCursor),
                             m_cmdCursor + 1);
    return true;
  }
  return true;
}

void VimMode::executeCommandLine() {
  const QString text = m_cmdText;
  const QChar type = m_cmdType;
  const QStringList opKeys = m_cmdOperatorKeys;
  m_cmdOperatorKeys.clear();
  const bool fromVisual = m_cmdFromVisual;
  const VimEditMode returnMode = m_cmdReturnMode;
  const int count = m_cmdCount;
  m_cmdCount = 0;
  const int origin = m_incsearchOrigin;
  m_incsearchOrigin = -1;

  if (!text.isEmpty()) {
    QStringList &history = type == ':' ? m_exHistory : m_searchHistory;
    history.removeAll(text);
    history.prepend(text);
    while (history.size() > kMaxHistory)
      history.removeLast();
  }

  m_cmdText.clear();
  m_cmdCursor = 0;
  m_cmdFromVisual = false;

  if (type == '=') {
    QString result, error;
    bool isList = false;
    const bool ok = evalExpression(text, &result, &isList, nullptr, &error);
    emit commandBufferChanged(QString());
    const bool toInsert =
        returnMode == VimEditMode::Insert || returnMode == VimEditMode::Replace;
    if (toInsert) {
      setMode(returnMode);
      if (ok)
        insertTextAtCursor(isList ? result + "\n" : result);
      else
        emit statusMessage(error);
      return;
    }
    setMode(VimEditMode::Normal);
    if (ok) {
      m_exprRegister = isList ? result + "\n" : result;
      m_exprRegisterLinewise = isList;
    } else {
      emit statusMessage(error);
      resetPending();
      failCommand();
    }
    return;
  }

  if (type == ':') {
    setMode(VimEditMode::Normal);
    emit commandBufferChanged(QString());
    if (!text.trimmed().isEmpty()) {
      m_undoCursors[doc()->availableUndoSteps()] = -1;
      m_repeatedCommandLine = false;
      executeEx(text);
      if (!m_repeatedCommandLine)
        m_lastExCommand = text;
      m_repeatedCommandLine = false;
    }
    return;
  }

  if (fromVisual) {
    setMode(returnMode);
    m_visualPos = origin >= 0 ? origin : m_visualPos;
  } else {
    setMode(VimEditMode::Normal);
    QTextCursor c = m_editor->textCursor();
    c.setPosition(qBound(0, origin >= 0 ? origin : c.position(), docLength()));
    m_editor->setTextCursor(c);
  }
  emit commandBufferChanged(QString());

  MotionResult r;
  const bool ok = doSearchCommand(text, type == '/', qMax(1, count), r);
  if (!ok) {
    if (fromVisual)
      updateVisualSelection();
    return;
  }

  if (!opKeys.isEmpty()) {
    NormalCmd cmd;
    QStringList probe = opKeys;
    probe << "l";
    if (parseCommand(probe, cmd, false) != Parse::Complete || cmd.op.isEmpty())
      return;
    const int from = cursorPos();
    Range range = motionRange(from, r.pos, r.type);
    QStringList dotKeys = opKeys;
    dotKeys << QString(type);
    for (QChar ch : text)
      dotKeys << (ch == '<' ? QString("<") : QString(ch));
    dotKeys << "<CR>";
    m_opForceNumbered = true;
    m_opCursor = qMin(from, r.pos);
    const int revision = doc()->revision();
    applyOperator(cmd.op, range, cmd.reg, cmd.count, false);
    m_opForceNumbered = false;
    m_opCursor = -1;
    if (cmd.op == "c") {
      m_insertEditOpen = doc()->revision() != revision;
      startInsertSession(1, "c", dotKeys);
    } else if (cmd.op != "y" && cmd.op != "!") {
      setDotCommand(dotKeys);
    }
    return;
  }

  if (fromVisual) {
    m_visualPos = r.pos;
    updateVisualSelection();
    return;
  }
  pushJump(cursorPos());
  setCursorPos(clampNormal(r.pos));
}
