#ifndef SQLKEYWORDS_H
#define SQLKEYWORDS_H

#include <QStringList>

namespace SqlKeywords {

inline QStringList keywords() {
  return {"ADD",       "ALL",         "ALTER",     "AND",
          "ANY",       "AS",          "ASC",       "AUTHORIZATION",
          "BACKUP",    "BEGIN",       "BETWEEN",   "BREAK",
          "BY",        "CASCADE",     "CASE",      "CHECK",
          "CLOSE",     "CLUSTERED",   "COLLATE",   "COLUMN",
          "COMMIT",    "CONSTRAINT",  "CONTINUE",  "CREATE",
          "CROSS",     "CURRENT",     "CURSOR",    "DATABASE",
          "DECLARE",   "DEFAULT",     "DELETE",    "DENY",
          "DESC",      "DESCRIBE",    "DISTINCT",  "DROP",
          "ELSE",      "END",         "ESCAPE",    "EXCEPT",
          "EXEC",      "EXECUTE",     "EXISTS",    "EXPLAIN",
          "FETCH",     "FILTER",      "FOR",       "FOREIGN",
          "FROM",      "FULL",        "FUNCTION",  "GO",
          "GRANT",     "GROUP",       "HAVING",    "IF",
          "IN",        "INDEX",       "INNER",     "INSERT",
          "INTERSECT", "INTO",        "IS",        "JOIN",
          "KEY",       "LATERAL",     "LEFT",      "LIKE",
          "LIMIT",     "LOCK",        "MERGE",     "NATURAL",
          "NOT",       "NULL",        "OFFSET",    "ON",
          "OPEN",      "OR",          "ORDER",     "OUTER",
          "OVER",      "PARTITION",   "PIVOT",     "PRIMARY",
          "PRINT",     "PROCEDURE",   "RECURSIVE", "REFERENCES",
          "RETURN",    "RETURNING",   "REVOKE",    "RIGHT",
          "ROLLBACK",  "ROW",         "ROWS",      "SAVEPOINT",
          "SCHEMA",    "SELECT",      "SET",       "SHOW",
          "TABLE",     "TEMP",        "TEMPORARY", "THEN",
          "TOP",       "TRANSACTION", "TRIGGER",   "TRUNCATE",
          "UNION",     "UNIQUE",      "UNPIVOT",   "UPDATE",
          "USE",       "USING",       "VALUES",    "VIEW",
          "WHEN",      "WHERE",       "WHILE",     "WINDOW",
          "WITH"};
}

inline QStringList types() {
  return {"BIGINT",    "BIGSERIAL", "BINARY",    "BIT",      "BLOB",
          "BOOL",      "BOOLEAN",   "BYTEA",     "CHAR",     "CLOB",
          "DATE",      "DATETIME",  "DATETIME2", "DECIMAL",  "DOUBLE",
          "FLOAT",     "IMAGE",     "INT",       "INTEGER",  "INTERVAL",
          "JSON",      "JSONB",     "MONEY",     "NCHAR",    "NUMERIC",
          "NVARCHAR",  "REAL",      "SERIAL",    "SMALLINT", "TEXT",
          "TIME",      "TIMESTAMP", "TINYINT",   "UUID",     "UNIQUEIDENTIFIER",
          "VARBINARY", "VARCHAR",   "XML"};
}

inline QStringList functions() {
  return {"ABS",
          "AVG",
          "CAST",
          "CEIL",
          "CEILING",
          "COALESCE",
          "CONCAT",
          "CONVERT",
          "COUNT",
          "CURRENT_DATE",
          "CURRENT_TIMESTAMP",
          "DATEADD",
          "DATEDIFF",
          "DATE_TRUNC",
          "EXTRACT",
          "FLOOR",
          "GETDATE",
          "IFNULL",
          "ISNULL",
          "LAG",
          "LEAD",
          "LENGTH",
          "LOWER",
          "LTRIM",
          "MAX",
          "MIN",
          "NOW",
          "NULLIF",
          "RANK",
          "REPLACE",
          "ROUND",
          "ROW_NUMBER",
          "RTRIM",
          "STRING_AGG",
          "SUBSTRING",
          "SUM",
          "TRIM",
          "UPPER"};
}

} // namespace SqlKeywords

#endif
