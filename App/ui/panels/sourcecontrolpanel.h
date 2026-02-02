#ifndef SOURCECONTROLPANEL_H
#define SOURCECONTROLPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QStackedWidget>
#include <QTimer>
#include "../../git/gitintegration.h"
#include "../../settings/theme.h"

class GitInitDialog;
class MergeConflictDialog;
class GitRemoteDialog;
class GitStashDialog;

/**
 * @brief Source Control Panel for Git integration
 * 
 * Provides UI for viewing and managing git repository status,
 * staging/unstaging files, committing changes, and branch management.
 * Also handles edge cases like uninitialized repositories and merge conflicts.
 */
class SourceControlPanel : public QWidget {
    Q_OBJECT

public:
    explicit SourceControlPanel(QWidget* parent = nullptr);
    ~SourceControlPanel();

    /**
     * @brief Set the git integration instance
     */
    void setGitIntegration(GitIntegration* git);

    /**
     * @brief Set the working path for the current project
     */
    void setWorkingPath(const QString& path);

    /**
     * @brief Refresh the panel with current status
     */
    void refresh();

    /**
     * @brief Apply theme to the panel
     */
    void applyTheme(const Theme& theme);

signals:
    /**
     * @brief Emitted when user wants to open a file
     */
    void fileOpenRequested(const QString& filePath);

    /**
     * @brief Emitted when user wants to view diff
     */
    void diffRequested(const QString& filePath, bool staged);

    /**
     * @brief Emitted when repository is initialized
     */
    void repositoryInitialized(const QString& path);

    /**
     * @brief Emitted when user wants to view commit diff
     */
    void commitDiffRequested(const QString& commitHash, const QString& shortHash);

private slots:
    void onStageAllClicked();
    void onUnstageAllClicked();
    void onCommitClicked();
    void onRefreshClicked();
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onItemContextMenu(const QPoint& pos);
    void onStatusChanged();
    void onBranchChanged(const QString& branchName);
    void onOperationCompleted(const QString& message);
    void onErrorOccurred(const QString& error);
    void onBranchSelectorChanged(int index);
    void onCreateBranchClicked();
    void onDeleteBranchClicked();
    void onCommitMessageChanged();
    void onItemCheckChanged(QTreeWidgetItem* item, int column);
    void onHistoryContextMenu(const QPoint& pos);
    
    // New slots for extended functionality
    void onInitRepositoryClicked();
    void onPushClicked();
    void onPullClicked();
    void onFetchClicked();
    void onStashClicked();
    void onMergeConflictsDetected(const QStringList& files);
    void onResolveConflictsClicked();

private:
    void setupUI();
    void setupNoRepoUI();
    void setupRepoUI();
    void setupMergeConflictUI();
    void updateTree();
    void updateBranchSelector();
    void updateBranchLabel();
    void updateUIState();
    void updateHistory();
    void resetChangeCounts();
    void stageOrUnstageSelectedFiles(QTreeWidget* tree, bool stage);
    QString statusIcon(GitFileStatus status) const;
    QString statusText(GitFileStatus status) const;
    QColor statusColor(GitFileStatus status) const;
    void addEmptyStateItem(QTreeWidget* tree, const QString& text);
    void updateCounts();
    void updateHeaderTitle();
    void scheduleRefresh();

    GitIntegration* m_git;
    QString m_workingPath;
    Theme m_theme;
    bool m_themeInitialized;
    
    // Main stacked widget for different states
    QStackedWidget* m_stackedWidget;
    
    // No repository state UI
    QWidget* m_noRepoWidget;
    QPushButton* m_initRepoButton;
    QLabel* m_noRepoLabel;
    QLabel* m_headerTitleLabel;
    
    // Merge conflict state UI
    QWidget* m_conflictWidget;
    QLabel* m_conflictLabel;
    QListWidget* m_conflictFilesList;
    QPushButton* m_resolveConflictsButton;
    QPushButton* m_abortMergeButton;
    
    // Normal repository state UI elements
    QWidget* m_repoWidget;
    QLabel* m_branchLabel;
    QLabel* m_statusLabel;
    QComboBox* m_branchSelector;
    QPushButton* m_newBranchButton;
    QPushButton* m_deleteBranchButton;
    QTextEdit* m_commitMessage;
    QPushButton* m_commitButton;
    QCheckBox* m_amendCheckbox;
    QPushButton* m_stageAllButton;
    QPushButton* m_unstageAllButton;
    QPushButton* m_refreshButton;
    QPushButton* m_pushButton;
    QPushButton* m_pullButton;
    QPushButton* m_fetchButton;
    QPushButton* m_stashButton;
    QLabel* m_stagedLabel;
    QTreeWidget* m_stagedTree;
    QLabel* m_changesLabel;
    QTreeWidget* m_changesTree;
    
    // Commit history section
    QWidget* m_historyHeader;
    QLabel* m_historyLabel;
    QTreeWidget* m_historyTree;
    QPushButton* m_historyToggleButton;
    bool m_historyExpanded;
    bool m_updatingBranchSelector;
    bool m_updatingTree;
    int m_stagedCount;
    int m_changesCount;
    QTimer* m_refreshTimer;
};

#endif // SOURCECONTROLPANEL_H
