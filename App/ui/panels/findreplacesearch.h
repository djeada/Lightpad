#ifndef FINDREPLACESEARCH_H
#define FINDREPLACESEARCH_H

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

struct GlobalSearchResult {
  QString filePath;
  int lineNumber;
  int columnNumber;
  int matchStart;
  int matchLength;
  QString lineContent;
};

namespace FindReplaceSearch {

QString expandRegexReplacement(const QString &replaceWord,
                               const QRegularExpressionMatch &match);

QVector<GlobalSearchResult>
collectMatchesInContent(const QString &filePath, const QString &content,
                        const QRegularExpression &pattern);

QStringList projectFiles(const QString &rootPath, const QString &maskText,
                         const std::function<bool()> &isCancelled = {});

} // namespace FindReplaceSearch

#endif
