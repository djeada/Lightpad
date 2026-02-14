#include "gitfilesystemmodel.h"
#include "../core/logging/logger.h"
#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QStyle>

QIcon GitFileSystemModel::s_modifiedIcon;
QIcon GitFileSystemModel::s_stagedIcon;
QIcon GitFileSystemModel::s_untrackedIcon;
QIcon GitFileSystemModel::s_addedIcon;
QIcon GitFileSystemModel::s_deletedIcon;
QIcon GitFileSystemModel::s_conflictIcon;
bool GitFileSystemModel::s_iconsInitialized = false;

void GitFileSystemModel::initializeIcons() {
  if (s_iconsInitialized) {
    return;
  }

  auto createCircleIcon = [](const QColor &color) -> QIcon {
    QPixmap pixmap(12, 12);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(color.darker(120));
    painter.drawEllipse(1, 1, 10, 10);
    return QIcon(pixmap);
  };

  s_modifiedIcon = createCircleIcon(QColor(255, 165, 0));
  s_stagedIcon = createCircleIcon(QColor(0, 200, 0));
  s_untrackedIcon = createCircleIcon(QColor(128, 128, 128));
  s_addedIcon = createCircleIcon(QColor(0, 255, 0));
  s_deletedIcon = createCircleIcon(QColor(255, 0, 0));
  s_conflictIcon = createCircleIcon(QColor(255, 0, 255));

  s_iconsInitialized = true;
}

GitFileSystemModel::GitFileSystemModel(QObject *parent)
    : QFileSystemModel(parent), m_gitIntegration(nullptr),
      m_gitStatusEnabled(true), m_refreshTimer(new QTimer(this)) {
  initializeIcons();
  setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden |
            QDir::System);

  m_refreshTimer->setSingleShot(true);
  m_refreshTimer->setInterval(GIT_STATUS_REFRESH_DEBOUNCE_MS);
  connect(m_refreshTimer, &QTimer::timeout, this,
          &GitFileSystemModel::updateStatusCache);
}

GitFileSystemModel::~GitFileSystemModel() {}

void GitFileSystemModel::setRootHeaderLabel(const QString &label) {
  if (m_rootHeaderLabel == label) {
    return;
  }

  m_rootHeaderLabel = label;
  emit headerDataChanged(Qt::Horizontal, 0, 0);
}

void GitFileSystemModel::setGitIntegration(GitIntegration *git) {
  if (m_gitIntegration) {
    disconnect(m_gitIntegration, nullptr, this, nullptr);
  }

  m_gitIntegration = git;

  if (m_gitIntegration) {
    connect(m_gitIntegration, &GitIntegration::statusChanged, this,
            &GitFileSystemModel::onGitStatusChanged);
    updateStatusCache();
  }
}

GitIntegration *GitFileSystemModel::gitIntegration() const {
  return m_gitIntegration;
}

void GitFileSystemModel::setGitStatusEnabled(bool enabled) {
  if (m_gitStatusEnabled != enabled) {
    m_gitStatusEnabled = enabled;
    if (enabled) {
      updateStatusCache();
    }

    emit layoutChanged();
  }
}

bool GitFileSystemModel::isGitStatusEnabled() const {
  return m_gitStatusEnabled;
}

QVariant GitFileSystemModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return QFileSystemModel::data(index, role);
  }

  if (index.column() == 0 && m_gitStatusEnabled && m_gitIntegration &&
      m_gitIntegration->isValidRepository()) {

    if (role == Qt::DecorationRole) {

      QVariant baseIcon = QFileSystemModel::data(index, role);
      QString filePath = this->filePath(index);

      QIcon statusIcon = getStatusIcon(filePath);
      if (!statusIcon.isNull()) {

        QIcon fileIcon = baseIcon.value<QIcon>();
        if (fileIcon.isNull()) {
          return statusIcon;
        }

        QPixmap basePixmap = fileIcon.pixmap(16, 16);
        QPixmap statusPixmap = statusIcon.pixmap(8, 8);

        QPainter painter(&basePixmap);

        painter.drawPixmap(basePixmap.width() - 8, basePixmap.height() - 8,
                           statusPixmap);

        return QIcon(basePixmap);
      }
    }

    if (role == Qt::ForegroundRole) {
      QString filePath = this->filePath(index);
      QColor statusColor = getStatusColor(filePath);
      if (statusColor.isValid()) {
        return statusColor;
      }
    }
  }

  return QFileSystemModel::data(index, role);
}

