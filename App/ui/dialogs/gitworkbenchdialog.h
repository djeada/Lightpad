#ifndef GITWORKBENCHDIALOG_H
#define GITWORKBENCHDIALOG_H

#include "../../git/gitintegration.h"
#include "../../settings/theme.h"
#include <QCheckBox>
#include <QDialog>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTextEdit;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

enum class OperationRisk { Low, Medium, High, Critical };

struct WorkbenchRebaseEntry {
  QString action;
  QString hash;
  QString shortHash;
  QString subject;
  QString author;
  QString authorEmail;
  QString date;
  QString relativeDate;
  QStringList parents;
  QStringList refs;
  QList<GitCommitFileStat> fileStats;
};

class GitWorkbenchDialog : public QDialog {
  Q_OBJECT

public:
  explicit GitWorkbenchDialog(GitIntegration *git, const Theme &theme,
                              QWidget *parent = nullptr);

  void loadRepository();

signals:
  void viewCommitDiff(const QString &commitHash);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private slots:

  void onBranchItemClicked(QTreeWidgetItem *item, int column);
  void onBranchItemDoubleClicked(QTreeWidgetItem *item, int column);
  void onBranchContextMenu(const QPoint &pos);
  void onBranchSearchChanged(const QString &text);
  void onCreateBranchClicked();
  void onRenameBranchClicked();
  void onMergeBranchClicked();
  void onRebaseBranchClicked();

  void onCreateStashClicked();

  void onCommitSelectionChanged();
  void onCommitContextMenu(const QPoint &pos);
  void onCommitSearchChanged(const QString &text);
  void onCommitDoubleClicked(QTreeWidgetItem *item, int column);

  void onToggleRewriteMode();
  void onMoveUp();
  void onMoveDown();
  void onSquashSelected();
  void onDropSelected();
  void onFixupSelected();
  void onRewordSelected();
  void onPickAll();
  void onApplyRebase();
  void onActionComboChanged(int index);

  void onCherryPickClicked();
  void onMoveToBranchClicked();
  void onRevertCommitClicked();
  void onResetToBranchClicked();
  void onEditCommitMessage();

  void onCommandPalette();

private:
  void buildUi();
  void buildBranchExplorer(QVBoxLayout *layout);
  void buildCommitCanvas(QVBoxLayout *layout);
  void buildInspector(QVBoxLayout *layout);
  void buildBottomBar(QVBoxLayout *mainLayout);
  void applyTheme();

  void loadBranches();
  void loadCommits(const QString &branch = QString());
  void loadStashes();
  void loadTags();

  void populateCommitRow(QTreeWidgetItem *item, int index);
  void rebuildActionCombo(QTreeWidgetItem *item, int index);
  void syncEntriesFromTree();
  void updatePlanSummary();

  void showCommitInspector(const QString &hash);
  void showBranchInspector(const QString &branchName);
  void showStashInspector(int stashIndex);
  void showPlanInspector();
  void showRecoveryCenter();
  void clearInspector();

  OperationRisk assessRebaseRisk() const;
  QString riskLabel(OperationRisk risk) const;
  QString riskColor(OperationRisk risk) const;
  bool confirmOperation(const QString &title, const QString &description,
                        OperationRisk risk, bool offerBackup = true);

  void navigateCommit(int delta, bool extendSelection = false);
  void setCommitAction(const QString &action);
  void updateSelectionUI();

  QString actionColor(const QString &action) const;
  QString toolButtonStyle() const;
  QString dangerButtonStyle() const;
  QString accentButtonStyle() const;
  QString ghostButtonStyle() const;

  GitIntegration *m_git;
  Theme m_theme;
  bool m_rewriteMode;
  QString m_currentBranch;
  QList<WorkbenchRebaseEntry> m_entries;
  QList<GitBranchInfo> m_branches;
  QList<GitStashEntry> m_stashes;
  QList<GitTagInfo> m_tags;
  QMap<QString, QStringList> m_commitRefsCache;

  QSplitter *m_mainSplitter;

  QWidget *m_branchPanel;
  QLineEdit *m_branchSearch;
  QTreeWidget *m_branchTree;
  QPushButton *m_createBranchBtn;

  QWidget *m_commitPanel;
  QLineEdit *m_commitSearch;
  QTreeWidget *m_commitTree;
  QToolBar *m_rewriteToolbar;
  QPushButton *m_rewriteToggleBtn;
  QPushButton *m_moveUpBtn;
  QPushButton *m_moveDownBtn;
  QPushButton *m_squashBtn;
  QPushButton *m_dropBtn;
  QPushButton *m_fixupBtn;
  QPushButton *m_rewordBtn;
  QPushButton *m_pickAllBtn;
  QLabel *m_commitStatusLabel;
  QWidget *m_selectionBar;
  QLabel *m_selectionCountLabel;

  QWidget *m_inspectorPanel;
  QStackedWidget *m_inspectorStack;

  QWidget *m_commitInspectorPage;
  QLabel *m_inspCommitHash;
  QLabel *m_inspCommitAuthor;
  QLabel *m_inspCommitDate;
  QLabel *m_inspCommitParents;
  QLabel *m_inspCommitRefs;
  QTextEdit *m_inspCommitMessage;
  QTreeWidget *m_inspFileList;
  QLabel *m_inspPatchStats;
  QPushButton *m_inspCherryPickBtn;
  QPushButton *m_inspEditMessageBtn;
  QPushButton *m_inspMoveToBranchBtn;
  QPushButton *m_inspRevertBtn;
  QPushButton *m_inspViewDiffBtn;

  QWidget *m_branchInspectorPage;
  QLabel *m_inspBranchName;
  QLabel *m_inspBranchTip;
  QLabel *m_inspBranchUpstream;
  QLabel *m_inspBranchAheadBehind;
  QLabel *m_inspBranchActivity;
  QPushButton *m_inspBranchCheckoutBtn;
  QPushButton *m_inspBranchRenameBtn;
  QPushButton *m_inspBranchMergeBtn;
  QPushButton *m_inspBranchRebaseBtn;
  QPushButton *m_inspBranchDeleteBtn;
  QPushButton *m_inspBranchResetBtn;

  QWidget *m_planInspectorPage;
  QTreeWidget *m_planStepsList;
  QLabel *m_planRiskLabel;
  QLabel *m_planAffectedRefs;
  QLabel *m_planRecoveryInfo;

  QWidget *m_recoveryCenterPage;
  QTreeWidget *m_recoveryList;
  QLabel *m_recoveryStatusLabel;
  QPushButton *m_recoveryRestoreBtn;

  QWidget *m_stashInspectorPage;
  QLabel *m_inspStashRef;
  QLabel *m_inspStashMessage;
  QLabel *m_inspStashBranch;
  QLabel *m_inspStashHash;
  QPushButton *m_inspStashApplyBtn;
  QPushButton *m_inspStashPopBtn;
  QPushButton *m_inspStashDropBtn;

  QWidget *m_bottomBar;
  QLabel *m_planSummaryLabel;
  QLabel *m_riskIndicator;
  QCheckBox *m_backupCheckbox;
  QPushButton *m_applyBtn;
  QPushButton *m_cancelBtn;

  QMap<QString, int> m_hashToEntryIndex;
  QString m_selectedCommitHash;
  QString m_selectedBranchName;

  static const QStringList REBASE_ACTIONS;
  static const QStringList REBASE_ACTIONS_EXTENDED;
  static const QMap<QString, QString> ACTION_COLORS;
  static const QMap<QString, QString> ACTION_ICONS;
};

#endif
