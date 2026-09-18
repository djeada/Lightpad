#ifndef MERGESTARTDIALOG_H
#define MERGESTARTDIALOG_H

#include "../../git/gitmergeplan.h"
#include "styleddialog.h"

class GitIntegration;
class QComboBox;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;
class QRadioButton;
class QVBoxLayout;

class MergeStartDialog : public StyledDialog {
  Q_OBJECT

public:
  MergeStartDialog(GitIntegration *git, const Theme &theme,
                   QWidget *parent = nullptr);

  QString selectedRef() const;
  QString selectedTarget() const;
  GitMergeOptions mergeOptions() const;
  const GitMergePlan &plan() const { return m_plan; }

  void selectBranch(const QString &name);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void onBranchChanged();
  void onCopyChanged();
  void onPreferenceChanged();

private:
  void buildUi();
  void buildSourceStep(QVBoxLayout *layout);
  void buildTargetStep(QVBoxLayout *layout);
  void buildOutcomeStep(QVBoxLayout *layout);
  void buildPreferenceStep(QVBoxLayout *layout);
  void buildActions(QVBoxLayout *layout);

  void loadChoices();
  void rebuildCopyOptions();
  void refreshPlan();
  void updateStartButton();

  const GitMergeChoice *currentChoice() const;

  GitIntegration *m_git = nullptr;
  QList<GitMergeChoice> m_choices;
  GitMergePlan m_plan;

  QComboBox *m_branchCombo = nullptr;
  QGroupBox *m_copyGroup = nullptr;
  QVBoxLayout *m_copyLayout = nullptr;
  QList<QRadioButton *> m_copyButtons;
  QLabel *m_copyWarningLabel = nullptr;

  QLabel *m_targetLabel = nullptr;
  QComboBox *m_targetCombo = nullptr;
  QLabel *m_targetHintLabel = nullptr;

  QLabel *m_headlineLabel = nullptr;
  QLabel *m_outcomeLabel = nullptr;
  QLabel *m_filesLabel = nullptr;
  QLabel *m_conflictLabel = nullptr;
  QListWidget *m_conflictList = nullptr;

  QGroupBox *m_preferenceGroup = nullptr;
  QRadioButton *m_manualRadio = nullptr;
  QRadioButton *m_keepOursRadio = nullptr;
  QRadioButton *m_keepTheirsRadio = nullptr;
  QLabel *m_preferenceExplanation = nullptr;

  QLabel *m_blockerLabel = nullptr;
  QPushButton *m_startButton = nullptr;
  QPushButton *m_cancelButton = nullptr;
};

#endif
