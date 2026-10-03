#ifndef RESULTEDIT_H
#define RESULTEDIT_H

#include "dbcatalog.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace ResultEdit {

struct CellEdit {
  int row = 0;
  int column = 0;
  QString text;
  bool setNull = false;
};

struct Preview {
  QStringList statements;
  QStringList errors;
  bool ok() const { return errors.isEmpty() && !statements.isEmpty(); }
};

enum class ValueKind { Number, Boolean, Text };
ValueKind kindOfType(const QString &sqlType);

bool literalFor(DbEngine engine, const DbColumnInfo &column,
                const QString &text, QString *literal,
                QString *error = nullptr);

Preview buildUpdates(DbEngine engine, const DbTableInfo &table,
                     const DbResultSet &result, const QVector<CellEdit> &edits);

QString pagedSql(DbEngine engine, const QString &selectSql, int pageSize,
                 int page);

} // namespace ResultEdit

#endif
