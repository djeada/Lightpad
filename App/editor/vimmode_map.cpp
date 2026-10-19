#include "vimmode.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTimer>

namespace {

struct MapEntry {
  QStringList lhs;
  QString rhs;
  bool noremap = false;
  bool expr = false;
  bool nowait = false;
};

using MapTable = QMap<QString, MapEntry>;

QHash<QChar, MapTable> &mapTables() {
  static QHash<QChar, MapTable> tables;
  return tables;
}

QHash<QChar, MapTable> &abbrevTables() {
  static QHash<QChar, MapTable> tables;
  return tables;
}

constexpr int kMaxMapDepth = 1000;
constexpr int kMaxSourceDepth = 20;

QString tableKey(const QStringList &tokens) { return tokens.join(QChar(0x1f)); }

bool isKeywordChar(QChar c) { return c.isLetterOrNumber() || c == '_'; }

bool isInsertNavigation(const QString &token) {
  static const QStringList keys = {
      "<Left>",   "<Right>",    "<Up>",     "<Down>",    "<Home>",   "<End>",
      "<PageUp>", "<PageDown>", "<C-Left>", "<C-Right>", "<C-Home>", "<C-End>"};
  return keys.contains(token) ||
         (token.startsWith("<S-") && token != "<S-Tab>");
}

struct MapCommand {
  QString modes;
  enum Kind { Map, Unmap, Clear } kind = Map;
  bool noremap = false;
  bool abbrev = false;
};

// Vim's map command names with their shortest abbreviations.
bool lookupMapCommand(const QString &name, bool bang, MapCommand &out) {
  struct Spec {
    const char *full;
    int minLen;
    const char *modes;
    MapCommand::Kind kind;
    bool noremap;
    bool abbrev;
  };
  static const Spec specs[] = {
      {"map", 3, "nxo", MapCommand::Map, false, false},
      {"nmap", 2, "n", MapCommand::Map, false, false},
      {"vmap", 2, "x", MapCommand::Map, false, false},
      {"xmap", 2, "x", MapCommand::Map, false, false},
      {"smap", 4, "", MapCommand::Map, false, false},
      {"omap", 2, "o", MapCommand::Map, false, false},
      {"imap", 2, "i", MapCommand::Map, false, false},
      {"cmap", 2, "c", MapCommand::Map, false, false},
      {"noremap", 2, "nxo", MapCommand::Map, true, false},
      {"nnoremap", 2, "n", MapCommand::Map, true, false},
      {"vnoremap", 2, "x", MapCommand::Map, true, false},
      {"xnoremap", 2, "x", MapCommand::Map, true, false},
      {"snoremap", 4, "", MapCommand::Map, true, false},
      {"onoremap", 3, "o", MapCommand::Map, true, false},
      {"inoremap", 3, "i", MapCommand::Map, true, false},
      {"cnoremap", 3, "c", MapCommand::Map, true, false},
      {"unmap", 3, "nxo", MapCommand::Unmap, false, false},
      {"nunmap", 3, "n", MapCommand::Unmap, false, false},
      {"vunmap", 2, "x", MapCommand::Unmap, false, false},
      {"xunmap", 2, "x", MapCommand::Unmap, false, false},
      {"sunmap", 4, "", MapCommand::Unmap, false, false},
      {"ounmap", 2, "o", MapCommand::Unmap, false, false},
      {"iunmap", 2, "i", MapCommand::Unmap, false, false},
      {"cunmap", 2, "c", MapCommand::Unmap, false, false},
      {"mapclear", 4, "nxo", MapCommand::Clear, false, false},
      {"nmapclear", 5, "n", MapCommand::Clear, false, false},
      {"vmapclear", 5, "x", MapCommand::Clear, false, false},
      {"xmapclear", 5, "x", MapCommand::Clear, false, false},
      {"smapclear", 5, "", MapCommand::Clear, false, false},
      {"omapclear", 5, "o", MapCommand::Clear, false, false},
      {"imapclear", 5, "i", MapCommand::Clear, false, false},
      {"cmapclear", 5, "c", MapCommand::Clear, false, false},
      {"abbreviate", 2, "ic", MapCommand::Map, false, true},
      {"iabbrev", 3, "i", MapCommand::Map, false, true},
      {"cabbrev", 2, "c", MapCommand::Map, false, true},
      {"noreabbrev", 5, "ic", MapCommand::Map, true, true},
      {"inoreabbrev", 6, "i", MapCommand::Map, true, true},
      {"cnoreabbrev", 6, "c", MapCommand::Map, true, true},
      {"unabbreviate", 3, "ic", MapCommand::Unmap, false, true},
      {"iunabbrev", 4, "i", MapCommand::Unmap, false, true},
      {"cunabbrev", 4, "c", MapCommand::Unmap, false, true},
      {"abclear", 3, "ic", MapCommand::Clear, false, true},
      {"iabclear", 4, "i", MapCommand::Clear, false, true},
      {"cabclear", 4, "c", MapCommand::Clear, false, true},
  };
  for (const Spec &spec : specs) {
    const QString full = QString::fromLatin1(spec.full);
    if (name.size() < spec.minLen || !full.startsWith(name))
      continue;
    out.modes = QString::fromLatin1(spec.modes);
    out.kind = spec.kind;
    out.noremap = spec.noremap;
    out.abbrev = spec.abbrev;
    // ":map!" and friends apply to Insert and Command-line mode.
    if (bang && !spec.abbrev && QString("nxo") == out.modes)
      out.modes = "ic";
    return true;
  }
  return false;
}

QString modeLabel(QChar mode) {
  switch (mode.unicode()) {
  case 'n':
    return "n";
  case 'x':
    return "x";
  case 'o':
    return "o";
  case 'i':
    return "i";
  case 'c':
    return "c";
  }
  return QString(mode);
}

} // namespace

