#include "vimmode.h"

#include <QSet>
#include <QTextBlock>

namespace {

const QSet<QString> &motionKeys() {
  static const QSet<QString> keys = {
      "h",         "j",        "k",        "l",       "<Left>",    "<Right>",
      "<Up>",      "<Down>",   "<BS>",     "<C-h>",   " ",         "w",
      "W",         "b",        "B",        "e",       "E",         "0",
      "^",         "$",        "|",        "G",       "%",         "{",
      "}",         "(",        ")",        ";",       ",",         "H",
      "M",         "L",        "n",        "N",       "*",         "#",
      "+",         "-",        "_",        "<CR>",    "<C-m>",     "<C-j>",
      "<C-n>",     "<C-p>",    "<Home>",   "<End>",   "<S-Right>", "<S-Left>",
      "<C-Right>", "<C-Left>", "<C-Home>", "<C-End>", "gg",        "ge",
      "gE",        "g_",       "g0",       "g^",      "g$",        "gm",
      "gj",        "gk",       "g*",       "g#",      "go",        "[(",
      "[{",        "])",       "]}",       "[[",      "]]",        "[]",
      "][",        "/",        "?"};
  return keys;
}

const QSet<QString> &normalCommands() {
  static const QSet<QString> keys = {
      "i",          "a",        "I",          "A",          "o",
      "O",          "gi",       "gI",         "v",          "V",
      "<C-v>",      "<C-q>",    "gv",         ":",          "x",
      "X",          "<Del>",    "D",          "C",          "s",
      "S",          "Y",        "p",          "P",          "gp",
      "gP",         "]p",       "[p",         "J",          "gJ",
      "R",          "~",        "u",          "U",          "<C-r>",
      ".",          "<C-a>",    "<C-x>",      "ZZ",         "ZQ",
      "zz",         "zt",       "zb",         "z<CR>",      "z.",
      "z-",         "zo",       "zO",         "zc",         "zC",
      "za",         "zA",       "zR",         "zM",         "zv",
      "<C-e>",      "<C-y>",    "<C-d>",      "<C-u>",      "<C-f>",
      "<C-b>",      "<PageUp>", "<PageDown>", "<S-Up>",     "<S-Down>",
      "<C-o>",      "<C-i>",    "<Tab>",      "<C-g>",      "ga",
      "&",          "g&",       "gt",         "gT",         "gd",
      "gD",         "g;",       "g,",         "<C-l>",      "<Insert>",
      "<C-w>s",     "<C-w>S",   "<C-w>v",     "<C-w>w",     "<C-w>W",
      "<C-w>h",     "<C-w>j",   "<C-w>k",     "<C-w>l",     "<C-w>p",
      "<C-w>q",     "<C-w>c",   "<C-w>o",     "<C-w><C-w>", "<C-w><C-s>",
      "<C-w><C-v>", "K",        "gn",         "gN",         "gf",
      "<C-]>"};
  return keys;
}

const QSet<QString> &visualCommands() {
  static const QSet<QString> keys = {
      "o",      "O",      "v",     "V",  "<C-v>", "<C-q>", ":",     "gv",
      "d",      "x",      "<Del>", "X",  "D",     "y",     "Y",     "c",
      "s",      "C",      "S",     "R",  "J",     "gJ",    ">",     "<",
      "=",      "~",      "u",     "U",  "g~",    "gu",    "gU",    "g?",
      "gq",     "gw",     "p",     "P",  "I",     "A",     "<C-a>", "<C-x>",
      "g<C-a>", "g<C-x>", "<C-c>", "zf", "<C-e>", "<C-y>", "<C-d>", "<C-u>",
      "<C-f>",  "<C-b>",  "zz",    "zt", "zb",    "gn",    "gN"};
  return keys;
}

bool isDigitToken(const QString &t, bool allowZero) {
  return t.size() == 1 && t[0].isDigit() && (allowZero || t != "0");
}

} // namespace

bool VimMode::isMotionKey(const QString &key) {
  return motionKeys().contains(key);
}

