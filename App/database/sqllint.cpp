#include "sqllint.h"

#include <QHash>
#include <QSet>
#include <algorithm>

namespace {

struct Tok {
  enum Type { Word, Quoted, String, Number, Punct } type = Punct;
  QString text; // identifiers are unquoted
  int start = 0;
  int length = 0;
  bool is(const char *kw) const {
    return type == Word && text.compare(QLatin1String(kw), Qt::CaseInsensitive) == 0;
  }
  bool isPunct(char c) const { return type == Punct && text.size() == 1 && text[0] == c; }
  bool isName() const { return type == Word || type == Quoted; }
};

struct Tokenized {
  QVector<Tok> toks;
  QVector<SqlIssue> issues;
};

bool identStart(QChar c) { return c.isLetter() || c == '_' || c == '#' || c == '@'; }
bool identChar(QChar c) { return c.isLetterOrNumber() || c == '_' || c == '$' || c == '#' || c == '@'; }

Tokenized tokenize(const QString &s) {
  Tokenized r;
  const int n = s.size();
  int i = 0;
  auto err = [&](int start, int len, const QString &m) {
    SqlIssue is;
    is.severity = SqlIssue::Severity::Error;
    is.start = start;
    is.length = std::max(1, len);
    is.message = m;
    r.issues.append(is);
  };
  while (i < n) {
    const QChar c = s[i];
    if (c.isSpace()) {
      ++i;
    } else if (c == '-' && i + 1 < n && s[i + 1] == '-') {
      const int e = s.indexOf('\n', i);
      i = e < 0 ? n : e + 1;
    } else if (c == '/' && i + 1 < n && s[i + 1] == '*') {
      const int e = s.indexOf(QLatin1String("*/"), i + 2);
      if (e < 0) {
        err(i, 2, QStringLiteral("Unterminated block comment"));
        i = n;
      } else {
        i = e + 2;
      }
    } else if (c == '\'' || c == '"' || c == '`' || c == '[') {
      const QChar close = c == '[' ? QChar(']') : c;
      const int start = i++;
      QString text;
      bool closed = false;
      while (i < n) {
        if (s[i] == close) {
          if (close != ']' && i + 1 < n && s[i + 1] == close) {
            text += close;
            i += 2;
            continue;
          }
          closed = true;
          ++i;
          break;
        }
        text += s[i++];
      }
      if (!closed) {
        err(start, 1,
            c == '\'' ? QStringLiteral("Unterminated string literal")
                      : QStringLiteral("Unterminated quoted identifier"));
      }
      Tok t;
      t.type = c == '\'' ? Tok::String : Tok::Quoted;
      t.text = text;
      t.start = start;
      t.length = i - start;
      r.toks.append(t);
    } else if (identStart(c)) {
      const int start = i;
      while (i < n && identChar(s[i])) {
        ++i;
      }
      Tok t;
      t.type = Tok::Word;
      t.text = s.mid(start, i - start);
      t.start = start;
      t.length = i - start;
      r.toks.append(t);
    } else if (c.isDigit()) {
      const int start = i;
      while (i < n && (s[i].isLetterOrNumber() || s[i] == '.')) {
        ++i;
      }
      Tok t;
      t.type = Tok::Number;
      t.text = s.mid(start, i - start);
      t.start = start;
      t.length = i - start;
      r.toks.append(t);
    } else {
      Tok t;
      t.type = Tok::Punct;
      t.start = i;
      // keep two-character operators together
      const QString two = s.mid(i, 2);
      if (two == QLatin1String("<>") || two == QLatin1String("!=") ||
          two == QLatin1String("<=") || two == QLatin1String(">=") ||
          two == QLatin1String("::")) {
        t.text = two;
      } else {
        t.text = QString(c);
      }
      t.length = t.text.size();
      i += t.length;
      r.toks.append(t);
    }
  }
  return r;
}

int editDistance(const QString &a, const QString &b) {
  const QString x = a.toLower();
  const QString y = b.toLower();
  QVector<int> prev(y.size() + 1), cur(y.size() + 1);
  for (int j = 0; j <= y.size(); ++j) {
    prev[j] = j;
  }
  for (int i = 1; i <= x.size(); ++i) {
    cur[0] = i;
    for (int j = 1; j <= y.size(); ++j) {
      cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1,
                         prev[j - 1] + (x[i - 1] == y[j - 1] ? 0 : 1)});
    }
    std::swap(prev, cur);
  }
  return prev[y.size()];
}

