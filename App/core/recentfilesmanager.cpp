#include "recentfilesmanager.h"
#include <QFileInfo>

RecentFilesManager::RecentFilesManager(QObject *parent)
    : QObject(parent), m_maxFiles(DEFAULT_MAX_FILES) {
  load();
}

RecentFilesManager::~RecentFilesManager() { save(); }

void RecentFilesManager::addFile(const QString &filePath) {
  if (filePath.isEmpty()) {
    return;
  }

  // Get absolute path for consistency
  QFileInfo fileInfo(filePath);
  QString absolutePath = fileInfo.absoluteFilePath();

  // Remove if already exists to move to front
  m_recentFiles.removeAll(absolutePath);

  // Add to front of list
  m_recentFiles.prepend(absolutePath);

  // Trim to max size
  while (m_recentFiles.size() > m_maxFiles) {
    m_recentFiles.removeLast();
  }

  save();
  emit recentFilesChanged();
}

void RecentFilesManager::removeFile(const QString &filePath) {
  QFileInfo fileInfo(filePath);
  QString absolutePath = fileInfo.absoluteFilePath();

  if (m_recentFiles.removeAll(absolutePath) > 0) {
    save();
    emit recentFilesChanged();
  }
}

QStringList RecentFilesManager::recentFiles() const {
  // Filter out files that no longer exist
  QStringList existingFiles;
  for (const QString &file : m_recentFiles) {
    if (QFileInfo::exists(file)) {
      existingFiles.append(file);
    }
  }
  return existingFiles;
}

void RecentFilesManager::clearAll() {
  m_recentFiles.clear();
  save();
  emit recentFilesChanged();
}

int RecentFilesManager::maxFiles() const { return m_maxFiles; }

void RecentFilesManager::setMaxFiles(int max) {
  m_maxFiles = qMax(1, max);

  // Trim if necessary
  while (m_recentFiles.size() > m_maxFiles) {
    m_recentFiles.removeLast();
  }

  save();
}

bool RecentFilesManager::contains(const QString &filePath) const {
  QFileInfo fileInfo(filePath);
  QString absolutePath = fileInfo.absoluteFilePath();
  return m_recentFiles.contains(absolutePath);
}

void RecentFilesManager::load() {
  QSettings settings("Lightpad", "Lightpad");
  m_recentFiles = settings.value("recentFiles/files").toStringList();
  m_maxFiles =
      settings.value("recentFiles/maxFiles", DEFAULT_MAX_FILES).toInt();
}

void RecentFilesManager::save() {
  QSettings settings("Lightpad", "Lightpad");
  settings.setValue("recentFiles/files", m_recentFiles);
  settings.setValue("recentFiles/maxFiles", m_maxFiles);
}