bool VimMode::isMotionCommand(const QString &key) {
  return motionKeys().contains(key) || key == "f" || key == "F" || key == "t" ||
         key == "T" || key == "'" || key == "`" || key == "g'" || key == "g`";
}

bool VimMode::isOperatorKey(const QString &key) {
  static const QSet<QString> ops = {"d",  "c",  "y",  "<",  ">",  "=",
                                    "g~", "gu", "gU", "g?", "gq", "gw"};
  return ops.contains(key);
}

bool VimMode::isTextObjectChar(const QString &key) {
  return key.size() == 1 && QString("wWsp()b[]{}B<>t\"'`").contains(key[0]);
}

bool VimMode::tokenToArg(const QString &token, QChar &arg, bool allowNewline) {
  if (token.size() == 1) {
    arg = token[0];
    return true;
  }
  if (token == "<Tab>") {
    arg = '\t';
    return true;
  }
  if (allowNewline && token == "<CR>") {
    arg = '\n';
    return true;
  }
  return false;
}

VimMode::Parse VimMode::parseMotionKey(const QStringList &keys, int &i,
                                       QString &key, QChar &arg) const {
  const int n = keys.size();
  if (i >= n)
    return Parse::Incomplete;
  const QString t = keys[i];
  if (t == "g" || t == "[" || t == "]") {
    if (i + 1 >= n)
      return Parse::Incomplete;
    key = t + keys[i + 1];
    i += 2;
    if (key == "g'" || key == "g`") {
      if (i >= n)
        return Parse::Incomplete;
      if (!tokenToArg(keys[i], arg, false))
        return Parse::Invalid;
      ++i;
      return Parse::Complete;
    }
    return isMotionKey(key) ? Parse::Complete : Parse::Invalid;
  }
  if (t == "f" || t == "F" || t == "t" || t == "T" || t == "'" || t == "`") {
    if (i + 1 >= n)
      return Parse::Incomplete;
    if (!tokenToArg(keys[i + 1], arg, false))
      return Parse::Invalid;
    key = t;
    i += 2;
    return Parse::Complete;
  }
  if (isMotionKey(t)) {
    key = t;
    ++i;
    return Parse::Complete;
  }
  return Parse::Invalid;
}