const QSet<QString> &stopWords() {
  static const QSet<QString> w = {
      "where",  "on",     "inner",    "left",      "right",  "full",
      "cross",  "join",   "group",    "order",     "having", "limit",
      "union",  "set",    "using",    "natural",   "outer",  "values",
      "select", "from",   "offset",   "fetch",     "for",    "window",
      "except", "intersect", "returning", "with",  "lateral", "output",
      "when",   "then",   "else",     "end",       "and",    "or",
      "into",   "top",    "straight_join", "apply", "pivot", "unpivot"};
  return w;
}

bool isSystemSchema(const QString &s) {
  const QString l = s.toLower();
  return l == QLatin1String("information_schema") ||
         l == QLatin1String("pg_catalog") || l == QLatin1String("sys") ||
         l == QLatin1String("mysql") || l == QLatin1String("performance_schema") ||
         l == QLatin1String("pg_temp") || l == QLatin1String("sqlite_master") ||
         l == QLatin1String("sqlite_temp_master") || l == QLatin1String("main") ||
         l == QLatin1String("temp");
}

struct TableRef {
  QString alias; // lower case
  const DbTableInfo *table = nullptr;
};

const DbTableInfo *resolve(const DbSchema &schema, const QStringList &parts,
                           bool *known) {
  *known = true;
  if (parts.isEmpty()) {
    return nullptr;
  }
  const QString name = parts.last();
  const QString sch = parts.size() >= 2 ? parts[parts.size() - 2] : QString();
  if (!sch.isEmpty()) {
    for (const DbTableInfo &t : schema.tables) {
      if (t.name.compare(name, Qt::CaseInsensitive) == 0 &&
          t.schema.compare(sch, Qt::CaseInsensitive) == 0) {
        return &t;
      }
    }
    // schema unknown to the catalog (e.g. other database): do not complain
    bool schemaSeen = false;
    for (const DbTableInfo &t : schema.tables) {
      if (t.schema.compare(sch, Qt::CaseInsensitive) == 0) {
        schemaSeen = true;
        break;
      }
    }
    if (!schemaSeen || isSystemSchema(sch)) {
      *known = false;
    }
    return nullptr;
  }
  const DbTableInfo *t = schema.findByName(name);
  if (!t && isSystemSchema(name)) {
    *known = false;
  }
  return t;
}

void add(QVector<SqlIssue> &out, SqlIssue::Severity sev, const Tok &t,
         const QString &msg) {
  SqlIssue is;
  is.severity = sev;
  is.start = t.start;
  is.length = t.length;
  is.message = msg;
  out.append(is);
}

