#ifndef UNDOCHANGESWIZARDDIALOG_H
#define UNDOCHANGESWIZARDDIALOG_H

#include "../../git/gitundoplan.h"
#include "styleddialog.h"
#include <QMap>

class QLabel;
class QListWidget;
class QPushButton;
class QRadioButton;
class QVBoxLayout;

class UndoChangesWizardDialog : public StyledDialog {
  Q_OBJECT

public:
  UndoChangesWizardDialog(GitIntegration *git, const Theme &theme,
                          QWidget *parent = nullptr);

  void setTarget(const QString &commit, const QStringList &paths);

  const GitUndoPlan &plan() const { return m_plan; }
  GitUndoGoal selectedGoal() const;

signals:
  void repositoryChanged();
  void amendRequested();

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void reload();
  void onGoalChanged();
  void onShowPreview();
  void onExecute();

private:
  void buildUi();
  void buildOptions(QVBoxLayout *layout);
  void buildMatrix(QVBoxLayout *layout);
  void buildFooter(QVBoxLayout *layout);
  void updateSelectedOption();
  const GitUndoOption *optionFor(GitUndoGoal goal) const;

  GitIntegration *m_git;
  GitUndoPlan m_plan;
  QString m_targetCommit;
  QStringList m_targetPaths;

  QLabel *m_headerLabel;
  QLabel *m_recommendationLabel;
  QMap<GitUndoGoal, QRadioButton *> m_radios;
  QMap<GitUndoGoal, QLabel *> m_explanations;

  QLabel *m_matrixLabel;
  QLabel *m_commandLabel;
  QListWidget *m_riskList;

  QPushButton *m_previewButton;
  QPushButton *m_executeButton;
  QPushButton *m_cancelButton;

  bool m_updating;
};

#endif
