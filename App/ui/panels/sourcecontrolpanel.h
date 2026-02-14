#ifndef SOURCECONTROLPANEL_H
#define SOURCECONTROLPANEL_H

#include "../../git/gitintegration.h"
#include "../../settings/theme.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

class GitInitDialog;
class MergeConflictDialog;
class GitRemoteDialog;
class GitStashDialog;

class SourceControlPanel : public QWidget {
  Q_OBJECT

public:
  explicit SourceControlPanel(QWidget *parent = nullptr);
  ~SourceControlPanel();

  void setGitIntegration(GitIntegration *git);

  void setWorkingPath(const QString &path);

  void refresh();

  void applyTheme(const Theme &theme);

signals:

  void fileOpenRequested(const QString &filePath);

  void diffRequested(const QString &filePath, bool staged);

  void repositoryInitialized(const QString &path);

  void commitDiffRequested(const QString &commitHash, const QString &shortHash);

  void compareBranchesRequested(const QString &branch1, const QString &branch2);

private slots:
  void onStageAllClicked();
  void onUnstageAllClicked();
  void onCommitClicked();
  void onRefreshClicked();
  void onItemDoubleClicked(QTreeWidgetItem *item, int column);
  void onItemContextMenu(const QPoint &pos);
  void onStatusChanged();
  void onBranchChanged(const QString &branchName);
  void onOperationCompleted(const QString &message);
  void onErrorOccurred(const QString &error);
  void onBranchSelectorChanged(int index);
  void onCreateBranchClicked();
  void onDeleteBranchClicked();
  void onCommitMessageChanged();
  void onItemCheckChanged(QTreeWidgetItem *item, int column);
  void onHistoryContextMenu(const QPoint &pos);

  void onInitRepositoryClicked();
  void onPushClicked();
  void onPullClicked();
  void onFetchClicked();
  void onStashClicked();
  void onMergeConflictsDetected(const QStringList &files);
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
  void stageOrUnstageSelectedFiles(QTreeWidget *tree, bool stage);
  QString statusIcon(GitFileStatus status) const;
  QString statusText(GitFileStatus status) const;
  QColor statusColor(GitFileStatus status) const;
  void addEmptyStateItem(QTreeWidget *tree, const QString &text);
  void updateCounts();
  void updateHeaderTitle();
  void scheduleRefresh();

  GitIntegration *m_git;
  QString m_workingPath;
  Theme m_theme;
  bool m_themeInitialized;

  QStackedWidget *m_stackedWidget;

  QWidget *m_noRepoWidget;
  QPushButton *m_initRepoButton;
  QLabel *m_noRepoLabel;
  QLabel *m_headerTitleLabel;

  QWidget *m_conflictWidget;
  QLabel *m_conflictLabel;
  QListWidget *m_conflictFilesList;
  QPushButton *m_resolveConflictsButton;
  QPushButton *m_abortMergeButton;

  QWidget *m_repoWidget;
  QLabel *m_branchLabel;
  QLabel *m_statusLabel;
  QComboBox *m_branchSelector;
  QPushButton *m_newBranchButton;
  QPushButton *m_deleteBranchButton;
  QTextEdit *m_commitMessage;
  QPushButton *m_commitButton;
  QCheckBox *m_amendCheckbox;
  QPushButton *m_stageAllButton;
  QPushButton *m_unstageAllButton;
  QPushButton *m_refreshButton;
  QPushButton *m_pushButton;
  QPushButton *m_pullButton;
  QPushButton *m_fetchButton;
  QPushButton *m_stashButton;
  QLabel *m_stagedLabel;
  QTreeWidget *m_stagedTree;
  QLabel *m_changesLabel;
  QTreeWidget *m_changesTree;

  QWidget *m_historyHeader;
  QLabel *m_historyLabel;
  QTreeWidget *m_historyTree;
  QPushButton *m_historyToggleButton;
  bool m_historyExpanded;
  bool m_updatingBranchSelector;
  bool m_updatingTree;
  int m_stagedCount;
  int m_changesCount;
  QTimer *m_refreshTimer;
};

#endif
