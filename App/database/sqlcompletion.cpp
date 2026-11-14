#include "sqlcompletion.h"
#include "sqlkeywords.h"
#include "sqlquerycontext.h"
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace {
QString prefixName(QString name) {
  if (name.startsWith('"') || name.startsWith('`') || name.startsWith('[')) {
    const QChar quote = name[0], close = quote == '[' ? QChar(']') : quote;
    name.remove(0, 1);
    if (name.endsWith(close))
      name.chop(1);
    name.replace(QString(close) + close, QString(close));
  }
  return name;
}
int rank(const QString &candidate, const QString &prefix) {
  if (prefix.isEmpty() || candidate.startsWith(prefix, Qt::CaseSensitive))
    return 0;
  if (candidate.startsWith(prefix, Qt::CaseInsensitive))
    return 1;
  return candidate.contains(prefix, Qt::CaseInsensitive) ? 2 : -1;
}
} // namespace

namespace SqlCompletion {
QString identifierForInsert(DbEngine engine, const QString &name) {
  static const QRegularExpression plain("^[a-z_][a-z0-9_]*$");
  static const QRegularExpression mixed("^[A-Za-z_][A-Za-z0-9_]*$");
  const bool quote =
      SqlKeywords::keywords().contains(name.toUpper()) ||
      !(engine == DbEngine::PostgreSql ? plain : mixed).match(name).hasMatch();
  return quote ? DbCatalog::quoteIdentifier(engine, name) : name;
}

QHash<QString, QString> aliasMap(const QString &statement) {
  QHash<QString, QString> aliases;
  const auto context = SqlQueryContext::analyze(statement, statement.size(),
                                                DbEngine::PostgreSql);
  for (const auto &source : context.sources) {
    if (source.name.isEmpty())
      continue;
    aliases[source.name.toLower()] = source.qualifiedName;
    if (!source.alias.isEmpty())
      aliases[source.alias.toLower()] = source.qualifiedName;
  }
  return aliases;
}
QStringList referencedTables(const QString &statement) {
  QStringList names;
  for (const auto &name : aliasMap(statement))
    if (!names.contains(name))
      names << name;
  return names;
}

QVector<SqlSuggestion> suggest(const DbSchema *schema, DbEngine engine,
                               const QString &before, const QString &full,
                               const QString &prefixIn) {
  QVector<SqlSuggestion> suggestions;
  const QString prefix = prefixName(prefixIn);
  const int prefixStart = qMax(0, int(before.size() - prefixIn.size()));
  const auto context =
      SqlQueryContext::analyze(full, prefixStart, engine, schema);
  if (context.inCommentOrString)
    return suggestions;
  const auto qualifier = SqlQueryContext::qualifierBefore(before, prefixStart);
  const bool quotedPrefix = prefixIn.startsWith('"') ||
                            prefixIn.startsWith('`') ||
                            prefixIn.startsWith('[');
  auto identifier = [&](const QString &name) {
    if (!quotedPrefix)
      return identifierForInsert(engine, name);
    const QChar open = prefixIn[0], close = open == '[' ? QChar(']') : open;
    QString escaped = name;
    escaped.replace(QString(close), QString(close) + close);
    return QString(open) + escaped + close;
  };
  QSet<QString> seen;
  auto add = [&](const QString &label, const QString &insert,
                 const QString &detail, SqlSuggestion::Kind kind,
                 int priority) {
    if (label.isEmpty())
      return;
    const int match = rank(label, prefix);
    const QString key = QString::number(int(kind)) + ':' + insert;
    if (match < 0 || seen.contains(key))
      return;
    seen.insert(key);
    suggestions << SqlSuggestion{label, insert, detail, kind,
                                 priority + match * 10};
  };
  auto columns = [&](const SqlQueryContext::Source &source, bool showSource,
                     int priority, const QHash<QString, int> &duplicates = {}) {
    for (const auto &column : source.columns) {
      QString detail = column.type;
      if (column.primaryKey)
        detail += QStringLiteral(" · key");
      if (showSource)
        detail = (source.alias.isEmpty() ? source.name : source.alias) + " · " +
                 detail;
      QString insert = identifier(column.name);
      if (duplicates.value(column.name.toLower()) > 1 &&
          !source.alias.isEmpty())
        insert = identifierForInsert(engine, source.alias) + '.' + insert;
      add(column.name, insert, detail, SqlSuggestion::Kind::Column, priority);
    }
  };
  if (!qualifier.isEmpty()) {
    const QString name = qualifier.last();
    const SqlQueryContext::Source *match = nullptr;
    for (const auto &source : context.sources) {
      const bool hit =
          qualifier.size() == 1
              ? source.alias.compare(name, Qt::CaseInsensitive) == 0
              : source.alias == source.name &&
                    source.name.compare(name, Qt::CaseInsensitive) == 0 &&
                    source.schema.compare(qualifier[qualifier.size() - 2],
                                          Qt::CaseInsensitive) == 0;
      if (hit) {
        match = &source;
        break;
      }
    }
    if (!match && qualifier.size() == 1)
      for (const auto &cte : context.commonTables)
        if (cte.name.compare(name, Qt::CaseInsensitive) == 0) {
          match = &cte;
          break;
        }
    const bool tablePosition =
        context.keyword == "FROM" || context.keyword == "JOIN" ||
        context.keyword == "INTO" || context.keyword == "UPDATE" ||
        context.keyword == "TABLE";
    if (match && !(tablePosition && !match->columnsKnown &&
                   match->kind == SqlQueryContext::SourceKind::Table)) {
      columns(*match, false, 10);
      std::stable_sort(
          suggestions.begin(), suggestions.end(),
          [](const auto &a, const auto &b) { return a.priority < b.priority; });
      return suggestions;
    } else if (schema) {
      const DbTableInfo *table =
          qualifier.size() > 1
              ? schema->find(qualifier[qualifier.size() - 2], name)
              : schema->findByName(name);
      if (table) {
        SqlQueryContext::Source source;
        source.columns = table->columns;
        columns(source, false, 10);
      } else
        for (const auto &candidate : schema->tables)
          if (candidate.schema.compare(name, Qt::CaseInsensitive) == 0)
            add(candidate.name, identifier(candidate.name),
                candidate.isView ? "view" : "table",
                candidate.isView ? SqlSuggestion::Kind::View
                                 : SqlSuggestion::Kind::Table,
                10);
    }
  } else {
    const QString keyword = context.keyword;
    const bool tablesWanted = keyword == "FROM" || keyword == "JOIN" ||
                              keyword == "INTO" || keyword == "UPDATE" ||
                              keyword == "TABLE";
    const bool columnsWanted = !tablesWanted && !keyword.isEmpty();
    QSet<QString> commonNames;
    for (const auto &cte : context.commonTables) {
      commonNames.insert(cte.name.toLower());
      add(cte.name, identifier(cte.name),
          QStringLiteral("CTE · %1 columns").arg(cte.columns.size()),
          SqlSuggestion::Kind::Table, tablesWanted ? 5 : 55);
    }
    if (schema) {
      QHash<QString, int> names;
      for (const auto &table : schema->tables)
        ++names[table.name.toLower()];
      QSet<QString> schemas;
      for (const auto &table : schema->tables) {
        if (!commonNames.contains(table.name.toLower())) {
          const QString insert =
              names[table.name.toLower()] > 1 && !table.schema.isEmpty()
                  ? identifierForInsert(engine, table.schema) + '.' +
                        identifier(table.name)
                  : identifier(table.name);
          add(table.name, insert,
              (table.isView ? "view" : "table") +
                  (table.schema.isEmpty() ? QString() : " · " + table.schema),
              table.isView ? SqlSuggestion::Kind::View
                           : SqlSuggestion::Kind::Table,
              tablesWanted ? 10 : 60);
        }
        if (!table.schema.isEmpty() && !schemas.contains(table.schema) &&
            engine != DbEngine::Sqlite) {
          schemas.insert(table.schema);
          add(table.schema, identifier(table.schema), "schema",
              SqlSuggestion::Kind::Schema, tablesWanted ? 20 : 70);
        }
      }
    }
    if (columnsWanted || keyword.isEmpty()) {
      QHash<QString, int> duplicates;
      for (const auto &source : context.sources)
        for (const auto &column : source.columns)
          ++duplicates[column.name.toLower()];
      for (const auto &source : context.sources) {
        columns(source, true, 10, duplicates);
        if (!source.alias.isEmpty() && source.alias != source.name)
          add(source.alias, identifier(source.alias) + '.',
              source.kind == SqlQueryContext::SourceKind::Subquery
                  ? "subquery alias"
                  : "table alias · " + source.name,
              SqlSuggestion::Kind::Table, 30);
      }
      const bool aliasesAllowed =
          keyword == "ORDER BY" ||
          (keyword == "GROUP BY" && engine != DbEngine::SqlServer) ||
          (keyword == "HAVING" &&
           (engine == DbEngine::Sqlite || engine == DbEngine::MySql));
      if (aliasesAllowed)
        for (const auto &column : context.outputAliases)
          add(column.name, identifier(column.name), "SELECT alias",
              SqlSuggestion::Kind::Column, 5);
    }
    if (!quotedPrefix) {
      const bool lower = !prefixIn.isEmpty() && prefixIn == prefixIn.toLower();
      auto cased = [&](const QString &name) {
        return lower ? name.toLower() : name;
      };
      for (const auto &name : SqlKeywords::keywords())
        add(cased(name), cased(name), "keyword", SqlSuggestion::Kind::Keyword,
            40);
      for (const auto &name : SqlKeywords::functions())
        add(cased(name), cased(name) + '(', "function",
            SqlSuggestion::Kind::Function, 45);
      for (const auto &name : SqlKeywords::types())
        add(cased(name), cased(name), "type", SqlSuggestion::Kind::Type, 50);
    }
  }
  std::stable_sort(suggestions.begin(), suggestions.end(),
                   [](const auto &a, const auto &b) {
                     if (a.priority != b.priority)
                       return a.priority < b.priority;
                     return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
                   });
  return suggestions;
}
} // namespace SqlCompletion