VimMode::Parse VimMode::parseCommand(const QStringList &keys, NormalCmd &cmd,
                                     bool visual) const {
  const int n = keys.size();
  int i = 0;
  if (keys[0] == "\"") {
    if (n < 2)
      return Parse::Incomplete;
    if (keys[1].size() != 1 || !isValidRegister(keys[1][0]))
      return Parse::Invalid;
    cmd.reg = keys[1][0];
    i = 2;
  }
  int count1 = 0;
  while (i < n && isDigitToken(keys[i], count1 > 0)) {
    count1 = qMin(99999999, count1 * 10 + keys[i][0].digitValue());
    ++i;
  }
  if (i >= n)
    return Parse::Incomplete;
  if (i > 0 && keys[i] == "\"" && cmd.reg.isNull()) {
    if (i + 1 >= n)
      return Parse::Incomplete;
    if (keys[i + 1].size() != 1 || !isValidRegister(keys[i + 1][0]))
      return Parse::Invalid;
    cmd.reg = keys[i + 1][0];
    i += 2;
    while (i < n && isDigitToken(keys[i], count1 > 0)) {
      count1 = qMin(99999999, count1 * 10 + keys[i][0].digitValue());
      ++i;
    }
    if (i >= n)
      return Parse::Incomplete;
  }
  cmd.count = count1;

  const int keyIndex = i;
  const QString t = keys[i];

  if (visual && (t == "i" || t == "a")) {
    if (i + 1 >= n)
      return Parse::Incomplete;
    if (!isTextObjectChar(keys[i + 1]))
      return Parse::Invalid;
    cmd.textObject = t + keys[i + 1];
    return i + 2 == n ? Parse::Complete : Parse::Invalid;
  }

  QString key = t;
  if (t == "g" || t == "z" || t == "Z" || t == "[" || t == "]" ||
      t == "<C-w>") {
    if (i + 1 >= n)
      return Parse::Incomplete;
    key = t + keys[i + 1];
    i += 2;
  } else {
    ++i;
  }

  if (!visual && isOperatorKey(key)) {
    cmd.op = key;
    int count2 = 0;
    while (i < n && isDigitToken(keys[i], count2 > 0)) {
      count2 = qMin(99999999, count2 * 10 + keys[i][0].digitValue());
      ++i;
    }
    if (i >= n)
      return Parse::Incomplete;
    if (count1 > 0 || count2 > 0)
      cmd.count = qMax(1, count1) * qMax(1, count2);
    QString t2 = keys[i];
    if (t2 == "v" || t2 == "V" || t2 == "<C-v>") {
      cmd.force = t2 == "<C-v>" ? QChar(0x16) : t2[0];
      ++i;
      if (i >= n)
        return Parse::Incomplete;
      t2 = keys[i];
    }
    if (t2 == "<Esc>")
      return Parse::Invalid;
    if (key.size() == 1 && t2 == key) {
      cmd.doubledOp = true;
      return i + 1 == n ? Parse::Complete : Parse::Invalid;
    }
    if (key.size() == 2) {
      if (t2 == key.mid(1)) {
        cmd.doubledOp = true;
        return i + 1 == n ? Parse::Complete : Parse::Invalid;
      }
      if (t2 == "g") {
        if (i + 1 >= n)
          return Parse::Incomplete;
        if (keys[i + 1] == key.mid(1)) {
          cmd.doubledOp = true;
          return i + 2 == n ? Parse::Complete : Parse::Invalid;
        }
      }
    }
    if (t2 == "g" && i + 1 < n && (keys[i + 1] == "n" || keys[i + 1] == "N")) {
      cmd.textObject = "g" + keys[i + 1];
      return i + 2 == n ? Parse::Complete : Parse::Invalid;
    }
    if (t2 == "i" || t2 == "a") {
      if (i + 1 >= n)
        return Parse::Incomplete;
      if (!isTextObjectChar(keys[i + 1]))
        return Parse::Invalid;
      cmd.textObject = t2 + keys[i + 1];
      return i + 2 == n ? Parse::Complete : Parse::Invalid;
    }
    Parse p = parseMotionKey(keys, i, cmd.key, cmd.arg);
    if (p == Parse::Complete && i != n)
      return Parse::Invalid;
    return p;
  }

  int mi = keyIndex;
  if (isMotionCommand(t) || (key.size() == 2 && isMotionCommand(key))) {
    QString motion;
    QChar arg;
    Parse p = parseMotionKey(keys, mi, motion, arg);
    if (p == Parse::Complete) {
      cmd.key = motion;
      cmd.arg = arg;
      return mi == n ? Parse::Complete : Parse::Invalid;
    }
    if (p == Parse::Incomplete)
      return p;
  }

  cmd.key = key;
  if (key == "r" || key == "m" || key == "@" ||
      (key == "q" && !m_macroRecording && !visual)) {
    if (visual && key != "r")
      return Parse::Invalid;
    if (i >= n)
      return Parse::Incomplete;
    if (!tokenToArg(keys[i], cmd.arg, key == "r"))
      return Parse::Invalid;
    return i + 1 == n ? Parse::Complete : Parse::Invalid;
  }
  if (key == "q" && m_macroRecording && !visual)
    return i == n ? Parse::Complete : Parse::Invalid;

  const QSet<QString> &commands = visual ? visualCommands() : normalCommands();
  if (commands.contains(key))
    return i == n ? Parse::Complete : Parse::Invalid;
  return Parse::Invalid;
}