void lintStatement(const QVector<Tok> &toks, DbEngine engine,
                   const DbSchema *schema, QVector<SqlIssue> &out) {
  Q_UNUSED(engine);
  if (toks.isEmpty()) {
    return;
  }
  const bool haveCatalog = schema && !schema->isEmpty();

  // balanced parentheses
  int depth = 0;
  for (const Tok &t : toks) {
    if (t.isPunct('(')) {
      ++depth;
    } else if (t.isPunct(')')) {
      if (--depth < 0) {
        add(out, SqlIssue::Severity::Error, t, QStringLiteral("Unmatched ')'"));
        depth = 0;
      }
    }
  }
  if (depth > 0) {
    add(out, SqlIssue::Severity::Error, toks.first(),
        QStringLiteral("Missing %1 closing parenthesis").arg(depth));
  }

  // CTE names
  QSet<QString> cte;
  for (int i = 0; i + 2 < toks.size(); ++i) {
    if (toks[i].isName() && toks[i + 1].is("as") && toks[i + 2].isPunct('(') && i > 0 &&
        (toks[i - 1].is("with") || toks[i - 1].isPunct(',') || toks[i - 1].is("recursive"))) {
      cte.insert(toks[i].text.toLower());
    }
  }

  const bool isUpdate = toks[0].is("update");
  const bool isDelete = toks[0].is("delete");
  bool hasWhere = false;
  for (const Tok &t : toks) {
    if (t.is("where")) {
      hasWhere = true;
    }
  }
  if ((isUpdate || isDelete) && !hasWhere) {
    add(out, SqlIssue::Severity::Warning, toks[0],
        QStringLiteral("%1 without WHERE affects every row")
            .arg(isUpdate ? QStringLiteral("UPDATE") : QStringLiteral("DELETE")));
  }

  // "= NULL"
  for (int i = 0; i + 1 < toks.size(); ++i) {
    if (toks[i].type == Tok::Punct &&
        (toks[i].text == QLatin1String("=") || toks[i].text == QLatin1String("<>") ||
         toks[i].text == QLatin1String("!=")) &&
        toks[i + 1].is("null") && i > 0 && !toks[i - 1].is("set")) {
      // skip assignments in UPDATE ... SET col = NULL (first '=' after SET/commas)
      bool inSet = false;
      if (isUpdate) {
        inSet = true;
        for (int j = i; j >= 0; --j) {
          if (toks[j].is("where")) {
            inSet = false;
            break;
          }
          if (toks[j].is("set")) {
            break;
          }
        }
      }
      if (!inSet) {
        add(out, SqlIssue::Severity::Warning, toks[i + 1],
            toks[i].text == QLatin1String("=")
                ? QStringLiteral("Comparing with NULL is never true; use IS NULL")
                : QStringLiteral("Comparing with NULL is never true; use IS NOT NULL"));
      }
    }
  }

  if (!haveCatalog) {
    return;
  }

  // table references
  QVector<TableRef> refs;
  QSet<int> consumed;
  QHash<QString, int> aliasSeen;
  auto parseRef = [&](int &i, bool allowParen) {
    // returns after the ref (and alias) at i
    if (i >= toks.size() || !toks[i].isName()) {
      return;
    }
    const int first = i;
    QStringList parts;
    parts << toks[i].text;
    consumed.insert(i);
    ++i;
    while (i + 1 < toks.size() && toks[i].isPunct('.') && toks[i + 1].isName()) {
      consumed.insert(i + 1);
      parts << toks[i + 1].text;
      i += 2;
    }
    if (!allowParen && i < toks.size() && toks[i].isPunct('(')) {
      return; // table function
    }
    bool known = true;
    const DbTableInfo *t = nullptr;
    const bool local = toks[first].text.startsWith('#') || toks[first].text.startsWith('@');
    if (local || (parts.size() == 1 && cte.contains(parts[0].toLower()))) {
      known = false;
    } else {
      t = resolve(*schema, parts, &known);
    }
    if (!t && known) {
      QString msg = QStringLiteral("Unknown table or view '%1'").arg(parts.join('.'));
      int best = 3;
      QString hint;
      for (const DbTableInfo &c : schema->tables) {
        const int d = editDistance(c.name, parts.last());
        if (d < best) {
          best = d;
          hint = c.name;
        }
      }
      if (!hint.isEmpty() && parts.last().size() > 2) {
        msg += QStringLiteral(" (did you mean '%1'?)").arg(hint);
      }
      SqlIssue is;
      is.severity = SqlIssue::Severity::Warning;
      is.start = toks[first].start;
      is.length = toks[i - 1].start + toks[i - 1].length - toks[first].start;
      is.message = msg;
      out.append(is);
    }
    QString alias = parts.last().toLower();
    int aliasTok = -1;
    if (i < toks.size() && toks[i].is("as") && i + 1 < toks.size() && toks[i + 1].isName()) {
      alias = toks[i + 1].text.toLower();
      aliasTok = i + 1;
      i += 2;
    } else if (i < toks.size() && toks[i].isName() &&
               !(toks[i].type == Tok::Word && stopWords().contains(toks[i].text.toLower()))) {
      alias = toks[i].text.toLower();
      aliasTok = i;
      ++i;
    }
    if (aliasTok >= 0) {
      consumed.insert(aliasTok);
      if (aliasSeen.contains(alias)) {
        add(out, SqlIssue::Severity::Warning, toks[aliasTok],
            QStringLiteral("Alias '%1' is used more than once").arg(toks[aliasTok].text));
      }
    }
    aliasSeen.insert(alias, 1);
    TableRef r;
    r.alias = alias;
    r.table = t;
    refs.append(r);
    if (t && alias != t->name.toLower()) {
      TableRef byName;
      byName.alias = t->name.toLower();
      byName.table = t;
      refs.append(byName);
    }
  };

  for (int i = 0; i < toks.size();) {
    const Tok &t = toks[i];
    if (t.is("from") || t.is("join") || t.is("update") ||
        (t.is("into") && i > 0 && toks[i - 1].is("insert"))) {
      const bool fromList = t.is("from");
      ++i;
      if (i < toks.size() && toks[i].is("only")) {
        ++i;
      }
      parseRef(i, t.is("into"));
      while (fromList && i < toks.size() && toks[i].isPunct(',')) {
        ++i;
        parseRef(i, false);
      }
    } else {
      ++i;
    }
  }

  // qualified columns: alias.column
  for (int i = 0; i + 2 < toks.size(); ++i) {
    if (!toks[i].isName() || !toks[i + 1].isPunct('.') || consumed.contains(i) ||
        (i > 0 && toks[i - 1].isPunct('.'))) {
      continue;
    }
    const Tok &colTok = toks[i + 2];
    if (!colTok.isName() && !colTok.isPunct('*')) {
      continue;
    }
    if (i + 3 < toks.size() && toks[i + 3].isPunct('.')) {
      continue; // schema.table.column and friends
    }
    const QString q = toks[i].text.toLower();
    const TableRef *hit = nullptr;
    for (const TableRef &r : refs) {
      if (r.alias == q) {
        hit = &r;
        break;
      }
    }
    if (!hit || !hit->table || colTok.isPunct('*')) {
      continue;
    }
    bool found = false;
    for (const DbColumnInfo &c : hit->table->columns) {
      if (c.name.compare(colTok.text, Qt::CaseInsensitive) == 0) {
        found = true;
        break;
      }
    }
    if (!found && !hit->table->columns.isEmpty()) {
      QString msg = QStringLiteral("Table '%1' has no column '%2'")
                        .arg(hit->table->name, colTok.text);
      int best = 3;
      QString hint;
      for (const DbColumnInfo &c : hit->table->columns) {
        const int d = editDistance(c.name, colTok.text);
        if (d < best) {
          best = d;
          hint = c.name;
        }
      }
      if (!hint.isEmpty()) {
        msg += QStringLiteral(" (did you mean '%1'?)").arg(hint);
      }
      add(out, SqlIssue::Severity::Warning, colTok, msg);
    }
  }

  // INSERT INTO t (a, b) VALUES (x)
  if (toks[0].is("insert") && toks.size() > 4 && !refs.isEmpty()) {
    int i = 0;
    while (i < toks.size() && !toks[i].is("into")) {
      ++i;
    }
    ++i;
    while (i < toks.size() && !toks[i].isPunct('(') && !toks[i].is("values") &&
           !toks[i].is("select")) {
      ++i;
    }
    if (i < toks.size() && toks[i].isPunct('(')) {
      QVector<int> cols;
      int j = i + 1;
      int ncols = 0;
      bool expectName = true;
      for (; j < toks.size() && !toks[j].isPunct(')'); ++j) {
        if (toks[j].isPunct(',')) {
          expectName = true;
        } else if (expectName && toks[j].isName()) {
          ++ncols;
          cols << j;
          expectName = false;
        }
      }
      const DbTableInfo *tbl = refs.first().table;
      if (tbl) {
        for (int c : cols) {
          bool found = false;
          for (const DbColumnInfo &ci : tbl->columns) {
            if (ci.name.compare(toks[c].text, Qt::CaseInsensitive) == 0) {
              found = true;
              break;
            }
          }
          if (!found) {
            add(out, SqlIssue::Severity::Warning, toks[c],
                QStringLiteral("Table '%1' has no column '%2'")
                    .arg(tbl->name, toks[c].text));
          }
        }
      }
      ++j;
      if (j < toks.size() && toks[j].is("values") && j + 1 < toks.size() &&
          toks[j + 1].isPunct('(')) {
        int d = 0;
        int count = 1;
        bool empty = true;
        for (int k = j + 1; k < toks.size(); ++k) {
          if (toks[k].isPunct('(')) {
            ++d;
          } else if (toks[k].isPunct(')')) {
            if (--d == 0) {
              break;
            }
          } else if (d == 1 && toks[k].isPunct(',')) {
            ++count;
          } else if (d == 1) {
            empty = false;
          }
        }
        if (!empty && count != ncols && ncols > 0) {
          add(out, SqlIssue::Severity::Error, toks[j],
              QStringLiteral("%1 column(s) listed but %2 value(s) given")
                  .arg(ncols)
                  .arg(count));
        }
      }
    }
  }
}

} // namespace

namespace SqlLint {

QVector<SqlIssue> lint(const QString &sql, DbEngine engine,
                       const DbSchema *schema) {
  Tokenized tk = tokenize(sql);
  QVector<SqlIssue> out = tk.issues;
  QVector<Tok> current;
  for (const Tok &t : tk.toks) {
    if (t.isPunct(';')) {
      lintStatement(current, engine, schema, out);
      current.clear();
    } else {
      current.append(t);
    }
  }
  lintStatement(current, engine, schema, out);
  std::sort(out.begin(), out.end(), [](const SqlIssue &a, const SqlIssue &b) {
    return a.start < b.start;
  });
  return out;
}

} // namespace SqlLint
