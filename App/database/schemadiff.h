#ifndef SCHEMADIFF_H
#define SCHEMADIFF_H

#include "dbcatalog.h"

#include <QString>
#include <QStringList>
#include <QVector>

struct DbColumnChange {
  QString name;
  DbColumnInfo before;
  DbColumnInfo after;
  bool typeChanged = false;
  bool nullabilityChanged = false;
};

struct DbTableDiff {
  QString schema;
  QString name;
  QVector<DbColumnInfo> addedColumns;
  QVector<DbColumnInfo> droppedColumns;
  QVector<DbColumnChange> changedColumns;
  bool primaryKeyChanged = false;
  QStringList oldPrimaryKey;
  QStringList newPrimaryKey;

  bool isEmpty() const {
    return addedColumns.isEmpty() && droppedColumns.isEmpty() &&
           changedColumns.isEmpty() && !primaryKeyChanged;
  }
};

struct DbSchemaDiff {
  QVector<DbTableInfo> addedTables;
  QVector<DbTableInfo> droppedTables;
  QVector<DbTableDiff> changedTables;

  bool isEmpty() const {
    return addedTables.isEmpty() && droppedTables.isEmpty() &&
           changedTables.isEmpty();
  }
};

namespace SchemaDiff {

DbSchemaDiff diff(const DbSchema &from, const DbSchema &to);

QStringList summary(const DbSchemaDiff &diff);

QString migrationSql(DbEngine engine, const DbSchemaDiff &diff,
                     bool includeDrops = true);

QString schemaDdl(DbEngine engine, const DbSchema &schema);

} // namespace SchemaDiff

#endif
