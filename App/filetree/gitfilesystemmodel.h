#ifndef GITFILESYSTEMMODEL_H
#define GITFILESYSTEMMODEL_H

#include "../git/gitintegration.h"
#include <QHash>
#include <QFileSystemModel>
#include <QIcon>
#include <QSet>
#include <QTimer>

constexpr int GIT_STATUS_REFRESH_DEBOUNCE_MS = 500;

enum GitModelRole {
  GitStatusBadgeRole = Qt::UserRole + 100,
  GitStatusBadgeColorRole
};

class GitFileSystemModel : public QFileSystemModel {
  Q_OBJECT

public:
  explicit GitFileSystemModel(QObject *parent = nullptr);
  ~GitFileSystemModel();
  void setRootHeaderLabel(const QString &label);

  void setGitIntegration(GitIntegration *git);

  GitIntegration *gitIntegration() const;

  void setGitStatusEnabled(bool enabled);

  bool isGitStatusEnabled() const;

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  void refreshGitStatus();

  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

  QString statusBadge(const QString &filePath) const;
  QColor statusBadgeColor(const QString &filePath) const;
  bool isDirtyDirectory(const QString &dirPath) const;

private slots:
  void onGitStatusChanged();

private:
  GitIntegration *m_gitIntegration;
  bool m_gitStatusEnabled;
  QTimer *m_refreshTimer;
  mutable QMap<QString, GitFileInfo> m_statusCache;
  mutable QSet<QString> m_dirtyDirectories;
  QString m_rootHeaderLabel;
  mutable QHash<QString, QIcon> m_fileIconCache;
  mutable QIcon m_folderIcon;
  mutable QIcon m_fallbackFileIcon;

  QIcon getStatusIcon(const QString &filePath) const;
  QIcon getBaseIcon(const QModelIndex &index, const QString &filePath) const;
  QIcon getFileTypeIcon(const QString &filePath) const;

  QColor getStatusColor(const QString &filePath) const;
  static QColor colorForFileExtension(const QString &extension);

  void updateStatusCache();
  void rebuildDirtyDirectories();

  static void initializeIcons();

  static QIcon s_modifiedIcon;
  static QIcon s_stagedIcon;
  static QIcon s_untrackedIcon;
  static QIcon s_addedIcon;
  static QIcon s_deletedIcon;
  static QIcon s_conflictIcon;
  static bool s_iconsInitialized;
};

#endif
