#ifndef FILEDIRTREEMODEL_H
#define FILEDIRTREEMODEL_H

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QStringList>

enum class ClipboardOperation { None, Copy, Cut };

enum class DeleteMode { Trash, Permanent };

class FileDirTreeModel : public QObject {
  Q_OBJECT

public:
  explicit FileDirTreeModel(QObject *parent = nullptr);
  ~FileDirTreeModel() = default;

  bool createNewFile(const QString &dirPath, const QString &fileName,
                     QString *createdPath = nullptr);
  bool createNewDirectory(const QString &parentPath, const QString &dirName,
                          QString *createdPath = nullptr);

  bool removeFileOrDirectory(const QString &path,
                             DeleteMode mode = DeleteMode::Trash);
  bool renameFileOrDirectory(const QString &oldPath, const QString &newPath);

  bool duplicateEntry(const QString &path, QString *createdPath = nullptr);

  bool copyToClipboard(const QStringList &paths);
  bool cutToClipboard(const QStringList &paths);
  bool pasteFromClipboard(const QString &destPath,
                          QStringList *createdPaths = nullptr);

  bool canPaste() const;

  bool moveInto(const QString &srcPath, const QString &destDir,
                QString *createdPath = nullptr);
  bool copyInto(const QString &srcPath, const QString &destDir,
                QString *createdPath = nullptr);

  QString getAbsolutePath(const QString &path);
  QString addUniqueSuffix(const QString &fileName);

  static bool isInside(const QString &parentDir, const QString &path);

signals:
  void modelUpdated();
  void errorOccurred(const QString &error);
  void clipboardChanged();

private:
  QStringList clipboardPaths() const;
  ClipboardOperation clipboardOperation() const;
  void writeClipboard(const QStringList &paths, ClipboardOperation operation);

  bool copyRecursively(const QString &srcPath, const QString &destPath);
  bool removeRecursively(const QString &path);

  static QDir::Filters entryFilters();
};

#endif
