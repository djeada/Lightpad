#include "filedirtreemodel.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>

namespace {

constexpr char kGnomeCopiedFiles[] = "x-special/gnome-copied-files";
constexpr char kKdeCutSelection[] = "application/x-kde-cutselection";

bool isSafeRelativeName(const QString &name) {
  if (name.isEmpty() || name.contains(QChar::Null) ||
      QDir::isAbsolutePath(name) || name.startsWith('/') ||
      name.startsWith('\\')) {
    return false;
  }
  const QStringList parts = name.split(
      QRegularExpression(QStringLiteral("[/\\\\]")), Qt::SkipEmptyParts);
  if (parts.isEmpty()) {
    return false;
  }
  for (const QString &part : parts) {
    if (part == QLatin1String(".") || part == QLatin1String("..")) {
      return false;
    }
  }
  return true;
}

QString joinPath(const QString &dir, const QString &name) {
  if (dir.isEmpty()) {
    return name;
  }
  return QDir::cleanPath(dir + QLatin1Char('/') + name);
}

bool entryExists(const QString &path) {
  const QFileInfo info(path);
  return info.exists() || info.isSymLink();
}

} // namespace

FileDirTreeModel::FileDirTreeModel(QObject *parent) : QObject(parent) {
  if (QClipboard *clipboard = QGuiApplication::clipboard()) {
    connect(clipboard, &QClipboard::dataChanged, this,
            &FileDirTreeModel::clipboardChanged);
  }
}

QDir::Filters FileDirTreeModel::entryFilters() {
  return QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System;
}

QStringList FileDirTreeModel::topLevelPaths(const QStringList &paths) {
  QStringList normalized;
  for (const QString &path : paths) {
    if (!path.isEmpty()) {
      const QString absolute =
          QDir::cleanPath(QFileInfo(path).absoluteFilePath());
      if (!normalized.contains(absolute)) {
        normalized.append(absolute);
      }
    }
  }
  QStringList result;
  for (const QString &path : normalized) {
    const bool hasSelectedParent = std::any_of(
        normalized.cbegin(), normalized.cend(), [&](const QString &other) {
          const QFileInfo parent(other);
          return other != path && parent.isDir() && !parent.isSymLink() &&
                 path.startsWith(other.endsWith('/') ? other : other + '/');
        });
    if (!hasSelectedParent) {
      result.append(path);
    }
  }
  return result;
}

bool FileDirTreeModel::validateDestination(const QString &directory) {
  const QFileInfo info(directory);
  if (directory.isEmpty() || !info.isDir()) {
    emit errorOccurred(
        tr("The destination folder no longer exists: %1").arg(directory));
    return false;
  }
  if (!info.isWritable()) {
    emit errorOccurred(
        tr("The destination folder is read-only: %1").arg(directory));
    return false;
  }
  return true;
}

bool FileDirTreeModel::isInside(const QString &parentDir, const QString &path) {
  if (parentDir.isEmpty() || path.isEmpty()) {
    return false;
  }

  auto contains = [](const QString &parent, const QString &child) {
    return parent == child || child.startsWith(parent.endsWith(QLatin1Char('/'))
                                                   ? parent
                                                   : parent + QLatin1Char('/'));
  };

  const QString parent =
      QDir::cleanPath(QFileInfo(parentDir).absoluteFilePath());
  const QString child = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  if (contains(parent, child)) {
    return true;
  }

  return contains(canonicalPath(parentDir), canonicalPath(path));
}

QString FileDirTreeModel::canonicalPath(const QString &path) {
  const QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  QString current = absolute;
  QString suffix;
  while (true) {
    const QString canonical = QFileInfo(current).canonicalFilePath();
    if (!canonical.isEmpty()) {
      return QDir::cleanPath(canonical + suffix);
    }
    const QFileInfo info(current);
    const QString parent = info.absolutePath();
    if (parent == current || info.fileName().isEmpty()) {
      return absolute;
    }
    suffix = QLatin1Char('/') + info.fileName() + suffix;
    current = parent;
  }
}

bool FileDirTreeModel::createNewFile(const QString &dirPath,
                                     const QString &fileName,
                                     QString *createdPath) {
  const QString trimmed = fileName.trimmed();
  if (trimmed.isEmpty()) {
    emit errorOccurred(tr("Enter a file name."));
    return false;
  }
  if (!isSafeRelativeName(trimmed)) {
    emit errorOccurred(tr("\"%1\" is not a valid file name.").arg(trimmed));
    return false;
  }

  if (!validateDestination(dirPath)) {
    return false;
  }

  const QString fullPath = joinPath(dirPath, trimmed);
  if (!isInside(canonicalPath(dirPath), canonicalPath(fullPath))) {
    emit errorOccurred(
        tr("The new file must stay inside the selected folder."));
    return false;
  }
  if (entryExists(fullPath)) {
    emit errorOccurred(tr("\"%1\" already exists.").arg(trimmed));
    return false;
  }

  const QString parentDir = QFileInfo(fullPath).absolutePath();
  if (!QDir().mkpath(parentDir)) {
    emit errorOccurred(tr("Could not create the folder %1.").arg(parentDir));
    return false;
  }

  QFile file(fullPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
    emit errorOccurred(
        tr("Could not create \"%1\": %2").arg(trimmed, file.errorString()));
    return false;
  }
  file.close();

  if (createdPath) {
    *createdPath = fullPath;
  }
  emit modelUpdated();
  return true;
}