void VimMode::clearAllMappings() {
  mapTables().clear();
  abbrevTables().clear();
}

QString VimMode::defaultVimrcPath() {
  QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  if (dir.isEmpty())
    dir = QDir::homePath() + "/.config/lightpad";
  return QDir(dir).filePath("vimrc");
}

void VimMode::executeCommand(const QString &command) { executeEx(command); }

void VimMode::loadUserConfig() {
  if (m_userConfigLoaded)
    return;
  m_userConfigLoaded = true;
  const QString path = defaultVimrcPath();
  if (!QFileInfo::exists(path))
    return;
  m_variables.insert("MYVIMRC", path);
  QString error;
  if (!sourceFile(path, &error) || !error.isEmpty())
    emit statusMessage(error);
}

QChar VimMode::currentMapMode() const {
  switch (m_mode) {
  case VimEditMode::Insert:
  case VimEditMode::Replace:
    if (m_insertRegisterPending || m_insertLiteralNext)
      return QChar();
    return 'i';
  case VimEditMode::Command:
    if (m_cmdRegisterPending || m_subConfirmActive)
      return QChar();
    return 'c';
  case VimEditMode::Visual:
  case VimEditMode::VisualLine:
  case VimEditMode::VisualBlock:
    return awaitingCharArgument() ? QChar() : QChar('x');
  case VimEditMode::Normal:
    break;
  }
  if (m_pending.isEmpty())
    return 'n';
  if (awaitingCharArgument())
    return QChar();
  NormalCmd cmd;
  if (parseCommand(m_pending, cmd, false) == Parse::Incomplete &&
      !cmd.op.isEmpty())
    return 'o';
  return 'n';
}

// Keys that complete a command which takes a character argument are never
// remapped, just like Vim reads them with mappings disabled.
bool VimMode::awaitingCharArgument() const {
  if (m_pending.isEmpty())
    return false;
  QStringList keys;
  for (int i = 0; i < m_pending.size(); ++i) {
    if (m_pending[i] == "\"" && i + 1 < m_pending.size()) {
      ++i;
      continue;
    }
    keys << m_pending[i];
  }
  if (keys.isEmpty())
    return false;
  static const QStringList argKeys = {"f", "F", "t",     "T",  "'", "`", "r",
                                      "m", "@", "q",     "\"", "g", "z", "Z",
                                      "[", "]", "<C-w>", "i",  "a"};
  return argKeys.contains(keys.last());
}

