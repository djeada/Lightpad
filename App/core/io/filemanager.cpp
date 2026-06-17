#include "filemanager.h"
#include "../logging/logger.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtGlobal>

FileManager &FileManager::instance() {
  static FileManager instance;
  return instance;
}

FileManager::FileManager() : QObject(nullptr) {}

bool FileManager::isPythonFile(const QString &filePath,
                               const QString &content) {
  const QString suffix = QFileInfo(filePath).suffix().toLower();
  if (suffix == "py" || suffix == "pyw" || suffix == "pyi") {
    return true;
  }

  const int firstLineEnd = content.indexOf('\n');
  const QString firstLine =
      (firstLineEnd >= 0 ? content.left(firstLineEnd) : content).trimmed();
  return firstLine.startsWith("#!") &&
         firstLine.contains("python", Qt::CaseInsensitive);
}

QString FileManager::expandTabsToSpaces(const QString &content, int tabWidth) {
  const int width = qMax(1, tabWidth);
  QString expanded;
  expanded.reserve(content.size());

  int column = 0;
  for (const QChar &ch : content) {
    if (ch == '\t') {
      const int spaces = width - (column % width);
      expanded.append(QString(" ").repeated(spaces));
      column += spaces;
    } else {
      expanded.append(ch);
      if (ch == '\n' || ch == '\r') {
        column = 0;
      } else {
        ++column;
      }
    }
  }

  return expanded;
}

QString FileManager::normalizeContentForSave(const QString &filePath,
                                             const QString &content,
                                             int tabWidth) {
  if (!isPythonFile(filePath, content) || !content.contains('\t')) {
    return content;
  }

  return expandTabsToSpaces(content, tabWidth);
}

FileManager::FileResult FileManager::readFile(const QString &filePath) {
  FileResult result;
  result.success = false;

  if (filePath.isEmpty()) {
    result.errorMessage = "File path is empty";
    LOG_WARNING(result.errorMessage);
    emit fileError(filePath, result.errorMessage);
    return result;
  }

  QFile file(filePath);
  if (!file.exists()) {
    result.errorMessage = QString("File does not exist: %1").arg(filePath);
    LOG_WARNING(result.errorMessage);
    emit fileError(filePath, result.errorMessage);
    return result;
  }

  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    result.errorMessage =
        QString("Cannot open file for reading: %1").arg(filePath);
    LOG_ERROR(result.errorMessage);
    emit fileError(filePath, result.errorMessage);
    return result;
  }

  QTextStream stream(&file);
  result.content = stream.readAll();
  result.success = true;
  file.close();

  LOG_INFO(QString("Successfully read file: %1").arg(filePath));
  emit fileOpened(filePath);

  return result;
}

FileManager::FileResult FileManager::writeFile(const QString &filePath,
                                               const QString &content) {
  FileResult result;
  result.success = false;

  if (filePath.isEmpty()) {
    result.errorMessage = "File path is empty";
    LOG_WARNING(result.errorMessage);
    emit fileError(filePath, result.errorMessage);
    return result;
  }

  QFile file(filePath);
  if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
    result.errorMessage =
        QString("Cannot open file for writing: %1").arg(filePath);
    LOG_ERROR(result.errorMessage);
    emit fileError(filePath, result.errorMessage);
    return result;
  }

  QTextStream stream(&file);
  stream << normalizeContentForSave(filePath, content);
  file.close();

  result.success = true;
  LOG_INFO(QString("Successfully saved file: %1").arg(filePath));
  emit fileSaved(filePath);

  return result;
}

bool FileManager::fileExists(const QString &filePath) {
  return QFileInfo(filePath).exists();
}

QString FileManager::getFileExtension(const QString &filePath) {
  return QFileInfo(filePath).completeSuffix();
}

QString FileManager::getFileName(const QString &filePath) {
  return QFileInfo(filePath).fileName();
}

QString FileManager::getDirectory(const QString &filePath) {
  return QFileInfo(filePath).dir().path();
}
