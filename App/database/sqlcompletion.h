#ifndef SQLCOMPLETION_H
#define SQLCOMPLETION_H

#include "dbcatalog.h"

#include <QHash>
#include <QString>
#include <QVector>

struct SqlSuggestion {
  enum class Kind { Keyword, Type, Function, Schema, Table, View, Column };
  QString label;
  QString insertText;
  QString detail;
  Kind kind = Kind::Keyword;
  int priority = 100;
};

namespace SqlCompletion {

QHash<QString, QString> aliasMap(const QString &statement);

QStringList referencedTables(const QString &statement);

QVector<SqlSuggestion> suggest(const DbSchema *schema, DbEngine engine,
                               const QString &statementBeforeCursor,
                               const QString &fullStatement,
                               const QString &prefix);

QString identifierForInsert(DbEngine engine, const QString &name);

} // namespace SqlCompletion

#endif