bool VimMode::inputKey(const QString &token, QKeyEvent *event, bool remap) {
  const bool record = m_macroRecording && m_replayDepth == 0;
  if (record)
    m_macroKeys << token;
  if (m_typeahead.isEmpty()) {
    const QChar mode = remap ? currentMapMode() : QChar();
    bool candidate = false;
    if (!mode.isNull()) {
      const MapTable &table = mapTables().value(mode);
      for (auto it = table.constBegin(); it != table.constEnd(); ++it) {
        if (it->lhs.first() == token) {
          candidate = true;
          break;
        }
      }
    }
    if (!candidate) {
      const bool handled = dispatchKey(token, event);
      if (!handled && record && m_macroRecording && !m_macroKeys.isEmpty())
        m_macroKeys.removeLast();
      return handled;
    }
  }
  m_typeahead.append({token, remap, event, false});
  m_unhandledEvent = nullptr;
  processTypeahead(false);
  for (TypeaheadKey &key : m_typeahead)
    key.event = nullptr;
  const bool handled = !event || m_unhandledEvent != event;
  m_unhandledEvent = nullptr;
  return handled;
}

bool VimMode::dispatchKey(const QString &token, QKeyEvent *event) {
  const bool handled = handleKey(token, event);
  if (handled || event)
    return handled;
  if ((m_mode == VimEditMode::Insert || m_mode == VimEditMode::Replace) &&
      isInsertNavigation(token)) {
    std::unique_ptr<QKeyEvent> synthetic = tokenToKeyEvent(token);
    if (synthetic) {
      m_passthrough = true;
      QCoreApplication::sendEvent(m_editor, synthetic.get());
      m_passthrough = false;
      return true;
    }
  }
  return false;
}

void VimMode::processTypeahead(bool timedOut) {
  if (m_mapTimer)
    m_mapTimer->stop();
  while (!m_typeahead.isEmpty()) {
    const QChar mode = m_typeahead.first().remap ? currentMapMode() : QChar();
    if (!mode.isNull()) {
      int avail = 0;
      while (avail < m_typeahead.size() && m_typeahead[avail].remap)
        ++avail;
      const MapTable &table = mapTables().value(mode);
      const MapEntry *exact = nullptr;
      bool longer = false;
      for (auto it = table.constBegin(); it != table.constEnd(); ++it) {
        const QStringList &lhs = it->lhs;
        int k = 0;
        while (k < lhs.size() && k < avail && lhs[k] == m_typeahead[k].token)
          ++k;
        if (k == lhs.size()) {
          if (!exact || lhs.size() > exact->lhs.size())
            exact = &it.value();
        } else if (k == avail && avail == m_typeahead.size()) {
          longer = true;
        }
      }
      if (longer && !timedOut && !(exact && exact->nowait)) {
        if (m_timeout) {
          if (!m_mapTimer) {
            m_mapTimer = new QTimer(this);
            m_mapTimer->setSingleShot(true);
            connect(m_mapTimer, &QTimer::timeout, this,
                    [this]() { processTypeahead(true); });
          }
          m_mapTimer->start(m_timeoutLen);
        }
        updatePendingKeys();
        return;
      }
      if (exact) {
        if (++m_mapDepth > kMaxMapDepth) {
          m_typeahead.clear();
          m_mapDepth = 0;
          emit statusMessage("E223: Recursive mapping");
          break;
        }
        QString rhs = exact->rhs;
        if (exact->expr) {
          QString result, error;
          if (!evalExpression(rhs, &result, nullptr, nullptr, &error)) {
            emit statusMessage(error);
            result.clear();
          }
          rhs = result;
        }
        const QStringList lhs = exact->lhs;
        const bool noremap = exact->noremap;
        const QStringList tokens = parseKeyNotation(rhs);
        const bool startsWithLhs =
            tokens.size() >= lhs.size() && tokens.mid(0, lhs.size()) == lhs;
        for (int k = 0; k < lhs.size(); ++k)
          m_typeahead.removeFirst();
        for (int k = tokens.size() - 1; k >= 0; --k)
          m_typeahead.prepend({tokens[k],
                               !noremap && !(startsWithLhs && k < lhs.size()),
                               nullptr, true});
        timedOut = false;
        continue;
      }
    }
    const TypeaheadKey head = m_typeahead.takeFirst();
    timedOut = false;
    const bool handled = dispatchKey(head.token, head.event);
    if (!handled && head.event)
      m_unhandledEvent = head.event;
    if (head.fromMap && m_abortReplay) {
      while (!m_typeahead.isEmpty() && m_typeahead.first().fromMap)
        m_typeahead.removeFirst();
    }
  }
  m_mapDepth = 0;
  updatePendingKeys();
}

