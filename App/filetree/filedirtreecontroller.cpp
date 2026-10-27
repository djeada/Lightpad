#include "filedirtreecontroller.h"
#include "../ui/dialogs/themedmessagebox.h"

#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <utility>

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
  const QStringList sources = FileDirTreeModel::topLevelPaths(paths);
  if (sources.isEmpty() || !confirmDeletion(sources, mode) ||
      (m_removalGuard && !m_removalGuard(sources))) {
    return;
  }

  m_batching = true;
  QStringList removed;
  for (const QString &path : sources) {
    if (model->removeFileOrDirectory(path, mode)) {
      removed.append(path);
      emit fileRemoved(path);
    }
  }

  finishBatch();
  if (!removed.isEmpty()) {
    emit actionCompleted();
    emit statusMessage(mode == DeleteMode::Trash
                           ? tr("Moved %1 to the trash").arg(describe(removed))
                           : tr("Deleted %1").arg(describe(removed)));
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

  const QString trimmedName = newName.trimmed();
  if (trimmedName.contains('/') || trimmedName.contains('\\') ||
      trimmedName == QLatin1String(".") || trimmedName == QLatin1String("..")) {
    showError(tr("\"%1\" is not a valid name.").arg(trimmedName));
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

void FileDirTreeController::handleDuplicate(const QStringList &paths) {
  m_batching = true;
  QStringList created;
  for (const QString &path : FileDirTreeModel::topLevelPaths(paths)) {
    QString duplicate;
    if (model->duplicateEntry(path, &duplicate)) {
      created.append(duplicate);
    }
  }
  finishBatch();
  if (!created.isEmpty()) {
    emit actionCompleted();
    emit pathCreated(created.first(), QFileInfo(created.first()).isDir());
    emit statusMessage(tr("Duplicated %1").arg(describe(created)));
  }
}

QStringList FileDirTreeController::handleTransfer(const QStringList &paths,
                                                  const QString &destination,
                                                  bool copy) {
  const QStringList sources = FileDirTreeModel::topLevelPaths(paths);
  if (sources.isEmpty() || (!copy && !confirmMove(sources, destination))) {
    return {};
  }

  m_batching = true;
  QStringList created;
  for (const QString &path : sources) {
    QString target;
    if (copy ? model->copyInto(path, destination, &target)
             : model->moveInto(path, destination, &target)) {
      created.append(target);
    }
  }
  finishBatch();
  if (!created.isEmpty()) {
    emit actionCompleted();
    emit statusMessage(copy ? tr("Copied %1").arg(describe(created))
                            : tr("Moved %1").arg(describe(created)));
  }
  return created;
}

void FileDirTreeController::handleTransferTo(const QStringList &paths,
                                             const QString &startDirectory,
                                             bool copy) {
  const QString destination = QFileDialog::getExistingDirectory(
      parentWidget, copy ? tr("Copy to Folder") : tr("Move to Folder"),
      startDirectory);
  if (!destination.isEmpty()) {
    const QStringList created = handleTransfer(paths, destination, copy);
    if (!created.isEmpty()) {
      emit pathCreated(created.first(), QFileInfo(created.first()).isDir());
    }
  }
}

void FileDirTreeController::setRemovalGuard(
    std::function<bool(const QStringList &)> guard) {
  m_removalGuard = std::move(guard);
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
  m_batching = true;
  QStringList createdPaths;
  const bool pasted = model->pasteFromClipboard(destPath, &createdPaths);
  finishBatch();
  if (!pasted) {
    return;
  }

  emit actionCompleted();
  emit statusMessage(tr("Pasted %1").arg(describe(createdPaths)));
  if (!createdPaths.isEmpty()) {
    const QString &first = createdPaths.first();
    emit pathCreated(first, QFileInfo(first).isDir());
  }
}

void FileDirTreeController::handleCopyFileName(const QStringList &paths) {
  if (paths.isEmpty()) {
    return;
  }

  QStringList names;
  for (const QString &path : paths) {
    names.append(QFileInfo(path).fileName());
  }
  QGuiApplication::clipboard()->setText(names.join(QLatin1Char('\n')));
  emit statusMessage(tr("Copied name: %1").arg(names.join(", ")));
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
  if (m_batching) {
    m_errors.append(message);
    return;
  }
  ThemedMessageBox::warning(parentWidget, tr("File Explorer"), message);
}

void FileDirTreeController::finishBatch() {
  m_batching = false;
  if (!m_errors.isEmpty()) {
    const QStringList errors = std::exchange(m_errors, {});
    showError(errors.join(QStringLiteral("\n\n")));
  }
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

bool FileDirTreeController::confirmMove(const QStringList &paths,
                                       const QString &destination) {
  ThemedMessageBox box(parentWidget);
  box.setWindowTitle(tr("Move Files"));
  box.setIcon(ThemedMessageBox::Question);
  box.setText(tr("Move %1 to \"%2\"?")
                  .arg(describe(paths), QDir::toNativeSeparators(destination)));
  box.setInformativeText(tr("The selected files and folders will leave their "
                            "current locations."));
  box.setDetailedText(paths.join(QLatin1Char('\n')));
  box.setStandardButtons(ThemedMessageBox::Yes | ThemedMessageBox::No);
  box.setButtonText(ThemedMessageBox::Yes, tr("Move"));
  box.setButtonText(ThemedMessageBox::No, tr("Cancel"));
  box.setDefaultButton(ThemedMessageBox::No);
  return box.exec() == ThemedMessageBox::Yes;
}
