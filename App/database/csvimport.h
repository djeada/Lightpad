#ifndef CSVIMPORT_H
#define CSVIMPORT_H

#include "dbcatalog.h"

#include <QString>
#include <QStringList>
#include <QVector>

// Turns a CSV/TSV text into CREATE TABLE + INSERT statements (the "import
// wizard" without the GUI). Nothing here touches the database.
namespace CsvImport {

struct Table {
  QStringList header; // sanitised and unique
  QVector<QStringList> rows;
  QChar delimiter = ',';
  bool ok = true;
  QString error;
};

enum class ColumnKind { Integer, BigInt, Real, Boolean, Date, Timestamp, Text };

struct Column {
  QString name;
  ColumnKind kind = ColumnKind::Text;
  bool nullable = false;
};

QChar detectDelimiter(const QString &text);

// delimiter == QChar() autodetects. Without a header, columns are col_1, ...
Table parse(const QString &text, QChar delimiter = QChar(),
            bool hasHeader = true);

QVector<Column> inferColumns(const Table &table);

QString sqlTypeName(DbEngine engine, ColumnKind kind);

QString createTableSql(DbEngine engine, const QString &tableName,
                       const QVector<Column> &columns);

// Multi-row INSERTs, at most 'batchSize' rows each. Rows with the wrong
// number of fields are padded/truncated; their numbers are added to 'warnings'.
QStringList insertSql(DbEngine engine, const QString &tableName,
                      const QVector<Column> &columns, const Table &table,
                      int batchSize = 500, QStringList *warnings = nullptr);

} // namespace CsvImport

#endif
