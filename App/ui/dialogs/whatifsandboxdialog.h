#ifndef WHATIFSANDBOXDIALOG_H
#define WHATIFSANDBOXDIALOG_H

#include "../../git/gitsandbox.h"
#include "styleddialog.h"

class GitIntegration;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QVBoxLayout;

class WhatIfSandboxDialog : public StyledDialog {
  Q_OBJECT

public:
  WhatIfSandboxDialog(GitIntegration *git, const Theme &theme,
                      QWidget *parent = nullptr);

  const GitSandbox &sandbox() const { return m_sandbox; }

signals:

  void planRequested(const QList<GitOperationRequest> &plan);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void refreshViews();
  void onMerge();
  void onRebase();
  void onReset();
  void onCherryPick();
  void onDeleteRef();
  void onResetSandbox();
  void onApplyPlan();

private:
  void buildUi();
  QString selectedRef(QComboBox *combo) const;
  QString selectedCommitId() const;
  void note(const QString &message);

  GitIntegration *m_git;
  GitSandbox m_sandbox;

  QLabel *m_bannerLabel;
  QComboBox *m_leftRefCombo;
  QComboBox *m_rightRefCombo;
  QListWidget *m_graphList;
  QListWidget *m_journalList;
  QLabel *m_unreachableLabel;
  QListWidget *m_limitationList;

  QPushButton *m_mergeButton;
  QPushButton *m_rebaseButton;
  QPushButton *m_resetButton;
  QPushButton *m_cherryPickButton;
  QPushButton *m_deleteButton;
  QPushButton *m_resetSandboxButton;
  QPushButton *m_applyButton;
  QPushButton *m_closeButton;
};

#endif
