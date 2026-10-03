#include "vimmode.h"

#include <QFile>
#include <QProcess>
#include <QTextBlock>

bool VimMode::runShell(const QString &commandIn, const QString &input,
                       QString *output) {
  QString command = commandIn.trimmed();
  QString expanded;
  for (int k = 0; k < command.size(); ++k) {
    if (command[k] == '\\' && k + 1 < command.size() && command[k + 1] == '!') {
      expanded += '!';
      ++k;
    } else if (command[k] == '!' && !m_lastShellCommand.isEmpty()) {
      expanded += m_lastShellCommand;
    } else {
      expanded += command[k];
    }
  }
  command = expanded;
  m_lastShellCommand = command;
  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start("/bin/sh", {"-c", command});
  if (!process.waitForStarted(5000)) {
    emit statusMessage("E282: Cannot start shell");
    return false;
  }
  if (!input.isEmpty())
    process.write(input.toUtf8());
  process.closeWriteChannel();
  if (!process.waitForFinished(10000)) {
    process.kill();
    process.waitForFinished(1000);
    emit statusMessage("E282: Shell command timed out");
  }
  if (output)
    *output = QString::fromUtf8(process.readAllStandardOutput());
  return true;
}

void VimMode::exFilter(const QString &command, int line1, int line2) {
  const QString input = textBetween(lineStart(line1), lineEndPos(line2)) + "\n";
  QString output;
  if (!runShell(command, input, &output))
    return;
  if (output.endsWith('\n'))
    output.chop(1);
  const bool empty = output.isEmpty();
  QTextCursor block(doc());
  block.beginEditBlock();
  if (empty) {
    if (line2 < lineCount() - 1)
      replaceRange(lineStart(line1), lineStart(line2 + 1), QString());
    else if (line1 > 0)
      replaceRange(lineEndPos(line1 - 1), lineEndPos(line2), QString());
    else
      replaceRange(0, lineEndPos(line2), QString());
  } else {
    replaceRange(lineStart(line1), lineEndPos(line2), output);
  }
  block.endEditBlock();
  setMark('\'', cursorPos());
  setCursorPos(firstNonBlankPos(qMin(line1, lineCount() - 1)));
  recordChangePosition(cursorPos());
}

void VimMode::exRead(const QString &args, int line) {
  QString text;
  bool fromShell = false;
  if (args.startsWith('!')) {
    fromShell = true;
    if (!runShell(args.mid(1), QString(), &text))
      return;
  } else {
    if (args.isEmpty()) {
      emit statusMessage("E32: No file name");
      return;
    }
    QFile file(args);
    if (!file.open(QIODevice::ReadOnly)) {
      emit statusMessage(QString("E484: Can't open file %1").arg(args));
      return;
    }
    text = QString::fromUtf8(file.readAll());
  }
  text.replace("\r\n", "\n");
  if (text.endsWith('\n'))
    text.chop(1);
  if (text.isEmpty() && fromShell)
    return;
  const int count = text.count('\n') + 1;
  const int last = lineCount() - 1;
  QTextCursor block(doc());
  block.beginEditBlock();
  int first;
  if (line < 0) {
    replaceRange(0, 0, text + "\n");
    first = 0;
  } else if (line >= last) {
    replaceRange(docLength(), docLength(), "\n" + text);
    first = last + 1;
  } else {
    replaceRange(lineStart(line + 1), lineStart(line + 1), text + "\n");
    first = line + 1;
  }
  block.endEditBlock();
  setMark('[', lineStart(first));
  setMark(']', lineStart(first + count - 1));
  setCursorPos(firstNonBlankPos(fromShell ? first + count - 1 : first));
  recordChangePosition(cursorPos());
}

void VimMode::exAlign(const QString &kind, const QString &args, int line1,
                      int line2) {
  bool ok = false;
  int width = args.trimmed().toInt(&ok);
  if (!ok)
    width = kind == "left" ? 0 : (m_textWidth > 0 ? m_textWidth : 80);
  QTextCursor block(doc());
  block.beginEditBlock();
  for (int l = line1; l <= line2; ++l) {
    const QString text = lineText(l);
    QString trimmedRight = text;
    while (trimmedRight.endsWith(' ') || trimmedRight.endsWith('\t'))
      trimmedRight.chop(1);
    int s = 0;
    while (s < trimmedRight.size() &&
           (trimmedRight[s] == ' ' || trimmedRight[s] == '\t'))
      ++s;
    const QString body = trimmedRight.mid(s);
    QString replacement;
    if (!body.isEmpty()) {
      int indent = 0;
      if (kind == "left") {
        indent = width;
      } else {
        const int len = vcolOf(trimmedRight, trimmedRight.size()) -
                        vcolOf(trimmedRight, s);
        indent = kind == "center" ? (width - len) / 2 : width - len;
        if (indent < 0)
          indent = 0;
      }
      replacement = indentString(indent) + body;
    }
    if (replacement != text)
      replaceRange(lineStart(l), lineEndPos(l), replacement);
  }
  block.endEditBlock();
  setCursorPos(firstNonBlankPos(line1));
}

