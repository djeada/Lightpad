#include "csvimport.h"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>

namespace CsvImport {

QChar detectDelimiter(const QString &text) {
  const QChar candidates[] = {',', ';', '\t', '|'};
  QChar best = ',';
  int bestScore = -1;
  for (QChar cand : candidates) {
    // count delimiters outside quotes on the first lines
    int lines = 0, count = 0, lineCount = 0;
    bool inQuote = false;
    int firstLineCount = -1;
    bool consistent = true;
    for (int i = 0; i < text.size() && lines < 5; ++i) {
      const QChar c = text[i];
      if (c == '"') {
        inQuote = !inQuote;
      } else if (!inQuote && c == cand) {
        ++lineCount;
      } else if (!inQuote && c == '\n') {
        if (firstLineCount < 0) {
          firstLineCount = lineCount;
        } else if (lineCount != firstLineCount) {
          consistent = false;
        }
        count += lineCount;
        lineCount = 0;
        ++lines;
      }
    }
    if (lines == 0) {
      if (firstLineCount < 0) {
        firstLineCount = lineCount;
      }
      count += lineCount;
    }
    const int score = (firstLineCount > 0 ? 1000 : 0) + (consistent ? 500 : 0) +
                      std::min(count, 400);
    if (score > bestScore && firstLineCount > 0) {
      bestScore = score;
      best = cand;
    }
  }
  return best;
}

Table parse(const QString &textIn, QChar delimiter, bool hasHeader) {
  Table t;
  QString text = textIn;
  if (text.startsWith(QChar(0xFEFF))) {
    text.remove(0, 1);
  }
  if (delimiter.isNull()) {
    delimiter = detectDelimiter(text);
  }
  t.delimiter = delimiter;

  QVector<QStringList> all;
  QStringList row;
  QString field;
  bool inQuote = false;
  bool fieldQuoted = false;
  bool any = false;
  auto endField = [&]() {
    row << field;
    field.clear();
    fieldQuoted = false;
  };
  auto endRow = [&]() {
    endField();
    if (!(row.size() == 1 && row[0].isEmpty() && !any)) {
      all.append(row);
    }
    row.clear();
    any = false;
  };
  for (int i = 0; i < text.size(); ++i) {
    const QChar c = text[i];
    if (inQuote) {
      if (c == '"') {
        if (i + 1 < text.size() && text[i + 1] == '"') {
          field += '"';
          ++i;
        } else {
          inQuote = false;
        }
      } else {
        field += c;
      }
      continue;
    }
    if (c == '"' && field.isEmpty() && !fieldQuoted) {
      inQuote = true;
      fieldQuoted = true;
      any = true;
    } else if (c == delimiter) {
      endField();
      any = true;
    } else if (c == '\r') {
      if (i + 1 < text.size() && text[i + 1] == '\n') {
        ++i;
      }
      endRow();
    } else if (c == '\n') {
      endRow();
    } else {
      field += c;
      any = true;
    }
  }
  if (inQuote) {
    t.ok = false;
    t.error = QStringLiteral("Unterminated quoted field");
  }
  if (any || !field.isEmpty() || !row.isEmpty()) {
    endRow();
  }
  if (all.isEmpty()) {
    t.ok = false;
    if (t.error.isEmpty()) {
      t.error = QStringLiteral("The file is empty");
    }
    return t;
  }

  int width = 0;
  for (const QStringList &r : all) {
    width = std::max(width, int(r.size()));
  }
  QStringList header;
  if (hasHeader) {
    header = all.takeFirst();
  }
  QSet<QString> used;
  QStringList clean;
  const int cols = hasHeader ? std::max(int(header.size()), width) : width;
  for (int i = 0; i < cols; ++i) {
    QString name = i < header.size() ? header[i].trimmed() : QString();
    if (name.isEmpty()) {
      name = QStringLiteral("col_%1").arg(i + 1);
    }
    QString candidate = name;
    int n = 2;
    while (used.contains(candidate.toLower())) {
      candidate = QStringLiteral("%1_%2").arg(name).arg(n++);
    }
    used.insert(candidate.toLower());
    clean << candidate;
  }
  t.header = clean;
  t.rows = all;
  return t;
}

static bool isNullish(const QString &v) { return v.trimmed().isEmpty(); }

QVector<Column> inferColumns(const Table &table) {
  static const QRegularExpression intRe(QStringLiteral("^[-+]?\\d+$"));
  static const QRegularExpression realRe(
      QStringLiteral("^[-+]?(\\d+\\.\\d*|\\.\\d+|\\d+)([eE][-+]?\\d+)?$"));
  static const QRegularExpression boolRe(
      QStringLiteral("^(true|false)$"), QRegularExpression::CaseInsensitiveOption);
  QVector<Column> cols;
  for (int c = 0; c < table.header.size(); ++c) {
    bool allInt = true, allBig = false, allReal = true, allBool = true,
         allDate = true, allTs = true, seen = false, nullable = false;
    for (const QStringList &r : table.rows) {
      const QString v = c < r.size() ? r[c].trimmed() : QString();
      if (v.isEmpty()) {
        nullable = true;
        continue;
      }
      seen = true;
      if (allInt) {
        if (!intRe.match(v).hasMatch() || (v.size() > 1 && v.startsWith('0')) ||
            v.size() > 18) {
          allInt = false;
        } else {
          bool ok = false;
          const qlonglong n = v.toLongLong(&ok);
          if (!ok) {
            allInt = false;
          } else if (n > 2147483647LL || n < -2147483648LL) {
            allBig = true;
          }
        }
      }
      if (allReal && (!realRe.match(v).hasMatch() ||
                      (v.size() > 1 && v.startsWith('0') && !v.startsWith("0.")))) {
        allReal = false;
      }
      if (allBool && !boolRe.match(v).hasMatch()) {
        allBool = false;
      }
      if (allDate && (v.size() != 10 || !QDate::fromString(v, Qt::ISODate).isValid())) {
        allDate = false;
      }
      if (allTs) {
        const QDateTime dt = QDateTime::fromString(QString(v).replace(' ', 'T'), Qt::ISODate);
        if (!dt.isValid() || v.size() < 16) {
          allTs = false;
        }
      }
    }
    Column col;
    col.name = table.header[c];
    col.nullable = nullable || !seen;
    if (!seen) {
      col.kind = ColumnKind::Text;
    } else if (allInt) {
      col.kind = allBig ? ColumnKind::BigInt : ColumnKind::Integer;
    } else if (allReal) {
      col.kind = ColumnKind::Real;
    } else if (allBool) {
      col.kind = ColumnKind::Boolean;
    } else if (allDate) {
      col.kind = ColumnKind::Date;
    } else if (allTs) {
      col.kind = ColumnKind::Timestamp;
    } else {
      col.kind = ColumnKind::Text;
    }
    cols.append(col);
  }
  return cols;
}

QString sqlTypeName(DbEngine e, ColumnKind k) {
  switch (k) {
  case ColumnKind::Integer:
    return QStringLiteral("INTEGER");
  case ColumnKind::BigInt:
    return e == DbEngine::Sqlite ? QStringLiteral("INTEGER")
                                 : QStringLiteral("BIGINT");
  case ColumnKind::Real:
    switch (e) {
    case DbEngine::PostgreSql:
      return QStringLiteral("DOUBLE PRECISION");
    case DbEngine::MySql:
      return QStringLiteral("DOUBLE");
    case DbEngine::SqlServer:
      return QStringLiteral("FLOAT");
    case DbEngine::Sqlite:
      return QStringLiteral("REAL");
    }
    break;
  case ColumnKind::Boolean:
    switch (e) {
    case DbEngine::SqlServer:
      return QStringLiteral("BIT");
    case DbEngine::Sqlite:
      return QStringLiteral("INTEGER");
    default:
      return QStringLiteral("BOOLEAN");
    }
  case ColumnKind::Date:
    return e == DbEngine::Sqlite ? QStringLiteral("TEXT") : QStringLiteral("DATE");
  case ColumnKind::Timestamp:
    switch (e) {
    case DbEngine::PostgreSql:
      return QStringLiteral("TIMESTAMP");
    case DbEngine::MySql:
      return QStringLiteral("DATETIME");
    case DbEngine::SqlServer:
      return QStringLiteral("DATETIME2");
    case DbEngine::Sqlite:
      return QStringLiteral("TEXT");
    }
    break;
  case ColumnKind::Text:
    break;
  }
  return e == DbEngine::SqlServer ? QStringLiteral("NVARCHAR(MAX)")
                                  : QStringLiteral("TEXT");
}

QString createTableSql(DbEngine e, const QString &tableName,
                       const QVector<Column> &columns) {
  QStringList lines;
  for (const Column &c : columns) {
    lines << QStringLiteral("  %1 %2%3")
                 .arg(DbCatalog::quoteIdentifier(e, c.name), sqlTypeName(e, c.kind),
                      c.nullable ? QString() : QStringLiteral(" NOT NULL"));
  }
  return QStringLiteral("CREATE TABLE %1 (\n%2\n);")
      .arg(DbCatalog::quoteIdentifier(e, tableName),
           lines.join(QStringLiteral(",\n")));
}

static QString literal(DbEngine e, const Column &c, const QString &raw) {
  const QString v = raw.trimmed();
  if (v.isEmpty() && c.kind != ColumnKind::Text) {
    return QStringLiteral("NULL");
  }
  switch (c.kind) {
  case ColumnKind::Integer:
  case ColumnKind::BigInt:
  case ColumnKind::Real:
    return v;
  case ColumnKind::Boolean: {
    const bool t = v.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    if (e == DbEngine::PostgreSql || e == DbEngine::MySql) {
      return t ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    }
    return t ? QStringLiteral("1") : QStringLiteral("0");
  }
  default:
    break;
  }
  QString s = c.kind == ColumnKind::Text ? raw : v;
  s.replace('\'', QLatin1String("''"));
  if (e == DbEngine::MySql) {
    s.replace('\\', QLatin1String("\\\\"));
  }
  const QString quoted = QLatin1Char('\'') + s + QLatin1Char('\'');
  return (e == DbEngine::SqlServer && c.kind == ColumnKind::Text)
             ? QLatin1Char('N') + quoted
             : quoted;
}

QStringList insertSql(DbEngine e, const QString &tableName,
                      const QVector<Column> &columns, const Table &table,
                      int batchSize, QStringList *warnings) {
  QStringList out;
  if (columns.isEmpty() || table.rows.isEmpty()) {
    return out;
  }
  QStringList names;
  for (const Column &c : columns) {
    names << DbCatalog::quoteIdentifier(e, c.name);
  }
  const QString head = QStringLiteral("INSERT INTO %1 (%2) VALUES\n")
                           .arg(DbCatalog::quoteIdentifier(e, tableName),
                                names.join(QStringLiteral(", ")));
  QStringList batch;
  int rowNo = 0;
  auto flush = [&]() {
    if (!batch.isEmpty()) {
      out << head + batch.join(QStringLiteral(",\n")) + QLatin1Char(';');
      batch.clear();
    }
  };
  for (const QStringList &r : table.rows) {
    ++rowNo;
    if (r.size() != columns.size() && warnings) {
      *warnings << QStringLiteral("Row %1 has %2 field(s), expected %3")
                       .arg(rowNo + 1)
                       .arg(r.size())
                       .arg(columns.size());
    }
    QStringList vals;
    for (int i = 0; i < columns.size(); ++i) {
      vals << literal(e, columns[i], i < r.size() ? r[i] : QString());
    }
    batch << QStringLiteral("  (%1)").arg(vals.join(QStringLiteral(", ")));
    if (batch.size() >= std::max(1, batchSize)) {
      flush();
    }
  }
  flush();
  return out;
}

} // namespace CsvImport