bool FileDirTreeModel::createNewDirectory(const QString &parentPath,
                                          const QString &dirName,
                                          QString *createdPath) {
  const QString trimmed = dirName.trimmed();
  if (trimmed.isEmpty()) {
    emit errorOccurred(tr("Enter a folder name."));
    return false;
  }
  if (!isSafeRelativeName(trimmed)) {
    emit errorOccurred(tr("\"%1\" is not a valid folder name.").arg(trimmed));
    return false;
  }

  if (!validateDestination(parentPath)) {
    return false;
  }

  const QString fullPath = joinPath(parentPath, trimmed);
  if (!isInside(canonicalPath(parentPath), canonicalPath(fullPath))) {
    emit errorOccurred(
        tr("The new folder must stay inside the selected folder."));
    return false;
  }
  if (entryExists(fullPath)) {
    emit errorOccurred(tr("\"%1\" already exists.").arg(trimmed));
    return false;
  }

  if (!QDir().mkpath(fullPath)) {
    emit errorOccurred(tr("Could not create the folder \"%1\".").arg(trimmed));
    return false;
  }

  if (createdPath) {
    *createdPath = fullPath;
  }
  emit modelUpdated();
  return true;
}

bool FileDirTreeModel::removeFileOrDirectory(const QString &path,
                                             DeleteMode mode) {
  const QFileInfo info(path);
  if (!info.exists() && !info.isSymLink()) {
    emit errorOccurred(tr("\"%1\" no longer exists.").arg(path));
    return false;
  }

  if (mode == DeleteMode::Trash) {
    if (QFile::moveToTrash(path)) {
      emit modelUpdated();
      return true;
    }
    emit errorOccurred(tr("Could not move \"%1\" to the trash. Use Delete "
                          "Permanently (Shift+Delete) to remove it for good.")
                           .arg(info.fileName()));
    return false;
  }

  if (removeRecursively(path)) {
    emit modelUpdated();
    return true;
  }

  emit errorOccurred(tr("Could not delete \"%1\".").arg(info.fileName()));
  return false;
}

bool FileDirTreeModel::renameFileOrDirectory(const QString &oldPath,
                                             const QString &newPath) {
  if (oldPath.isEmpty() || newPath.isEmpty() || !entryExists(oldPath) ||
      newPath.contains(QChar::Null)) {
    emit errorOccurred(tr("Choose an existing item and a valid new name."));
    return false;
  }
  if (QDir::cleanPath(oldPath) == QDir::cleanPath(newPath)) {
    return true;
  }

  if (entryExists(newPath)) {
    emit errorOccurred(
        tr("\"%1\" already exists.").arg(QFileInfo(newPath).fileName()));
    return false;
  }

  if (QFileInfo(oldPath).isDir() && isInside(oldPath, newPath)) {
    emit errorOccurred(tr("A folder cannot be moved inside itself."));
    return false;
  }

  if (!validateDestination(QFileInfo(newPath).absolutePath())) {
    return false;
  }

  if (QFile::rename(oldPath, newPath)) {
    emit pathMoved(QFileInfo(oldPath).absoluteFilePath(),
                   QFileInfo(newPath).absoluteFilePath());
    emit modelUpdated();
    return true;
  }

  emit errorOccurred(
      tr("Could not rename \"%1\".").arg(QFileInfo(oldPath).fileName()));
  return false;
}

bool FileDirTreeModel::duplicateEntry(const QString &path,
                                      QString *createdPath) {
  const QFileInfo info(path);
  if (!info.exists() && !info.isSymLink()) {
    emit errorOccurred(tr("\"%1\" no longer exists.").arg(path));
    return false;
  }

  const QString newPath = addUniqueSuffix(path);
  const bool ok = copyRecursively(path, newPath);
  if (!ok) {
    emit errorOccurred(tr("Could not duplicate \"%1\".").arg(info.fileName()));
    return false;
  }

  if (createdPath) {
    *createdPath = newPath;
  }
  emit modelUpdated();
  return true;
}