bool VimMode::handleNormalKey(const QString &token) {
  if (m_pending.isEmpty()) {
    QTextCursor c = m_editor->textCursor();
    if (c.hasSelection()) {
      int a = c.anchor();
      int p = c.position();
      startVisual(VimEditMode::Visual, a > p ? a - 1 : a, p > a ? p - 1 : p);
      return handleVisualKey(token);
    }
  }

  if (token == "<Esc>" || token == "<C-c>") {
    if (!m_pending.isEmpty()) {
      resetPending();
    } else if (m_insertOneCommand) {
      m_insertOneCommand = false;
      setMode(VimEditMode::Insert);
    }
    return true;
  }

  m_pending << token;
  NormalCmd cmd;
  Parse p = parseCommand(m_pending, cmd, false);
  if (p == Parse::Incomplete) {
    updatePendingKeys();
    return true;
  }
  const QStringList keys = m_pending;
  resetPending();
  if (p == Parse::Invalid) {
    if (keys.size() == 1 && token.startsWith("<") && token.size() > 3 &&
        token != "<lt>")
      return false;
    return true;
  }

  m_undoCursors[doc()->availableUndoSteps()] = cursorPos();
  executeNormal(cmd, keys);

  if (m_insertOneCommand && m_mode == VimEditMode::Normal) {
    m_insertOneCommand = false;
    setMode(VimEditMode::Insert);
    return true;
  }
  if (m_mode == VimEditMode::Normal) {
    QTextCursor c = m_editor->textCursor();
    int clamped = clampNormal(c.position());
    if (c.hasSelection() || clamped != c.position()) {
      c.setPosition(clamped);
      m_editor->setTextCursor(c);
    }
  }
  return true;
}

void VimMode::executeNormal(const NormalCmd &cmdIn, const QStringList &keys) {
  NormalCmd cmd = cmdIn;
  if (m_dotCountOverride > 0) {
    cmd.count = m_dotCountOverride;
    m_dotCountOverride = 0;
  }

  if (!cmd.op.isEmpty()) {
    executeOperatorMotion(cmd, keys);
    return;
  }

  if (isMotionCommand(cmd.key)) {
    if (cmd.key == "/" || cmd.key == "?") {
      m_cmdOperatorKeys.clear();
      m_cmdCount = cmd.count;
      enterCommandLine(cmd.key[0], QString());
      return;
    }
    const int from = cursorPos();
    MotionResult r =
        evalMotion(cmd.key, cmd.arg, cmd.count, false, from, false);
    if (!r.ok)
      return;
    if (r.jump)
      pushJump(from);
    if (m_insertOneCommand && r.wantEol) {
      setCursorPos(lineEndPos(lineOf(r.pos)), true);
      m_wantEol = true;
      return;
    }
    setCursorPos(clampNormal(r.pos), !r.keepWantCol);
    if (r.wantEol)
      m_wantEol = true;
    return;
  }

  executeSimpleCommand(cmd, keys);
}

