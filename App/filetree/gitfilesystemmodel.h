#ifndef GITFILESYSTEMMODEL_H
#define GITFILESYSTEMMODEL_H

#include <QFileSystemModel>
#include <QIcon>
#include <QTimer>
#include "../git/gitintegration.h"

/**
 * @brief Default debounce interval for git status refresh in milliseconds
 */
constexpr int GIT_STATUS_REFRESH_DEBOUNCE_MS = 500;

/**
 * @brief A QFileSystemModel that displays git status icons
 * 
 * This model extends QFileSystemModel to show git status indicators
 * (modified, staged, untracked) next to files in the file tree.
 */
class GitFileSystemModel : public QFileSystemModel {
    Q_OBJECT

public:
    explicit GitFileSystemModel(QObject* parent = nullptr);
    ~GitFileSystemModel();
    void setRootHeaderLabel(const QString& label);

    /**
     * @brief Set the git integration instance to use for status
     */
    void setGitIntegration(GitIntegration* git);

    /**
     * @brief Get the git integration instance
     */
    GitIntegration* gitIntegration() const;

    /**
     * @brief Enable or disable git status display
     */
    void setGitStatusEnabled(bool enabled);

    /**
     * @brief Check if git status display is enabled
     */
    bool isGitStatusEnabled() const;

    /**
     * @brief Override data() to provide git status icons
     */
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /**
     * @brief Refresh git status for the repository
     */
    void refreshGitStatus();

    /**
     * @brief Override header data to display custom root label
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private slots:
    void onGitStatusChanged();

private:
    GitIntegration* m_gitIntegration;
    bool m_gitStatusEnabled;
    QTimer* m_refreshTimer;
    mutable QMap<QString, GitFileInfo> m_statusCache;
    QString m_rootHeaderLabel;

    /**
     * @brief Get the status icon for a file
     */
    QIcon getStatusIcon(const QString& filePath) const;

    /**
     * @brief Get the status decoration for a file
     */
    QColor getStatusColor(const QString& filePath) const;

    /**
     * @brief Update the status cache
     */
    void updateStatusCache();

    /**
     * @brief Initialize status icons
     */
    static void initializeIcons();

    static QIcon s_modifiedIcon;
    static QIcon s_stagedIcon;
    static QIcon s_untrackedIcon;
    static QIcon s_addedIcon;
    static QIcon s_deletedIcon;
    static QIcon s_conflictIcon;
    static bool s_iconsInitialized;
};

#endif // GITFILESYSTEMMODEL_H
