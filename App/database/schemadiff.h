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
  QVector<DbTableInfo> addedTables;   // present only in 'to'
  QVector<DbTableInfo> droppedTables; // present only in 'from'
  QVector<DbTableDiff> changedTables;

  bool isEmpty() const {
    return addedTables.isEmpty() && droppedTables.isEmpty() &&
           changedTables.isEmpty();
  }
};

namespace SchemaDiff {

// Describes what has to change to turn 'from' into 'to'. Views are compared by
// presence only.
DbSchemaDiff diff(const DbSchema &from, const DbSchema &to);

// Human readable summary ("+ table x", "~ x: +col a, -col b, ~col c").
QStringList summary(const DbSchemaDiff &diff);

// Migration script for the engine. Destructive statements (DROP) are emitted
// last and can be left out with includeDrops = false.
QString migrationSql(DbEngine engine, const DbSchemaDiff &diff,
                     bool includeDrops = true);

// Full DDL for every table of the schema.
QString schemaDdl(DbEngine engine, const DbSchema &schema);

} // namespace SchemaDiff

#endif
