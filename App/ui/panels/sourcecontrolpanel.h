#ifndef SOURCECONTROLPANEL_H
#define SOURCECONTROLPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QTextEdit>
#include "../../git/gitintegration.h"

/**
 * @brief Source Control Panel for Git integration
 * 
 * Provides UI for viewing and managing git repository status,
 * staging/unstaging files, and committing changes.
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
     * @brief Refresh the panel with current status
     */
    void refresh();

signals:
    /**
     * @brief Emitted when user wants to open a file
     */
    void fileOpenRequested(const QString& filePath);

    /**
     * @brief Emitted when user wants to view diff
     */
    void diffRequested(const QString& filePath);

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

private:
    void setupUI();
    void updateTree();
    void updateBranchLabel();
    QString statusIcon(GitFileStatus status) const;
    QString statusText(GitFileStatus status) const;
    QColor statusColor(GitFileStatus status) const;

    GitIntegration* m_git;
    
    // UI elements
    QLabel* m_branchLabel;
    QLabel* m_statusLabel;
    QTextEdit* m_commitMessage;
    QPushButton* m_commitButton;
    QPushButton* m_stageAllButton;
    QPushButton* m_unstageAllButton;
    QPushButton* m_refreshButton;
    QTreeWidget* m_stagedTree;
    QTreeWidget* m_changesTree;
};

#endif // SOURCECONTROLPANEL_H