void FileDirTreeModel::writeClipboard(const QStringList &paths,
                                      ClipboardOperation operation) {
  QClipboard *clipboard = QGuiApplication::clipboard();
  if (!clipboard) {
    return;
  }

  QList<QUrl> urls;
  QStringList uriLines;
  urls.reserve(paths.size());
  for (const QString &path : paths) {
    const QUrl url = QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath());
    urls.append(url);
    uriLines << url.toString();
  }

  auto *mime = new QMimeData;
  mime->setUrls(urls);
  mime->setText(paths.join(QLatin1Char('\n')));

  const QString verb = operation == ClipboardOperation::Cut
                           ? QStringLiteral("cut")
                           : QStringLiteral("copy");
  mime->setData(
      QLatin1String(kGnomeCopiedFiles),
      (verb + QLatin1Char('\n') + uriLines.join(QLatin1Char('\n'))).toUtf8());
  if (operation == ClipboardOperation::Cut) {
    mime->setData(QLatin1String(kKdeCutSelection), QByteArrayLiteral("1"));
  }

  clipboard->setMimeData(mime);
}

QStringList FileDirTreeModel::clipboardPaths() const {
  const QClipboard *clipboard = QGuiApplication::clipboard();
  const QMimeData *mime = clipboard ? clipboard->mimeData() : nullptr;
  if (!mime || !mime->hasUrls()) {
    return {};
  }

  QStringList paths;
  const QList<QUrl> urls = mime->urls();
  for (const QUrl &url : urls) {
    const QString local = url.toLocalFile();
    if (!local.isEmpty()) {
      paths << local;
    }
  }
  return paths;
}

ClipboardOperation FileDirTreeModel::clipboardOperation() const {
  const QClipboard *clipboard = QGuiApplication::clipboard();
  const QMimeData *mime = clipboard ? clipboard->mimeData() : nullptr;
  if (!mime || !mime->hasUrls()) {
    return ClipboardOperation::None;
  }

  if (mime->hasFormat(QLatin1String(kKdeCutSelection)) &&
      mime->data(QLatin1String(kKdeCutSelection)).trimmed() == "1") {
    return ClipboardOperation::Cut;
  }
  if (mime->hasFormat(QLatin1String(kGnomeCopiedFiles))) {
    const QByteArray payload = mime->data(QLatin1String(kGnomeCopiedFiles));
    if (payload.startsWith("cut")) {
      return ClipboardOperation::Cut;
    }
  }

  return ClipboardOperation::Copy;
}

bool FileDirTreeModel::copyToClipboard(const QStringList &paths) {
  const QStringList sources = topLevelPaths(paths);
  if (sources.isEmpty()) {
    return false;
  }
  writeClipboard(sources, ClipboardOperation::Copy);
  return true;
}

bool FileDirTreeModel::cutToClipboard(const QStringList &paths) {
  const QStringList sources = topLevelPaths(paths);
  if (sources.isEmpty()) {
    return false;
  }
  writeClipboard(sources, ClipboardOperation::Cut);
  return true;
}

bool FileDirTreeModel::canPaste() const { return !clipboardPaths().isEmpty(); }

bool FileDirTreeModel::pasteFromClipboard(const QString &destPath,
                                          QStringList *createdPaths) {
  const QStringList originalClipboard = clipboardPaths();
  const QStringList sources = topLevelPaths(originalClipboard);
  if (sources.isEmpty()) {
    emit errorOccurred(tr("The clipboard holds no files to paste."));
    return false;
  }

  QString targetDir = destPath;
  if (QFileInfo(destPath).isFile()) {
    targetDir = QFileInfo(destPath).absolutePath();
  }
  if (!validateDestination(targetDir)) {
    return false;
  }

  const ClipboardOperation operation = clipboardOperation();
  bool anySucceeded = false;
  QStringList results;
  QStringList remaining;

  for (const QString &source : sources) {
    QString created;
    const bool ok = operation == ClipboardOperation::Cut
                        ? moveInto(source, targetDir, &created)
                        : copyInto(source, targetDir, &created);
    if (ok) {
      anySucceeded = true;
      results << created;
    } else {
      remaining << source;
    }
  }

  if (anySucceeded && operation == ClipboardOperation::Cut &&
      clipboardPaths() == originalClipboard &&
      clipboardOperation() == ClipboardOperation::Cut) {
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
      if (remaining.isEmpty()) {
        clipboard->clear();
      } else {
        writeClipboard(remaining, ClipboardOperation::Cut);
      }
    }
  }

  if (createdPaths) {
    *createdPaths = results;
  }
  if (anySucceeded) {
    emit modelUpdated();
  }
  return anySucceeded;
}

