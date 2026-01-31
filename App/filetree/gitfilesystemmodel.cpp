#include "gitfilesystemmodel.h"
#include "../core/logging/logger.h"
#include <QPainter>
#include <QApplication>
#include <QStyle>

// Static member initialization
QIcon GitFileSystemModel::s_modifiedIcon;
QIcon GitFileSystemModel::s_stagedIcon;
QIcon GitFileSystemModel::s_untrackedIcon;
QIcon GitFileSystemModel::s_addedIcon;
QIcon GitFileSystemModel::s_deletedIcon;
QIcon GitFileSystemModel::s_conflictIcon;
bool GitFileSystemModel::s_iconsInitialized = false;

void GitFileSystemModel::initializeIcons()
{
    if (s_iconsInitialized) {
        return;
    }

    // Create simple colored circle icons for git status
    auto createCircleIcon = [](const QColor& color) -> QIcon {
        QPixmap pixmap(12, 12);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(color);
        painter.setPen(color.darker(120));
        painter.drawEllipse(1, 1, 10, 10);
        return QIcon(pixmap);
    };

    s_modifiedIcon = createCircleIcon(QColor(255, 165, 0));   // Orange for modified
    s_stagedIcon = createCircleIcon(QColor(0, 200, 0));       // Green for staged
    s_untrackedIcon = createCircleIcon(QColor(128, 128, 128)); // Gray for untracked
    s_addedIcon = createCircleIcon(QColor(0, 255, 0));        // Bright green for added
    s_deletedIcon = createCircleIcon(QColor(255, 0, 0));      // Red for deleted
    s_conflictIcon = createCircleIcon(QColor(255, 0, 255));   // Magenta for conflict

    s_iconsInitialized = true;
}

GitFileSystemModel::GitFileSystemModel(QObject* parent)
    : QFileSystemModel(parent)
    , m_gitIntegration(nullptr)
    , m_gitStatusEnabled(true)
    , m_refreshTimer(new QTimer(this))
{
    initializeIcons();

    // Set up refresh timer for debouncing git status updates
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(GIT_STATUS_REFRESH_DEBOUNCE_MS);
    connect(m_refreshTimer, &QTimer::timeout, this, &GitFileSystemModel::updateStatusCache);
}

GitFileSystemModel::~GitFileSystemModel()
{
}

void GitFileSystemModel::setGitIntegration(GitIntegration* git)
{
    if (m_gitIntegration) {
        disconnect(m_gitIntegration, nullptr, this, nullptr);
    }

    m_gitIntegration = git;

    if (m_gitIntegration) {
        connect(m_gitIntegration, &GitIntegration::statusChanged,
                this, &GitFileSystemModel::onGitStatusChanged);
        updateStatusCache();
    }
}

GitIntegration* GitFileSystemModel::gitIntegration() const
{
    return m_gitIntegration;
}

void GitFileSystemModel::setGitStatusEnabled(bool enabled)
{
    if (m_gitStatusEnabled != enabled) {
        m_gitStatusEnabled = enabled;
        if (enabled) {
            updateStatusCache();
        }
        // Emit dataChanged for the entire model to refresh icons
        emit layoutChanged();
    }
}

bool GitFileSystemModel::isGitStatusEnabled() const
{
    return m_gitStatusEnabled;
}

QVariant GitFileSystemModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QFileSystemModel::data(index, role);
    }

    // Only modify the first column (file name column)
    if (index.column() == 0 && m_gitStatusEnabled && m_gitIntegration && 
        m_gitIntegration->isValidRepository()) {
        
        if (role == Qt::DecorationRole) {
            // Get the base file icon first
            QVariant baseIcon = QFileSystemModel::data(index, role);
            QString filePath = this->filePath(index);
            
            // Check if this file has git status
            QIcon statusIcon = getStatusIcon(filePath);
            if (!statusIcon.isNull()) {
                // For simplicity, we'll overlay the status on the existing icon
                // A more sophisticated approach would composite the icons
                QIcon fileIcon = baseIcon.value<QIcon>();
                if (fileIcon.isNull()) {
                    return statusIcon;
                }
                
                // Create a composite icon
                QPixmap basePixmap = fileIcon.pixmap(16, 16);
                QPixmap statusPixmap = statusIcon.pixmap(8, 8);
                
                QPainter painter(&basePixmap);
                // Draw status indicator in bottom-right corner
                painter.drawPixmap(basePixmap.width() - 8, basePixmap.height() - 8, statusPixmap);
                
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

void GitFileSystemModel::refreshGitStatus()
{
    if (m_gitIntegration) {
        m_gitIntegration->refresh();
    }
}

void GitFileSystemModel::onGitStatusChanged()
{
    // Debounce the refresh
    m_refreshTimer->start();
}

void GitFileSystemModel::updateStatusCache()
{
    if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
        m_statusCache.clear();
        return;
    }

    m_statusCache.clear();
    QList<GitFileInfo> statusList = m_gitIntegration->getStatus();
    
    QString repoPath = m_gitIntegration->repositoryPath();
    
    for (const GitFileInfo& info : statusList) {
        // Store with absolute path
        QString absolutePath = repoPath + "/" + info.filePath;
        m_statusCache[absolutePath] = info;
    }

    // Notify view to update
    emit layoutChanged();
}

QIcon GitFileSystemModel::getStatusIcon(const QString& filePath) const
{
    auto it = m_statusCache.find(filePath);
    if (it == m_statusCache.end()) {
        return QIcon();
    }

    const GitFileInfo& info = it.value();

    // Priority: staged status > worktree status
    if (info.indexStatus != GitFileStatus::Clean) {
        switch (info.indexStatus) {
            case GitFileStatus::Added:
                return s_addedIcon;
            case GitFileStatus::Modified:
                return s_stagedIcon;  // Green for staged modifications
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

    // Worktree status
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

QColor GitFileSystemModel::getStatusColor(const QString& filePath) const
{
    auto it = m_statusCache.find(filePath);
    if (it == m_statusCache.end()) {
        return QColor();  // Invalid color - use default
    }

    const GitFileInfo& info = it.value();

    // Staged files get green text
    if (info.indexStatus != GitFileStatus::Clean) {
        switch (info.indexStatus) {
            case GitFileStatus::Added:
            case GitFileStatus::Modified:
            case GitFileStatus::Renamed:
            case GitFileStatus::Copied:
                return QColor(0, 180, 0);  // Green
            case GitFileStatus::Deleted:
                return QColor(200, 0, 0);  // Red
            case GitFileStatus::Unmerged:
                return QColor(200, 0, 200);  // Magenta
            default:
                break;
        }
    }

    // Worktree status colors
    switch (info.workTreeStatus) {
        case GitFileStatus::Modified:
            return QColor(200, 140, 0);  // Orange
        case GitFileStatus::Untracked:
            return QColor(128, 128, 128);  // Gray
        case GitFileStatus::Deleted:
            return QColor(200, 0, 0);  // Red
        case GitFileStatus::Unmerged:
            return QColor(200, 0, 200);  // Magenta
        default:
            break;
    }

    return QColor();  // Invalid - use default
}