void VimMode::executeOperatorMotion(const NormalCmd &cmd,
                                    const QStringList &keys) {
  const int from = cursorPos();
  const int count = cmd.count;
  Range range;
  m_opForceNumbered = false;

  if (cmd.doubledOp) {
    const int line = lineOf(from);
    const int last = lineCount() - 1;
    int endLine = line + qMax(1, count) - 1;
    if (endLine > last) {
      if (line == last && count > 1)
        return;
      endLine = last;
    }
    range = lineRange(line, endLine);
    if (cmd.op == "y" || cmd.op == "d" || cmd.op == "<" || cmd.op == ">" ||
        cmd.op == "c")
      m_opCursor = from;
    else
      m_opCursor = qMin(from, lineStart(endLine) + firstNonBlankCol(endLine));
  } else if (!cmd.textObject.isEmpty()) {
    int s = 0, e = 0;
    MotionType type = MotionType::Exclusive;
    if (!evalTextObject(cmd.textObject, count, s, e, type, false))
      return;
    range = motionRange(s, e, type);
    m_opCursor = s;
  } else {
    if (cmd.key == "/" || cmd.key == "?") {
      m_cmdOperatorKeys = keys.mid(0, keys.size() - 1);
      m_cmdCount = count;
      enterCommandLine(cmd.key[0], QString());
      return;
    }
    MotionResult r;
    const QChar under = charAt(from);
    if (cmd.op == "c" && (cmd.key == "w" || cmd.key == "W") &&
        !under.isNull() && under != '\n' && under != ' ' && under != '\t') {
      TextPos p{lineOf(from), colOf(from)};
      endWord(p, qMax(1, count), cmd.key == "W", true, false);
      r.ok = true;
      r.pos = lineStart(p.line) + qMin(p.col, lineLength(p.line));
      r.type = MotionType::Inclusive;
    } else {
      r = evalMotion(cmd.key, cmd.arg, count, true, from, false);
    }
    if (!r.ok)
      return;
    MotionType type = r.type;
    if (cmd.force == 'v') {
      if (type == MotionType::Linewise || type == MotionType::Inclusive)
        type = MotionType::Exclusive;
      else
        type = MotionType::Inclusive;
      range = type == MotionType::Inclusive
                  ? charRange(qMin(from, r.pos), qMax(from, r.pos) + 1)
                  : charRange(from, r.pos);
    } else if (cmd.force == 'V') {
      range = lineRange(lineOf(from), lineOf(r.pos));
    } else if (cmd.force == QChar(0x16)) {
      range = blockRange(from, r.pos, false);
    } else {
      range = motionRange(from, r.pos, type);
    }
    static const QSet<QString> numbered = {"%", "(", ")", "`", "/",
                                           "?", "n", "N", "{", "}"};
    m_opForceNumbered = numbered.contains(cmd.key);
    if (cmd.key == "<BS>" && lineOf(r.pos) < lineOf(from) &&
        (cmd.op == "d" || cmd.op == "c") && lineLength(lineOf(r.pos)) > 0)
      range = charRange(lineEndPos(lineOf(r.pos)), from);
    if (r.jump)
      setMark('\'', from);
    m_opCursor = (cmd.key == "_" && cmd.op == "y") ? from : qMin(from, r.pos);
  }

  if (cmd.op == "d" && cmd.force.isNull() &&
      range.type == VimRegisterType::Charwise &&
      range.endLine > range.startLine) {
    const QString rest =
        lineText(range.endLine).mid(range.end - lineStart(range.endLine));
    if (rest.trimmed().isEmpty() && inIndent(range.start))
      range = lineRange(range.startLine, range.endLine);
  }

  const int revision = doc()->revision();
  applyOperator(cmd.op, range, cmd.reg, count, false);
  m_opForceNumbered = false;
  m_opCursor = -1;

  if (cmd.op == "c") {
    m_insertEditOpen = doc()->revision() != revision;
    startInsertSession(1, "c", keys);
  } else if (cmd.op != "y") {
    setDotCommand(keys);
  }
}