void VimMode::exDelmarks(const QString &args, bool bang) {
  if (bang) {
    for (char c = 'a'; c <= 'z'; ++c)
      m_marks.remove(QChar(c));
    return;
  }
  for (int k = 0; k < args.size(); ++k) {
    const QChar c = args[k];
    if (c == ' ')
      continue;
    if (k + 2 < args.size() && args[k + 1] == '-') {
      const QChar e = args[k + 2];
      for (ushort u = c.unicode(); u <= e.unicode(); ++u)
        m_marks.remove(QChar(u));
      k += 2;
    } else {
      m_marks.remove(c);
    }
  }
}

void VimMode::exLet(const QString &args) {
  static const QRegularExpression assign(
      "^(@.|[A-Za-z_][A-Za-z0-9_:]*)\\s*(\\.=|\\+=|-=|=)\\s*(.*)$",
      QRegularExpression::DotMatchesEverythingOption);
  const auto m = assign.match(args);
  if (!m.hasMatch()) {
    emit statusMessage(QString("E15: Invalid expression: \"%1\"").arg(args));
    return;
  }
  const QString target = m.captured(1);
  const QString op = m.captured(2);
  QString value, error;
  bool isList = false;
  if (!evalExpression(m.captured(3), &value, &isList, nullptr, &error)) {
    emit statusMessage(error);
    return;
  }
  if (target.startsWith('@')) {
    const QChar reg = target[1];
    if (!isValidRegister(reg)) {
      emit statusMessage(QString("E354: Invalid register name: '%1'").arg(reg));
      return;
    }
    if (isList)
      value += "\n";
    if (reg == '/') {
      m_searchPattern = op == ".=" ? m_searchPattern + value : value;
      return;
    }
    if (reg == ':' || reg == '.' || reg == '%')
      return;
    QString text = value;
    if (op == ".=")
      text = getRegister(reg).content + value;
    const bool linewise = text.endsWith('\n') || isList;
    setRegister(reg, text,
                linewise ? VimRegisterType::Linewise
                         : VimRegisterType::Charwise);
    if (reg != '"' && reg != '_')
      m_registers['"'] = getRegister(reg);
    return;
  }
  const QString key = target.startsWith("g:") ? target.mid(2) : target;
  QString repr;
  if (op == "=") {
    evalExpressionRepr(m.captured(3), &repr, nullptr, false);
  } else {
    const QString expr = key + (op == ".=" ? " . " : op == "+=" ? " + " : " - ") +
                         "(" + m.captured(3) + ")";
    evalExpressionRepr(expr, &repr, nullptr, false);
  }
  m_variables[key] = repr;
}

void VimMode::autoWrapForInsert(QChar typed) {
  Q_UNUSED(typed);
  for (int guard = 0; guard < 10000; ++guard) {
    const int pos = m_editor->textCursor().position();
    const int line = lineOf(pos);
    const QString text = lineText(line);
    const int col = pos - lineStart(line);
    if (vcolOf(text, col) + 1 <= m_textWidth)
      return;
    const int wantCol = colForVcol(text, m_textWidth);
    int foundCol = 0;
    int endFound = 0;
    for (int i = qMin(col, int(text.size())) - 1; i >= 0; --i) {
      if (text[i] != ' ' && text[i] != '\t')
        continue;
      const int endCol = i;
      int j = i;
      while (j > 0 && (text[j] == ' ' || text[j] == '\t'))
        --j;
      if (j == 0 && (text[0] == ' ' || text[0] == '\t'))
        break;
      foundCol = j + 1;
      endFound = endCol + 1;
      i = j;
      if (foundCol <= wantCol)
        break;
    }
    if (foundCol == 0)
      return;
    QString indent;
    if (m_autoIndent)
      indent = text.left(firstNonBlankCol(line));
    const QString head = text.left(foundCol);
    const QString tail = text.mid(endFound);
    QTextCursor c = m_editor->textCursor();
    if (m_insertEditOpen) {
      c.joinPreviousEditBlock();
    } else {
      c.beginEditBlock();
      m_insertEditOpen = true;
    }
    c.setPosition(lineStart(line));
    c.setPosition(lineEndPos(line), QTextCursor::KeepAnchor);
    c.insertText(head + "\n" + indent + tail);
    c.endEditBlock();
    const int newCol = qMax(0, col - endFound) + int(indent.size());
    QTextCursor placed = m_editor->textCursor();
    placed.setPosition(lineStart(line + 1) +
                       qMin(newCol, int(indent.size() + tail.size())));
    m_editor->setTextCursor(placed);
  }
}
