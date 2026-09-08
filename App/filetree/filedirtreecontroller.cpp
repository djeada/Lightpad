#include "filedirtreecontroller.h"
#include "../ui/dialogs/themedmessagebox.h"

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLineEdit>

FileDirTreeController::FileDirTreeController(FileDirTreeModel *model,
                                             QWidget *parent)
    : QObject(parent), model(model), parentWidget(parent) {
  connect(model, &FileDirTreeModel::errorOccurred, this,
          &FileDirTreeController::showError);
}

QString FileDirTreeController::describe(const QStringList &paths) {
  if (paths.isEmpty()) {
    return QString();
  }
  if (paths.size() == 1) {
    return QFileInfo(paths.first()).fileName();
  }
  return tr("%n items", nullptr, paths.size());
}

void FileDirTreeController::handleNewFile(const QString &dirPath) {
  bool ok = false;
  const QString folderName = QFileInfo(dirPath).fileName();
  const QString fileName = QInputDialog::getText(
      parentWidget, tr("New File"),
      tr("New file in %1:").arg(folderName.isEmpty() ? dirPath : folderName),
      QLineEdit::Normal, QString(), &ok);

  if (!ok || fileName.trimmed().isEmpty()) {
    return;
  }

  QString createdPath;
  if (model->createNewFile(dirPath, fileName, &createdPath)) {
    emit actionCompleted();
    emit pathCreated(createdPath, false);
  }
}

void FileDirTreeController::handleNewDirectory(const QString &parentPath) {
  bool ok = false;
  const QString folderName = QFileInfo(parentPath).fileName();
  const QString dirName = QInputDialog::getText(
      parentWidget, tr("New Folder"),
      tr("New folder in %1:")
          .arg(folderName.isEmpty() ? parentPath : folderName),
      QLineEdit::Normal, QString(), &ok);

  if (!ok || dirName.trimmed().isEmpty()) {
    return;
  }

  QString createdPath;
  if (model->createNewDirectory(parentPath, dirName, &createdPath)) {
    emit actionCompleted();
    emit pathCreated(createdPath, true);
  }
}

void FileDirTreeController::handleRemove(const QStringList &paths,
                                         DeleteMode mode) {
  if (paths.isEmpty() || !confirmDeletion(paths, mode)) {
    return;
  }

  bool anyRemoved = false;
  for (const QString &path : paths) {
    if (model->removeFileOrDirectory(path, mode)) {
      anyRemoved = true;
      emit fileRemoved(path);
    }
  }

  if (anyRemoved) {
    emit actionCompleted();
    emit statusMessage(mode == DeleteMode::Trash
                           ? tr("Moved %1 to the trash").arg(describe(paths))
                           : tr("Deleted %1").arg(describe(paths)));
  }
}

void FileDirTreeController::handleRename(const QString &oldPath) {
  const QFileInfo fileInfo(oldPath);
  bool ok = false;
  const QString newName =
      QInputDialog::getText(parentWidget, tr("Rename"),
                            tr("Rename \"%1\" to:").arg(fileInfo.fileName()),
                            QLineEdit::Normal, fileInfo.fileName(), &ok);

  if (!ok || newName.trimmed().isEmpty() ||
      newName.trimmed() == fileInfo.fileName()) {
    return;
  }

  const QString newPath =
      QDir(fileInfo.absolutePath()).absoluteFilePath(newName.trimmed());
  if (model->renameFileOrDirectory(oldPath, newPath)) {
    emit actionCompleted();
    emit pathCreated(newPath, QFileInfo(newPath).isDir());
  }
}

void FileDirTreeController::handleDuplicate(const QString &path) {
  QString createdPath;
  if (model->duplicateEntry(path, &createdPath)) {
    emit actionCompleted();
    emit pathCreated(createdPath, QFileInfo(createdPath).isDir());
  }
}

void FileDirTreeController::handleCopy(const QStringList &paths) {
  if (model->copyToClipboard(paths)) {
    emit statusMessage(tr("Copied %1").arg(describe(paths)));
  }
}

void FileDirTreeController::handleCut(const QStringList &paths) {
  if (model->cutToClipboard(paths)) {
    emit statusMessage(tr("Cut %1").arg(describe(paths)));
  }
}

void FileDirTreeController::handlePaste(const QString &destPath) {
  QStringList createdPaths;
  if (!model->pasteFromClipboard(destPath, &createdPaths)) {
    return;
  }

  emit actionCompleted();
  emit statusMessage(tr("Pasted %1").arg(describe(createdPaths)));
  if (!createdPaths.isEmpty()) {
    const QString &first = createdPaths.first();
    emit pathCreated(first, QFileInfo(first).isDir());
  }
}

void FileDirTreeController::handleCopyAbsolutePath(const QStringList &paths) {
  if (paths.isEmpty()) {
    return;
  }

  QStringList absolutePaths;
  absolutePaths.reserve(paths.size());
  for (const QString &path : paths) {
    absolutePaths << model->getAbsolutePath(path);
  }

  QGuiApplication::clipboard()->setText(absolutePaths.join(QLatin1Char('\n')));

  emit statusMessage(tr("Copied path: %1").arg(absolutePaths.join(", ")));
}

void FileDirTreeController::handleCopyRelativePath(const QStringList &paths,
                                                   const QString &basePath) {
  if (paths.isEmpty()) {
    return;
  }

  const QDir base(basePath);
  QStringList relativePaths;
  relativePaths.reserve(paths.size());
  for (const QString &path : paths) {
    const QString absolute = model->getAbsolutePath(path);
    relativePaths << (basePath.isEmpty() ? absolute
                                         : base.relativeFilePath(absolute));
  }

  QGuiApplication::clipboard()->setText(relativePaths.join(QLatin1Char('\n')));
  emit statusMessage(tr("Copied path: %1").arg(relativePaths.join(", ")));
}

void FileDirTreeController::showError(const QString &message) {
  ThemedMessageBox::warning(parentWidget, tr("File Explorer"), message);
}

bool FileDirTreeController::confirmDeletion(const QStringList &paths,
                                            DeleteMode mode) {

  const QString subject = describe(paths);
  const QString question = mode == DeleteMode::Trash
                               ? tr("Move \"%1\" to the trash?").arg(subject)
                               : tr("Permanently delete \"%1\"?").arg(subject);
  const QString detail = mode == DeleteMode::Trash
                             ? tr("You can restore it from your system trash.")
                             : tr("This cannot be undone.");

  QString body = question + QLatin1String("\n\n") + detail;
  if (paths.size() > 1) {
    QStringList names;
    for (const QString &path : paths) {
      names << QFileInfo(path).fileName();
    }
    body += QLatin1String("\n\n") + names.join(QLatin1String("\n"));
  }

  return ThemedMessageBox::question(parentWidget, tr("Delete"), body,
                                    ThemedMessageBox::Yes |
                                        ThemedMessageBox::No) ==
         ThemedMessageBox::Yes;
}