bool FileDirTreeModel::moveInto(const QString &srcPath, const QString &destDir,
                                QString *createdPath) {
  if (!validateDestination(destDir)) {
    return false;
  }
  const QFileInfo srcInfo(srcPath);
  if (!srcInfo.exists() && !srcInfo.isSymLink()) {
    emit errorOccurred(tr("\"%1\" no longer exists.").arg(srcPath));
    return false;
  }

  if (srcInfo.isDir() && isInside(srcPath, destDir)) {
    emit errorOccurred(
        tr("\"%1\" cannot be moved inside itself.").arg(srcInfo.fileName()));
    return false;
  }

  if (canonicalPath(srcInfo.absolutePath()) == canonicalPath(destDir)) {

    if (createdPath) {
      *createdPath = srcInfo.absoluteFilePath();
    }
    return true;
  }

  const QString target = addUniqueSuffix(joinPath(destDir, srcInfo.fileName()));

  if (QFile::rename(srcPath, target)) {
    if (createdPath) {
      *createdPath = target;
    }
    emit pathMoved(srcInfo.absoluteFilePath(),
                   QFileInfo(target).absoluteFilePath());
    emit modelUpdated();
    return true;
  }

  if (copyRecursively(srcPath, target)) {
    if (removeRecursively(srcPath)) {
      if (createdPath) {
        *createdPath = target;
      }
      emit pathMoved(srcInfo.absoluteFilePath(),
                     QFileInfo(target).absoluteFilePath());
      emit modelUpdated();
      return true;
    }
    emit errorOccurred(tr("Copied \"%1\" but could not remove the original.")
                           .arg(srcInfo.fileName()));
    return false;
  }

  emit errorOccurred(tr("Could not move \"%1\".").arg(srcInfo.fileName()));
  return false;
}

bool FileDirTreeModel::copyInto(const QString &srcPath, const QString &destDir,
                                QString *createdPath) {
  if (!validateDestination(destDir)) {
    return false;
  }
  const QFileInfo srcInfo(srcPath);
  if (!srcInfo.exists() && !srcInfo.isSymLink()) {
    emit errorOccurred(tr("\"%1\" no longer exists.").arg(srcPath));
    return false;
  }

  if (srcInfo.isDir() && isInside(srcPath, destDir)) {
    emit errorOccurred(
        tr("\"%1\" cannot be copied inside itself.").arg(srcInfo.fileName()));
    return false;
  }

  const QString target = addUniqueSuffix(joinPath(destDir, srcInfo.fileName()));

  if (copyRecursively(srcPath, target)) {
    if (createdPath) {
      *createdPath = target;
    }
    emit modelUpdated();
    return true;
  }

  emit errorOccurred(tr("Could not copy \"%1\".").arg(srcInfo.fileName()));
  return false;
}

QString FileDirTreeModel::getAbsolutePath(const QString &path) {
  return QFileInfo(path).absoluteFilePath();
}

QString FileDirTreeModel::addUniqueSuffix(const QString &fileName) {
  if (!entryExists(fileName)) {
    return fileName;
  }

  const QFileInfo fileInfo(fileName);
  // Dotted folder names and dotfiles are names, not multi-part extensions.
  QString secondPart = fileInfo.isDir() || fileInfo.fileName().startsWith('.')
                           ? QString()
                           : fileInfo.completeSuffix();
  QString firstPart;
  if (!secondPart.isEmpty()) {
    secondPart = "." + secondPart;
    firstPart = fileName.left(fileName.size() - secondPart.size());
  } else {
    firstPart = fileName;
  }

  for (int index = 1;; index++) {
    const QString candidate =
        QString("%1 (%2)%3").arg(firstPart).arg(index).arg(secondPart);
    if (!entryExists(candidate)) {
      return candidate;
    }
  }
}

bool FileDirTreeModel::copyRecursively(const QString &srcPath,
                                       const QString &destPath) {
  const QFileInfo srcInfo(srcPath);

  if (srcInfo.isSymLink()) {
    return QFile::link(srcInfo.symLinkTarget(), destPath);
  }

  if (srcInfo.isFile()) {
    return QFile::copy(srcPath, destPath);
  }

  if (!srcInfo.isDir()) {
    return false;
  }

  if (!QDir().mkdir(destPath)) {
    return false;
  }

  const QDir srcDir(srcPath);
  const QStringList entries = srcDir.entryList(entryFilters());
  for (const QString &entry : entries) {
    if (!copyRecursively(joinPath(srcPath, entry), joinPath(destPath, entry))) {
      removeRecursively(destPath);
      return false;
    }
  }

  return true;
}

bool FileDirTreeModel::removeRecursively(const QString &path) {
  const QFileInfo info(path);

  if (info.isSymLink()) {
    return QFile::remove(path);
  }

  if (info.isFile()) {
    return QFile::remove(path);
  }

  if (!info.isDir()) {
    return false;
  }

  const QDir dir(path);
  const QStringList entries = dir.entryList(entryFilters());
  for (const QString &entry : entries) {
    if (!removeRecursively(joinPath(path, entry))) {
      return false;
    }
  }

  return QDir().rmdir(path);
}
