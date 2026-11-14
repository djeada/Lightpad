#include "dbstatistics.h"
#include "dbcatalog.h"

#include <QLocale>

namespace {
QString literal(QString value) {
  value.replace('\'', "''");
  return '\'' + value + '\'';
}
QString sqliteDatabase(const QString &schema) {
  const QString name = literal(schema);
  const QString page = "(SELECT page_size FROM pragma_page_size(" + name + "))";
  const QString free =
      schema == "main"
          ? "(SELECT freelist_count FROM pragma_freelist_count)*" + page
          : "NULL";
  return "SELECT 'database' AS object_type, " + name + " AS database_name, " +
         name +
         " AS schema_name, '' AS table_name, "
         "(SELECT page_count FROM pragma_page_count(" +
         name + "))*" + page + " AS total_bytes, " + free + " AS free_bytes";
}
} // namespace

const DbStorageInfo *DbStatistics::table(const QString &schema,
                                         const QString &name) const {
  const auto it = tables.constFind({schema, name});
  return it == tables.cend() ? nullptr : &it.value();
}

namespace DbStatisticsQueries {

QStringList databaseQueries(DbEngine engine, const QStringList &schemas) {
  switch (engine) {
  case DbEngine::Sqlite: {
    QStringList queries;
    for (const auto &schema : schemas)
      queries << sqliteDatabase(schema);
    return queries;
  }
  case DbEngine::PostgreSql:
    return {QStringLiteral(
        "SELECT 'database' AS object_type, datname AS database_name, "
        "pg_database_size(oid) AS total_bytes FROM pg_database "
        "WHERE datallowconn AND has_database_privilege(oid, 'CONNECT')")};
  case DbEngine::MySql:
    return {QStringLiteral(
        "SELECT 'database' AS object_type, TABLE_SCHEMA AS database_name, "
        "SUM(DATA_LENGTH) AS data_bytes, SUM(INDEX_LENGTH) AS index_bytes, "
        "SUM(DATA_LENGTH+INDEX_LENGTH) AS total_bytes, 1 AS size_estimated "
        "FROM information_schema.TABLES WHERE TABLE_SCHEMA NOT IN "
        "('information_schema','performance_schema','mysql','sys') GROUP BY "
        "TABLE_SCHEMA")};
  case DbEngine::SqlServer:
    return {QStringLiteral(
        "SELECT 'database' AS object_type, DB_NAME() AS database_name, "
        "SUM(CAST(size AS bigint))*8192 AS total_bytes FROM "
        "sys.database_files")};
  }
  return {};
}

QStringList tableQueries(DbEngine engine, const QStringList &schemas) {
  switch (engine) {
  case DbEngine::Sqlite: {
    QStringList queries;
    for (const auto &schema : schemas) {
      queries << "SELECT 'table' AS object_type, " + literal(schema) +
                     " AS schema_name, s.tbl_name AS table_name, "
                     "SUM(CASE WHEN s.type='table' THEN d.pgsize ELSE 0 END) "
                     "AS data_bytes, "
                     "SUM(CASE WHEN s.type='index' THEN d.pgsize ELSE 0 END) "
                     "AS index_bytes, "
                     "SUM(d.pgsize) AS total_bytes FROM " +
                     DbCatalog::quoteIdentifier(engine, schema) +
                     ".sqlite_schema s JOIN dbstat(" + literal(schema) +
                     ", TRUE) d ON d.name=s.name "
                     "WHERE s.type IN ('table','index') AND s.tbl_name NOT "
                     "LIKE 'sqlite_%' GROUP BY s.tbl_name";
    }
    return queries;
  }
  case DbEngine::PostgreSql:
    return {QStringLiteral(
        "SELECT 'table' AS object_type, n.nspname AS schema_name, c.relname AS "
        "table_name, "
        "CASE WHEN c.reltuples < 0 THEN NULL ELSE c.reltuples::bigint END AS "
        "row_count, "
        "1 AS rows_estimated, pg_table_size(c.oid) AS data_bytes, "
        "pg_indexes_size(c.oid) AS index_bytes, pg_total_relation_size(c.oid) "
        "AS total_bytes "
        "FROM pg_class c JOIN pg_namespace n ON n.oid=c.relnamespace "
        "WHERE c.relkind IN ('r','m','p') AND n.nspname NOT IN "
        "('pg_catalog','information_schema') "
        "AND n.nspname NOT LIKE 'pg_toast%'")};
  case DbEngine::MySql:
    return {QStringLiteral(
        "SELECT 'table' AS object_type, TABLE_SCHEMA AS schema_name, "
        "TABLE_NAME AS table_name, "
        "TABLE_ROWS AS row_count, 1 AS rows_estimated, DATA_LENGTH AS "
        "data_bytes, "
        "INDEX_LENGTH AS index_bytes, DATA_LENGTH+INDEX_LENGTH AS total_bytes, "
        "1 AS size_estimated "
        "FROM information_schema.TABLES WHERE TABLE_TYPE='BASE TABLE' AND "
        "TABLE_SCHEMA NOT IN "
        "('information_schema','performance_schema','mysql','sys')")};
  case DbEngine::SqlServer:
    return {
        QStringLiteral("SELECT 'table' AS object_type, s.name AS schema_name, "
                       "t.name AS table_name, "
                       "SUM(CASE WHEN p.index_id IN (0,1) THEN p.row_count "
                       "ELSE 0 END) AS row_count, "
                       "1 AS rows_estimated, "
                       "SUM(CASE WHEN p.index_id IN (0,1) THEN "
                       "p.reserved_page_count ELSE 0 END)*8192 AS data_bytes, "
                       "SUM(CASE WHEN p.index_id > 1 THEN "
                       "p.reserved_page_count ELSE 0 END)*8192 AS index_bytes, "
                       "SUM(p.reserved_page_count)*8192 AS total_bytes "
                       "FROM sys.dm_db_partition_stats p JOIN sys.tables t ON "
                       "t.object_id=p.object_id "
                       "JOIN sys.schemas s ON s.schema_id=t.schema_id GROUP BY "
                       "s.name,t.name")};
  }
  return {};
}

void merge(DbStatistics &statistics, const DbResultSet &rows) {
  QMap<QString, int> columns;
  for (int i = 0; i < rows.columns.size(); ++i)
    columns[rows.columns[i].name.toLower()] = i;
  for (const auto &row : rows.rows) {
    auto value = [&](const QString &name) -> QVariant {
      const int index = columns.value(name, -1);
      return index >= 0 && index < row.size() ? row[index] : QVariant();
    };
    auto number = [&](const QString &name) -> qint64 {
      bool ok = false;
      const auto v = value(name);
      const qint64 result = v.toLongLong(&ok);
      return ok && !v.isNull() && result >= 0 ? result : -1;
    };
    DbStorageInfo info;
    info.database = value("database_name").toString();
    info.schema = value("schema_name").toString();
    info.table = value("table_name").toString();
    info.rows = number("row_count");
    info.dataBytes = number("data_bytes");
    info.indexBytes = number("index_bytes");
    info.totalBytes = number("total_bytes");
    info.freeBytes = number("free_bytes");
    info.rowsEstimated = value("rows_estimated").toInt() != 0;
    info.sizeEstimated = value("size_estimated").toInt() != 0;
    if (value("object_type") == "database" && !info.database.isEmpty())
      statistics.databases[info.database] = info;
    else if (!info.table.isEmpty())
      statistics.tables[{info.schema, info.table}] = info;
  }
}

QString formatBytes(qint64 bytes) {
  if (bytes < 0)
    return QString::fromUtf8("—");
  static const QStringList units{"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
  double amount = bytes;
  int unit = 0;
  while (amount >= 1024 && unit < units.size() - 1) {
    amount /= 1024;
    ++unit;
  }
  return QLocale().toString(amount, 'f',
                            unit == 0 || amount >= 100 ? 0
                            : amount >= 10             ? 1
                                                       : 2) +
         ' ' + units[unit];
}

QString formatRows(qint64 rows, bool estimated) {
  return rows < 0 ? QString::fromUtf8("—")
                  : (estimated ? QString::fromUtf8("≈ ") : QString()) +
                        QLocale().toString(rows);
}

QString description(DbEngine engine) {
  switch (engine) {
  case DbEngine::Sqlite:
    return QStringLiteral(
        "Allocated SQLite table and index pages. Database totals also include "
        "internal and free pages. WAL is listed separately. Virtual-table "
        "storage may belong to shadow tables.");
  case DbEngine::PostgreSql:
    return QStringLiteral(
        "Relation sizes include indexes and TOAST. Partition parents show "
        "their own storage; child tables are listed separately. Row counts are "
        "planner estimates.");
  case DbEngine::MySql:
    return QStringLiteral("Storage and row counts come from the engine catalog "
                          "and may be estimates, especially for InnoDB. "
                          "Database totals sum visible table allocations.");
  case DbEngine::SqlServer:
    return QStringLiteral(
        "Reserved table and index pages; row counts are estimates. Database "
        "allocation includes data and log files. Table statistics require "
        "database-state permissions.");
  }
  return {};
}
} // namespace DbStatisticsQueries
