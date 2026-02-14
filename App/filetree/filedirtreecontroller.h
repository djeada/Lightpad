#ifndef FILEDIRTREECONTROLLER_H
#define FILEDIRTREECONTROLLER_H

#include "filedirtreemodel.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QObject>
#include <QWidget>

class FileDirTreeController : public QObject {
  Q_OBJECT

public:
  explicit FileDirTreeController(FileDirTreeModel *model,
                                 QWidget *parent = nullptr);
  ~FileDirTreeController() = default;

  void handleNewFile(const QString &dirPath);
  void handleNewDirectory(const QString &parentPath);
  void handleRemove(const QString &path);
  void handleRename(const QString &oldPath);
  void handleDuplicate(const QString &path);
  void handleCopy(const QString &path);
  void handleCut(const QString &path);
  void handlePaste(const QString &destPath);
  void handleCopyAbsolutePath(const QString &path);

signals:
  void actionCompleted();
  void fileRemoved(const QString &path);

private:
  FileDirTreeModel *model;
  QWidget *parentWidget;

  void showError(const QString &message);
  void showInfo(const QString &message);
  bool confirmAction(const QString &message);
};

#endif
