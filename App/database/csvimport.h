#ifndef CSVIMPORT_H
#define CSVIMPORT_H

#include "dbcatalog.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace CsvImport {

struct Table {
  QStringList header;
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

Table parse(const QString &text, QChar delimiter = QChar(),
            bool hasHeader = true);

QVector<Column> inferColumns(const Table &table);

QString sqlTypeName(DbEngine engine, ColumnKind kind);

QString createTableSql(DbEngine engine, const QString &tableName,
                       const QVector<Column> &columns);

QStringList insertSql(DbEngine engine, const QString &tableName,
                      const QVector<Column> &columns, const Table &table,
                      int batchSize = 500, QStringList *warnings = nullptr);

} // namespace CsvImport

#endif