bool VimMode::executeSimpleCommand(const NormalCmd &cmd,
                                   const QStringList &keys) {
  const QString &key = cmd.key;
  const int count = cmd.count;
  const int c1 = qMax(1, count);
  const int pos = cursorPos();
  const int line = lineOf(pos);

  auto asOperator = [&](const QString &op, const QString &motion,
                        bool doubled) {
    NormalCmd c = cmd;
    c.op = op;
    c.key = motion;
    c.doubledOp = doubled;
    executeOperatorMotion(c, keys);
  };

  if (key == "i" || key == "a" || key == "I" || key == "A" || key == "gI" ||
      key == "gi" || key == "o" || key == "O" || key == "<Insert>") {
    enterInsert(key == "<Insert>" ? "i" : key, c1, keys);
  } else if (key == "v") {
    startVisual(VimEditMode::Visual, pos, pos);
  } else if (key == "V") {
    startVisual(VimEditMode::VisualLine, pos, pos);
  } else if (key == "<C-v>" || key == "<C-q>") {
    startVisual(VimEditMode::VisualBlock, pos, pos);
  } else if (key == "gv") {
    int s = 0, e = 0;
    if (!markPosition('<', s) || !markPosition('>', e))
      return true;
    VimEditMode mode = m_lastVisualMode;
    bool toEol = m_lastVisualToEol;
    if (mode == VimEditMode::VisualLine)
      e = lineStart(lineOf(e));
    startVisual(mode, s, e);
    m_visualToEol = toEol;
    updateVisualSelection();
  } else if (key == ":") {
    QString initial;
    if (count == 1)
      initial = ".";
    else if (count > 1)
      initial = QString(".,.+%1").arg(count - 1);
    enterCommandLine(':', initial);
  } else if (key == "x" || key == "<Del>") {
    if (lineLength(line) > 0)
      asOperator("d", "l", false);
  } else if (key == "X") {
    if (colOf(pos) > 0)
      asOperator("d", "h", false);
  } else if (key == "D") {
    asOperator("d", "$", false);
  } else if (key == "C") {
    asOperator("c", "$", false);
  } else if (key == "s") {
    if (lineLength(line) == 0)
      enterInsert("i", 1, keys);
    else
      asOperator("c", "l", false);
  } else if (key == "S") {
    asOperator("c", QString(), true);
  } else if (key == "Y") {
    asOperator("y", QString(), true);
  } else if (key == "p" || key == "P" || key == "gp" || key == "gP" ||
             key == "]p" || key == "[p") {
    bool after = key == "p" || key == "gp" || key == "]p";
    put(cmd.reg, c1, after, key.startsWith('g'),
        key.startsWith('[') || key.startsWith(']'));
    setDotCommand(keys);
  } else if (key == "J" || key == "gJ") {
    joinLines(line, c1, key == "J");
    setDotCommand(keys);
  } else if (key == "r") {
    replaceChars(cmd.arg, c1);
    setDotCommand(keys);
  } else if (key == "R") {
    m_replacedChars.clear();
    startInsertSession(c1, "R", keys);
  } else if (key == "~") {
    toggleCaseChars(c1);
    setDotCommand(keys);
  } else if (key == "u" || key == "U") {
    undo(c1);
  } else if (key == "<C-r>") {
    redo(c1);
  } else if (key == ".") {
    repeatLastChange(count);
  } else if (key == "q") {
    if (m_macroRecording) {
      stopMacroRecording();
    } else if (cmd.arg.unicode() < 128 &&
               (cmd.arg.isLetterOrNumber() || cmd.arg == '"')) {
      startMacroRecording(cmd.arg);
    }
  } else if (key == "@") {
    playMacro(cmd.arg, c1);
  } else if (key == "m") {
    if ((cmd.arg.unicode() < 128 && cmd.arg.isLetter()) ||
        QString("'`[]<>").contains(cmd.arg))
      setMark(cmd.arg, pos);
  } else if (key == "<C-a>" || key == "<C-x>") {
    QTextCursor block(doc());
    block.beginEditBlock();
    int result = 0;
    bool ok = incrementNumber(line, colOf(pos), key == "<C-a>" ? c1 : -c1, -1,
                              &result);
    block.endEditBlock();
    if (ok) {
      setCursorPos(result);
      recordChangePosition(result);
      setDotCommand(keys);
    }
  } else if (key == "ZZ") {
    emit commandExecuted("save");
    emit commandExecuted("quit");
  } else if (key == "ZQ") {
    emit commandExecuted("closeWithoutSaving");
  } else if (key == "zz" || key == "z.") {
    scrollCursorTo(1, key == "z.");
  } else if (key == "zt" || key == "z<CR>") {
    scrollCursorTo(0, key == "z<CR>");
  } else if (key == "zb" || key == "z-") {
    scrollCursorTo(2, key == "z-");
  } else if (key == "zo" || key == "zO" || key == "zv") {
    emit commandExecuted("unfold");
  } else if (key == "zc" || key == "zC") {
    emit commandExecuted("fold");
  } else if (key == "za" || key == "zA") {
    emit commandExecuted("toggleFold");
  } else if (key == "zR") {
    emit commandExecuted("unfoldAll");
  } else if (key == "zM") {
    emit commandExecuted("foldAll");
  } else if (key == "<C-e>") {
    scrollLines(c1, false);
  } else if (key == "<C-y>") {
    scrollLines(-c1, false);
  } else if (key == "<C-d>") {
    scrollHalfPage(true, count);
  } else if (key == "<C-u>") {
    scrollHalfPage(false, count);
  } else if (key == "<C-f>" || key == "<PageDown>" || key == "<S-Down>") {
    scrollPage(true, c1);
  } else if (key == "<C-b>" || key == "<PageUp>" || key == "<S-Up>") {
    scrollPage(false, c1);
  } else if (key == "<C-o>") {
    jumpOlder(c1);
  } else if (key == "<C-i>" || key == "<Tab>") {
    jumpNewer(c1);
  } else if (key == "g;" || key == "g,") {
    if (m_changeList.isEmpty()) {
      emit statusMessage("E664: Change list is empty");
      return true;
    }
    int target = m_changeIndex + (key == "g;" ? -c1 : c1);
    if (target < 0 || target >= m_changeList.size()) {
      emit statusMessage(key == "g;" ? "E662: At start of changelist"
                                     : "E663: At end of changelist");
      return true;
    }
    m_changeIndex = target;
    setCursorPos(clampNormal(m_changeList[target].position()));
  } else if (key == "<C-g>") {
    int lines = lineCount();
    emit statusMessage(QString("%1 line%2 --%3%%--")
                           .arg(lines)
                           .arg(lines == 1 ? "" : "s")
                           .arg((line + 1) * 100 / qMax(1, lines)));
  } else if (key == "ga") {
    QChar ch = charAt(pos);
    if (ch.isNull() || ch == '\n') {
      emit statusMessage("NUL");
    } else {
      // QChar::unicode() is char16_t, which Qt 6.9 no longer accepts as an
      // integral arg(); it would otherwise be formatted as a character.
      const int code = static_cast<int>(ch.unicode());
      emit statusMessage(QString("<%1> %2, Hex %3, Oct %4")
                             .arg(ch)
                             .arg(code)
                             .arg(code, 2, 16, QChar('0'))
                             .arg(code, 3, 8, QChar('0')));
    }
  } else if (key == "&") {
    executeEx("s");
    setDotCommand(keys);
  } else if (key == "g&") {
    executeEx("%s//~/&");
  } else if (key == "gt") {
    emit commandExecuted("nextTab");
  } else if (key == "gT") {
    emit commandExecuted("prevTab");
  } else if (key == "gf") {
    const QString text = lineText(line);
    int s = colOf(pos), e = s;
    auto pathChar = [](QChar c) {
      return c.isLetterOrNumber() || QString("/._-~+:\\").contains(c);
    };
    while (s > 0 && pathChar(text[s - 1]))
      --s;
    while (e < text.size() && pathChar(text[e]))
      ++e;
    const QString path = text.mid(s, e - s);
    if (path.isEmpty())
      emit statusMessage("E446: No file name under cursor");
    else
      emit commandExecuted(QString("edit:%1").arg(path));
  } else if (key == "gd" || key == "gD" || key == "<C-]>") {
    emit commandExecuted("goToDefinition");
  } else if (key == "gn" || key == "gN") {
    int s = 0, e = 0;
    if (selectSearchMatch(key == "gn", s, e)) {
      if (key == "gn")
        startVisual(VimEditMode::Visual, s, e);
      else
        startVisual(VimEditMode::Visual, e, s);
    }
  } else if (key == "K") {
    emit commandExecuted("hover");
  } else if (key == "<C-w>s" || key == "<C-w>S" || key == "<C-w><C-s>") {
    emit commandExecuted("splitHorizontal");
  } else if (key == "<C-w>v" || key == "<C-w><C-v>") {
    emit commandExecuted("splitVertical");
  } else if (key == "<C-w>w" || key == "<C-w>l" || key == "<C-w>j" ||
             key == "<C-w><C-w>") {
    emit commandExecuted("focusNextSplit");
  } else if (key == "<C-w>W" || key == "<C-w>h" || key == "<C-w>k" ||
             key == "<C-w>p") {
    emit commandExecuted("focusPrevSplit");
  } else if (key == "<C-w>q" || key == "<C-w>c") {
    emit commandExecuted("closeSplit");
  } else if (key == "<C-w>o") {
    emit commandExecuted("unsplitAll");
  } else if (key == "<C-l>") {
    m_editor->viewport()->update();
  } else {
    return false;
  }
  return true;
}
