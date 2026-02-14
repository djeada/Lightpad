#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QString>

class FileManager : public QObject {
  Q_OBJECT

public:
  struct FileResult {
    bool success;
    QString content;
    QString errorMessage;
  };

  static FileManager &instance();

  FileResult readFile(const QString &filePath);

  FileResult writeFile(const QString &filePath, const QString &content);

  bool fileExists(const QString &filePath);

  QString getFileExtension(const QString &filePath);

  QString getFileName(const QString &filePath);

  QString getDirectory(const QString &filePath);

signals:

  void fileOpened(const QString &filePath);

  void fileSaved(const QString &filePath);

  void fileError(const QString &filePath, const QString &error);

private:
  FileManager();
  ~FileManager() = default;
  FileManager(const FileManager &) = delete;
  FileManager &operator=(const FileManager &) = delete;
};

#endif
