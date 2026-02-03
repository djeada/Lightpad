#ifndef RECENTFILESMANAGER_H
#define RECENTFILESMANAGER_H

#include <QObject>
#include <QSettings>
#include <QStringList>

/**
 * @brief Manager for tracking recently opened files
 *
 * Maintains a list of recently opened files that persists across sessions.
 */
class RecentFilesManager : public QObject {
  Q_OBJECT

public:
  explicit RecentFilesManager(QObject *parent = nullptr);
  ~RecentFilesManager();

  /**
   * @brief Add a file to the recent files list
   */
  void addFile(const QString &filePath);

  /**
   * @brief Remove a file from the recent files list
   */
  void removeFile(const QString &filePath);

  /**
   * @brief Get the list of recent files
   */
  QStringList recentFiles() const;

  /**
   * @brief Clear all recent files
   */
  void clearAll();

  /**
   * @brief Get maximum number of files to track
   */
  int maxFiles() const;

  /**
   * @brief Set maximum number of files to track
   */
  void setMaxFiles(int max);

  /**
   * @brief Check if a file exists in the recent files list
   */
  bool contains(const QString &filePath) const;

signals:
  /**
   * @brief Emitted when the recent files list changes
   */
  void recentFilesChanged();

private:
  void load();
  void save();

  QStringList m_recentFiles;
  int m_maxFiles;
  static const int DEFAULT_MAX_FILES = 20;
};

#endif // RECENTFILESMANAGER_H
