#ifndef FILEDIRTREECONTROLLER_H
#define FILEDIRTREECONTROLLER_H

#include "filedirtreemodel.h"
#include <QObject>
#include <QStringList>
#include <QWidget>
#include <functional>

class FileDirTreeController : public QObject {
  Q_OBJECT

public:
  explicit FileDirTreeController(FileDirTreeModel *model,
                                 QWidget *parent = nullptr);
  ~FileDirTreeController() = default;

  void handleNewFile(const QString &dirPath);
  void handleNewDirectory(const QString &parentPath);
  void handleRemove(const QStringList &paths,
                    DeleteMode mode = DeleteMode::Trash);
  void handleRename(const QString &oldPath);
  void handleDuplicate(const QString &path);
  void handleDuplicate(const QStringList &paths);
  QStringList handleTransfer(const QStringList &paths,
                             const QString &destination, bool copy);
  void handleTransferTo(const QStringList &paths, const QString &startDirectory,
                        bool copy);
  void setRemovalGuard(std::function<bool(const QStringList &)> guard);
  void handleCopy(const QStringList &paths);
  void handleCut(const QStringList &paths);
  void handlePaste(const QString &destPath);
  void handleCopyAbsolutePath(const QStringList &paths);
  void handleCopyRelativePath(const QStringList &paths,
                              const QString &basePath);

signals:
  void actionCompleted();
  void fileRemoved(const QString &path);

  void pathCreated(const QString &path, bool isDirectory);
  void statusMessage(const QString &message);

private:
  FileDirTreeModel *model;
  QWidget *parentWidget;
  std::function<bool(const QStringList &)> m_removalGuard;
  bool m_batching = false;
  QStringList m_errors;

  void showError(const QString &message);
  void finishBatch();
  bool confirmDeletion(const QStringList &paths, DeleteMode mode);
  static QString describe(const QStringList &paths);
};

#endif
