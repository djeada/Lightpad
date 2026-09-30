#include "sqlcompletion.h"

#include "sqlkeywords.h"
#include "sqlstatementsplitter.h"

#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace {

const QSet<QString> &nonAliasWords() {
  static const QSet<QString> words = {
      "where",   "on",        "inner", "left",   "right",  "full",      "cross",
      "join",    "group",     "order", "having", "limit",  "union",     "set",
      "using",   "natural",   "outer", "values", "select", "from",      "into",
      "offset",  "fetch",     "for",   "window", "except", "intersect", "as",
      "lateral", "returning", "top",   "with"};
  return words;
}

QString unquote(QString s) {
  s = s.trimmed();
  if (s.size() >= 2 && ((s.startsWith('"') && s.endsWith('"')) ||
                        (s.startsWith('`') && s.endsWith('`')) ||
                        (s.startsWith('[') && s.endsWith(']')))) {
    return s.mid(1, s.size() - 2);
  }
  return s;
}

QString qualifierBefore(const QString &text, int end) {
  int i = end;

  if (i <= 0 || text[i - 1] != '.') {
    return {};
  }
  --i;
  int stop = i;
  while (i > 0) {
    const QChar c = text[i - 1];
    if (c == '"' || c == '`' || c == ']') {
      const QChar open = c == ']' ? QChar('[') : c;
      int j = text.lastIndexOf(open, i - 2);
      if (j < 0) {
        break;
      }
      i = j;
    } else if (c.isLetterOrNumber() || c == '_' || c == '$' || c == '.') {
      --i;
    } else {
      break;
    }
  }
  return text.mid(i, stop - i);
}

int rank(const QString &candidate, const QString &prefix) {
  if (prefix.isEmpty()) {
    return 0;
  }
  if (candidate.startsWith(prefix, Qt::CaseSensitive)) {
    return 0;
  }
  if (candidate.startsWith(prefix, Qt::CaseInsensitive)) {
    return 1;
  }
  if (candidate.contains(prefix, Qt::CaseInsensitive)) {
    return 2;
  }
  return -1;
}

} // namespace

