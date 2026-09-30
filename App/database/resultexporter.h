#ifndef RESULTEXPORTER_H
#define RESULTEXPORTER_H

#include "dbtypes.h"

#include <QString>

namespace ResultExporter {

enum class Format { Csv, Tsv, Json, Markdown, SqlInsert };

QString formatName(Format format);
QString fileExtension(Format format);

QString displayText(const QVariant &value);

QString exportResult(const DbResultSet &result, Format format,
                     const QVector<int> &rows = {},
                     const QVector<int> &columns = {},
                     bool includeHeader = true,
                     const QString &tableName = QStringLiteral("table_name"),
                     DbEngine engine = DbEngine::PostgreSql);

QString sqlLiteral(const QVariant &value);

} // namespace ResultExporter

#endif
