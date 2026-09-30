#ifndef DBCATALOG_H
#define DBCATALOG_H

#include "dbtypes.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

struct DbColumnInfo {
  QString name;
  QString type;
  bool nullable = true;
  bool primaryKey = false;
};

struct DbTableInfo {
  QString schema;
  QString name;
  bool isView = false;
  QVector<DbColumnInfo> columns;

  QStringList primaryKey() const;
};

struct DbSchema {
  QVector<DbTableInfo> tables;

  bool isEmpty() const { return tables.isEmpty(); }
  QStringList schemaNames() const;
  QVector<const DbTableInfo *> tablesIn(const QString &schema) const;
  const DbTableInfo *find(const QString &schema, const QString &name) const;

  const DbTableInfo *findByName(const QString &name) const;
  int columnCount() const;
};

namespace DbCatalog {

QString schemaQuery(DbEngine engine);
QString databaseListQuery(DbEngine engine);

QString useDatabaseStatement(DbEngine engine, const QString &database);

DbSchema buildSchema(const DbResultSet &rows);
QStringList parseDatabaseList(const DbResultSet &rows);

QString quoteIdentifier(DbEngine engine, const QString &name);
QString qualifiedName(DbEngine engine, const DbTableInfo &table);

QString selectTopSql(DbEngine engine, const DbTableInfo &table, int limit);
QString countSql(DbEngine engine, const DbTableInfo &table);
QString selectColumnsSql(DbEngine engine, const DbTableInfo &table);
QString insertTemplateSql(DbEngine engine, const DbTableInfo &table);
QString updateTemplateSql(DbEngine engine, const DbTableInfo &table);
QString createTableSql(DbEngine engine, const DbTableInfo &table);

QString explainPrefix(DbEngine engine);

} // namespace DbCatalog

#endif
