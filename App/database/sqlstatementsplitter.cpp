#include "sqlstatementsplitter.h"

#include <QRegularExpression>

namespace {

bool isIdentStart(QChar c) { return c.isLetter() || c == '_' || c == '@'; }
bool isIdentChar(QChar c) {
  return c.isLetterOrNumber() || c == '_' || c == '$';
}

QString wordAt(const QString &s, int i) {
  int j = i;
  while (j < s.size() && isIdentChar(s[j])) {
    ++j;
  }
  return s.mid(i, j - i);
}

QString nextWord(const QString &s, int i) {
  const int n = s.size();
  while (i < n) {
    if (s[i].isSpace()) {
      ++i;
    } else if (i + 1 < n && s[i] == '-' && s[i + 1] == '-') {
      while (i < n && s[i] != '\n') {
        ++i;
      }
    } else if (i + 1 < n && s[i] == '/' && s[i + 1] == '*') {
      const int close = s.indexOf(QLatin1String("*/"), i + 2);
      i = close < 0 ? n : close + 2;
    } else {
      break;
    }
  }
  if (i < n && isIdentStart(s[i])) {
    return wordAt(s, i);
  }
  return {};
}

int dollarTagLength(const QString &s, int i) {
  if (s[i] != '$') {
    return 0;
  }
  int j = i + 1;
  while (j < s.size() && (s[j].isLetterOrNumber() || s[j] == '_')) {
    ++j;
  }
  if (j < s.size() && s[j] == '$') {

    if (j > i + 1 && s[i + 1].isDigit()) {
      return 0;
    }
    return j - i + 1;
  }
  return 0;
}

bool lineIsGo(const QString &line, int *repeat = nullptr) {
  static const QRegularExpression re(
      QStringLiteral("^\\s*GO(?:\\s+(\\d+))?\\s*(?:--.*)?$"),
      QRegularExpression::CaseInsensitiveOption);
  const auto m = re.match(line);
  if (!m.hasMatch()) {
    return false;
  }
  if (repeat) {
    *repeat = m.captured(1).isEmpty() ? 1 : m.captured(1).toInt();
  }
  return true;
}

bool hasExecutableContent(const QString &text) {
  return !SqlStatementSplitter::stripComments(text).trimmed().isEmpty();
}

} // namespace

QString SqlStatementSplitter::stripComments(const QString &sql) {
  QString out;
  out.reserve(sql.size());
  const int n = sql.size();
  int i = 0;
  while (i < n) {
    const QChar c = sql[i];
    if (c == '\'' || c == '"' || c == '`') {
      const QChar q = c;
      out += c;
      ++i;
      while (i < n) {
        out += sql[i];
        if (sql[i] == q) {
          if (i + 1 < n && sql[i + 1] == q) {
            out += sql[i + 1];
            i += 2;
            continue;
          }
          ++i;
          break;
        }
        ++i;
      }
      continue;
    }
    if (c == '-' && i + 1 < n && sql[i + 1] == '-') {
      while (i < n && sql[i] != '\n') {
        ++i;
      }
      continue;
    }
    if (c == '/' && i + 1 < n && sql[i + 1] == '*') {
      const int close = sql.indexOf(QLatin1String("*/"), i + 2);
      i = close < 0 ? n : close + 2;
      out += ' ';
      continue;
    }
    out += c;
    ++i;
  }
  return out;
}

