#include "schemadiff.h"

#include <QHash>

namespace {

QString key(const QString &schema, const QString &name) {
  return schema.toLower() + QLatin1Char('\x1f') + name.toLower();
}

QString typeOrText(const DbColumnInfo &c) {
  return c.type.isEmpty() ? QStringLiteral("TEXT") : c.type;
}

QString tableName(DbEngine e, const QString &schema, const QString &name) {
  DbTableInfo t;
  t.schema = schema;
  t.name = name;
  return DbCatalog::qualifiedName(e, t);
}

QString normType(const QString &t) { return t.simplified().toLower(); }

} // namespace

namespace SchemaDiff {

DbSchemaDiff diff(const DbSchema &from, const DbSchema &to) {
  DbSchemaDiff d;
  QHash<QString, const DbTableInfo *> fromMap;
  QHash<QString, const DbTableInfo *> toMap;
  for (const DbTableInfo &t : from.tables) {
    fromMap.insert(key(t.schema, t.name), &t);
  }
  for (const DbTableInfo &t : to.tables) {
    toMap.insert(key(t.schema, t.name), &t);
  }
  for (const DbTableInfo &t : to.tables) {
    if (!fromMap.contains(key(t.schema, t.name))) {
      d.addedTables.append(t);
    }
  }
  for (const DbTableInfo &t : from.tables) {
    const auto it = toMap.constFind(key(t.schema, t.name));
    if (it == toMap.constEnd()) {
      d.droppedTables.append(t);
      continue;
    }
    const DbTableInfo &b = **it;
    if (t.isView || b.isView) {
      continue;
    }
    DbTableDiff td;
    td.schema = t.schema;
    td.name = t.name;
    QHash<QString, const DbColumnInfo *> oldCols;
    QHash<QString, const DbColumnInfo *> newCols;
    for (const DbColumnInfo &c : t.columns) {
      oldCols.insert(c.name.toLower(), &c);
    }
    for (const DbColumnInfo &c : b.columns) {
      newCols.insert(c.name.toLower(), &c);
    }
    for (const DbColumnInfo &c : b.columns) {
      const auto o = oldCols.constFind(c.name.toLower());
      if (o == oldCols.constEnd()) {
        td.addedColumns.append(c);
        continue;
      }
      DbColumnChange ch;
      ch.name = c.name;
      ch.before = **o;
      ch.after = c;
      ch.typeChanged = normType(ch.before.type) != normType(c.type);
      ch.nullabilityChanged = ch.before.nullable != c.nullable;
      if (ch.typeChanged || ch.nullabilityChanged) {
        td.changedColumns.append(ch);
      }
    }
    for (const DbColumnInfo &c : t.columns) {
      if (!newCols.contains(c.name.toLower())) {
        td.droppedColumns.append(c);
      }
    }
    td.oldPrimaryKey = t.primaryKey();
    td.newPrimaryKey = b.primaryKey();
    td.primaryKeyChanged = td.oldPrimaryKey.join('\n').toLower() !=
                           td.newPrimaryKey.join('\n').toLower();
    if (!td.isEmpty()) {
      d.changedTables.append(td);
    }
  }
  return d;
}

QStringList summary(const DbSchemaDiff &d) {
  QStringList out;
  for (const DbTableInfo &t : d.addedTables) {
    out << QStringLiteral("+ %1 %2").arg(t.isView ? "view" : "table", t.name);
  }
  for (const DbTableInfo &t : d.droppedTables) {
    out << QStringLiteral("- %1 %2").arg(t.isView ? "view" : "table", t.name);
  }
  for (const DbTableDiff &t : d.changedTables) {
    QStringList parts;
    for (const DbColumnInfo &c : t.addedColumns) {
      parts << QStringLiteral("+col %1").arg(c.name);
    }
    for (const DbColumnInfo &c : t.droppedColumns) {
      parts << QStringLiteral("-col %1").arg(c.name);
    }
    for (const DbColumnChange &c : t.changedColumns) {
      parts << QStringLiteral("~col %1").arg(c.name);
    }
    if (t.primaryKeyChanged) {
      parts << QStringLiteral("~primary key");
    }
    out << QStringLiteral("~ %1: %2").arg(t.name, parts.join(", "));
  }
  return out;
}

static QString columnDef(DbEngine e, const DbColumnInfo &c) {
  return QStringLiteral("%1 %2%3").arg(
      DbCatalog::quoteIdentifier(e, c.name), typeOrText(c),
      c.nullable ? QString() : QStringLiteral(" NOT NULL"));
}

static QString pkList(DbEngine e, const QStringList &cols) {
  QStringList q;
  for (const QString &c : cols) {
    q << DbCatalog::quoteIdentifier(e, c);
  }
  return q.join(QStringLiteral(", "));
}

QString migrationSql(DbEngine e, const DbSchemaDiff &d, bool includeDrops) {
  QStringList stmts;
  for (const DbTableInfo &t : d.addedTables) {
    stmts << (t.isView ? QStringLiteral("-- view %1 must be created manually")
                             .arg(t.name)
                       : DbCatalog::createTableSql(e, t));
  }
  QStringList drops;
  for (const DbTableDiff &t : d.changedTables) {
    const QString tn = tableName(e, t.schema, t.name);
    for (const DbColumnInfo &c : t.addedColumns) {
      stmts << QStringLiteral("ALTER TABLE %1 ADD %2%3;")
                   .arg(tn,
                        e == DbEngine::PostgreSql || e == DbEngine::MySql ||
                                e == DbEngine::Sqlite
                            ? QStringLiteral("COLUMN ")
                            : QString(),
                        columnDef(e, c));
    }
    for (const DbColumnChange &c : t.changedColumns) {
      const QString col = DbCatalog::quoteIdentifier(e, c.name);
      switch (e) {
      case DbEngine::PostgreSql:
        if (c.typeChanged) {
          stmts << QStringLiteral("ALTER TABLE %1 ALTER COLUMN %2 TYPE %3;")
                       .arg(tn, col, typeOrText(c.after));
        }
        if (c.nullabilityChanged) {
          stmts << QStringLiteral("ALTER TABLE %1 ALTER COLUMN %2 %3 NOT NULL;")
                       .arg(tn, col,
                            c.after.nullable ? QStringLiteral("DROP")
                                             : QStringLiteral("SET"));
        }
        break;
      case DbEngine::MySql:
        stmts << QStringLiteral("ALTER TABLE %1 MODIFY COLUMN %2;")
                     .arg(tn, columnDef(e, c.after));
        break;
      case DbEngine::SqlServer:
        stmts << QStringLiteral("ALTER TABLE %1 ALTER COLUMN %2 %3 %4;")
                     .arg(tn, col, typeOrText(c.after),
                          c.after.nullable ? QStringLiteral("NULL")
                                           : QStringLiteral("NOT NULL"));
        break;
      case DbEngine::Sqlite:
        stmts << QStringLiteral("-- SQLite cannot alter column %1 of %2; "
                                "rebuild the table")
                     .arg(c.name, t.name);
        break;
      }
    }
    if (t.primaryKeyChanged) {
      if (e == DbEngine::Sqlite) {
        stmts << QStringLiteral(
                     "-- SQLite cannot change the primary key of %1; "
                     "rebuild the table")
                     .arg(t.name);
      } else {
        if (!t.oldPrimaryKey.isEmpty()) {
          stmts << (e == DbEngine::MySql
                        ? QStringLiteral("ALTER TABLE %1 DROP PRIMARY KEY;")
                              .arg(tn)
                        : QStringLiteral("ALTER TABLE %1 DROP CONSTRAINT %2;")
                              .arg(tn,
                                   DbCatalog::quoteIdentifier(
                                       e, t.name + QStringLiteral("_pkey"))));
        }
        if (!t.newPrimaryKey.isEmpty()) {
          stmts << QStringLiteral("ALTER TABLE %1 ADD PRIMARY KEY (%2);")
                       .arg(tn, pkList(e, t.newPrimaryKey));
        }
      }
    }
    for (const DbColumnInfo &c : t.droppedColumns) {
      drops << QStringLiteral("ALTER TABLE %1 DROP COLUMN %2;")
                   .arg(tn, DbCatalog::quoteIdentifier(e, c.name));
    }
  }
  if (includeDrops) {
    stmts << drops;
    for (const DbTableInfo &t : d.droppedTables) {
      stmts << QStringLiteral("DROP %1 %2;")
                   .arg(t.isView ? "VIEW" : "TABLE",
                        DbCatalog::qualifiedName(e, t));
    }
  } else if (!drops.isEmpty() || !d.droppedTables.isEmpty()) {
    stmts << QStringLiteral("-- %1 destructive change(s) omitted")
                 .arg(drops.size() + d.droppedTables.size());
  }
  return stmts.join(QStringLiteral("\n\n"));
}

QString schemaDdl(DbEngine e, const DbSchema &schema) {
  QStringList out;
  for (const DbTableInfo &t : schema.tables) {
    out << DbCatalog::createTableSql(e, t);
  }
  return out.join(QStringLiteral("\n\n"));
}

} // namespace SchemaDiff
