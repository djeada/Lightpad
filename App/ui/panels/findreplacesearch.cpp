#include "findreplacesearch.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <algorithm>

namespace {

int parseGroupReference(const QString &text, int start, int groupCount,
                        int *consumed) {
  int end = start;
  while (end < text.size() && text.at(end).isDigit()) {
    ++end;
  }
  for (int length = end - start; length > 0; --length) {
    bool ok = false;
    const int group = text.mid(start, length).toInt(&ok);
    if (ok && group <= groupCount) {
      *consumed = length;
      return group;
    }
  }
  *consumed = 0;
  return -1;
}

} // namespace

namespace FindReplaceSearch {

QString expandRegexReplacement(const QString &replaceWord,
                               const QRegularExpressionMatch &match) {
  const int groupCount = match.regularExpression().captureCount();
  QString result;
  result.reserve(replaceWord.size());

  int i = 0;
  while (i < replaceWord.size()) {
    const QChar c = replaceWord.at(i);
    const QChar next =
        i + 1 < replaceWord.size() ? replaceWord.at(i + 1) : QChar();

    if (c == '\\') {
      if (next == '\\') {
        result += '\\';
        i += 2;
        continue;
      }
      if (next.isDigit()) {
        int consumed = 0;
        const int group =
            parseGroupReference(replaceWord, i + 1, groupCount, &consumed);
        if (group >= 0) {
          result += match.captured(group);
          i += 1 + consumed;
          continue;
        }
      }
    } else if (c == '$') {
      if (next == '$') {
        result += '$';
        i += 2;
        continue;
      }
      if (next == '{') {
        const int close = replaceWord.indexOf('}', i + 2);
        if (close > i + 2) {
          bool ok = false;
          const QString digits = replaceWord.mid(i + 2, close - i - 2);
          const bool allDigits =
              std::all_of(digits.cbegin(), digits.cend(),
                          [](const QChar &ch) { return ch.isDigit(); });
          const int group = allDigits ? digits.toInt(&ok) : -1;
          if (ok && group <= groupCount) {
            result += match.captured(group);
            i = close + 1;
            continue;
          }
        }
      } else if (next.isDigit()) {
        int consumed = 0;
        const int group =
            parseGroupReference(replaceWord, i + 1, groupCount, &consumed);
        if (group >= 0) {
          result += match.captured(group);
          i += 1 + consumed;
          continue;
        }
      }
    }

    result += c;
    ++i;
  }

  return result;
}

QVector<GlobalSearchResult>
collectMatchesInContent(const QString &filePath, const QString &content,
                        const QRegularExpression &pattern) {
  QVector<GlobalSearchResult> matchesForFile;
  QStringList lines = content.split('\n');

  QVector<int> lineStarts;
  int lineStart = 0;
  lineStarts.reserve(lines.size());
  for (const QString &line : lines) {
    lineStarts.append(lineStart);
    lineStart += line.length() + 1;
  }

  QRegularExpressionMatchIterator matches = pattern.globalMatch(content);
  while (matches.hasNext()) {
    QRegularExpressionMatch match = matches.next();
    const int matchStart = match.capturedStart();
    const int matchLength = match.capturedLength();
    if (matchStart < 0) {
      continue;
    }

    int lineNum = 0;
    for (int i = 0; i < lineStarts.size(); ++i) {
      if (i + 1 < lineStarts.size() && matchStart >= lineStarts[i + 1]) {
        continue;
      }
      lineNum = i;
      break;
    }

    GlobalSearchResult result;
    result.filePath = filePath;
    result.lineNumber = lineNum + 1;
    result.columnNumber = matchStart - lineStarts.value(lineNum) + 1;
    result.matchStart = matchStart;
    result.matchLength = matchLength;
    result.lineContent =
        (lineNum < lines.size()) ? lines[lineNum].trimmed() : QString();
    matchesForFile.append(result);
  }

  return matchesForFile;
}

QStringList projectFiles(const QString &rootPath, const QString &maskText,
                         const std::function<bool()> &isCancelled) {
  QStringList files;

  if (rootPath.isEmpty()) {
    return files;
  }

  QStringList maskPatterns;
  const QString trimmedMask = maskText.trimmed();
  if (!trimmedMask.isEmpty()) {
    for (const QString &token : trimmedMask.split(',')) {
      QString pattern = token.trimmed();
      if (!pattern.isEmpty()) {
        maskPatterns.append(pattern);
      }
    }
  }

  QDirIterator it(rootPath, QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);

  static const QStringList searchableExtensions = {
      "cpp",   "hpp",  "c",       "h",    "cc",   "cxx",  "hxx",  "py",
      "pyw",   "js",   "jsx",     "ts",   "tsx",  "java", "go",   "rs",
      "rb",    "php",  "swift",   "kt",   "kts",  "cs",   "html", "htm",
      "css",   "scss", "sass",    "less", "json", "xml",  "yaml", "yml",
      "toml",  "md",   "txt",     "rst",  "sql",  "sh",   "bash", "zsh",
      "cmake", "make", "makefile"};

  QVector<QRegularExpression> maskRegexes;
  for (const QString &pattern : maskPatterns) {
    QString regexPattern =
        QRegularExpression::wildcardToRegularExpression(pattern);
    QRegularExpression re(regexPattern,
                          QRegularExpression::CaseInsensitiveOption);
    if (re.isValid()) {
      maskRegexes.append(re);
    }
  }

  int scanned = 0;
  while (it.hasNext()) {
    if (isCancelled && (++scanned & 0xFF) == 0 && isCancelled()) {
      return QStringList();
    }
    QString filePath = it.next();
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();

    if (!maskRegexes.isEmpty()) {
      bool matched = std::any_of(maskRegexes.cbegin(), maskRegexes.cend(),
                                 [&fileName](const QRegularExpression &re) {
                                   return re.match(fileName).hasMatch();
                                 });
      if (matched) {
        files.append(filePath);
      }
      continue;
    }

    QString ext = fileInfo.suffix().toLower();
    QString fileNameLower = fileInfo.fileName().toLower();

    if (searchableExtensions.contains(ext) ||
        searchableExtensions.contains(fileNameLower)) {
      files.append(filePath);
    }
  }

  return files;
}

} // namespace FindReplaceSearch