namespace SqlCompletion {

QString identifierForInsert(DbEngine engine, const QString &name) {
  static const QRegularExpression plain(QStringLiteral("^[a-z_][a-z0-9_]*$"));
  static const QRegularExpression plainMixed(
      QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
  const QString lower = name.toLower();
  const bool reserved = SqlKeywords::keywords().contains(name.toUpper());

  const bool needsQuote =
      reserved ||
      (engine == DbEngine::PostgreSql ? !plain.match(name).hasMatch()
                                      : !plainMixed.match(name).hasMatch());
  Q_UNUSED(lower)
  return needsQuote ? DbCatalog::quoteIdentifier(engine, name) : name;
}

QHash<QString, QString> aliasMap(const QString &statement) {
  QHash<QString, QString> map;
  const QString sql = SqlStatementSplitter::stripComments(statement);
  static const QRegularExpression re(
      QStringLiteral("\\b(?:FROM|JOIN|UPDATE|INTO|TABLE)\\s+"
                     "((?:(?:\"[^\"]+\"|`[^`]+`|\\[[^\\]]+\\]|[A-Za-z_][\\w$]*)"
                     "\\.?){1,3})"
                     "(?:\\s+(?:AS\\s+)?(\"[^\"]+\"|`[^`]+`|\\[[^\\]]+\\]|"
                     "[A-Za-z_][\\w$]*))?"),
      QRegularExpression::CaseInsensitiveOption);
  auto it = re.globalMatch(sql);
  while (it.hasNext()) {
    const auto m = it.next();
    const QString table = m.captured(1);
    const QString alias = unquote(m.captured(2));
    QString bare = table;
    const int dot = bare.lastIndexOf('.');
    if (dot >= 0) {
      bare = bare.mid(dot + 1);
    }
    map.insert(unquote(bare).toLower(), table);
    if (!alias.isEmpty() && !nonAliasWords().contains(alias.toLower())) {
      map.insert(alias.toLower(), table);
    }
  }
  return map;
}

QStringList referencedTables(const QString &statement) {
  QStringList out;
  const auto map = aliasMap(statement);
  for (auto it = map.begin(); it != map.end(); ++it) {
    if (!out.contains(it.value())) {
      out << it.value();
    }
  }
  return out;
}

QVector<SqlSuggestion> suggest(const DbSchema *schema, DbEngine engine,
                               const QString &before, const QString &full,
                               const QString &prefixIn) {
  QVector<SqlSuggestion> out;
  const QString prefix = unquote(prefixIn);
  const int prefixStart = before.size() - prefixIn.size();
  const QString qualifier = qualifierBefore(before, prefixStart);

  auto add = [&](const QString &label, const QString &insert,
                 const QString &detail, SqlSuggestion::Kind kind,
                 int basePrio) {
    const int r = rank(label, prefix);
    if (r < 0) {
      return;
    }
    SqlSuggestion s;
    s.label = label;
    s.insertText = insert;
    s.detail = detail;
    s.kind = kind;
    s.priority = basePrio + r * 10;
    out.append(s);
  };

  auto addColumns = [&](const DbTableInfo &t, bool withTable, int prio) {
    for (const DbColumnInfo &c : t.columns) {
      QString detail = c.type;
      if (c.primaryKey) {
        detail += QStringLiteral(" · key");
      }
      if (withTable) {
        detail = t.name + QStringLiteral(" · ") + detail;
      }
      add(c.name, identifierForInsert(engine, c.name), detail,
          SqlSuggestion::Kind::Column, prio);
    }
  };

  if (!qualifier.isEmpty()) {
    if (!schema) {
      return out;
    }
    const auto aliases = aliasMap(full);
    QString key = unquote(qualifier.section('.', -1)).toLower();
    const DbTableInfo *table = nullptr;
    if (qualifier.contains('.')) {
      table = schema->findByName(qualifier);
    }
    if (!table && aliases.contains(key)) {
      table = schema->findByName(aliases.value(key));
    }
    if (!table) {
      table = schema->findByName(qualifier);
    }
    if (table) {
      addColumns(*table, false, 10);

      std::stable_sort(out.begin(), out.end(),
                       [](const SqlSuggestion &a, const SqlSuggestion &b) {
                         return a.priority < b.priority;
                       });
      return out;
    }

    const QString schemaName = unquote(qualifier.section('.', -1));
    for (const DbTableInfo &t : schema->tables) {
      if (t.schema.compare(schemaName, Qt::CaseInsensitive) == 0) {
        add(t.name, identifierForInsert(engine, t.name),
            t.isView ? QStringLiteral("view") : QStringLiteral("table"),
            t.isView ? SqlSuggestion::Kind::View : SqlSuggestion::Kind::Table,
            10);
      }
    }
    return out;
  }

  static const QRegularExpression lastKeyword(
      QStringLiteral("\\b(SELECT|FROM|JOIN|INTO|UPDATE|TABLE|WHERE|ON|BY|SET|"
                     "HAVING|AND|OR|VALUES|WHEN|THEN|ELSE|DISTINCT|TOP)\\b"
                     "(?![\\s\\S]*\\b(?:SELECT|FROM|JOIN|INTO|UPDATE|TABLE|"
                     "WHERE|ON|BY|SET|HAVING|VALUES)\\b)"),
      QRegularExpression::CaseInsensitiveOption);
  const QString head =
      SqlStatementSplitter::stripComments(before.left(prefixStart));
  const auto lk = lastKeyword.match(head);
  const QString kw = lk.hasMatch() ? lk.captured(1).toUpper() : QString();
  const bool wantsTables = kw == "FROM" || kw == "JOIN" || kw == "INTO" ||
                           kw == "UPDATE" || kw == "TABLE";
  const bool wantsColumns = kw == "SELECT" || kw == "WHERE" || kw == "ON" ||
                            kw == "BY" || kw == "SET" || kw == "HAVING" ||
                            kw == "AND" || kw == "OR" || kw == "WHEN" ||
                            kw == "THEN" || kw == "ELSE" || kw == "DISTINCT" ||
                            kw == "TOP" || kw == "VALUES";

  if (schema) {
    const int tablePrio = wantsTables ? 10 : 60;
    const int schemaPrio = wantsTables ? 20 : 70;
    const int columnPrio = wantsColumns ? 10 : 50;
    for (const DbTableInfo &t : schema->tables) {
      add(t.name, identifierForInsert(engine, t.name),
          (t.isView ? QStringLiteral("view") : QStringLiteral("table")) +
              (t.schema.isEmpty() || engine == DbEngine::Sqlite
                   ? QString()
                   : QStringLiteral(" · ") + t.schema),
          t.isView ? SqlSuggestion::Kind::View : SqlSuggestion::Kind::Table,
          tablePrio);
    }
    if (wantsTables || prefix.size() >= 1) {
      QSet<QString> seen;
      for (const DbTableInfo &t : schema->tables) {
        if (!t.schema.isEmpty() && engine != DbEngine::Sqlite &&
            !seen.contains(t.schema)) {
          seen.insert(t.schema);
          add(t.schema, identifierForInsert(engine, t.schema),
              QStringLiteral("schema"), SqlSuggestion::Kind::Schema,
              schemaPrio);
        }
      }
    }
    if (wantsColumns || kw.isEmpty()) {
      QSet<QString> done;
      for (const QString &ref : referencedTables(full)) {
        if (const DbTableInfo *t = schema->findByName(ref)) {
          const QString id = t->schema + '.' + t->name;
          if (!done.contains(id)) {
            done.insert(id);
            addColumns(*t, true, columnPrio);
          }
        }
      }
    }
  }

  const bool lower = !prefixIn.isEmpty() && prefixIn == prefixIn.toLower();
  auto cased = [&](const QString &w) { return lower ? w.toLower() : w; };
  const int kwPrio = 40;
  for (const QString &k : SqlKeywords::keywords()) {
    add(cased(k), cased(k), QStringLiteral("keyword"),
        SqlSuggestion::Kind::Keyword, kwPrio);
  }
  for (const QString &f : SqlKeywords::functions()) {
    add(cased(f), cased(f) + QStringLiteral("("), QStringLiteral("function"),
        SqlSuggestion::Kind::Function, kwPrio + 5);
  }
  for (const QString &t : SqlKeywords::types()) {
    add(cased(t), cased(t), QStringLiteral("type"), SqlSuggestion::Kind::Type,
        kwPrio + 10);
  }

  std::stable_sort(out.begin(), out.end(),
                   [](const SqlSuggestion &a, const SqlSuggestion &b) {
                     if (a.priority != b.priority) {
                       return a.priority < b.priority;
                     }
                     return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
                   });
  return out;
}

} // namespace SqlCompletion
