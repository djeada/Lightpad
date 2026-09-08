#ifndef BRANCHHYGIENEDIALOG_H
#define BRANCHHYGIENEDIALOG_H

#include "../../git/gitbranchhygiene.h"
#include "styleddialog.h"

class GitIntegration;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class BranchHygieneDialog : public StyledDialog {
  Q_OBJECT

public:
  BranchHygieneDialog(GitIntegration *git, const Theme &theme,
                      QWidget *parent = nullptr);

  const GitBranchHygieneReport &report() const { return m_report; }
  QString selectedBranchName() const;

  static QStringList pinnedBranches();
  static void setPinnedBranches(const QStringList &names);

signals:
  void repositoryChanged();
  void showBranchInGraphRequested(const QString &branchName);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void reload();
  void onBaseChanged();
  void onSelectionChanged();
  void onTogglePin();
  void onDeleteSelected();
  void onCleanupMerged();
  void onSetUpstreamOrDelete();
  void onShowInGraph();

private:
  void buildUi();
  const GitBranchHealth *currentBranch() const;
  QList<GitBranchHealth> selectedBranches() const;

  GitIntegration *m_git;
  GitBranchHygieneReport m_report;

  QComboBox *m_baseCombo;
  QLineEdit *m_filterEdit;
  QTreeWidget *m_branchTree;
  QLabel *m_detailLabel;
  QLabel *m_warningLabel;
  QListWidget *m_stateList;

  QPushButton *m_pinButton;
  QPushButton *m_deleteButton;
  QPushButton *m_cleanupButton;
  QPushButton *m_upstreamButton;
  QPushButton *m_graphButton;
  QPushButton *m_closeButton;
};

#endif
