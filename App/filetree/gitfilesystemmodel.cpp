#include "gitfilesystemmodel.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
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

  auto createBadgeIcon = [](const QColor &color) -> QIcon {
    QPixmap pixmap(12, 12);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor border = color.darker(135);
    border.setAlpha(240);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(color);
    painter.drawEllipse(1, 1, 10, 10);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 70));
    painter.drawEllipse(3, 2, 4, 3);
    return QIcon(pixmap);
  };

  s_modifiedIcon = createBadgeIcon(QColor("#d8a13c"));
  s_stagedIcon = createBadgeIcon(QColor("#3fb97f"));
  s_untrackedIcon = createBadgeIcon(QColor("#8b949e"));
  s_addedIcon = createBadgeIcon(QColor("#2fbf71"));
  s_deletedIcon = createBadgeIcon(QColor("#e35d6a"));
  s_conflictIcon = createBadgeIcon(QColor("#c678dd"));

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
  } else if (!m_statusCache.isEmpty()) {
    m_statusCache.clear();
    emit layoutChanged();
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

  if (index.column() == 0 && role == Qt::DecorationRole) {
    const QString currentFilePath = filePath(index);
    QIcon baseIcon = getBaseIcon(index, currentFilePath);

    if (m_gitStatusEnabled && m_gitIntegration &&
        m_gitIntegration->isValidRepository()) {
      QIcon statusIcon = getStatusIcon(currentFilePath);
      if (!statusIcon.isNull()) {
        QPixmap basePixmap = baseIcon.pixmap(18, 18);
        if (basePixmap.isNull()) {
          basePixmap =
              QFileSystemModel::data(index, role).value<QIcon>().pixmap(18, 18);
        }
        if (!basePixmap.isNull()) {
          QPainter painter(&basePixmap);
          painter.setRenderHint(QPainter::Antialiasing);
          QPixmap statusPixmap = statusIcon.pixmap(10, 10);
          painter.drawPixmap(basePixmap.width() - statusPixmap.width(),
                             basePixmap.height() - statusPixmap.height(),
                             statusPixmap);
          return QIcon(basePixmap);
        }
      }
    }

    if (!baseIcon.isNull()) {
      return baseIcon;
    }
  }

  if (index.column() == 0 && role == Qt::ForegroundRole && m_gitStatusEnabled &&
      m_gitIntegration && m_gitIntegration->isValidRepository()) {
    QString currentFilePath = filePath(index);
    if (isDir(index)) {
      if (isDirtyDirectory(currentFilePath)) {
        return QColor("#d8a13c");
      }
    } else {
      QColor statusColor = getStatusColor(currentFilePath);
      if (statusColor.isValid()) {
        return statusColor;
      }
    }
  }

  if (index.column() == 0 && role == GitStatusBadgeRole && m_gitStatusEnabled &&
      m_gitIntegration && m_gitIntegration->isValidRepository()) {
    const QString currentFilePath = filePath(index);
    if (isDir(index)) {
      if (isDirtyDirectory(currentFilePath)) {
        return QStringLiteral("\u2022");
      }
      return QVariant();
    }
    return statusBadge(currentFilePath);
  }

  if (index.column() == 0 && role == GitStatusBadgeColorRole &&
      m_gitStatusEnabled && m_gitIntegration &&
      m_gitIntegration->isValidRepository()) {
    const QString currentFilePath = filePath(index);
    if (isDir(index)) {
      if (isDirtyDirectory(currentFilePath)) {
        return QColor("#d8a13c");
      }
      return QVariant();
    }
    QColor color = statusBadgeColor(currentFilePath);
    if (color.isValid()) {
      return color;
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
    const bool hadEntries = !m_statusCache.isEmpty();
    m_statusCache.clear();
    m_dirtyDirectories.clear();
    if (hadEntries) {
      emit layoutChanged();
    }
    return;
  }

  m_statusCache.clear();
  QList<GitFileInfo> statusList = m_gitIntegration->getStatus();

  QString repoPath = m_gitIntegration->repositoryPath();

  for (const GitFileInfo &info : statusList) {

    QString absolutePath = repoPath + "/" + info.filePath;
    m_statusCache[absolutePath] = info;
  }

  rebuildDirtyDirectories();

  emit layoutChanged();
}

void GitFileSystemModel::rebuildDirtyDirectories() {
  m_dirtyDirectories.clear();
  for (auto it = m_statusCache.constBegin(); it != m_statusCache.constEnd();
       ++it) {
    QDir dir = QFileInfo(it.key()).absoluteDir();
    while (!dir.isRoot()) {
      const QString dirPath = dir.absolutePath();
      if (m_dirtyDirectories.contains(dirPath)) {
        break;
      }
      m_dirtyDirectories.insert(dirPath);
      if (!dir.cdUp()) {
        break;
      }
    }
  }
}

QIcon GitFileSystemModel::getBaseIcon(const QModelIndex &index,
                                      const QString &filePath) const {
  if (isDir(index)) {
    if (m_folderIcon.isNull()) {
      m_folderIcon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
      if (m_folderIcon.isNull()) {
        m_folderIcon =
            QFileSystemModel::data(index, Qt::DecorationRole).value<QIcon>();
      }
    }
    return m_folderIcon;
  }

  return getFileTypeIcon(filePath);
}

QIcon GitFileSystemModel::getFileTypeIcon(const QString &filePath) const {
  const QString suffix = QFileInfo(filePath).suffix().toLower();
  const QString cacheKey =
      suffix.isEmpty() ? QStringLiteral("__plain__") : suffix;
  auto cached = m_fileIconCache.constFind(cacheKey);
  if (cached != m_fileIconCache.constEnd()) {
    return cached.value();
  }

  if (m_fallbackFileIcon.isNull()) {
    m_fallbackFileIcon =
        QApplication::style()->standardIcon(QStyle::SP_FileIcon);
  }

  const bool darkPalette =
      QApplication::palette().color(QPalette::Base).lightness() < 128;
  const QColor paper = darkPalette ? QColor("#d7e0ea") : QColor("#f6f8fb");
  const QColor stroke = darkPalette ? QColor("#4f5a67") : QColor("#b6c0cc");
  const QColor fold = darkPalette ? paper.lighter(112) : paper.darker(104);
  const QColor accent = colorForFileExtension(suffix);

  QPixmap pixmap(18, 18);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  QPainterPath body;
  body.moveTo(4, 2);
  body.lineTo(11, 2);
  body.lineTo(14, 5);
  body.lineTo(14, 15);
  body.lineTo(4, 15);
  body.closeSubpath();

  painter.setPen(QPen(stroke, 1.0));
  painter.setBrush(paper);
  painter.drawPath(body);

  QPolygonF foldedCorner;
  foldedCorner << QPointF(11, 2) << QPointF(11, 5) << QPointF(14, 5);
  painter.setPen(QPen(stroke, 1.0));
  painter.setBrush(fold);
  painter.drawPolygon(foldedCorner);

  painter.setPen(Qt::NoPen);
  painter.setBrush(accent);
  painter.drawRoundedRect(QRectF(5.5, 12.0, 7.0, 2.0), 1.0, 1.0);

  QIcon icon = QIcon(pixmap);
  if (icon.isNull()) {
    icon = m_fallbackFileIcon;
  }
  m_fileIconCache.insert(cacheKey, icon);
  return icon;
}

QColor GitFileSystemModel::colorForFileExtension(const QString &extension) {
  static const QHash<QString, QColor> extensionColors = {
      {"c", QColor("#4ea5ff")},     {"cc", QColor("#4ea5ff")},
      {"cpp", QColor("#4ea5ff")},   {"cxx", QColor("#4ea5ff")},
      {"h", QColor("#74c0fc")},     {"hh", QColor("#74c0fc")},
      {"hpp", QColor("#74c0fc")},   {"py", QColor("#f2cc60")},
      {"js", QColor("#f7df1e")},    {"ts", QColor("#5aa9ff")},
      {"tsx", QColor("#5aa9ff")},   {"json", QColor("#d7ba7d")},
      {"md", QColor("#7ee787")},    {"txt", QColor("#8b949e")},
      {"cmake", QColor("#c792ea")}, {"yml", QColor("#f087b3")},
      {"yaml", QColor("#f087b3")},  {"toml", QColor("#f29e74")},
      {"xml", QColor("#f29e74")},   {"html", QColor("#e06c75")},
      {"css", QColor("#61afef")},   {"sh", QColor("#8bd49c")},
      {"bash", QColor("#8bd49c")},  {"zsh", QColor("#8bd49c")},
      {"go", QColor("#56d4dd")},    {"rs", QColor("#f08f68")},
      {"java", QColor("#f89820")},  {"kt", QColor("#c792ea")}};

  return extensionColors.value(extension, QColor("#9aa6b2"));
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
      return QColor("#3fb97f");
    case GitFileStatus::Deleted:
      return QColor("#e35d6a");
    case GitFileStatus::Unmerged:
      return QColor("#c678dd");
    default:
      break;
    }
  }

  switch (info.workTreeStatus) {
  case GitFileStatus::Modified:
    return QColor("#d8a13c");
  case GitFileStatus::Untracked:
    return QColor("#9aa6b2");
  case GitFileStatus::Deleted:
    return QColor("#e35d6a");
  case GitFileStatus::Unmerged:
    return QColor("#c678dd");
  default:
    break;
  }

  return QColor();
}

