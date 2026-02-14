#ifndef FILEDIRTREEMODEL_H
#define FILEDIRTREEMODEL_H

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

enum class ClipboardOperation { None, Copy, Cut };

class FileDirTreeModel : public QObject {
  Q_OBJECT

public:
  explicit FileDirTreeModel(QObject *parent = nullptr);
  ~FileDirTreeModel() = default;

  bool createNewFile(const QString &dirPath, const QString &fileName);
  bool createNewDirectory(const QString &parentPath, const QString &dirName);
  bool removeFileOrDirectory(const QString &path);
  bool renameFileOrDirectory(const QString &oldPath, const QString &newPath);
  bool duplicateFile(const QString &filePath);
  bool copyToClipboard(const QString &path);
  bool cutToClipboard(const QString &path);
  bool pasteFromClipboard(const QString &destPath);
  QString getAbsolutePath(const QString &path);
  QString addUniqueSuffix(const QString &fileName);

signals:
  void modelUpdated();
  void errorOccurred(const QString &error);

private:
  QString clipboardPath;
  ClipboardOperation clipboardOperation;

  bool copyRecursively(const QString &srcPath, const QString &destPath);
  bool removeRecursively(const QString &path);
};

#endif
