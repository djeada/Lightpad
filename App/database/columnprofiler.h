#ifndef COLUMNPROFILER_H
#define COLUMNPROFILER_H

#include "dbtypes.h"

#include <QString>
#include <QVector>

enum class ColumnKind { Empty, Number, Boolean, Temporal, Text };

struct ValueCount {
  QString value;
  qint64 count = 0;
};

struct ColumnProfile {
  QString name;
  ColumnKind kind = ColumnKind::Empty;
  qint64 rows = 0;
  qint64 nulls = 0;
  qint64 distinct = 0;
  bool distinctIsLowerBound = false;
  QString min;
  QString max;
  bool hasNumbers = false;
  double sum = 0;
  double mean = 0;
  double median = 0;
  double stddev = 0;
  int minLength = 0;
  int maxLength = 0;
  QVector<ValueCount> top;

  qint64 nonNull() const { return rows - nulls; }
  double nullPercent() const { return rows ? 100.0 * nulls / rows : 0.0; }
  bool isUnique() const {
    return !distinctIsLowerBound && nonNull() > 0 && distinct == nonNull();
  }
  QString kindName() const;
};

namespace ColumnProfiler {

ColumnProfile profileColumn(const DbResultSet &result, int column,
                            int topValues = 5);
QVector<ColumnProfile> profileAll(const DbResultSet &result, int topValues = 5);

bool parseNumber(const QVariant &value, double *out);

} // namespace ColumnProfiler

#endif