QVector<SqlStatement> SqlStatementSplitter::split(const QString &script,
                                                  DbEngine engine) {
  QVector<SqlStatement> result;
  const int n = script.size();

  int stmtStart = -1;
  int depth = 0;
  bool trackBlocks = engine == DbEngine::SqlServer;
  int wordIndex = 0;
  QString firstWords;
  int line = 0;
  int stmtLine = 0;

  auto flush = [&](int endExclusive, int terminatorEnd) {
    if (stmtStart < 0) {
      return;
    }
    QString text = script.mid(stmtStart, endExclusive - stmtStart).trimmed();
    if (hasExecutableContent(text)) {
      SqlStatement st;
      st.text = text;
      st.start = stmtStart;
      st.end = terminatorEnd;
      st.line = stmtLine;
      result.append(st);
    }
    stmtStart = -1;
    depth = 0;
    wordIndex = 0;
    firstWords.clear();
    trackBlocks = engine == DbEngine::SqlServer;
  };

  auto beginStatement = [&](int at) {
    if (stmtStart < 0) {
      stmtStart = at;
      stmtLine = line;
    }
  };

  int i = 0;
  bool atLineStart = true;
  while (i < n) {
    const QChar c = script[i];

    if (atLineStart && engine == DbEngine::SqlServer) {

      int eol = script.indexOf('\n', i);
      if (eol < 0) {
        eol = n;
      }
      const QString lineText = script.mid(i, eol - i);
      int repeat = 1;
      if (lineIsGo(lineText, &repeat)) {
        flush(i, i);
        i = eol;
        atLineStart = false;
        continue;
      }
    }
    atLineStart = false;

    if (c == '\n') {
      ++line;
      atLineStart = true;
      ++i;
      continue;
    }
    if (c.isSpace()) {
      ++i;
      continue;
    }

    if (c == '-' && i + 1 < n && script[i + 1] == '-') {
      beginStatement(i);
      while (i < n && script[i] != '\n') {
        ++i;
      }
      continue;
    }
    if (c == '#' && engine == DbEngine::MySql) {
      beginStatement(i);
      while (i < n && script[i] != '\n') {
        ++i;
      }
      continue;
    }
    if (c == '/' && i + 1 < n && script[i + 1] == '*') {
      beginStatement(i);
      int nest = 1;
      int j = i + 2;
      const bool nested =
          engine == DbEngine::PostgreSql || engine == DbEngine::SqlServer;
      while (j < n && nest > 0) {
        if (script[j] == '\n') {
          ++line;
        }
        if (nested && script[j] == '/' && j + 1 < n && script[j + 1] == '*') {
          ++nest;
          j += 2;
        } else if (script[j] == '*' && j + 1 < n && script[j + 1] == '/') {
          --nest;
          j += 2;
        } else {
          ++j;
        }
      }
      i = j;
      continue;
    }

    beginStatement(i);

    if (c == '\'' || c == '"' || c == '`' ||
        (c == '[' && engine == DbEngine::SqlServer)) {
      const QChar close = c == '[' ? QChar(']') : c;
      const bool backslash =
          engine == DbEngine::MySql && (c == '\'' || c == '"');
      int j = i + 1;
      while (j < n) {
        if (script[j] == '\n') {
          ++line;
        }
        if (backslash && script[j] == '\\' && j + 1 < n) {
          j += 2;
          continue;
        }
        if (script[j] == close) {
          if (j + 1 < n && script[j + 1] == close) {
            j += 2;
            continue;
          }
          ++j;
          break;
        }
        ++j;
      }
      i = j;
      continue;
    }

    if (c == '$' && engine == DbEngine::PostgreSql) {
      const int tagLen = dollarTagLength(script, i);
      if (tagLen > 0) {
        const QString tag = script.mid(i, tagLen);
        int close = script.indexOf(tag, i + tagLen);
        int j = close < 0 ? n : close + tagLen;
        for (int k = i; k < j; ++k) {
          if (script[k] == '\n') {
            ++line;
          }
        }
        i = j;
        continue;
      }
    }

    if (c == ';') {
      if (depth <= 0) {
        flush(i, i + 1);
      }
      ++i;
      continue;
    }

    if (isIdentStart(c)) {
      const QString word = wordAt(script, i);
      const QString upper = word.toUpper();
      if (wordIndex < 6) {
        firstWords += upper + QLatin1Char(' ');
      }
      ++wordIndex;
      if (engine == DbEngine::Sqlite && wordIndex <= 6 &&
          firstWords.contains(QLatin1String("TRIGGER"))) {
        trackBlocks = true;
      }
      if (trackBlocks) {
        if (upper == QLatin1String("BEGIN")) {
          const QString next = nextWord(script, i + word.size()).toUpper();
          const bool transaction = next == QLatin1String("TRAN") ||
                                   next == QLatin1String("TRANSACTION") ||
                                   next == QLatin1String("DISTRIBUTED") ||
                                   next == QLatin1String("WORK") ||
                                   next == QLatin1String("DEFERRED") ||
                                   next == QLatin1String("IMMEDIATE") ||
                                   next == QLatin1String("EXCLUSIVE");
          if (!(transaction && wordIndex == 1)) {
            ++depth;
          }
        } else if (upper == QLatin1String("CASE")) {
          ++depth;
        } else if (upper == QLatin1String("END") && depth > 0) {

          --depth;
        }
      }
      i += qMax<int>(1, word.size());
      continue;
    }

    ++i;
  }
  flush(n, n);
  return result;
}

SqlStatement SqlStatementSplitter::statementAt(const QString &script,
                                               int offset, DbEngine engine) {
  const QVector<SqlStatement> all = split(script, engine);
  if (all.isEmpty()) {
    return {};
  }
  const SqlStatement *before = nullptr;
  for (const SqlStatement &st : all) {
    if (offset >= st.start && offset <= st.end) {
      return st;
    }
    if (st.end <= offset) {
      before = &st;
    } else if (offset < st.start) {
      return before ? *before : st;
    }
  }
  return before ? *before : all.first();
}

