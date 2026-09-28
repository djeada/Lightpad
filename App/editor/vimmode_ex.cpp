#include "vimmode.h"

#include <QSet>
#include <QTextBlock>
#include <algorithm>

namespace {

int findUnescapedDelim(const QString &text, QChar delim, int from) {
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

qint64 parseCount(const QString &text, bool *ok) {
  *ok = !text.isEmpty();
  qint64 value = 0;
  for (QChar c : text) {
    if (c.unicode() < '0' || c.unicode() > '9') {
      *ok = false;
      return 0;
    }
    value = qMin<qint64>(value * 10 + c.digitValue(), 999999999999LL);
  }
  return value;
}

struct DepthGuard {
  explicit DepthGuard(int &depth) : m_depth(depth) { ++m_depth; }
  ~DepthGuard() { --m_depth; }
  int &m_depth;
};

QString visibleText(QString text) {
  text.replace('\n', "^J");
  text.replace('\t', "^I");
  if (text.size() > 60)
    text = text.left(57) + "...";
  return text;
}

} // namespace

bool VimMode::parseExAddress(const QString &cmd, int &i, int curLine, int &line,
                             bool &found, QString &error) {
  const int n = cmd.size();
  while (i < n && cmd[i] == ' ')
    ++i;
  found = false;
  if (i < n) {
    QChar c = cmd[i];
    if (c.isDigit()) {
      int value = 0;
      while (i < n && cmd[i].isDigit()) {
        value = qMin(99999999, value * 10 + cmd[i].digitValue());
        ++i;
      }
      line = value - 1;
      found = true;
    } else if (c == '.') {
      line = curLine;
      ++i;
      found = true;
    } else if (c == '$') {
      line = lineCount() - 1;
      ++i;
      found = true;
    } else if (c == '\'' && i + 1 < n) {
      int pos = 0;
      if (!markPosition(cmd[i + 1], pos)) {
        error = "E20: Mark not set";
        return false;
      }
      line = lineOf(pos);
      i += 2;
      found = true;
    } else if (c == '/' || c == '?') {
      int end = findUnescapedDelim(cmd, c, i + 1);
      QString pattern = end < 0 ? cmd.mid(i + 1) : cmd.mid(i + 1, end - i - 1);
      i = end < 0 ? n : end + 1;
      if (pattern.isEmpty())
        pattern = m_searchPattern;
      if (pattern.isEmpty()) {
        error = "E35: No previous regular expression";
        return false;
      }
      m_searchPattern = pattern;
      int from = c == '/' ? lineEndPos(curLine) : lineStart(curLine);
      int ms = 0, me = 0;
      if (!search(pattern, c == '/', 1, from, ms, me, nullptr, true)) {
        error = QString("E486: Pattern not found: %1").arg(pattern);
        return false;
      }
      line = lineOf(ms);
      if (line == curLine) {
        error = QString("E486: Pattern not found: %1").arg(pattern);
        return false;
      }
      found = true;
    } else if (c == '+' || c == '-') {
      line = curLine;
      found = true;
    }
  }
  qint64 target = line;
  while (found && i < n && (cmd[i] == '+' || cmd[i] == '-')) {
    const int sign = cmd[i] == '+' ? 1 : -1;
    ++i;
    qint64 value = 0;
    bool digits = false;
    while (i < n && cmd[i].isDigit()) {
      value = qMin<qint64>(value * 10 + cmd[i].digitValue(), 999999999999LL);
      digits = true;
      ++i;
    }
    target += sign * (digits ? value : 1);
    target = qBound(qint64(-999999999999LL), target, qint64(999999999999LL));
  }
  if (found)
    line = int(qBound(qint64(-100000000), target, qint64(100000000)));
  return true;
}

bool VimMode::parseExRange(const QString &cmd, int &i, int &line1, int &line2,
                           bool &hasRange, QString &error) {
  const int n = cmd.size();
  while (i < n && (cmd[i] == ' ' || cmd[i] == ':'))
    ++i;
  int cur = lineOf(cursorPos());
  line1 = line2 = cur;
  hasRange = false;
  if (i < n && cmd[i] == '%') {
    line1 = 0;
    line2 = lineCount() - 1;
    hasRange = true;
    ++i;
    return true;
  }
  if (i < n && cmd[i] == '*') {
    int s = 0, e = 0;
    if (!markPosition('<', s) || !markPosition('>', e)) {
      error = "E20: Mark not set";
      return false;
    }
    line1 = lineOf(s);
    line2 = lineOf(e);
    hasRange = true;
    ++i;
    return true;
  }
  int a = cur;
  bool found = false;
  if (!parseExAddress(cmd, i, cur, a, found, error))
    return false;
  if (!found) {
    if (i >= n || (cmd[i] != ',' && cmd[i] != ';'))
      return true;
    a = cur;
  }
  hasRange = true;
  line1 = line2 = a;
  while (i < n && (cmd[i] == ',' || cmd[i] == ';')) {
    if (cmd[i] == ';')
      cur = line2;
    ++i;
    int b = cur;
    bool found2 = false;
    if (!parseExAddress(cmd, i, cur, b, found2, error))
      return false;
    if (!found2)
      b = cur;
    line1 = line2;
    line2 = b;
  }
  if (line1 > line2)
    std::swap(line1, line2);
  if (line1 < -1) {
    error = "E16: Invalid range";
    return false;
  }
  return true;
}

void VimMode::executeEx(const QString &command) {
  if (m_exDepth >= kMaxExDepth) {
    emit statusMessage("E169: Command too recursive");
    return;
  }
  DepthGuard depthGuard(m_exDepth);
  QString cmd = command;
  while (cmd.startsWith(':') || cmd.startsWith(' '))
    cmd = cmd.mid(1);
  if (cmd.trimmed().isEmpty())
    return;

  int i = 0;
  int line1 = 0, line2 = 0;
  bool hasRange = false;
  QString error;
  if (!parseExRange(cmd, i, line1, line2, hasRange, error)) {
    emit statusMessage(error);
    return;
  }
  while (i < cmd.size() && cmd[i] == ' ')
    ++i;

  QString name;
  if (i < cmd.size() && cmd[i].isLetter()) {
    while (i < cmd.size() && cmd[i].isLetter())
      name += cmd[i++];
  } else if (i < cmd.size()) {
    QChar c = cmd[i];
    if (c == '&' && i + 1 < cmd.size() && cmd[i + 1] == '&') {
      name = "&&";
      i += 2;
    } else if (QString("&~<>!=#@*").contains(c)) {
      name = c;
      ++i;
    }
  }
  static const QStringList barArgCommands = {
      "g",   "gl",   "glo",   "glob",   "globa",   "global", "v",     "vg",
      "vgl", "vglo", "vglob", "vgloba", "vglobal", "norm",   "norma", "normal"};
  if (!barArgCommands.contains(name)) {
    int scanFrom = i;
    if (!name.isEmpty() && QString("substitute").startsWith(name) &&
        i < cmd.size() && !cmd[i].isLetterOrNumber() && cmd[i] != '\\' &&
        cmd[i] != '"' && cmd[i] != '|' && !cmd[i].isSpace()) {
      const QChar delim = cmd[i];
      const int patternEnd = findUnescapedDelim(cmd, delim, i + 1);
      const int replacementEnd =
          patternEnd < 0 ? -1 : findUnescapedDelim(cmd, delim, patternEnd + 1);
      scanFrom = replacementEnd < 0 ? cmd.size() : replacementEnd + 1;
    }
    for (int k = scanFrom; k < cmd.size(); ++k) {
      if (cmd[k] == '\\') {
        ++k;
        continue;
      }
      if (cmd[k] == '|') {
        const QString rest = cmd.mid(k + 1);
        executeEx(cmd.left(k));
        if (!rest.trimmed().isEmpty())
          executeEx(rest);
        return;
      }
    }
  }
  bool bang = false;
  if (!name.isEmpty() && name != "!" && i < cmd.size() && cmd[i] == '!') {
    bang = true;
    ++i;
  }
  if (line2 >= lineCount()) {
    if (!name.isEmpty()) {
      emit statusMessage("E16: Invalid range");
      return;
    }
    line2 = lineCount() - 1;
    line1 = qMin(line1, line2);
  }
  const QString rawArgs = cmd.mid(i);
  const QString args = rawArgs.trimmed();
  const int last = lineCount() - 1;
  const int clampedLine2 = qBound(0, line2, last);

  auto is = [&](const QString &full, int minLen) {
    return name.size() >= minLen && full.startsWith(name);
  };

  if (name.isEmpty()) {
    if (hasRange) {
      pushJump(cursorPos());
      setCursorPos(firstNonBlankPos(qBound(0, line2, last)));
    } else {
      emit statusMessage(QString("E492: Not an editor command: %1").arg(cmd));
    }
    return;
  }

  auto countArg = [&](const QString &text, QChar *reg) {
    QString rest = text;
    if (reg && !rest.isEmpty() && !rest[0].isDigit() &&
        isValidRegister(rest[0])) {
      *reg = rest[0];
      rest = rest.mid(1).trimmed();
    }
    bool ok = false;
    const qint64 n = parseCount(rest, &ok);
    if (ok && n > 0) {
      line1 = clampedLine2;
      line2 = int(qMin<qint64>(last, qint64(line1) + n - 1));
    }
  };

  if (is("write", 1) || name == "w") {
    emit commandExecuted("save");
  } else if (is("wq", 2) || name == "x" || is("xit", 2) || is("exit", 3)) {
    if (name == "x" || name.startsWith("xi") || name.startsWith("exi")) {
      if (doc()->isModified())
        emit commandExecuted("save");
    } else {
      emit commandExecuted("save");
    }
    emit commandExecuted("quit");
  } else if (is("wall", 2)) {
    emit commandExecuted("saveAll");
  } else if (is("wqall", 3) || is("xall", 2)) {
    emit commandExecuted("saveAll");
    emit commandExecuted("quitAll");
  } else if (is("quit", 1)) {
    emit commandExecuted(bang ? "closeWithoutSaving" : "quit");
  } else if (is("qall", 2) || is("quitall", 5)) {
    emit commandExecuted(bang ? "forceQuit" : "quitAll");
  } else if (is("update", 2)) {
    if (doc()->isModified())
      emit commandExecuted("save");
  } else if (is("saveas", 3)) {
    emit commandExecuted("saveAs");
  } else if (is("edit", 1)) {
    if (!args.isEmpty())
      emit commandExecuted(QString("edit:%1").arg(args));
    else if (bang)
      emit commandExecuted("reload");
  } else if (is("enew", 3)) {
    emit commandExecuted("newFile");
  } else if (is("tabedit", 4) || is("tabnew", 4)) {
    emit commandExecuted(args.isEmpty() ? QString("newFile")
                                        : QString("edit:%1").arg(args));
  } else if (is("split", 2) || is("new", 3)) {
    emit commandExecuted("splitHorizontal");
    if (!args.isEmpty())
      emit commandExecuted(QString("edit:%1").arg(args));
  } else if (is("vsplit", 2) || is("vnew", 3)) {
    emit commandExecuted("splitVertical");
    if (!args.isEmpty())
      emit commandExecuted(QString("edit:%1").arg(args));
  } else if (is("close", 3)) {
    emit commandExecuted("closeSplit");
  } else if (is("only", 2)) {
    emit commandExecuted("unsplitAll");
  } else if (is("bnext", 2) || is("tabnext", 4)) {
    emit commandExecuted("nextTab");
    emit statusMessage("Next buffer");
  } else if (is("bprevious", 2) || is("bNext", 2) || is("tabprevious", 4) ||
             is("tabNext", 4)) {
    emit commandExecuted("prevTab");
    emit statusMessage("Previous buffer");
  } else if (is("bdelete", 2) || is("tabclose", 4)) {
    emit commandExecuted(bang ? "closeWithoutSaving" : "quit");
  } else if (is("nohlsearch", 3)) {
    clearSearchHighlight();
  } else if (is("set", 2) || is("setlocal", 4) || is("setglobal", 4)) {
    exSet(args);
  } else if (is("registers", 3) || is("display", 2)) {
    exRegisters();
  } else if (is("marks", 4)) {
    exMarks();
  } else if (is("undo", 1)) {
    undo(1);
  } else if (is("redo", 3)) {
    redo(1);
  } else if (is("delete", 1)) {
    QChar reg;
    countArg(args, &reg);
    QTextCursor block(doc());
    block.beginEditBlock();
    operatorDelete(lineRange(qMax(0, line1), qMax(0, line2)), reg, false);
    block.endEditBlock();
    recordChangePosition(cursorPos());
  } else if (is("yank", 1)) {
    QChar reg;
    countArg(args, &reg);
    operatorYank(lineRange(qMax(0, line1), qMax(0, line2)), reg, false);
  } else if (is("put", 2)) {
    QChar reg = args.isEmpty() ? QChar() : args[0];
    VimRegister r = getRegister(reg);
    if (r.content.isEmpty() && !reg.isNull() && reg != '"' &&
        !(reg.unicode() < 128 && reg.isLetterOrNumber() &&
          m_registers.contains(reg.toLower())) &&
        !(reg == '-' && m_registers.contains(reg))) {
      emit statusMessage(QString("E353: Nothing in register %1").arg(reg));
      return;
    }
    QString text = r.content;
    if (!text.endsWith('\n'))
      text += '\n';
    QTextCursor block(doc());
    block.beginEditBlock();
    int target = bang ? line2 - 1 : line2;
    int firstLine;
    if (target < 0) {
      replaceRange(0, 0, text);
      firstLine = 0;
    } else if (target >= last) {
      replaceRange(docLength(), docLength(), "\n" + text.left(text.size() - 1));
      firstLine = last + 1;
    } else {
      replaceRange(lineStart(target + 1), lineStart(target + 1), text);
      firstLine = target + 1;
    }
    block.endEditBlock();
    setCursorPos(firstNonBlankPos(firstLine + text.count('\n') - 1));
  } else if (is("move", 1) || is("copy", 2) || name == "t") {
    int j = 0;
    int dest = 0;
    bool found = false;
    QString err;
    if (!parseExAddress(args, j, lineOf(cursorPos()), dest, found, err) ||
        !found) {
      emit statusMessage(err.isEmpty() ? "E14: Invalid address" : err);
      return;
    }
    if (dest < -1 || dest > last) {
      emit statusMessage("E16: Invalid range");
      return;
    }
    line1 = qMax(0, line1);
    line2 = qMax(0, line2);
    const bool move = name.startsWith('m');
    if (move && dest >= line1 && dest < line2) {
      emit statusMessage("E134: Cannot move a range of lines into itself");
      return;
    }
    if (move && (dest == line2 || dest == line1 - 1))
      return;
    const int n = line2 - line1 + 1;
    QString text = textBetween(lineStart(line1), lineEndPos(line2));
    struct MovedMark {
      QChar mark;
      int offset;
      int col;
    };
    QVector<MovedMark> movedMarks;
    if (move) {
      for (auto it = m_marks.constBegin(); it != m_marks.constEnd(); ++it) {
        const ushort m = it.key().unicode();
        if (m < 'a' || m > 'z' || it->isNull() || it->document() != doc())
          continue;
        const int markLine = it->blockNumber();
        if (markLine >= line1 && markLine <= line2)
          movedMarks.append(
              {it.key(), markLine - line1, it->positionInBlock()});
      }
    }
    QTextCursor block(doc());
    block.beginEditBlock();
    auto insertAfter = [&](int after) {
      if (after < 0) {
        replaceRange(0, 0, text + "\n");
        return 0;
      }
      if (after >= lineCount() - 1) {
        replaceRange(docLength(), docLength(), "\n" + text);
      } else {
        replaceRange(lineStart(after + 1), lineStart(after + 1), text + "\n");
      }
      return after + 1;
    };
    int firstNew;
    if (move) {
      if (dest > line2) {
        firstNew = insertAfter(dest);
        if (line2 < lineCount() - 1)
          replaceRange(lineStart(line1), lineStart(line2 + 1), QString());
        else
          replaceRange(lineEndPos(line1 - 1), lineEndPos(line2), QString());
        firstNew -= n;
      } else {
        if (line2 < lineCount() - 1)
          replaceRange(lineStart(line1), lineStart(line2 + 1), QString());
        else
          replaceRange(lineEndPos(line1 - 1), lineEndPos(line2), QString());
        firstNew = insertAfter(dest);
      }
    } else {
      firstNew = insertAfter(dest);
    }
    block.endEditBlock();
    for (const MovedMark &m : movedMarks)
      setMark(m.mark, posOf(firstNew + m.offset, m.col));
    setCursorPos(firstNonBlankPos(firstNew + n - 1));
  } else if (is("join", 1)) {
    bool ok = false;
    const qint64 n = parseCount(args, &ok);
    int first = qMax(0, line1);
    int count;
    if (ok && n > 0) {
      first = clampedLine2;
      count = int(qMin<qint64>(n, lineCount()));
    } else if (hasRange && line2 > line1) {
      count = line2 - line1 + 1;
    } else {
      count = 2;
    }
    joinLines(first, count, !bang);
    setCursorPos(firstNonBlankPos(first));
  } else if (name == ">" || name == "<") {
    int amount = 1;
    QString rest = rawArgs;
    while (rest.startsWith(name[0]) || rest.startsWith(' ')) {
      if (rest[0] == name[0])
        ++amount;
      rest = rest.mid(1);
    }
    countArg(rest.trimmed(), nullptr);
    QTextCursor block(doc());
    block.beginEditBlock();
    operatorShift(lineRange(qMax(0, line1), qMax(0, line2)), name == ">",
                  amount);
    block.endEditBlock();
    setCursorPos(firstNonBlankPos(qMax(0, line2)));
  } else if (is("normal", 4)) {
    exNormal(rawArgs.startsWith(' ') ? rawArgs.mid(1) : rawArgs, line1, line2,
             hasRange);
  } else if (is("substitute", 1) || name == "&" || name == "&&" ||
             name == "~") {
    exSubstitute(rawArgs, qMax(0, line1), qMax(0, line2), hasRange, name);
  } else if (is("global", 1) || is("vglobal", 1)) {
    exGlobal(rawArgs.trimmed(), qMax(0, line1), qMax(0, line2), hasRange,
             bang || name.startsWith('v'));
  } else if (is("sort", 3)) {
    if (!hasRange) {
      line1 = 0;
      line2 = last;
    }
    exSort(args, bang, qMax(0, line1), qMax(0, line2));
  } else if (is("retab", 3)) {
    if (!hasRange) {
      line1 = 0;
      line2 = last;
    }
    bool ok = false;
    int ts = args.toInt(&ok);
    if (ok && ts > 0)
      m_tabStop = ts;
    QTextCursor block(doc());
    block.beginEditBlock();
    for (int l = qMax(0, line1); l <= line2; ++l) {
      const QString text = lineText(l);
      if (!text.contains('\t') && !bang)
        continue;
      QString out;
      int v = 0;
      for (QChar ch : text) {
        if (ch == '\t') {
          int w = m_tabStop - (v % m_tabStop);
          out += m_expandTab ? QString(w, ' ') : QString("\t");
          v += w;
        } else {
          out += ch;
          ++v;
        }
      }
      if (out != text)
        replaceRange(lineStart(l), lineEndPos(l), out);
    }
    block.endEditBlock();
  } else if (name == "k" || is("mark", 2)) {
    if (!args.isEmpty())
      setMark(args[0], lineStart(clampedLine2));
  } else if (name == "=") {
    emit statusMessage(QString::number(hasRange ? line2 + 1 : lineCount()));
  } else if (is("help", 1)) {
    emit statusMessage("Help is not available; see the Vim documentation");
  } else if (name == "!") {
    emit statusMessage("Shell commands are not supported");
  } else if (is("file", 1)) {
    emit statusMessage(
        QString("%1 lines --%2%%--")
            .arg(lineCount())
            .arg((lineOf(cursorPos()) + 1) * 100 / qMax(1, lineCount())));
  } else if (is("print", 1) || name == "#" || is("number", 2)) {
    emit statusMessage(lineText(clampedLine2));
  } else if (is("echo", 2)) {
    emit statusMessage(args);
  } else if (is("silent", 3)) {
    executeEx(args);
  } else if (is("goto", 2)) {
    bool ok = false;
    int n = args.toInt(&ok);
    setCursorPos(clampNormal(ok ? qMax(0, n - 1) : 0));
  } else if (name == "@" || name == "*") {
    QChar reg = args.isEmpty() ? QChar('@') : args[0];
    if (hasRange)
      setCursorPos(clampNormal(
          posOf(clampedLine2, colOf(m_editor->textCursor().position()))));
    if (reg == '@') {
      if (m_lastMacroRegister.isNull()) {
        emit statusMessage("E748: No previously used register");
        return;
      }
      reg = m_lastMacroRegister;
    }
    if (reg == ':') {
      playMacro(reg, 1);
      return;
    }
    if (!isValidRegister(reg)) {
      emit statusMessage(QString("E354: Invalid register name: '%1'").arg(reg));
      return;
    }
    m_lastMacroRegister = reg;
    const QStringList lines = getRegister(reg).content.split('\n');
    for (const QString &line : lines) {
      if (m_abortReplay)
        break;
      if (!line.trimmed().isEmpty())
        executeEx(line);
    }
  } else {
    emit statusMessage(QString("E492: Not an editor command: %1").arg(cmd));
  }
}

QString VimMode::expandReplacement(const QString &replacement,
                                   const QRegularExpressionMatch &match) const {
  QString out;
  QChar oneShot;
  QChar continuous;
  auto append = [&](const QString &s) {
    for (QChar c : s) {
      if (oneShot == 'u') {
        c = c.toUpper();
        oneShot = QChar();
      } else if (oneShot == 'l') {
        c = c.toLower();
        oneShot = QChar();
      } else if (continuous == 'U') {
        c = c.toUpper();
      } else if (continuous == 'L') {
        c = c.toLower();
      }
      out += c;
    }
  };
  for (int i = 0; i < replacement.size(); ++i) {
    QChar c = replacement[i];
    if (c == '\\' && i + 1 < replacement.size()) {
      QChar d = replacement[++i];
      if (d.isDigit()) {
        append(match.captured(d.digitValue()));
      } else if (d == 'n' || d == 'r') {
        out += '\n';
      } else if (d == 't') {
        out += '\t';
      } else if (d == 'u' || d == 'l') {
        oneShot = d;
      } else if (d == 'U' || d == 'L') {
        continuous = d;
      } else if (d == 'e' || d == 'E') {
        continuous = QChar();
        oneShot = QChar();
      } else {
        append(QString(d));
      }
    } else if (c == '&') {
      append(match.captured(0));
    } else {
      append(QString(c));
    }
  }
  return out;
}

void VimMode::exSubstitute(const QString &argsIn, int line1, int line2,
                           bool hasRange, const QString &cmdName) {
  QString args = argsIn;
  while (args.startsWith(' '))
    args = args.mid(1);
  QString pattern;
  QString replacement;
  QString flags;
  QString countText;

  const bool repeat = cmdName == "&" || cmdName == "&&" || cmdName == "~" ||
                      args.isEmpty() || args[0].isLetterOrNumber() ||
                      args[0] == '&' || args[0] == ' ';
  if (repeat) {
    if (!m_hasLastSub) {
      emit statusMessage("E35: No previous regular expression");
      return;
    }
    pattern = cmdName == "~" ? m_searchPattern : m_lastSubPattern;
    replacement = m_lastSubReplacement;
    QString rest = args;
    if (cmdName == "&&" || rest.startsWith('&')) {
      flags = m_lastSubFlags;
      if (rest.startsWith('&'))
        rest = rest.mid(1);
    }
    int k = 0;
    while (k < rest.size() && rest[k].isLetter())
      flags += rest[k++];
    countText = rest.mid(k).trimmed();
  } else {
    const QChar delim = args[0];
    int end = findUnescapedDelim(args, delim, 1);
    pattern = end < 0 ? args.mid(1) : args.mid(1, end - 1);
    QString rest = end < 0 ? QString() : args.mid(end + 1);
    int rend = findUnescapedDelim(rest, delim, 0);
    QString rawRepl = rend < 0 ? rest : rest.left(rend);
    rest = rend < 0 ? QString() : rest.mid(rend + 1);
    if (delim != '/') {
      pattern.replace(QString("\\") + delim, QString(delim));
    }
    for (int k = 0; k < rawRepl.size(); ++k) {
      if (rawRepl[k] == '\\' && k + 1 < rawRepl.size()) {
        if (rawRepl[k + 1] == delim) {
          replacement += delim;
        } else {
          replacement += rawRepl[k];
          replacement += rawRepl[k + 1];
        }
        ++k;
      } else if (rawRepl[k] == '~') {
        replacement += m_lastSubReplacement;
      } else {
        replacement += rawRepl[k];
      }
    }
    int k = 0;
    if (k < rest.size() && rest[k] == '&') {
      flags = m_lastSubFlags;
      ++k;
    }
    while (k < rest.size() && rest[k].isLetter())
      flags += rest[k++];
    countText = rest.mid(k).trimmed();
    if (pattern.isEmpty())
      pattern = m_searchPattern;
    if (pattern.isEmpty()) {
      emit statusMessage("E35: No previous regular expression");
      return;
    }
  }

  m_lastSubPattern = pattern;
  m_lastSubReplacement = replacement;
  m_lastSubFlags = flags;
  m_hasLastSub = true;
  m_searchPattern = pattern;

  bool ok = false;
  const qint64 count = parseCount(countText, &ok);
  if (ok && count > 0) {
    line1 = line2;
    line2 = int(qMin<qint64>(lineCount() - 1, qint64(line1) + count - 1));
  }

  const bool global = flags.count('g') % 2 == 1;
  const bool countOnly = flags.contains('n');
  const bool noError = flags.contains('e');
  QRegularExpression re = compilePattern(pattern);
  if (flags.contains('i') || flags.contains('I')) {
    auto options = re.patternOptions();
    if (flags.lastIndexOf('i') > flags.lastIndexOf('I'))
      options |= QRegularExpression::CaseInsensitiveOption;
    else
      options &= ~QRegularExpression::CaseInsensitiveOption;
    re.setPatternOptions(options);
  }
  if (!re.isValid()) {
    emit statusMessage(QString("E383: Invalid pattern: %1").arg(pattern));
    return;
  }

  const int base = lineStart(line1);
  const int rangeEnd = lineEndPos(line2);
  const int textEnd = line2 < lineCount() - 1 ? lineStart(line2 + 1) : rangeEnd;
  const QString text = textBetween(base, textEnd);

  struct Edit {
    int start;
    int end;
    QString replacement;
  };
  QVector<Edit> edits;
  QSet<int> changedLines;
  int lastLine = -1;
  int newlineCount = 0;
  int scanned = 0;
  const bool multilinePattern =
      pattern.contains("\\n") || pattern.contains("\\_");
  int previousEnd = -1;
  int previousEndLine = -1;
  int nextCol = -1;
  auto it = re.globalMatch(text);
  while (it.hasNext()) {
    auto m = it.next();
    int s = m.capturedStart();
    if (base + s > rangeEnd)
      break;
    newlineCount += text.mid(scanned, s - scanned).count('\n');
    scanned = s;
    int line = line1 + newlineCount;
    const int e = int(m.capturedEnd());
    if (line == previousEndLine) {
      if (s == e && s == previousEnd)
        continue;
      int lineEnd = text.indexOf('\n', s);
      if (lineEnd < 0)
        lineEnd = text.size();
      if (!multilinePattern && nextCol >= lineEnd)
        continue;
    }
    if (!global && changedLines.contains(line))
      continue;
    previousEnd = e;
    previousEndLine = line + text.mid(s, e - s).count('\n');
    nextCol = e > s ? e : e + 1;
    if (!global && m.capturedEnd() > s &&
        text.mid(s, m.capturedLength()).contains('\n')) {
      changedLines.insert(line);
    }
    changedLines.insert(line);
    lastLine = line;
    edits.append({base + s, base + int(m.capturedEnd()),
                  countOnly ? QString() : expandReplacement(replacement, m)});
  }

  if (edits.isEmpty()) {
    if (!noError)
      emit statusMessage(QString("E486: Pattern not found: %1").arg(pattern));
    return;
  }
  if (countOnly) {
    setCursorPos(firstNonBlankPos(line1));
    emit statusMessage(QString("%1 match%2 on %3 line%4")
                           .arg(edits.size())
                           .arg(edits.size() == 1 ? "" : "es")
                           .arg(changedLines.size())
                           .arg(changedLines.size() == 1 ? "" : "s"));
    return;
  }

  const Edit &lastEdit = edits.last();
  const bool multiline =
      lastEdit.replacement.contains('\n') ||
      textBetween(lastEdit.start, lastEdit.end).contains('\n');
  int delta = 0;
  for (int k = 0; k + 1 < edits.size(); ++k)
    delta += edits[k].replacement.size() - (edits[k].end - edits[k].start);
  QTextCursor block(doc());
  block.beginEditBlock();
  for (int k = edits.size() - 1; k >= 0; --k)
    replaceRange(edits[k].start, edits[k].end, edits[k].replacement);
  block.endEditBlock();
  int target = qBound(0, lastEdit.start + delta + lastEdit.replacement.size(),
                      docLength());
  if (!multiline || target == lineStart(lineOf(target)))
    target = firstNonBlankPos(lineOf(target));

  setMark('\'', cursorPos());
  setCursorPos(clampNormal(target));
  recordChangePosition(cursorPos());
  if (changedLines.size() > 2 || edits.size() > 2)
    emit statusMessage(QString("%1 substitution%2 on %3 line%4")
                           .arg(edits.size())
                           .arg(edits.size() == 1 ? "" : "s")
                           .arg(changedLines.size())
                           .arg(changedLines.size() == 1 ? "" : "s"));
  highlightSearch();
}

void VimMode::exGlobal(const QString &args, int line1, int line2, bool hasRange,
                       bool invert) {
  if (!hasRange) {
    line1 = 0;
    line2 = lineCount() - 1;
  }
  if (args.isEmpty()) {
    emit statusMessage("E476: Invalid command");
    return;
  }
  const QChar delim = args[0];
  int end = findUnescapedDelim(args, delim, 1);
  QString pattern = end < 0 ? args.mid(1) : args.mid(1, end - 1);
  QString command = end < 0 ? QString() : args.mid(end + 1).trimmed();
  if (pattern.isEmpty())
    pattern = m_searchPattern;
  if (pattern.isEmpty()) {
    emit statusMessage("E35: No previous regular expression");
    return;
  }
  if (command.isEmpty())
    command = "p";
  m_searchPattern = pattern;
  const QRegularExpression re = compilePattern(pattern);
  if (!re.isValid()) {
    emit statusMessage(QString("E383: Invalid pattern: %1").arg(pattern));
    return;
  }
  QList<QTextCursor> marks;
  for (int l = line1; l <= line2; ++l) {
    if (re.match(lineText(l)).hasMatch() != invert) {
      QTextCursor c(doc());
      c.setPosition(lineStart(l));
      marks.append(c);
    }
  }
  if (marks.isEmpty()) {
    emit statusMessage(QString("Pattern not found: %1").arg(pattern));
    return;
  }
  QTextCursor block(doc());
  block.beginEditBlock();
  const QList<QTextCursor> outerMarks = m_globalMarks;
  const bool outerAbort = m_abortReplay;
  m_globalMarks = marks;
  int previousLine = -1;
  for (int k = 0; k < m_globalMarks.size(); ++k) {
    const QTextCursor mark = m_globalMarks[k];
    if (mark.isNull())
      continue;
    int l = mark.blockNumber();
    if (l == previousLine && mark.position() != lineStart(l))
      continue;
    previousLine = l;
    m_abortReplay = false;
    setCursorPos(lineStart(l));
    executeEx(command);
    if (m_mode != VimEditMode::Normal)
      handleKey("<Esc>", nullptr);
  }
  m_globalMarks = outerMarks;
  m_abortReplay = outerAbort;
  block.endEditBlock();
}

void VimMode::exSort(const QString &args, bool reverse, int line1, int line2) {
  bool numeric = false, ignoreCase = false, unique = false, hex = false,
       useMatch = false;
  QString pattern;
  for (int k = 0; k < args.size(); ++k) {
    QChar c = args[k];
    if (c == 'n')
      numeric = true;
    else if (c == 'i')
      ignoreCase = true;
    else if (c == 'u')
      unique = true;
    else if (c == 'x')
      hex = true;
    else if (c == 'r')
      useMatch = true;
    else if (c == '/') {
      int end = findUnescapedDelim(args, '/', k + 1);
      pattern = end < 0 ? args.mid(k + 1) : args.mid(k + 1, end - k - 1);
      k = end < 0 ? args.size() : end;
    }
  }
  if (line2 <= line1)
    return;
  QStringList lines;
  for (int l = line1; l <= line2; ++l)
    lines << lineText(l);
  QRegularExpression re;
  if (!pattern.isEmpty())
    re = compilePattern(pattern);

  struct Item {
    QString line;
    QString key;
    bool hasNumber;
    double number;
  };
  QVector<Item> items;
  static const QRegularExpression decimal("-?\\d+");
  static const QRegularExpression hexRe("-?(?:0[xX])?[0-9a-fA-F]+");
  for (const QString &line : lines) {
    QString key = line;
    if (!pattern.isEmpty()) {
      auto m = re.match(line);
      if (m.hasMatch())
        key = useMatch ? m.captured() : line.mid(m.capturedEnd());
      else
        key = QString();
    }
    Item item{line, key, false, 0};
    if (numeric || hex) {
      auto m = (hex ? hexRe : decimal).match(key);
      if (m.hasMatch()) {
        bool ok = false;
        QString t = m.captured();
        if (hex) {
          bool neg = t.startsWith('-');
          if (neg)
            t = t.mid(1);
          if (t.startsWith("0x") || t.startsWith("0X"))
            t = t.mid(2);
          item.number = t.toLongLong(&ok, 16) * (neg ? -1 : 1);
        } else {
          item.number = t.toLongLong(&ok);
        }
        item.hasNumber = ok;
      }
    }
    items.append(item);
  }
  auto less = [&](const Item &a, const Item &b) {
    if (numeric || hex) {
      if (a.hasNumber != b.hasNumber)
        return !a.hasNumber;
      return a.number < b.number;
    }
    return QString::compare(a.key, b.key,
                            ignoreCase ? Qt::CaseInsensitive
                                       : Qt::CaseSensitive) < 0;
  };
  std::stable_sort(items.begin(), items.end(), less);
  if (reverse)
    std::reverse(items.begin(), items.end());
  QStringList sorted;
  for (int k = 0; k < items.size(); ++k) {
    if (unique && k > 0) {
      const Item &prev = items[k - 1];
      bool equal = (numeric || hex)
                       ? (prev.hasNumber == items[k].hasNumber &&
                          prev.number == items[k].number)
                       : QString::compare(prev.key, items[k].key,
                                          ignoreCase ? Qt::CaseInsensitive
                                                     : Qt::CaseSensitive) == 0;
      if (equal)
        continue;
    }
    sorted << items[k].line;
  }
  const QString replacement = sorted.join('\n');
  if (replacement != lines.join('\n')) {
    QTextCursor block(doc());
    block.beginEditBlock();
    replaceRange(lineStart(line1), lineEndPos(line2), replacement);
    block.endEditBlock();
  }
  setCursorPos(firstNonBlankPos(line1));
}

void VimMode::exNormal(const QString &args, int line1, int line2,
                       bool hasRange) {
  QStringList tokens;
  for (QChar c : args)
    tokens << QString(c);
  if (tokens.isEmpty())
    return;
  QList<QTextCursor> marks;
  if (hasRange) {
    for (int l = line1; l <= line2; ++l) {
      QTextCursor c(doc());
      c.setPosition(lineStart(l));
      marks.append(c);
    }
  }
  QTextCursor block(doc());
  block.beginEditBlock();
  const bool outerAbort = m_abortReplay;
  auto runOnce = [&]() {
    m_abortReplay = false;
    replayTokens(tokens);
    resetPending();
    if (m_mode == VimEditMode::Command) {
      m_incsearchOrigin = -1;
      handleKey("<Esc>", nullptr);
    }
    if (m_mode != VimEditMode::Normal)
      handleKey("<Esc>", nullptr);
  };
  if (hasRange) {
    for (const QTextCursor &mark : marks) {
      if (mark.isNull())
        continue;
      setCursorPos(lineStart(mark.blockNumber()));
      runOnce();
    }
  } else {
    runOnce();
  }
  m_abortReplay = outerAbort;
  block.endEditBlock();
}

void VimMode::exSet(const QString &args) {
  struct BoolOption {
    QStringList names;
    bool *value;
  };
  QVector<BoolOption> bools = {
      {{"ignorecase", "ic"}, &m_ignoreCase},
      {{"smartcase", "scs"}, &m_smartCase},
      {{"hlsearch", "hls"}, &m_hlSearch},
      {{"incsearch", "is"}, &m_incSearch},
      {{"wrapscan", "ws"}, &m_wrapScan},
      {{"expandtab", "et"}, &m_expandTab},
      {{"autoindent", "ai"}, &m_autoIndent},
      {{"joinspaces", "js"}, &m_joinSpaces},
  };
  struct IntOption {
    QStringList names;
    int *value;
  };
  QVector<IntOption> ints = {
      {{"tabstop", "ts"}, &m_tabStop},
      {{"shiftwidth", "sw"}, &m_shiftWidth},
      {{"textwidth", "tw"}, &m_textWidth},
  };

  const QStringList words = args.split(' ', Qt::SkipEmptyParts);
  if (words.isEmpty()) {
    emit statusMessage(QString("ts=%1 sw=%2 %3expandtab %4ignorecase")
                           .arg(m_tabStop)
                           .arg(m_shiftWidth)
                           .arg(m_expandTab ? "" : "no")
                           .arg(m_ignoreCase ? "" : "no"));
    return;
  }
  for (QString word : words) {
    if (word == "novim" || word == "no-vim") {
      emit commandExecuted("vim:off");
      emit statusMessage("Vim mode disabled");
      continue;
    }
    if (word == "vim") {
      emit commandExecuted("vim:on");
      emit statusMessage("Vim mode enabled");
      continue;
    }
    bool query = word.endsWith('?');
    if (query)
      word.chop(1);
    QString value;
    int eq = word.indexOf(QRegularExpression("[+\\-^]?="));
    QString op;
    if (eq > 0) {
      int eqPos = word.indexOf('=');
      op = word.mid(eq, eqPos - eq);
      value = word.mid(eqPos + 1);
      word = word.left(eq);
    }
    bool handled = false;
    for (const BoolOption &opt : bools) {
      QString base = word;
      bool setTo = true;
      bool toggle = false;
      if (base.endsWith('!')) {
        base.chop(1);
        toggle = true;
      } else if (base.startsWith("no") && opt.names.contains(base.mid(2))) {
        base = base.mid(2);
        setTo = false;
      } else if (base.startsWith("inv") && opt.names.contains(base.mid(3))) {
        base = base.mid(3);
        toggle = true;
      }
      if (!opt.names.contains(base))
        continue;
      handled = true;
      if (query) {
        emit statusMessage(
            QString("%1%2").arg(*opt.value ? "" : "no", opt.names.first()));
      } else {
        *opt.value = toggle ? !*opt.value : setTo;
        if (opt.value == &m_hlSearch) {
          if (m_hlSearch)
            highlightSearch();
          else
            clearSearchHighlight();
        }
      }
      break;
    }
    if (handled)
      continue;
    for (const IntOption &opt : ints) {
      if (!opt.names.contains(word))
        continue;
      handled = true;
      if (query || value.isEmpty()) {
        emit statusMessage(
            QString("%1=%2").arg(opt.names.first()).arg(*opt.value));
      } else {
        bool ok = false;
        int v = value.toInt(&ok);
        if (!ok || v < 0) {
          emit statusMessage(
              QString("E521: Number required after =: %1").arg(word));
        } else if (op == "+") {
          *opt.value += v;
        } else if (op == "-") {
          *opt.value -= v;
        } else if (op == "^") {
          *opt.value *= v;
        } else {
          *opt.value = v;
        }
        if (m_tabStop <= 0)
          m_tabStop = 8;
        if (m_shiftWidth <= 0)
          m_shiftWidth = m_tabStop;
      }
      break;
    }
    if (handled)
      continue;
    if (word == "clipboard" || word == "cb") {
      if (query || eq < 0)
        emit statusMessage(QString("clipboard=%1")
                               .arg(m_clipboardUnnamed ? "unnamedplus" : ""));
      else
        m_clipboardUnnamed = value.contains("unnamed");
      continue;
    }
    if (word == "nrformats" || word == "nf") {
      if (query || eq < 0)
        emit statusMessage(QString("nrformats=%1").arg(m_nrFormats));
      else
        m_nrFormats = value;
      continue;
    }
    static const QStringList passthrough = {"number",
                                            "nu",
                                            "nonumber",
                                            "nonu",
                                            "relativenumber",
                                            "rnu",
                                            "norelativenumber",
                                            "nornu",
                                            "wrap",
                                            "nowrap",
                                            "list",
                                            "nolist"};
    if (passthrough.contains(word)) {
      emit commandExecuted(QString("set:%1").arg(word));
      continue;
    }
    emit statusMessage(QString("E518: Unknown option: %1").arg(word));
  }
}

void VimMode::exRegisters() {
  QStringList parts;
  const QString order = "\"0123456789abcdefghijklmnopqrstuvwxyz-.:/+";
  for (QChar reg : order) {
    VimRegister r = getRegister(reg);
    if (r.content.isEmpty())
      continue;
    QString type = r.blockwise ? "b" : (r.linewise ? "l" : "c");
    parts << QString("%1 \"%2 %3")
                 .arg(type, QString(reg), visibleText(r.content));
  }
  emit statusMessage(parts.isEmpty() ? QString("No registers")
                                     : parts.join("   "));
  emit commandExecuted("showRegisters");
}

void VimMode::exMarks() {
  QStringList parts;
  for (auto it = m_marks.constBegin(); it != m_marks.constEnd(); ++it) {
    int pos = 0;
    if (!markPosition(it.key(), pos))
      continue;
    int line = lineOf(pos);
    parts << QString("%1 %2:%3")
                 .arg(it.key())
                 .arg(line + 1)
                 .arg(pos - lineStart(line));
  }
  emit statusMessage(parts.isEmpty() ? QString("No marks set")
                                     : parts.join("   "));
  emit commandExecuted("showMarks");
}
