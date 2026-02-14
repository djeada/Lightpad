#include "filedirtreemodel.h"
#include <QDebug>

FileDirTreeModel::FileDirTreeModel(QObject *parent)
    : QObject(parent), clipboardOperation(ClipboardOperation::None) {}

bool FileDirTreeModel::createNewFile(const QString &dirPath,
                                     const QString &fileName) {
  QString fullPath = dirPath + QDir::separator() + fileName;

  if (QFile::exists(fullPath)) {
    emit errorOccurred("File already exists");
    return false;
  }

  QFile file(fullPath);
  if (file.open(QIODevice::WriteOnly)) {
    file.close();
    emit modelUpdated();
    return true;
  }

  emit errorOccurred("Failed to create file");
  return false;
}

bool FileDirTreeModel::createNewDirectory(const QString &parentPath,
                                          const QString &dirName) {
  QString fullPath = parentPath + QDir::separator() + dirName;

  if (QDir(fullPath).exists()) {
    emit errorOccurred("Directory already exists");
    return false;
  }

  if (QDir().mkdir(fullPath)) {
    emit modelUpdated();
    return true;
  }

  emit errorOccurred("Failed to create directory");
  return false;
}

bool FileDirTreeModel::removeFileOrDirectory(const QString &path) {
  QFileInfo fileInfo(path);

  if (fileInfo.isFile()) {
    if (QFile::remove(path)) {
      emit modelUpdated();
      return true;
    }
  } else if (fileInfo.isDir()) {
    if (removeRecursively(path)) {
      emit modelUpdated();
      return true;
    }
  }

  emit errorOccurred("Failed to remove file or directory");
  return false;
}

bool FileDirTreeModel::renameFileOrDirectory(const QString &oldPath,
                                             const QString &newPath) {
  if (QFile::rename(oldPath, newPath)) {
    emit modelUpdated();
    return true;
  }

  emit errorOccurred("Failed to rename");
  return false;
}

bool FileDirTreeModel::duplicateFile(const QString &filePath) {
  if (!QFileInfo(filePath).isFile()) {
    emit errorOccurred("Can only duplicate files");
    return false;
  }

  QString newPath = addUniqueSuffix(filePath);
  if (QFile::copy(filePath, newPath)) {
    emit modelUpdated();
    return true;
  }

  emit errorOccurred("Failed to duplicate file");
  return false;
}

bool FileDirTreeModel::copyToClipboard(const QString &path) {
  clipboardPath = path;
  clipboardOperation = ClipboardOperation::Copy;
  return true;
}

bool FileDirTreeModel::cutToClipboard(const QString &path) {
  clipboardPath = path;
  clipboardOperation = ClipboardOperation::Cut;
  return true;
}

bool FileDirTreeModel::pasteFromClipboard(const QString &destPath) {
  if (clipboardPath.isEmpty() ||
      clipboardOperation == ClipboardOperation::None) {
    emit errorOccurred("Nothing to paste");
    return false;
  }

  if (!QFile::exists(clipboardPath)) {
    emit errorOccurred("Source file no longer exists");
    clipboardPath.clear();
    clipboardOperation = ClipboardOperation::None;
    return false;
  }

  QFileInfo sourceInfo(clipboardPath);
  QString fileName = sourceInfo.fileName();
  QString targetPath = destPath;

  if (QFileInfo(destPath).isFile()) {
    targetPath = QFileInfo(destPath).absolutePath();
  }

  QString fullTargetPath = targetPath + QDir::separator() + fileName;

  if (sourceInfo.isDir() && fullTargetPath.startsWith(clipboardPath)) {
    emit errorOccurred("Cannot move directory into itself or its subdirectory");
    return false;
  }

  fullTargetPath = addUniqueSuffix(fullTargetPath);

  bool success = false;

  if (clipboardOperation == ClipboardOperation::Copy) {
    if (sourceInfo.isFile()) {
      success = QFile::copy(clipboardPath, fullTargetPath);
    } else if (sourceInfo.isDir()) {
      success = copyRecursively(clipboardPath, fullTargetPath);
    }

  } else if (clipboardOperation == ClipboardOperation::Cut) {
    if (sourceInfo.isFile()) {
      success = QFile::rename(clipboardPath, fullTargetPath);
    } else if (sourceInfo.isDir()) {

      if (copyRecursively(clipboardPath, fullTargetPath)) {
        success = removeRecursively(clipboardPath);
      }
    }
    if (success) {
      clipboardPath.clear();
      clipboardOperation = ClipboardOperation::None;
    }
  }

  if (success) {
    emit modelUpdated();
    return true;
  }

  emit errorOccurred("Failed to paste");
  return false;
}

QString FileDirTreeModel::getAbsolutePath(const QString &path) {
  return QFileInfo(path).absoluteFilePath();
}

QString FileDirTreeModel::addUniqueSuffix(const QString &fileName) {
  if (!QFile::exists(fileName))
    return fileName;

  QFileInfo fileInfo(fileName);
  QString ret;

  QString secondPart = fileInfo.completeSuffix();
  QString firstPart;
  if (!secondPart.isEmpty()) {
    secondPart = "." + secondPart;
    firstPart = fileName.left(fileName.size() - secondPart.size());
  } else {
    firstPart = fileName;
  }

  for (int ii = 1;; ii++) {
    ret = QString("%1 (%2)%3").arg(firstPart).arg(ii).arg(secondPart);
    if (!QFile::exists(ret)) {
      return ret;
    }
  }
}

bool FileDirTreeModel::copyRecursively(const QString &srcPath,
                                       const QString &destPath) {
  QFileInfo srcInfo(srcPath);

  if (srcInfo.isFile()) {
    return QFile::copy(srcPath, destPath);
  }

  if (!srcInfo.isDir()) {
    return false;
  }

  QDir destDir(destPath);
  if (!destDir.exists()) {
    if (!QDir().mkdir(destPath)) {
      return false;
    }
  }

  QDir srcDir(srcPath);
  QStringList entries =
      srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

  for (const QString &entry : entries) {
    QString srcEntry = srcPath + QDir::separator() + entry;
    QString destEntry = destPath + QDir::separator() + entry;

    if (!copyRecursively(srcEntry, destEntry)) {
      return false;
    }
  }

  return true;
}

bool FileDirTreeModel::removeRecursively(const QString &path) {
  QFileInfo fileInfo(path);

  if (fileInfo.isFile()) {
    return QFile::remove(path);
  }

  if (!fileInfo.isDir()) {
    return false;
  }

  QDir dir(path);
  QStringList entries =
      dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

  for (const QString &entry : entries) {
    QString fullPath = path + QDir::separator() + entry;
    if (!removeRecursively(fullPath)) {
      return false;
    }
  }

  return dir.rmdir(path);
}
