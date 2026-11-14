#ifndef SQLQUERYCONTEXT_H
#define SQLQUERYCONTEXT_H

#include "dbcatalog.h"

namespace SqlQueryContext {
enum class SourceKind { Table, View, Cte, Subquery };
struct Source {
  QString name;
  QString schema;
  QString alias;
  QString qualifiedName;
  QVector<DbColumnInfo> columns;
  SourceKind kind = SourceKind::Table;
  bool columnsKnown = false;
  int start = 0;
  int aliasStart = -1;
};
struct Context {
  QVector<Source> sources;
  QVector<Source> commonTables;
  QVector<DbColumnInfo> outputAliases;
  QString keyword;
  int scopeStart = 0;
  bool inCommentOrString = false;
};
struct IdentifierPrefix {
  QString text;
  int start = 0;
  int end = 0;
};

Context analyze(const QString &statement, int cursor, DbEngine engine,
                const DbSchema *schema = nullptr);
QVector<Context> analyzePositions(const QString &statement,
                                  const QVector<int> &cursors, DbEngine engine,
                                  const DbSchema *schema = nullptr);
IdentifierPrefix identifierPrefix(const QString &text, int cursor);
QStringList qualifierBefore(const QString &text, int identifierStart);
} // namespace SqlQueryContext
#endif
