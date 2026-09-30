#include "dbcatalog.h"

#include <algorithm>

QStringList DbTableInfo::primaryKey() const {
  QStringList keys;
  for (const DbColumnInfo &col : columns) {
    if (col.primaryKey) {
      keys << col.name;
    }
  }
  return keys;
}

QStringList DbSchema::schemaNames() const {
  QStringList names;
  for (const DbTableInfo &t : tables) {
    if (!names.contains(t.schema)) {
      names << t.schema;
    }
  }
  return names;
}

QVector<const DbTableInfo *> DbSchema::tablesIn(const QString &schema) const {
  QVector<const DbTableInfo *> out;
  for (const DbTableInfo &t : tables) {
    if (t.schema == schema) {
      out.append(&t);
    }
  }
  return out;
}

const DbTableInfo *DbSchema::find(const QString &schema,
                                  const QString &name) const {
  for (const DbTableInfo &t : tables) {
    if (t.schema == schema && t.name == name) {
      return &t;
    }
  }
  return nullptr;
}

const DbTableInfo *DbSchema::findByName(const QString &name) const {
  QString cleaned = name;
  cleaned.remove('"').remove('`').remove('[').remove(']');
  const QStringList parts = cleaned.split('.');
  const QString table = parts.last();
  const QString schema = parts.size() > 1 ? parts[parts.size() - 2] : QString();
  const DbTableInfo *fallback = nullptr;
  for (const DbTableInfo &t : tables) {
    if (t.name.compare(table, Qt::CaseInsensitive) != 0) {
      continue;
    }
    if (!schema.isEmpty()) {
      if (t.schema.compare(schema, Qt::CaseInsensitive) == 0) {
        return &t;
      }
      continue;
    }
    if (!fallback) {
      fallback = &t;
    }
  }
  return fallback;
}

int DbSchema::columnCount() const {
  int n = 0;
  for (const DbTableInfo &t : tables) {
    n += t.columns.size();
  }
  return n;
}

