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

QString lineCommentPrefix(const QString &language) {
  static const QSet<QString> slash = {"c",    "cpp",  "go",   "glsl",
                                      "hlsl", "java", "js",   "ts",
                                      "rust", "wgsl", "metal"};
  static const QSet<QString> hash = {"py",         "sh",    "yaml",
                                     "dockerfile", "make",  "cmake",
                                     "bazel",      "meson", "ninja"};
  if (slash.contains(language)) {
    return QStringLiteral("//");
  }
  if (hash.contains(language)) {
    return QStringLiteral("#");
  }
  if (language == QLatin1String("sql")) {
    return QStringLiteral("--");
  }
  if (language == QLatin1String("latex")) {
    return QStringLiteral("%");
  }
  return {};
}

QStringList toggleLineComments(const QStringList &lines,
                               const QString &prefix) {
  if (prefix.isEmpty()) {
    return lines;
  }

  auto indentOf = [](const QString &line) {
    int i = 0;
    while (i < line.size() && line.at(i).isSpace()) {
      ++i;
    }
    return i;
  };

  bool anyCode = false;
  bool allCommented = true;
  int minIndent = -1;
  for (const QString &line : lines) {
    if (line.trimmed().isEmpty()) {
      continue;
    }
    anyCode = true;
    const int indent = indentOf(line);
    minIndent = minIndent < 0 ? indent : qMin(minIndent, indent);
    if (!line.mid(indent).startsWith(prefix)) {
      allCommented = false;
    }
  }
  if (!anyCode) {
    return lines;
  }

  QStringList result;
  result.reserve(lines.size());
  for (const QString &line : lines) {
    if (line.trimmed().isEmpty()) {
      result.append(line);
    } else if (allCommented) {
      const int indent = indentOf(line);
      int cut = indent + prefix.size();
      if (cut < line.size() && line.at(cut) == ' ') {
        ++cut;
      }
      result.append(line.left(indent) + line.mid(cut));
    } else {
      result.append(line.left(minIndent) + prefix + QLatin1Char(' ') +
                    line.mid(minIndent));
    }
  }
  return result;
}

} // namespace TextTransforms
