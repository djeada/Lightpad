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
    return sqliteSchemaQuery({QStringLiteral("main"), QStringLiteral("temp")});
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
    if (row.size() > 8)
      col.defaultValue = row[8].toString();
    if (row.size() > 9)
      col.hidden = row[9].toInt();
    if (row.size() > 10 && !row[10].isNull())
      current->createSql = row[10].toString();
    if (!row[3].isNull())
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

  if (table.schema.isEmpty()) {
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
    if (c.hidden != 0)
      continue;
    placeholders << QStringLiteral("/* %1 */ NULL").arg(c.name);
  }
  return QStringLiteral("INSERT INTO %1 (%2)\nVALUES (%3);")
      .arg(qualifiedName(engine, table))
      .arg([&]() {
        QStringList names;
        for (const auto &c : table.columns)
          if (c.hidden == 0)
            names << quoteIdentifier(engine, c.name);
        return names.join(", ");
      }())
      .arg(placeholders.join(QStringLiteral(", ")));
}

QString updateTemplateSql(DbEngine engine, const DbTableInfo &table) {
  QStringList sets;
  QStringList where;
  for (const DbColumnInfo &c : table.columns) {
    if (c.hidden != 0)
      continue;
    const QString q = quoteIdentifier(engine, c.name);
    if (c.primaryKey) {
      where << QStringLiteral("%1 = /* value */ NULL").arg(q);
    } else {
      sets << QStringLiteral("%1 = /* value */ NULL").arg(q);
    }
  }
  if (sets.isEmpty()) {
    for (const DbColumnInfo &c : table.columns) {
      if (c.hidden != 0)
        continue;
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
  if (engine == DbEngine::Sqlite && !table.createSql.isEmpty()) {
    return table.createSql +
           (table.createSql.trimmed().endsWith(';') ? "" : ";");
  }
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

static QString stringLiteral(DbEngine engine, QString text) {
  if (engine == DbEngine::MySql)
    text.replace('\\', "\\\\");
  text.replace('\'', "''");
  return (engine == DbEngine::SqlServer ? "N'" : "'") + text + '\'';
}

QString sqliteSchemaQuery(const QStringList &schemas) {
  QStringList queries;
  for (const QString &schema : schemas) {
    const QString literal = stringLiteral(DbEngine::Sqlite, schema);
    queries
        << QStringLiteral(
               "SELECT %1 AS schema_name, m.name AS table_name, "
               "CASE m.type WHEN 'view' THEN 'VIEW' ELSE 'BASE TABLE' END AS "
               "table_type, "
               "p.name AS column_name, p.type, CASE WHEN p.\"notnull\" = 1 OR "
               "(p.pk > 0 AND upper(p.type) = 'INTEGER' AND NOT EXISTS "
               "(SELECT 1 FROM pragma_index_list(m.name, %1) WHERE origin = "
               "'pk')) "
               "THEN 'NO' ELSE 'YES' END, CASE WHEN p.pk > 0 THEN 'YES' ELSE "
               "'NO' END, "
               "p.cid + 1 AS ordinal, p.dflt_value, p.hidden, CASE WHEN p.cid "
               "= 0 THEN m.sql END "
               "FROM %2.sqlite_schema m LEFT JOIN pragma_table_xinfo(m.name, "
               "%1) p ON 1=1 "
               "WHERE m.type IN ('table', 'view') AND m.name NOT GLOB "
               "'sqlite_*'")
               .arg(literal, quoteIdentifier(DbEngine::Sqlite, schema));
  }
  return queries.join(" UNION ALL ") + " ORDER BY 1, 2, 8";
}

QString tablePageSql(DbEngine engine, const DbTableInfo &table, int pageSize,
                     qint64 page, const QString &filter,
                     const QString &sortColumn, bool descending,
                     qint64 offset) {
  if (pageSize < 1 || pageSize > 1000 || page < 0 ||
      page > Q_INT64_C(1000000000))
    return {};
  QStringList columns;
  QStringList names;
  for (const auto &column : table.columns) {
    if (column.hidden == 1)
      continue;
    columns << quoteIdentifier(engine, column.name);
    names << column.name;
  }
  if (columns.isEmpty())
    return {};
  QString sql =
      "SELECT " + columns.join(", ") + " FROM " + qualifiedName(engine, table);
  if (!filter.isEmpty()) {
    QStringList predicates;
    QString pattern = filter;
    pattern.replace('!', "!!").replace('%', "!%").replace('_', "!_");
    const QString literal = stringLiteral(engine, "%" + pattern + "%");
    for (const auto &column : columns) {
      const QString cast =
          engine == DbEngine::SqlServer
              ? "CONVERT(NVARCHAR(MAX), " + column + ")"
              : "CAST(" + column + " AS " +
                    (engine == DbEngine::MySql ? "CHAR" : "TEXT") + ")";
      predicates << "LOWER(COALESCE(" + cast + ", 'NULL')) LIKE LOWER(" +
                        literal + ") ESCAPE '!'";
      if (engine == DbEngine::Sqlite) {
        predicates << "(typeof(" + column +
                          ") = 'blob' AND instr(lower('0x' || hex(" + column +
                          ")), lower(" + stringLiteral(engine, filter) +
                          ")) > 0)";
      }
    }
    sql += " WHERE (" + predicates.join(" OR ") + ")";
  }
  QStringList order;
  if (names.contains(sortColumn)) {
    order << quoteIdentifier(engine, sortColumn) +
                 (descending ? " DESC" : " ASC");
  }
  for (const auto &column : table.columns) {
    if (column.primaryKey && column.name != sortColumn)
      order << quoteIdentifier(engine, column.name);
  }
  if (engine == DbEngine::Sqlite && table.primaryKey().isEmpty() &&
      !table.isView) {
    for (const auto &name : {QStringLiteral("_rowid_"), QStringLiteral("rowid"),
                             QStringLiteral("oid")}) {
      if (!names.contains(name, Qt::CaseInsensitive)) {
        order << quoteIdentifier(engine, name);
        break;
      }
    }
  }
  if (order.isEmpty())
    order = columns;
  sql += " ORDER BY " + order.join(", ");
  if (offset < 0)
    offset = page * pageSize;
  return engine == DbEngine::SqlServer
             ? sql + QString(" OFFSET %1 ROWS FETCH NEXT %2 ROWS ONLY;")
                         .arg(offset)
                         .arg(pageSize + 1)
             : sql + QString(" LIMIT %1 OFFSET %2;")
                         .arg(pageSize + 1)
                         .arg(offset);
}

QStringList sqliteTableDetailsQueries(const DbTableInfo &table) {
  const QString schema = table.schema.isEmpty() ? "main" : table.schema;
  const QString name = stringLiteral(DbEngine::Sqlite, table.name);
  const QString schemaLiteral = stringLiteral(DbEngine::Sqlite, schema);
  const QString catalog =
      quoteIdentifier(DbEngine::Sqlite, schema) + ".sqlite_schema";
  return {"SELECT name AS column_name, type, \"notnull\" AS not_null, "
          "dflt_value AS default_value, "
          "pk AS primary_key_position, CASE hidden WHEN 1 THEN 'hidden' WHEN 2 "
          "THEN 'generated (virtual)' "
          "WHEN 3 THEN 'generated (stored)' ELSE '' END AS extra FROM "
          "pragma_table_xinfo(" +
              name + ", " + schemaLiteral + ") ORDER BY cid;",
          "SELECT l.name AS index_name, l.\"unique\" AS is_unique, l.origin, "
          "l.partial, x.seqno AS position, "
          "x.name AS column_name, x.desc AS descending, x.coll AS collation, "
          "m.sql AS definition "
          "FROM pragma_index_list(" +
              name + ", " + schemaLiteral +
              ") l LEFT JOIN pragma_index_xinfo(l.name, " + schemaLiteral +
              ") x ON x.key = 1 "
              "LEFT JOIN " +
              catalog + " m ON m.name = l.name ORDER BY l.name, x.seqno;",
          "SELECT id, seq AS position, \"table\" AS referenced_table, \"from\" "
          "AS column_name, \"to\" AS referenced_column, "
          "on_update, on_delete, match FROM pragma_foreign_key_list(" +
              name + ", " + schemaLiteral + ") ORDER BY id, seq;",
          "SELECT name AS trigger_name, sql AS definition FROM " + catalog +
              " WHERE type = 'trigger' AND tbl_name = " + name +
              " ORDER BY name;",
          "SELECT type, name, sql AS definition FROM " + catalog +
              " WHERE tbl_name = " + name +
              " AND sql IS NOT NULL "
              "ORDER BY CASE type WHEN 'table' THEN 0 WHEN 'view' THEN 0 WHEN "
              "'index' THEN 1 ELSE 2 END, name;"};
}

} // namespace DbCatalog
