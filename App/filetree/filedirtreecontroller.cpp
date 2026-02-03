#include "filedirtreecontroller.h"
#include <QDebug>

FileDirTreeController::FileDirTreeController(FileDirTreeModel *model,
                                             QWidget *parent)
    : QObject(parent), model(model), parentWidget(parent) {
  connect(model, &FileDirTreeModel::errorOccurred, this,
          &FileDirTreeController::showError);
}

void FileDirTreeController::handleNewFile(const QString &dirPath) {
  bool ok;
  QString fileName = QInputDialog::getText(
      parentWidget, "New File", "Enter file name:", QLineEdit::Normal, "", &ok);

  if (ok && !fileName.isEmpty()) {
    if (model->createNewFile(dirPath, fileName)) {
      emit actionCompleted();
    }
  }
}

void FileDirTreeController::handleNewDirectory(const QString &parentPath) {
  bool ok;
  QString dirName = QInputDialog::getText(
      parentWidget, "New Directory", "Enter directory name:", QLineEdit::Normal,
      "", &ok);

  if (ok && !dirName.isEmpty()) {
    if (model->createNewDirectory(parentPath, dirName)) {
      emit actionCompleted();
    }
  }
}

void FileDirTreeController::handleRemove(const QString &path) {
  if (confirmAction("Are you sure you want to remove this item?")) {
    if (model->removeFileOrDirectory(path)) {
      emit fileRemoved(path);
      emit actionCompleted();
    }
  }
}

void FileDirTreeController::handleRename(const QString &oldPath) {
  QFileInfo fileInfo(oldPath);
  bool ok;
  QString newName = QInputDialog::getText(parentWidget, "Rename",
                                          "Enter new name:", QLineEdit::Normal,
                                          fileInfo.fileName(), &ok);

  if (ok && !newName.isEmpty()) {
    QString newPath = fileInfo.absolutePath() + QDir::separator() + newName;
    if (model->renameFileOrDirectory(oldPath, newPath)) {
      emit actionCompleted();
    }
  }
}

void FileDirTreeController::handleDuplicate(const QString &path) {
  if (model->duplicateFile(path)) {
    emit actionCompleted();
  }
}

void FileDirTreeController::handleCopy(const QString &path) {
  model->copyToClipboard(path);
  // No modal dialog - just silently copy to clipboard
}

void FileDirTreeController::handleCut(const QString &path) {
  model->cutToClipboard(path);
  // No modal dialog - just silently cut to clipboard
}

void FileDirTreeController::handlePaste(const QString &destPath) {
  if (model->pasteFromClipboard(destPath)) {
    emit actionCompleted();
  }
}

void FileDirTreeController::handleCopyAbsolutePath(const QString &path) {
  QString absolutePath = model->getAbsolutePath(path);
  QClipboard *clipboard = QGuiApplication::clipboard();
  clipboard->setText(absolutePath);
  showInfo("Absolute path copied to clipboard");
}

void FileDirTreeController::showError(const QString &message) {
  QMessageBox::warning(parentWidget, "Error", message);
}

void FileDirTreeController::showInfo(const QString &message) {
  QMessageBox::information(parentWidget, "Information", message);
}

bool FileDirTreeController::confirmAction(const QString &message) {
  return QMessageBox::question(parentWidget, "Confirm", message,
                               QMessageBox::Yes | QMessageBox::No) ==
         QMessageBox::Yes;
}
