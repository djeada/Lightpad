#ifndef RECOVERYCENTERDIALOG_H
#define RECOVERYCENTERDIALOG_H

#include "../../git/gitrecovery.h"
#include "styleddialog.h"

class GitIntegration;
class QLabel;
class QListWidget;
class QPushButton;
class QTreeWidget;
class QVBoxLayout;

class RecoveryCenterDialog : public StyledDialog {
  Q_OBJECT

public:
  RecoveryCenterDialog(GitIntegration *git, const Theme &theme,
                       QWidget *parent = nullptr);

  const QList<GitRecoveryEvent> &events() const { return m_events; }
  int currentEventIndex() const;

signals:
  void repositoryChanged();
  void inspectCommitRequested(const QString &hash);
  void compareRequested(const QString &fromRef, const QString &toRef);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void reload();
  void onEventSelected();
  void onCreateRecoveryBranch();
  void onInspect();
  void onCompareWithHead();

private:
  void buildUi();
  const GitRecoveryEvent *currentEvent() const;

  GitIntegration *m_git;
  QList<GitRecoveryEvent> m_events;

  QLabel *m_headerLabel;
  QTreeWidget *m_timelineTree;
  QLabel *m_descriptionLabel;
  QLabel *m_guaranteeLabel;
  QListWidget *m_detailList;

  QPushButton *m_recoverButton;
  QPushButton *m_inspectButton;
  QPushButton *m_compareButton;
  QPushButton *m_closeButton;
};

#endif