QVariant GitFileSystemModel::headerData(int section,
                                        Qt::Orientation orientation,
                                        int role) const {
  if (orientation == Qt::Horizontal && section == 0 &&
      role == Qt::DisplayRole && !m_rootHeaderLabel.isEmpty()) {
    return m_rootHeaderLabel;
  }

  return QFileSystemModel::headerData(section, orientation, role);
}

void GitFileSystemModel::refreshGitStatus() {
  if (m_gitIntegration) {
    m_gitIntegration->refresh();
  }
}

void GitFileSystemModel::onGitStatusChanged() { m_refreshTimer->start(); }

void GitFileSystemModel::updateStatusCache() {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    m_statusCache.clear();
    return;
  }

  m_statusCache.clear();
  QList<GitFileInfo> statusList = m_gitIntegration->getStatus();

  QString repoPath = m_gitIntegration->repositoryPath();

  for (const GitFileInfo &info : statusList) {

    QString absolutePath = repoPath + "/" + info.filePath;
    m_statusCache[absolutePath] = info;
  }

  emit layoutChanged();
}

QIcon GitFileSystemModel::getStatusIcon(const QString &filePath) const {
  auto it = m_statusCache.find(filePath);
  if (it == m_statusCache.end()) {
    return QIcon();
  }

  const GitFileInfo &info = it.value();

  if (info.indexStatus != GitFileStatus::Clean) {
    switch (info.indexStatus) {
    case GitFileStatus::Added:
      return s_addedIcon;
    case GitFileStatus::Modified:
      return s_stagedIcon;
    case GitFileStatus::Deleted:
      return s_deletedIcon;
    case GitFileStatus::Renamed:
    case GitFileStatus::Copied:
      return s_stagedIcon;
    case GitFileStatus::Unmerged:
      return s_conflictIcon;
    default:
      break;
    }
  }

  switch (info.workTreeStatus) {
  case GitFileStatus::Modified:
    return s_modifiedIcon;
  case GitFileStatus::Untracked:
    return s_untrackedIcon;
  case GitFileStatus::Deleted:
    return s_deletedIcon;
  case GitFileStatus::Unmerged:
    return s_conflictIcon;
  default:
    break;
  }

  return QIcon();
}

QColor GitFileSystemModel::getStatusColor(const QString &filePath) const {
  auto it = m_statusCache.find(filePath);
  if (it == m_statusCache.end()) {
    return QColor();
  }

  const GitFileInfo &info = it.value();

  if (info.indexStatus != GitFileStatus::Clean) {
    switch (info.indexStatus) {
    case GitFileStatus::Added:
    case GitFileStatus::Modified:
    case GitFileStatus::Renamed:
    case GitFileStatus::Copied:
      return QColor(0, 180, 0);
    case GitFileStatus::Deleted:
      return QColor(200, 0, 0);
    case GitFileStatus::Unmerged:
      return QColor(200, 0, 200);
    default:
      break;
    }
  }

  switch (info.workTreeStatus) {
  case GitFileStatus::Modified:
    return QColor(200, 140, 0);
  case GitFileStatus::Untracked:
    return QColor(128, 128, 128);
  case GitFileStatus::Deleted:
    return QColor(200, 0, 0);
  case GitFileStatus::Unmerged:
    return QColor(200, 0, 200);
  default:
    break;
  }

  return QColor();
}
