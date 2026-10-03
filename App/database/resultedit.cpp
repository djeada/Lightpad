#include "resultedit.h"

#include "sqlstatementsplitter.h"

#include <QMap>
#include <QRegularExpression>

namespace ResultEdit {

ValueKind kindOfType(const QString &sqlType) {
  const QString t = sqlType.toLower();
  if (t.contains(QLatin1String("bool")) || t == QLatin1String("bit")) {
    return ValueKind::Boolean;
  }
  static const QRegularExpression num(QStringLiteral(
      "int|serial|numeric|decimal|number|float|double|real|money"));
  if (num.match(t).hasMatch() && !t.contains(QLatin1String("interval")) &&
      !t.contains(QLatin1String("point"))) {
    return ValueKind::Number;
  }
  return ValueKind::Text;
}

bool literalFor(DbEngine engine, const DbColumnInfo &column, const QString &text,
                QString *literal, QString *error) {
  const QString t = text.trimmed();
  switch (kindOfType(column.type)) {
  case ValueKind::Number: {
    static const QRegularExpression re(
        QStringLiteral("^[-+]?(\\d+(\\.\\d*)?|\\.\\d+)([eE][-+]?\\d+)?$"));
    if (!re.match(t).hasMatch()) {
      if (error) {
        *error = QStringLiteral("'%1' is not a number (column %2)")
                     .arg(text, column.name);
      }
      return false;
    }
    *literal = t;
    return true;
  }
  case ValueKind::Boolean: {
    const QString l = t.toLower();
    bool v;
    if (l == QLatin1String("true") || l == QLatin1String("1") ||
        l == QLatin1String("t") || l == QLatin1String("yes")) {
      v = true;
    } else if (l == QLatin1String("false") || l == QLatin1String("0") ||
               l == QLatin1String("f") || l == QLatin1String("no")) {
      v = false;
    } else {
      if (error) {
        *error = QStringLiteral("'%1' is not a boolean (column %2)")
                     .arg(text, column.name);
      }
      return false;
    }
    if (engine == DbEngine::PostgreSql || engine == DbEngine::MySql) {
      *literal = v ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    } else {
      *literal = v ? QStringLiteral("1") : QStringLiteral("0");
    }
    return true;
  }
  case ValueKind::Text:
    break;
  }
  QString s = text;
  s.replace('\'', QLatin1String("''"));
  if (engine == DbEngine::MySql) {
    s.replace('\\', QLatin1String("\\\\"));
  }
  *literal = (engine == DbEngine::SqlServer ? QStringLiteral("N'") : QStringLiteral("'")) +
             s + QLatin1Char('\'');
  return true;
}

static const DbColumnInfo *columnByName(const DbTableInfo &t, const QString &name) {
  for (const DbColumnInfo &c : t.columns) {
    if (c.name.compare(name, Qt::CaseInsensitive) == 0) {
      return &c;
    }
  }
  return nullptr;
}

static int resultColumn(const DbResultSet &r, const QString &name) {
  for (int i = 0; i < r.columns.size(); ++i) {
    if (r.columns[i].name.compare(name, Qt::CaseInsensitive) == 0) {
      return i;
    }
  }
  return -1;
}

Preview buildUpdates(DbEngine engine, const DbTableInfo &table,
                     const DbResultSet &result, const QVector<CellEdit> &edits) {
  Preview p;
  if (table.isView) {
    p.errors << QStringLiteral("'%1' is a view and cannot be edited").arg(table.name);
    return p;
  }
  const QStringList pk = table.primaryKey();
  if (pk.isEmpty()) {
    p.errors << QStringLiteral("Table '%1' has no primary key; rows cannot be "
                               "identified safely")
                    .arg(table.name);
    return p;
  }
  QVector<int> pkCols;
  for (const QString &k : pk) {
    const int idx = resultColumn(result, k);
    if (idx < 0) {
      p.errors << QStringLiteral("Primary key column '%1' is not part of the "
                                 "result; select it to edit rows")
                      .arg(k);
      return p;
    }
    pkCols << idx;
  }

  QMap<int, QVector<CellEdit>> byRow;
  for (const CellEdit &e : edits) {
    byRow[e.row].append(e);
  }
  for (auto it = byRow.constBegin(); it != byRow.constEnd(); ++it) {
    const int row = it.key();
    if (row < 0 || row >= result.rows.size()) {
      p.errors << QStringLiteral("Row %1 does not exist").arg(row + 1);
      continue;
    }
    QStringList sets;
    bool bad = false;
    for (const CellEdit &e : it.value()) {
      if (e.column < 0 || e.column >= result.columns.size()) {
        continue;
      }
      const DbColumnInfo *col = columnByName(table, result.columns[e.column].name);
      if (!col) {
        p.errors << QStringLiteral("Column '%1' does not belong to '%2'")
                        .arg(result.columns[e.column].name, table.name);
        bad = true;
        continue;
      }
      QString lit;
      if (e.setNull) {
        if (!col->nullable) {
          p.errors << QStringLiteral("Column '%1' does not allow NULL").arg(col->name);
          bad = true;
          continue;
        }
        lit = QStringLiteral("NULL");
      } else {
        QString err;
        if (!literalFor(engine, *col, e.text, &lit, &err)) {
          p.errors << err;
          bad = true;
          continue;
        }
      }
      sets << QStringLiteral("%1 = %2").arg(DbCatalog::quoteIdentifier(engine, col->name), lit);
    }
    if (bad || sets.isEmpty()) {
      continue;
    }
    QStringList where;
    for (int k = 0; k < pkCols.size(); ++k) {
      const DbColumnInfo *col = columnByName(table, pk[k]);
      const QVariant v = result.rows[row].value(pkCols[k]);
      const QString q = DbCatalog::quoteIdentifier(engine, pk[k]);
      if (v.isNull() || !v.isValid()) {
        where << q + QStringLiteral(" IS NULL");
        continue;
      }
      QString lit, err;
      DbColumnInfo c = col ? *col : DbColumnInfo{pk[k], QString(), false, true};
      if (!literalFor(engine, c, v.toString(), &lit, &err)) {
        p.errors << err;
        bad = true;
        break;
      }
      where << QStringLiteral("%1 = %2").arg(q, lit);
    }
    if (bad) {
      continue;
    }
    p.statements << QStringLiteral("UPDATE %1\nSET %2\nWHERE %3;")
                        .arg(DbCatalog::qualifiedName(engine, table),
                             sets.join(QStringLiteral(", ")),
                             where.join(QStringLiteral(" AND ")));
  }
  return p;
}

QString pagedSql(DbEngine engine, const QString &selectSql, int pageSize,
                 int page) {
  if (pageSize <= 0 || page < 0) {
    return {};
  }
  QString sql = SqlStatementSplitter::stripComments(selectSql).trimmed();
  while (sql.endsWith(';')) {
    sql.chop(1);
    sql = sql.trimmed();
  }
  const SqlStatementKind kind = SqlStatementSplitter::classify(sql, engine);
  if (kind != SqlStatementKind::Select) {
    return {};
  }
  static const QRegularExpression limited(
      QStringLiteral("\\b(limit|offset|fetch|top|into|for\\s+update)\\b"),
      QRegularExpression::CaseInsensitiveOption);
  if (limited.match(sql).hasMatch()) {
    return {};
  }
  const qint64 offset = qint64(page) * pageSize;
  if (engine == DbEngine::SqlServer) {
    static const QRegularExpression order(QStringLiteral("\\border\\s+by\\b"),
                                          QRegularExpression::CaseInsensitiveOption);
    if (!order.match(sql).hasMatch()) {
      return {};
    }
    return QStringLiteral("%1\nOFFSET %2 ROWS FETCH NEXT %3 ROWS ONLY;")
        .arg(sql)
        .arg(offset)
        .arg(pageSize);
  }
  return QStringLiteral("%1\nLIMIT %2 OFFSET %3;").arg(sql).arg(pageSize).arg(offset);
}

} // namespace ResultEdit