namespace DbCatalog {

QString schemaQuery(DbEngine engine) {
  switch (engine) {
  case DbEngine::Sqlite:
    return QStringLiteral(
        "SELECT 'main', m.name, CASE m.type WHEN 'view' THEN 'VIEW' ELSE "
        "'BASE TABLE' END, p.name, p.type, CASE WHEN p.\"notnull\" = 1 THEN "
        "'NO' ELSE 'YES' END, CASE WHEN p.pk > 0 THEN 'YES' ELSE 'NO' END, "
        "p.cid + 1 FROM sqlite_master m JOIN pragma_table_info(m.name) p WHERE "
        "m.type IN ('table', 'view') AND m.name NOT LIKE 'sqlite_%' ORDER BY "
        "m.name, p.cid");
  case DbEngine::PostgreSql:
    return QStringLiteral(
        "SELECT c.table_schema, c.table_name, t.table_type, c.column_name, "
        "c.data_type, c.is_nullable, CASE WHEN pk.column_name IS NULL THEN "
        "'NO' ELSE 'YES' END, c.ordinal_position FROM "
        "information_schema.columns c JOIN information_schema.tables t ON "
        "t.table_schema = c.table_schema AND t.table_name = c.table_name LEFT "
        "JOIN (SELECT kcu.table_schema, kcu.table_name, kcu.column_name FROM "
        "information_schema.table_constraints tc JOIN "
        "information_schema.key_column_usage kcu ON kcu.constraint_name = "
        "tc.constraint_name AND kcu.table_schema = tc.table_schema AND "
        "kcu.table_name = tc.table_name WHERE tc.constraint_type = 'PRIMARY "
        "KEY') pk ON pk.table_schema = c.table_schema AND pk.table_name = "
        "c.table_name AND pk.column_name = c.column_name WHERE c.table_schema "
        "NOT IN ('pg_catalog', 'information_schema') AND c.table_schema NOT "
        "LIKE 'pg_toast%' ORDER BY c.table_schema, c.table_name, "
        "c.ordinal_position");
  case DbEngine::MySql:
    return QStringLiteral(
        "SELECT c.table_schema, c.table_name, t.table_type, c.column_name, "
        "c.column_type, c.is_nullable, CASE WHEN c.column_key = 'PRI' THEN "
        "'YES' ELSE 'NO' END, c.ordinal_position FROM "
        "information_schema.columns c JOIN information_schema.tables t ON "
        "t.table_schema = c.table_schema AND t.table_name = c.table_name WHERE "
        "c.table_schema NOT IN ('information_schema', 'mysql', "
        "'performance_schema', 'sys') ORDER BY c.table_schema, c.table_name, "
        "c.ordinal_position");
  case DbEngine::SqlServer:
    return QStringLiteral(
        "SELECT s.name, o.name, CASE o.type WHEN 'V' THEN 'VIEW' ELSE 'BASE "
        "TABLE' END, c.name, CASE WHEN ty.name IN ('varchar', 'char', "
        "'varbinary', 'binary') THEN ty.name + '(' + CASE WHEN c.max_length = "
        "-1 THEN 'max' ELSE CAST(c.max_length AS varchar(10)) END + ')' WHEN "
        "ty.name IN ('nvarchar', 'nchar') THEN ty.name + '(' + CASE WHEN "
        "c.max_length = -1 THEN 'max' ELSE CAST(c.max_length / 2 AS "
        "varchar(10)) END + ')' WHEN ty.name IN ('decimal', 'numeric') THEN "
        "ty.name + '(' + CAST(c.precision AS varchar(10)) + ',' + "
        "CAST(c.scale AS varchar(10)) + ')' ELSE ty.name END, CASE "
        "c.is_nullable WHEN 1 THEN 'YES' ELSE 'NO' END, CASE WHEN ic.column_id "
        "IS NULL THEN 'NO' ELSE 'YES' END, c.column_id FROM sys.objects o JOIN "
        "sys.schemas s ON s.schema_id = o.schema_id JOIN sys.columns c ON "
        "c.object_id = o.object_id JOIN sys.types ty ON ty.user_type_id = "
        "c.user_type_id LEFT JOIN sys.indexes i ON i.object_id = o.object_id "
        "AND i.is_primary_key = 1 LEFT JOIN sys.index_columns ic ON "
        "ic.object_id = i.object_id AND ic.index_id = i.index_id AND "
        "ic.column_id = c.column_id WHERE o.type IN ('U', 'V') AND "
        "o.is_ms_shipped = 0 ORDER BY s.name, o.name, c.column_id");
  }
  return {};
}

QString databaseListQuery(DbEngine engine) {
  switch (engine) {
  case DbEngine::PostgreSql:
    return QStringLiteral(
        "SELECT datname FROM pg_database WHERE NOT datistemplate ORDER BY 1");
  case DbEngine::MySql:
    return QStringLiteral("SHOW DATABASES");
  case DbEngine::SqlServer:
    return QStringLiteral("SELECT name FROM sys.databases ORDER BY name");
  case DbEngine::Sqlite:
    break;
  }
  return {};
}

QString useDatabaseStatement(DbEngine engine, const QString &database) {
  switch (engine) {
  case DbEngine::PostgreSql:
    return QStringLiteral("\\c ") + quoteIdentifier(engine, database);
  case DbEngine::MySql:
  case DbEngine::SqlServer:
    return QStringLiteral("USE ") + quoteIdentifier(engine, database);
  case DbEngine::Sqlite:
    break;
  }
  return {};
}

DbSchema buildSchema(const DbResultSet &rows) {
  DbSchema schema;
  DbTableInfo *current = nullptr;
  for (const QVector<QVariant> &row : rows.rows) {
    if (row.size() < 6) {
      continue;
    }
    const QString sch = row[0].toString();
    const QString table = row[1].toString();
    if (!current || current->schema != sch || current->name != table) {
      DbTableInfo info;
      info.schema = sch;
      info.name = table;
      info.isView = row[2].toString().contains(QLatin1String("VIEW"),
                                               Qt::CaseInsensitive);
      schema.tables.append(info);
      current = &schema.tables.last();
    }
    DbColumnInfo col;
    col.name = row[3].toString();
    col.type = row[4].toString();
    col.nullable = row[5].toString().compare(QLatin1String("NO"),
                                             Qt::CaseInsensitive) != 0;
    col.primaryKey =
        row.size() > 6 && row[6].toString().compare(QLatin1String("YES"),
                                                    Qt::CaseInsensitive) == 0;
    current->columns.append(col);
  }
  return schema;
}

QStringList parseDatabaseList(const DbResultSet &rows) {
  QStringList names;
  for (const QVector<QVariant> &row : rows.rows) {
    if (!row.isEmpty() && !row[0].toString().isEmpty()) {
      names << row[0].toString();
    }
  }
  return names;
}

QString quoteIdentifier(DbEngine engine, const QString &name) {
  switch (engine) {
  case DbEngine::MySql: {
    QString q = name;
    q.replace('`', QLatin1String("``"));
    return QLatin1Char('`') + q + QLatin1Char('`');
  }
  case DbEngine::SqlServer: {
    QString q = name;
    q.replace(']', QLatin1String("]]"));
    return QLatin1Char('[') + q + QLatin1Char(']');
  }
  case DbEngine::PostgreSql:
  case DbEngine::Sqlite: {
    QString q = name;
    q.replace('"', QLatin1String("\"\""));
    return QLatin1Char('"') + q + QLatin1Char('"');
  }
  }
  return name;
}

QString qualifiedName(DbEngine engine, const DbTableInfo &table) {

  if (table.schema.isEmpty() || engine == DbEngine::Sqlite) {
    return quoteIdentifier(engine, table.name);
  }
  return quoteIdentifier(engine, table.schema) + QLatin1Char('.') +
         quoteIdentifier(engine, table.name);
}

QString selectTopSql(DbEngine engine, const DbTableInfo &table, int limit) {
  const QString name = qualifiedName(engine, table);
  if (engine == DbEngine::SqlServer) {
    return QStringLiteral("SELECT TOP %1 * FROM %2;").arg(limit).arg(name);
  }
  return QStringLiteral("SELECT * FROM %1 LIMIT %2;").arg(name).arg(limit);
}

QString countSql(DbEngine engine, const DbTableInfo &table) {
  return QStringLiteral("SELECT COUNT(*) AS row_count FROM %1;")
      .arg(qualifiedName(engine, table));
}

static QStringList quotedColumns(DbEngine engine, const DbTableInfo &table) {
  QStringList cols;
  for (const DbColumnInfo &c : table.columns) {
    cols << quoteIdentifier(engine, c.name);
  }
  return cols;
}

QString selectColumnsSql(DbEngine engine, const DbTableInfo &table) {
  return QStringLiteral("SELECT %1\nFROM %2;")
      .arg(quotedColumns(engine, table).join(QStringLiteral(",\n       ")))
      .arg(qualifiedName(engine, table));
}

QString insertTemplateSql(DbEngine engine, const DbTableInfo &table) {
  QStringList placeholders;
  for (const DbColumnInfo &c : table.columns) {
    placeholders << QStringLiteral("/* %1 */ NULL").arg(c.name);
  }
  return QStringLiteral("INSERT INTO %1 (%2)\nVALUES (%3);")
      .arg(qualifiedName(engine, table))
      .arg(quotedColumns(engine, table).join(QStringLiteral(", ")))
      .arg(placeholders.join(QStringLiteral(", ")));
}

QString updateTemplateSql(DbEngine engine, const DbTableInfo &table) {
  QStringList sets;
  QStringList where;
  for (const DbColumnInfo &c : table.columns) {
    const QString q = quoteIdentifier(engine, c.name);
    if (c.primaryKey) {
      where << QStringLiteral("%1 = /* value */ NULL").arg(q);
    } else {
      sets << QStringLiteral("%1 = /* value */ NULL").arg(q);
    }
  }
  if (sets.isEmpty()) {
    for (const DbColumnInfo &c : table.columns) {
      sets << QStringLiteral("%1 = /* value */ NULL")
                  .arg(quoteIdentifier(engine, c.name));
    }
  }
  if (where.isEmpty()) {
    where << QStringLiteral("/* condition */ 1 = 0");
  }
  return QStringLiteral("UPDATE %1\nSET %2\nWHERE %3;")
      .arg(qualifiedName(engine, table))
      .arg(sets.join(QStringLiteral(",\n    ")))
      .arg(where.join(QStringLiteral("\n  AND ")));
}

QString createTableSql(DbEngine engine, const DbTableInfo &table) {
  QStringList lines;
  for (const DbColumnInfo &c : table.columns) {
    QString line = QStringLiteral("  %1 %2")
                       .arg(quoteIdentifier(engine, c.name))
                       .arg(c.type.isEmpty() ? QStringLiteral("TEXT") : c.type);
    if (!c.nullable) {
      line += QStringLiteral(" NOT NULL");
    }
    lines << line;
  }
  const QStringList pk = table.primaryKey();
  if (!pk.isEmpty()) {
    QStringList quoted;
    for (const QString &k : pk) {
      quoted << quoteIdentifier(engine, k);
    }
    lines << QStringLiteral("  PRIMARY KEY (%1)").arg(quoted.join(", "));
  }
  return QStringLiteral("%1CREATE TABLE %2 (\n%3\n);")
      .arg(table.isView ? QStringLiteral("-- approximation of a view\n")
                        : QString())
      .arg(qualifiedName(engine, table))
      .arg(lines.join(QStringLiteral(",\n")));
}

QString explainPrefix(DbEngine engine) {
  switch (engine) {
  case DbEngine::PostgreSql:
  case DbEngine::MySql:
    return QStringLiteral("EXPLAIN ");
  case DbEngine::Sqlite:
    return QStringLiteral("EXPLAIN QUERY PLAN ");
  case DbEngine::SqlServer:
    break;
  }
  return {};
}

} // namespace DbCatalog
