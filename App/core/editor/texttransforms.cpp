#include "texttransforms.h"
#include <QSet>
#include <algorithm>

namespace TextTransforms {

QString sortLinesAscending(const QString &text) {
  QStringList lines = text.split('\n');
  std::sort(lines.begin(), lines.end(), [](const QString &a, const QString &b) {
    return a.compare(b, Qt::CaseInsensitive) < 0;
  });
  return lines.join('\n');
}

QString sortLinesDescending(const QString &text) {
  QStringList lines = text.split('\n');
  std::sort(lines.begin(), lines.end(), [](const QString &a, const QString &b) {
    return a.compare(b, Qt::CaseInsensitive) > 0;
  });
  return lines.join('\n');
}

QString toUppercase(const QString &text) { return text.toUpper(); }

QString toLowercase(const QString &text) { return text.toLower(); }

QString toTitleCase(const QString &text) {
  QString result;
  result.reserve(text.size());
  bool capitalizeNext = true;

  for (const QChar &c : text) {
    if (c.isLetter()) {
      if (capitalizeNext) {
        result.append(c.toUpper());
        capitalizeNext = false;
      } else {
        result.append(c.toLower());
      }
    } else {
      result.append(c);
      // Capitalize after word boundary characters
      if (c.isSpace() || c == '-' || c == '_' || c == '.' || c == ':' ||
          c == '/' || c == '\\' || c == '(' || c == '[' || c == '{' ||
          c == '<' || c == '"' || c == '\'' || c == '`') {
        capitalizeNext = true;
      }
    }
  }

  return result;
}

QString removeDuplicateLines(const QString &text) {
  QStringList lines = text.split('\n');
  QStringList result;
  QSet<QString> seen;

  for (const QString &line : lines) {
    if (!seen.contains(line)) {
      seen.insert(line);
      result.append(line);
    }
  }

  return result.join('\n');
}

QString reverseLines(const QString &text) {
  QStringList lines = text.split('\n');
  std::reverse(lines.begin(), lines.end());
  return lines.join('\n');
}

QString trimLines(const QString &text) {
  QStringList lines = text.split('\n');
  for (QString &line : lines) {
    line = line.trimmed();
  }
  return lines.join('\n');
}

} // namespace TextTransforms
