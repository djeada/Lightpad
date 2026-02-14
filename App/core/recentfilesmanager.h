#ifndef RECENTFILESMANAGER_H
#define RECENTFILESMANAGER_H

#include <QObject>
#include <QSettings>
#include <QStringList>

class RecentFilesManager : public QObject {
  Q_OBJECT

public:
  explicit RecentFilesManager(QObject *parent = nullptr);
  ~RecentFilesManager();

  void addFile(const QString &filePath);

  void removeFile(const QString &filePath);

  QStringList recentFiles() const;

  void clearAll();

  int maxFiles() const;

  void setMaxFiles(int max);

  bool contains(const QString &filePath) const;

signals:

  void recentFilesChanged();

private:
  void load();
  void save();

  QStringList m_recentFiles;
  int m_maxFiles;
  static const int DEFAULT_MAX_FILES = 20;
};

#endif
