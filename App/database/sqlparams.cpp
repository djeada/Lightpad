#include "sqlparams.h"

#include <QRegularExpression>
#include <QVector>

namespace {

struct Found {
  int start;
  int length;
  QString name;
};

bool isIdentStart(QChar c) { return c.isLetter() || c == '_'; }
bool isIdentChar(QChar c) { return c.isLetterOrNumber() || c == '_'; }

QVector<Found> scan(const QString &s) {
  QVector<Found> out;
  const int n = s.size();
  int i = 0;
  while (i < n) {
    const QChar c = s[i];
    if (c == '\'' || c == '"' || c == '`') {
      const QChar q = c;
      ++i;
      while (i < n) {
        if (s[i] == q) {
          if (i + 1 < n && s[i + 1] == q) {
            i += 2;
            continue;
          }
          break;
        }
        ++i;
      }
      ++i;
    } else if (c == '[') {
      const int e = s.indexOf(']', i);
      i = e < 0 ? n : e + 1;
    } else if (c == '-' && i + 1 < n && s[i + 1] == '-') {
      const int e = s.indexOf('\n', i);
      i = e < 0 ? n : e + 1;
    } else if (c == '/' && i + 1 < n && s[i + 1] == '*') {
      const int e = s.indexOf(QLatin1String("*/"), i + 2);
      i = e < 0 ? n : e + 2;
    } else if (c == '$') {

      int j = i + 1;
      while (j < n && isIdentChar(s[j])) {
        ++j;
      }
      if (j < n && s[j] == '$') {
        const QString tag = s.mid(i, j - i + 1);
        const int e = s.indexOf(tag, j + 1);
        i = e < 0 ? n : e + tag.size();
      } else {
        ++i;
      }
    } else if (c == ':') {
      if (i + 1 < n && s[i + 1] == ':') {
        i += 2;
        continue;
      }
      if (i > 0 && isIdentChar(s[i - 1])) {
        ++i;
        continue;
      }
      if (i + 1 < n && isIdentStart(s[i + 1])) {
        int j = i + 1;
        while (j < n && isIdentChar(s[j])) {
          ++j;
        }
        out.append({i, j - i, s.mid(i + 1, j - i - 1)});
        i = j;
      } else {
        ++i;
      }
    } else {
      ++i;
    }
  }
  return out;
}

} // namespace

namespace SqlParams {

QStringList findParameters(const QString &sql) {
  QStringList names;
  for (const Found &f : scan(sql)) {
    if (!names.contains(f.name)) {
      names << f.name;
    }
  }
  return names;
}

QString literalFromInput(const QString &input, DbEngine engine) {
  const QString t = input.trimmed();
  static const QRegularExpression number(
      QStringLiteral("^[-+]?(\\d+(\\.\\d+)?|\\.\\d+)([eE][-+]?\\d+)?$"));
  if (t.compare(QLatin1String("null"), Qt::CaseInsensitive) == 0) {
    return QStringLiteral("NULL");
  }
  if (number.match(t).hasMatch()) {
    return t;
  }
  const bool isTrue =
      t.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
  const bool isFalse =
      t.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0;
  if (isTrue || isFalse) {
    if (engine == DbEngine::PostgreSql || engine == DbEngine::MySql) {
      return isTrue ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    }
    return isTrue ? QStringLiteral("1") : QStringLiteral("0");
  }
  QString e = input;
  e.replace('\'', QLatin1String("''"));
  if (engine == DbEngine::MySql) {
    e.replace('\\', QLatin1String("\\\\"));
  }
  return QLatin1Char('\'') + e + QLatin1Char('\'');
}

QString bind(const QString &sql, const QHash<QString, QString> &inputs,
             DbEngine engine, QStringList *missing) {
  QString out = sql;
  const QVector<Found> found = scan(sql);
  for (int k = found.size() - 1; k >= 0; --k) {
    const Found &f = found[k];
    if (!inputs.contains(f.name)) {
      if (missing && !missing->contains(f.name)) {
        missing->prepend(f.name);
      }
      continue;
    }
    out.replace(f.start, f.length,
                literalFromInput(inputs.value(f.name), engine));
  }
  return out;
}

} // namespace SqlParams
