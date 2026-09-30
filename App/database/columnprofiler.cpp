#include "columnprofiler.h"

#include "resultexporter.h"

#include <QHash>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

QString ColumnProfile::kindName() const {
  switch (kind) {
  case ColumnKind::Empty:
    return QStringLiteral("empty");
  case ColumnKind::Number:
    return QStringLiteral("number");
  case ColumnKind::Boolean:
    return QStringLiteral("boolean");
  case ColumnKind::Temporal:
    return QStringLiteral("date/time");
  case ColumnKind::Text:
    return QStringLiteral("text");
  }
  return {};
}

namespace ColumnProfiler {

namespace {
constexpr int kDistinctCap = 50000;

bool looksTemporal(const QString &s) {
  static const QRegularExpression re(QStringLiteral(
      "^\\d{4}-\\d{2}-\\d{2}([ "
      "T]\\d{2}:\\d{2}(:\\d{2}(\\.\\d+)?)?(Z|[+-]\\d{2}(:?\\d{2})?)?)?$"));
  return re.match(s).hasMatch();
}
} // namespace

bool parseNumber(const QVariant &value, double *out) {
  switch (value.typeId()) {
  case QMetaType::Int:
  case QMetaType::UInt:
  case QMetaType::LongLong:
  case QMetaType::ULongLong:
  case QMetaType::Double:
  case QMetaType::Float:
    if (out) {
      *out = value.toDouble();
    }
    return true;
  case QMetaType::QString: {
    const QString s = value.toString().trimmed();
    if (s.isEmpty() || s.size() > 40) {
      return false;
    }
    const QChar first = s[0];
    if (!(first.isDigit() || first == '-' || first == '+' || first == '.')) {
      return false;
    }
    bool ok = false;
    const double d = s.toDouble(&ok);
    if (ok && std::isfinite(d)) {
      if (out) {
        *out = d;
      }
      return true;
    }
    return false;
  }
  default:
    return false;
  }
}

ColumnProfile profileColumn(const DbResultSet &rs, int column, int topValues) {
  ColumnProfile p;
  if (column < 0 || column >= rs.columns.size()) {
    return p;
  }
  p.name = rs.columns[column].name;
  p.rows = rs.rows.size();

  QHash<QString, qint64> counts;
  QVector<double> numbers;
  bool allNumeric = true;
  bool allBool = true;
  bool allTemporal = true;
  bool any = false;
  QString minText;
  QString maxText;
  bool firstText = true;
  bool firstLength = true;
  double minNum = 0;
  double maxNum = 0;
  QString minNumText;
  QString maxNumText;

  for (const QVector<QVariant> &row : rs.rows) {
    const QVariant v = column < row.size() ? row[column] : QVariant();
    if (v.isNull() || !v.isValid()) {
      ++p.nulls;
      continue;
    }
    any = true;
    const QString text = ResultExporter::displayText(v);
    const int len = text.size();
    if (firstLength) {
      p.minLength = p.maxLength = len;
      firstLength = false;
    } else {
      p.minLength = qMin(p.minLength, len);
      p.maxLength = qMax(p.maxLength, len);
    }

    if (counts.size() < kDistinctCap || counts.contains(text)) {
      ++counts[text];
    } else {
      p.distinctIsLowerBound = true;
    }

    double d = 0;
    if (allNumeric && parseNumber(v, &d)) {
      if (numbers.isEmpty() || d < minNum) {
        minNum = d;
        minNumText = text;
      }
      if (numbers.isEmpty() || d > maxNum) {
        maxNum = d;
        maxNumText = text;
      }
      numbers.append(d);
    } else {
      allNumeric = false;
    }
    if (allBool) {
      const bool isBool =
          v.typeId() == QMetaType::Bool || text == QLatin1String("true") ||
          text == QLatin1String("false") || text == QLatin1String("t") ||
          text == QLatin1String("f");
      if (!isBool) {
        allBool = false;
      }
    }
    if (allTemporal && !looksTemporal(text)) {
      allTemporal = false;
    }
    if (firstText) {
      minText = maxText = text;
      firstText = false;
    } else {
      if (QString::compare(text, minText, Qt::CaseSensitive) < 0) {
        minText = text;
      }
      if (QString::compare(text, maxText, Qt::CaseSensitive) > 0) {
        maxText = text;
      }
    }
  }

  p.distinct = counts.size();
  if (!any) {
    p.kind = ColumnKind::Empty;
    return p;
  }
  if (allNumeric && !numbers.isEmpty()) {

    p.kind = ColumnKind::Number;
    p.hasNumbers = true;
    p.min = minNumText;
    p.max = maxNumText;
    double sum = 0;
    for (double d : numbers) {
      sum += d;
    }
    p.sum = sum;
    p.mean = sum / numbers.size();
    double sq = 0;
    for (double d : numbers) {
      sq += (d - p.mean) * (d - p.mean);
    }
    p.stddev = numbers.size() > 1 ? std::sqrt(sq / (numbers.size() - 1)) : 0;
    std::sort(numbers.begin(), numbers.end());
    const int n = numbers.size();
    p.median =
        n % 2 ? numbers[n / 2] : (numbers[n / 2 - 1] + numbers[n / 2]) / 2;
  } else {
    p.kind = allBool ? ColumnKind::Boolean
                     : (allTemporal ? ColumnKind::Temporal : ColumnKind::Text);
    p.min = minText;
    p.max = maxText;
  }

  QVector<ValueCount> all;
  all.reserve(counts.size());
  for (auto it = counts.begin(); it != counts.end(); ++it) {
    all.append({it.key(), it.value()});
  }
  const int keep = qMin<int>(topValues, all.size());
  std::partial_sort(all.begin(), all.begin() + keep, all.end(),
                    [](const ValueCount &a, const ValueCount &b) {
                      if (a.count != b.count) {
                        return a.count > b.count;
                      }
                      return a.value < b.value;
                    });
  p.top = all.mid(0, keep);
  return p;
}

QVector<ColumnProfile> profileAll(const DbResultSet &rs, int topValues) {
  QVector<ColumnProfile> out;
  for (int c = 0; c < rs.columns.size(); ++c) {
    out.append(profileColumn(rs, c, topValues));
  }
  return out;
}

} // namespace ColumnProfiler
