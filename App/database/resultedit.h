#ifndef RESULTEDIT_H
#define RESULTEDIT_H

#include "dbcatalog.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

// Inline result editing without magic: edits are turned into UPDATE statements
// that are shown to the user (preview) before anything runs. Plus SQL paging.
namespace ResultEdit {

struct CellEdit {
  int row = 0;
  int column = 0; // index in the result set
  QString text;   // new value as typed
  bool setNull = false;
};

struct Preview {
  QStringList statements;
  QStringList errors; // edits that cannot be expressed (no key, bad number)
  bool ok() const { return errors.isEmpty() && !statements.isEmpty(); }
};

enum class ValueKind { Number, Boolean, Text };
ValueKind kindOfType(const QString &sqlType);

// Literal for a value typed into a cell of a column with the given type.
// Returns false (and sets error) if the text does not fit the type.
bool literalFor(DbEngine engine, const DbColumnInfo &column, const QString &text,
                QString *literal, QString *error = nullptr);

// One UPDATE per edited row, located by the table's primary key taken from the
// original (unedited) values of the result row. Result columns are matched to
// table columns by name.
Preview buildUpdates(DbEngine engine, const DbTableInfo &table,
                     const DbResultSet &result,
                     const QVector<CellEdit> &edits);

// Statement for one page (0 based) of a single SELECT. Empty if the statement
// cannot be paged safely (already limited, not a SELECT, SQL Server without
// ORDER BY ...).
QString pagedSql(DbEngine engine, const QString &selectSql, int pageSize,
                 int page);

} // namespace ResultEdit

#endif
