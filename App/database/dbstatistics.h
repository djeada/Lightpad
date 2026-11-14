#ifndef DBSTATISTICS_H
#define DBSTATISTICS_H

#include "dbtypes.h"
#include <QDateTime>
#include <QMap>
#include <QPair>

struct DbStorageInfo {
  QString database;
  QString schema;
  QString table;
  qint64 rows = -1;
  qint64 dataBytes = -1;
  qint64 indexBytes = -1;
  qint64 totalBytes = -1;
  qint64 freeBytes = -1;
  qint64 walBytes = -1;
  bool rowsEstimated = false;
  bool sizeEstimated = false;
};

struct DbStatistics {
  QMap<QString, DbStorageInfo> databases;
  QMap<QPair<QString, QString>, DbStorageInfo> tables;
  QDateTime updatedAt;
  bool tableSizesDeferred = false;

  const DbStorageInfo *table(const QString &schema, const QString &name) const;
};

namespace DbStatisticsQueries {
QStringList databaseQueries(DbEngine engine, const QStringList &schemas = {});
QStringList tableQueries(DbEngine engine, const QStringList &schemas = {});
void merge(DbStatistics &statistics, const DbResultSet &rows);
QString formatBytes(qint64 bytes);
QString formatRows(qint64 rows, bool estimated);
QString description(DbEngine engine);
} // namespace DbStatisticsQueries

#endif