SqlStatementKind SqlStatementSplitter::classify(const QString &statement,
                                                DbEngine engine) {
  Q_UNUSED(engine)
  const QString clean = stripComments(statement).trimmed();
  if (clean.isEmpty()) {
    return SqlStatementKind::Empty;
  }
  int i = 0;
  while (i < clean.size() && (clean[i] == '(' || clean[i].isSpace())) {
    ++i;
  }
  const QString first = wordAt(clean, i).toUpper();

  static const QStringList dml = {"INSERT", "UPDATE",  "DELETE",
                                  "MERGE",  "REPLACE", "UPSERT",
                                  "COPY",   "LOAD",    "BULK"};
  static const QStringList ddl = {"CREATE", "ALTER",   "DROP",    "TRUNCATE",
                                  "RENAME", "GRANT",   "REVOKE",  "COMMENT",
                                  "VACUUM", "REINDEX", "ANALYZE", "ATTACH",
                                  "DETACH", "CLUSTER", "REFRESH", "DENY"};
  static const QStringList txn = {"BEGIN",     "COMMIT",  "ROLLBACK", "START",
                                  "SAVEPOINT", "RELEASE", "END",      "ABORT"};
  static const QStringList show = {"SHOW", "DESCRIBE", "DESC", "PRAGMA",
                                   "HELP"};
  static const QStringList call = {"EXEC", "EXECUTE", "CALL", "DO"};

  if (first == QLatin1String("SELECT") || first == QLatin1String("VALUES") ||
      first == QLatin1String("TABLE")) {

    static const QRegularExpression into(
        QStringLiteral("\\bINTO\\s+(?!@)[#\\[\"`\\w]"),
        QRegularExpression::CaseInsensitiveOption);
    if (into.match(clean).hasMatch() &&
        !clean.contains(QRegularExpression(
            QStringLiteral("\\bINTO\\s+(OUTFILE|DUMPFILE)\\b"),
            QRegularExpression::CaseInsensitiveOption))) {
      return SqlStatementKind::Ddl;
    }
    return SqlStatementKind::Select;
  }
  if (first == QLatin1String("WITH")) {
    static const QRegularExpression writes(
        QStringLiteral("\\b(INSERT|UPDATE|DELETE|MERGE)\\b"),
        QRegularExpression::CaseInsensitiveOption);

    QString noStrings = clean;
    noStrings.replace(QRegularExpression(QStringLiteral("'(?:[^']|'')*'")),
                      QStringLiteral("''"));
    return writes.match(noStrings).hasMatch() ? SqlStatementKind::Dml
                                              : SqlStatementKind::Select;
  }
  if (first == QLatin1String("EXPLAIN")) {
    static const QRegularExpression analyze(
        QStringLiteral("^EXPLAIN\\s*(\\(|ANALYZE)"),
        QRegularExpression::CaseInsensitiveOption);
    return analyze.match(clean).hasMatch() ? SqlStatementKind::Call
                                           : SqlStatementKind::Explain;
  }
  if (dml.contains(first)) {
    return SqlStatementKind::Dml;
  }
  if (ddl.contains(first)) {
    return SqlStatementKind::Ddl;
  }
  if (txn.contains(first)) {
    return SqlStatementKind::Transaction;
  }
  if (first == QLatin1String("PRAGMA") && clean.contains(QLatin1Char('='))) {
    return SqlStatementKind::Other;
  }
  if (show.contains(first)) {
    return SqlStatementKind::Show;
  }
  if (first == QLatin1String("USE") || first == QLatin1String("SET")) {
    return SqlStatementKind::Session;
  }
  if (call.contains(first)) {
    return SqlStatementKind::Call;
  }
  return SqlStatementKind::Other;
}

bool SqlStatementSplitter::isReadOnly(SqlStatementKind kind) {
  switch (kind) {
  case SqlStatementKind::Empty:
  case SqlStatementKind::Select:
  case SqlStatementKind::Explain:
  case SqlStatementKind::Show:
  case SqlStatementKind::Transaction:
  case SqlStatementKind::Session:
    return true;
  case SqlStatementKind::Dml:
  case SqlStatementKind::Ddl:
  case SqlStatementKind::Call:
  case SqlStatementKind::Other:
    break;
  }
  return false;
}

QString SqlStatementSplitter::destructiveWarning(const QString &statement,
                                                 DbEngine engine) {
  const QString clean = stripComments(statement).trimmed();
  const QString first = wordAt(clean, 0).toUpper();
  if (first == QLatin1String("DROP")) {
    return QStringLiteral("DROP permanently removes an object and its data.");
  }
  if (first == QLatin1String("TRUNCATE")) {
    return QStringLiteral("TRUNCATE removes every row from the table.");
  }
  if (first == QLatin1String("DELETE") || first == QLatin1String("UPDATE")) {
    QString noStrings = clean;
    noStrings.replace(QRegularExpression(QStringLiteral("'(?:[^']|'')*'")),
                      QStringLiteral("''"));
    static const QRegularExpression where(
        QStringLiteral("\\bWHERE\\b"),
        QRegularExpression::CaseInsensitiveOption);
    if (!where.match(noStrings).hasMatch()) {
      return QStringLiteral("%1 without a WHERE clause affects every row in "
                            "the table.")
          .arg(first);
    }
  }
  Q_UNUSED(engine)
  return {};
}

QString SqlStatementSplitter::connectionDirective(const QString &script) {
  static const QRegularExpression re(
      QStringLiteral("^\\s*--\\s*connection\\s*:\\s*(.+?)\\s*$"),
      QRegularExpression::CaseInsensitiveOption);
  const QStringList lines = script.left(2048).split('\n');
  for (int i = 0; i < lines.size() && i < 5; ++i) {
    const auto m = re.match(lines[i]);
    if (m.hasMatch()) {
      return m.captured(1);
    }
    if (!lines[i].trimmed().isEmpty() && !lines[i].trimmed().startsWith("--")) {
      break;
    }
  }
  return {};
}