void VimMode::flushTypeahead() {
  if (!m_typeahead.isEmpty())
    processTypeahead(true);
}

bool VimMode::exMapCommand(const QString &name, const QString &rawArgs,
                           bool bang) {
  MapCommand spec;
  if (!lookupMapCommand(name, bang, spec))
    return false;
  QHash<QChar, MapTable> &tables = spec.abbrev ? abbrevTables() : mapTables();

  QString args = rawArgs;
  while (args.startsWith(' ') || args.startsWith('\t'))
    args = args.mid(1);
  bool expr = false;
  bool nowait = false;
  static const QStringList modifiers = {"<buffer>", "<silent>", "<nowait>",
                                        "<expr>",   "<unique>", "<special>",
                                        "<script>"};
  for (bool again = true; again;) {
    again = false;
    for (const QString &m : modifiers) {
      if (args.startsWith(m, Qt::CaseInsensitive)) {
        expr = expr || m == "<expr>";
        nowait = nowait || m == "<nowait>";
        args = args.mid(m.size());
        while (args.startsWith(' ') || args.startsWith('\t'))
          args = args.mid(1);
        again = true;
      }
    }
  }

  auto substituteLeader = [this](QString text) {
    const QString leader = m_variables.value("mapleader", "\\");
    const QString localLeader = m_variables.value("maplocalleader", leader);
    static const QRegularExpression leaderRe(
        "<leader>", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression localRe(
        "<localleader>", QRegularExpression::CaseInsensitiveOption);
    auto unquote = [](QString value) {
      if (value.size() >= 2 &&
          ((value.startsWith('"') && value.endsWith('"')) ||
           (value.startsWith('\'') && value.endsWith('\''))))
        value = value.mid(1, value.size() - 2);
      return value == " " ? QString("<Space>") : value;
    };
    text.replace(localRe, unquote(localLeader));
    text.replace(leaderRe, unquote(leader));
    return text;
  };

  int split = 0;
  while (split < args.size() && args[split] != ' ' && args[split] != '\t') {
    if (args[split] == '\\' || args[split].unicode() == 0x16)
      ++split;
    ++split;
  }
  split = qMin(split, int(args.size()));
  const QString lhsText = substituteLeader(args.left(split));
  QString rhsText = args.mid(split);
  while (rhsText.startsWith(' ') || rhsText.startsWith('\t'))
    rhsText = rhsText.mid(1);
  rhsText.replace("\\|", "|");
  rhsText = substituteLeader(rhsText);
  const QStringList lhs = parseKeyNotation(lhsText);

  if (spec.kind == MapCommand::Clear) {
    for (QChar mode : spec.modes)
      tables.remove(mode);
    return true;
  }

  if (spec.kind == MapCommand::Unmap) {
    if (lhs.isEmpty()) {
      emit statusMessage("E474: Invalid argument");
      return true;
    }
    bool removed = false;
    for (QChar mode : spec.modes)
      removed = tables[mode].remove(tableKey(lhs)) > 0 || removed;
    if (!removed)
      emit statusMessage(spec.abbrev ? "E24: No such abbreviation"
                                     : "E31: No such mapping");
    return true;
  }

  if (lhs.isEmpty() || rhsText.isEmpty()) {
    QStringList lines;
    for (QChar mode : spec.modes) {
      const MapTable &table = tables.value(mode);
      for (auto it = table.constBegin(); it != table.constEnd(); ++it) {
        if (!lhs.isEmpty() && it->lhs.mid(0, lhs.size()) != lhs)
          continue;
        lines << QString("%1  %2 %3%4")
                     .arg(modeLabel(mode), tokensToNotation(it->lhs),
                          it->noremap ? "*" : " ",
                          it->rhs.isEmpty() ? QString("<Nop>") : it->rhs);
      }
    }
    if (lines.isEmpty())
      emit statusMessage(spec.abbrev ? "No abbreviation found"
                                     : "No mapping found");
    else
      emit statusMessage(lines.join("   "));
    return true;
  }

  if (spec.abbrev) {
    const QString word = lhs.join(QString());
    if (word.contains(' ') || lhs.size() != word.size()) {
      emit statusMessage("E474: Invalid argument");
      return true;
    }
  }

  MapEntry entry;
  entry.lhs = lhs;
  entry.rhs =
      rhsText.compare("<Nop>", Qt::CaseInsensitive) == 0 ? QString() : rhsText;
  entry.noremap = spec.noremap;
  entry.expr = expr;
  entry.nowait = nowait;
  for (QChar mode : spec.modes)
    tables[mode].insert(tableKey(lhs), entry);
  return true;
}

// Expand an abbreviation that ends right before the cursor (Insert mode) or
// the command-line cursor. Returns true when text was replaced.
bool VimMode::expandAbbreviation(QChar mode) {
  if (m_expandingAbbreviation)
    return false;
  const MapTable table = abbrevTables().value(mode);
  if (table.isEmpty())
    return false;
  QString before;
  int minStart = 0;
  if (mode == 'c') {
    before = m_cmdText.left(m_cmdCursor);
  } else {
    const int pos = m_editor->textCursor().position();
    const int line = lineOf(pos);
    before = lineText(line).left(pos - lineStart(line));
    if (m_insertStartPos >= lineStart(line) && m_insertStartPos <= pos)
      minStart = m_insertStartPos - lineStart(line);
  }
  const MapEntry *match = nullptr;
  for (auto it = table.constBegin(); it != table.constEnd(); ++it) {
    const QString word = it->lhs.join(QString());
    if (word.isEmpty() || !before.endsWith(word))
      continue;
    const int start = before.size() - word.size();
    if (start < minStart)
      continue;
    bool fullId = true;
    for (QChar c : word)
      fullId = fullId && isKeywordChar(c);
    if (start > 0) {
      const QChar prev = before[start - 1];
      if (fullId ? isKeywordChar(prev) : !prev.isSpace())
        continue;
    }
    if (!match || word.size() > match->lhs.size())
      match = &it.value();
  }
  if (!match)
    return false;
  const int length = match->lhs.size();
  QString rhs = match->rhs;
  if (match->expr) {
    QString result, error;
    rhs = evalExpression(rhs, &result, nullptr, nullptr, &error) ? result
                                                                 : QString();
  }
  const QStringList tokens = parseKeyNotation(rhs);
  m_expandingAbbreviation = true;
  if (mode == 'c') {
    setCommandBufferInternal(m_cmdText.left(m_cmdCursor - length) +
                                 m_cmdText.mid(m_cmdCursor),
                             m_cmdCursor - length);
    for (const QString &t : tokens)
      handleCommandKey(t);
  } else {
    QTextCursor c = m_editor->textCursor();
    if (m_insertEditOpen) {
      c.joinPreviousEditBlock();
    } else {
      c.beginEditBlock();
      m_insertEditOpen = true;
    }
    c.setPosition(c.position() - length, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    c.endEditBlock();
    m_editor->setTextCursor(c);
    for (const QString &t : tokens) {
      if (m_mode == VimEditMode::Replace)
        handleReplaceKey(t, nullptr);
      else
        handleInsertKey(t, nullptr);
    }
  }
  m_expandingAbbreviation = false;
  return true;
}

bool VimMode::sourceFile(const QString &path, QString *error) {
  QString expanded = path.trimmed();
  if (expanded.startsWith("~/") || expanded == "~")
    expanded = QDir::homePath() + expanded.mid(1);
  QFile file(expanded);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString message = QString("E484: Can't open file %1").arg(path);
    if (error)
      *error = message;
    return false;
  }
  if (m_sourceDepth >= kMaxSourceDepth) {
    if (error)
      *error = "E169: Command too recursive";
    return false;
  }
  QStringList commands;
  QList<int> lineNumbers;
  const QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
  for (int i = 0; i < lines.size(); ++i) {
    QString line = lines[i];
    if (line.endsWith('\r'))
      line.chop(1);
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith('\\') && !commands.isEmpty()) {
      commands.last() += trimmed.mid(1);
      continue;
    }
    if (trimmed.startsWith("\"\\ ") && !commands.isEmpty())
      continue;
    commands << line;
    lineNumbers << i + 1;
  }

  QStringList errors;
  const QMetaObject::Connection errorSink = connect(
      this, &VimMode::statusMessage, this, [&errors](const QString &message) {
        static const QRegularExpression code("^E\\d+:");
        if (code.match(message).hasMatch())
          errors << message;
      });
  ++m_sourceDepth;
  // Lines inside :if blocks whose condition is false (or cannot be
  // evaluated) and inside :function definitions are skipped.
  struct Condition {
    bool parent;
    bool active;
    bool taken;
  };
  QVector<Condition> conditions;
  int functionDepth = 0;
  auto truthy = [this](const QString &expression) {
    QString result;
    return evalExpression(expression, &result) && result != "0" &&
           !result.isEmpty();
  };
  for (int i = 0; i < commands.size(); ++i) {
    const QString trimmed = commands[i].trimmed();
    const QString word = trimmed.section(QRegularExpression("[^A-Za-z]"), 0, 0);
    if (functionDepth > 0) {
      if (word.startsWith("endf"))
        --functionDepth;
      else if (word.startsWith("fu"))
        ++functionDepth;
      continue;
    }
    const bool running = conditions.isEmpty() || conditions.last().active;
    if (word == "if") {
      const bool value = running && truthy(trimmed.mid(2).trimmed());
      conditions.append({running, value, value});
      continue;
    }
    if (!conditions.isEmpty() && word.size() >= 5 &&
        QString("elseif").startsWith(word)) {
      Condition &c = conditions.last();
      const bool value =
          c.parent && !c.taken && truthy(trimmed.mid(word.size()).trimmed());
      c.active = value;
      c.taken = c.taken || value;
      continue;
    }
    if (!conditions.isEmpty() && word.size() >= 2 &&
        QString("else").startsWith(word)) {
      Condition &c = conditions.last();
      c.active = c.parent && !c.taken;
      c.taken = true;
      continue;
    }
    if (word.size() >= 2 && QString("endif").startsWith(word) &&
        word.startsWith("en")) {
      if (!conditions.isEmpty())
        conditions.removeLast();
      continue;
    }
    if (word.size() >= 2 && QString("function").startsWith(word)) {
      ++functionDepth;
      continue;
    }
    if (!running || trimmed.isEmpty() || trimmed.startsWith('"'))
      continue;
    const int before = errors.size();
    executeEx(trimmed);
    if (errors.size() > before)
      errors.last() =
          QString("line %1: %2").arg(lineNumbers[i]).arg(errors.last());
  }
  --m_sourceDepth;
  disconnect(errorSink);
  m_abortReplay = false;
  if (!errors.isEmpty() && error)
    *error = QString("Error detected while processing %1: %2")
                 .arg(QFileInfo(expanded).fileName(), errors.first());
  return true;
}
