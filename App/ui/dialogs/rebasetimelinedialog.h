#ifndef REBASETIMELINEDIALOG_H
#define REBASETIMELINEDIALOG_H

#include "../../git/gitrebaseplan.h"
#include "styleddialog.h"

class GitIntegration;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class RebaseTimelineDialog : public StyledDialog {
  Q_OBJECT

public:
  RebaseTimelineDialog(GitIntegration *git, const Theme &theme,
                       QWidget *parent = nullptr);

  const GitRebasePlan &plan() const { return m_plan; }
  bool rebaseIsRunning() const { return m_rebaseRunning; }

signals:
  void repositoryChanged();
  void resolveConflictsRequested();

protected:
  void applyTheme(const Theme &theme) override;
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void reload();
  void onRangeChanged();
  void onActionChanged(int row, GitRebaseAction action);
  void onMoveUp();
  void onMoveDown();
  void onAutosquash();
  void onStartRebase();
  void onContinue();
  void onSkip();
  void onAbort();

private:
  void buildUi();
  void buildRangeBar(QVBoxLayout *layout);
  void buildTimeline(QVBoxLayout *layout);
  void buildPreview(QVBoxLayout *layout);
  void buildFooter(QVBoxLayout *layout);
  void rebuildTimeline();
  void rebuildPreview();
  void updateProgressUi();
  int currentRow() const;

  GitIntegration *m_git;
  GitRebasePlan m_plan;
  QString m_onto;
  bool m_rebaseRunning;
  bool m_updating;

  QSpinBox *m_countSpin;
  QLabel *m_ontoLabel;
  QLabel *m_publishedLabel;
  QTreeWidget *m_timelineTree;
  QListWidget *m_previewList;
  QListWidget *m_problemList;
  QLabel *m_progressLabel;

  QPushButton *m_upButton;
  QPushButton *m_downButton;
  QPushButton *m_autosquashButton;
  QPushButton *m_startButton;
  QPushButton *m_continueButton;
  QPushButton *m_skipButton;
  QPushButton *m_abortButton;
  QPushButton *m_closeButton;
};

#endif
