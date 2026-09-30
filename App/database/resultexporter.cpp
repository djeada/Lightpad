#include "resultexporter.h"

#include "dbcatalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace ResultExporter {

QString formatName(Format f) {
  switch (f) {
  case Format::Csv:
    return QStringLiteral("CSV");
  case Format::Tsv:
    return QStringLiteral("TSV");
  case Format::Json:
    return QStringLiteral("JSON");
  case Format::Markdown:
    return QStringLiteral("Markdown table");
  case Format::SqlInsert:
    return QStringLiteral("SQL INSERT statements");
  }
  return {};
}

QString fileExtension(Format f) {
  switch (f) {
  case Format::Csv:
    return QStringLiteral("csv");
  case Format::Tsv:
    return QStringLiteral("tsv");
  case Format::Json:
    return QStringLiteral("json");
  case Format::Markdown:
    return QStringLiteral("md");
  case Format::SqlInsert:
    return QStringLiteral("sql");
  }
  return QStringLiteral("txt");
}

QString displayText(const QVariant &v) {
  if (v.isNull() || !v.isValid()) {
    return QStringLiteral("NULL");
  }
  if (v.typeId() == QMetaType::Bool) {
    return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  }
  if (v.typeId() == QMetaType::Double) {
    return QString::number(v.toDouble(), 'g', 15);
  }
  return v.toString();
}

QString sqlLiteral(const QVariant &v) {
  if (v.isNull() || !v.isValid()) {
    return QStringLiteral("NULL");
  }
  switch (v.typeId()) {
  case QMetaType::Int:
  case QMetaType::UInt:
  case QMetaType::LongLong:
  case QMetaType::ULongLong:
  case QMetaType::Double:
    return displayText(v);
  case QMetaType::Bool:
    return v.toBool() ? QStringLiteral("1") : QStringLiteral("0");
  default:
    break;
  }
  const QString text = v.toString();
  static const QRegularExpression number(
      QStringLiteral("^-?(0|[1-9]\\d*)(\\.\\d+)?$"));
  if (number.match(text).hasMatch() && text.size() < 16) {
    return text;
  }
  QString escaped = text;
  escaped.replace('\'', QLatin1String("''"));
  return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

static QString csvField(const QVariant &v, QChar sep) {
  if (v.isNull() || !v.isValid()) {
    return {};
  }
  QString text = displayText(v);
  if (text.contains(sep) || text.contains('"') || text.contains('\n') ||
      text.contains('\r')) {
    text.replace('"', QLatin1String("\"\""));
    return QLatin1Char('"') + text + QLatin1Char('"');
  }
  return text;
}

static QString tsvField(const QVariant &v) {
  if (v.isNull() || !v.isValid()) {
    return {};
  }
  QString text = displayText(v);
  text.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
  return text;
}

QString exportResult(const DbResultSet &rs, Format format,
                     const QVector<int> &rowSel, const QVector<int> &colSel,
                     bool includeHeader, const QString &tableName,
                     DbEngine engine) {
  QVector<int> cols = colSel;
  if (cols.isEmpty()) {
    for (int c = 0; c < rs.columns.size(); ++c) {
      cols.append(c);
    }
  }
  QVector<int> rows = rowSel;
  if (rows.isEmpty()) {
    for (int r = 0; r < rs.rows.size(); ++r) {
      rows.append(r);
    }
  }
  auto cell = [&](int r, int c) -> QVariant {
    if (r < 0 || r >= rs.rows.size()) {
      return {};
    }
    const QVector<QVariant> &row = rs.rows[r];
    return c >= 0 && c < row.size() ? row[c] : QVariant();
  };

  QString out;
  switch (format) {
  case Format::Csv:
  case Format::Tsv: {
    const bool csv = format == Format::Csv;
    const QChar sep = csv ? ',' : '\t';
    if (includeHeader) {
      QStringList head;
      for (int c : cols) {
        head << (csv ? csvField(rs.columns[c].name, sep)
                     : tsvField(rs.columns[c].name));
      }
      out += head.join(sep) + QLatin1Char('\n');
    }
    for (int r : rows) {
      QStringList line;
      for (int c : cols) {
        line << (csv ? csvField(cell(r, c), sep) : tsvField(cell(r, c)));
      }
      out += line.join(sep) + QLatin1Char('\n');
    }
    break;
  }
  case Format::Json: {
    QJsonArray arr;
    for (int r : rows) {
      QJsonObject obj;
      for (int c : cols) {
        const QVariant v = cell(r, c);
        if (v.isNull() || !v.isValid()) {
          obj.insert(rs.columns[c].name, QJsonValue::Null);
        } else if (v.typeId() == QMetaType::Bool) {
          obj.insert(rs.columns[c].name, v.toBool());
        } else if (v.typeId() == QMetaType::Int ||
                   v.typeId() == QMetaType::LongLong ||
                   v.typeId() == QMetaType::UInt ||
                   v.typeId() == QMetaType::ULongLong) {
          obj.insert(rs.columns[c].name, v.toLongLong());
        } else if (v.typeId() == QMetaType::Double) {
          obj.insert(rs.columns[c].name, v.toDouble());
        } else {
          obj.insert(rs.columns[c].name, v.toString());
        }
      }
      arr.append(obj);
    }
    out = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    break;
  }
  case Format::Markdown: {
    auto mdCell = [](const QVariant &v) {
      QString t = displayText(v);
      t.replace('|', QLatin1String("\\|")).replace('\n', QLatin1String("<br>"));
      return t;
    };
    QStringList head;
    QStringList rule;
    for (int c : cols) {
      head << mdCell(rs.columns[c].name);
      rule << QStringLiteral("---");
    }
    out += QStringLiteral("| ") + head.join(QStringLiteral(" | ")) +
           QStringLiteral(" |\n");
    out += QStringLiteral("| ") + rule.join(QStringLiteral(" | ")) +
           QStringLiteral(" |\n");
    for (int r : rows) {
      QStringList line;
      for (int c : cols) {
        line << mdCell(cell(r, c));
      }
      out += QStringLiteral("| ") + line.join(QStringLiteral(" | ")) +
             QStringLiteral(" |\n");
    }
    break;
  }
  case Format::SqlInsert: {
    QStringList names;
    for (int c : cols) {
      names << DbCatalog::quoteIdentifier(engine, rs.columns[c].name);
    }
    for (int r : rows) {
      QStringList vals;
      for (int c : cols) {
        vals << sqlLiteral(cell(r, c));
      }
      out += QStringLiteral("INSERT INTO %1 (%2) VALUES (%3);\n")
                 .arg(tableName, names.join(QStringLiteral(", ")),
                      vals.join(QStringLiteral(", ")));
    }
    break;
  }
  }
  return out;
}

} // namespace ResultExporter