QString GitFileSystemModel::statusBadge(const QString &filePath) const {
  auto it = m_statusCache.find(filePath);
  if (it == m_statusCache.end()) {
    return QString();
  }

  const GitFileInfo &info = it.value();

  if (info.indexStatus != GitFileStatus::Clean) {
    switch (info.indexStatus) {
    case GitFileStatus::Added:
      return QStringLiteral("A");
    case GitFileStatus::Modified:
      return QStringLiteral("M");
    case GitFileStatus::Deleted:
      return QStringLiteral("D");
    case GitFileStatus::Renamed:
      return QStringLiteral("R");
    case GitFileStatus::Copied:
      return QStringLiteral("C");
    case GitFileStatus::Unmerged:
      return QStringLiteral("!");
    default:
      break;
    }
  }

  switch (info.workTreeStatus) {
  case GitFileStatus::Modified:
    return QStringLiteral("M");
  case GitFileStatus::Untracked:
    return QStringLiteral("U");
  case GitFileStatus::Deleted:
    return QStringLiteral("D");
  case GitFileStatus::Unmerged:
    return QStringLiteral("!");
  default:
    break;
  }

  return QString();
}

QColor GitFileSystemModel::statusBadgeColor(const QString &filePath) const {
  return getStatusColor(filePath);
}

bool GitFileSystemModel::isDirtyDirectory(const QString &dirPath) const {
  return m_dirtyDirectories.contains(dirPath);
}
