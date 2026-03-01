#ifndef DIAGNOSTICUTILS_H
#define DIAGNOSTICUTILS_H

#include <QString>
#include <QUrl>

namespace DiagnosticUtils {

inline QString filePathToUri(const QString &filePath) {
  return QUrl::fromLocalFile(filePath).toString();
}

inline QString uriToFilePath(const QString &uri) {
  return QUrl(uri).toLocalFile();
}

inline int clampLine(int line, int lineCount) {
  if (lineCount <= 0)
    return 0;
  if (line < 0)
    return 0;
  if (line >= lineCount)
    return lineCount - 1;
  return line;
}

inline int clampColumn(int column, int lineLength) {
  if (lineLength <= 0)
    return 0;
  if (column < 0)
    return 0;
  if (column > lineLength)
    return lineLength;
  return column;
}

} // namespace DiagnosticUtils

#endif
