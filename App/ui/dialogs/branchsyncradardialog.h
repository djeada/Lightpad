#ifndef BRANCHSYNCRADARDIALOG_H
#define BRANCHSYNCRADARDIALOG_H

#include "../../git/gitintegration.h"
#include "styleddialog.h"

class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QTreeWidget;
class QVBoxLayout;

class BranchSyncRadarDialog : public StyledDialog {
  Q_OBJECT

public:
  BranchSyncRadarDialog(GitIntegration *git, const Theme &theme,
                        QWidget *parent = nullptr);

  const GitSyncState &syncState() const { return m_state; }
  GitPullStrategy selectedStrategy() const;
  GitPushForce selectedPushForce() const;

signals:
  void repositoryChanged();
  void commitRequested(const QString &hash);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void reload();
  void onFetchClicked();
  void onPullClicked();
  void onPushClicked();
  void onSetUpstreamClicked();
  void onStrategyChanged();
  void onPushModeChanged();

private:
  void buildUi();
  void buildLanes(QVBoxLayout *layout);
  void buildStrategyBar(QVBoxLayout *layout);
  void updateFromState();
  void fillLane(QTreeWidget *tree, const QList<GitCommitInfo> &commits);

  GitIntegration *m_git;
  GitSyncState m_state;

  QLabel *m_headerLabel;
  QLabel *m_summaryLabel;
  QLabel *m_mergeBaseLabel;
  QLabel *m_incomingLabel;
  QLabel *m_outgoingLabel;
  QLabel *m_divergenceLabel;
  QLabel *m_strategyPreview;
  QLabel *m_pushPreview;
  QLabel *m_configuredLabel;

  QTreeWidget *m_incomingTree;
  QTreeWidget *m_outgoingTree;

  QRadioButton *m_mergeRadio;
  QRadioButton *m_rebaseRadio;
  QRadioButton *m_ffRadio;

  QComboBox *m_pushModeCombo;

  QPushButton *m_fetchButton;
  QPushButton *m_pullButton;
  QPushButton *m_pushButton;
  QPushButton *m_upstreamButton;
  QPushButton *m_closeButton;
};

#endif
