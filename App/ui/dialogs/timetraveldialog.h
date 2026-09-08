#ifndef TIMETRAVELDIALOG_H
#define TIMETRAVELDIALOG_H

#include "../../git/gittimetravel.h"
#include "styleddialog.h"
#include <QMap>

class GitIntegration;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QVBoxLayout;

class TimeTravelDialog : public StyledDialog {
  Q_OBJECT

public:
  TimeTravelDialog(GitIntegration *git, const QString &commitHash,
                   const Theme &theme, QWidget *parent = nullptr);

  GitTimeTravelMode selectedMode() const;
  const QList<GitTimeTravelOption> &options() const { return m_options; }

signals:
  void inspectSnapshotRequested(const QString &commitHash);
  void worktreeOpened(const QString &path, const QString &commitHash);
  void repositoryChanged();
  void showInGraphRequested(const QString &commitHash);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void onModeChanged();
  void onExecute();

private:
  void buildUi();
  const GitTimeTravelOption *optionFor(GitTimeTravelMode mode) const;
  void updateSelectedOption();

  GitIntegration *m_git;
  QString m_commitHash;
  GitCommitInfo m_commit;
  QList<GitTimeTravelOption> m_options;

  QLabel *m_headerLabel;
  QMap<GitTimeTravelMode, QRadioButton *> m_radios;
  QMap<GitTimeTravelMode, QLabel *> m_explanations;
  QLabel *m_checkoutLabel;
  QLabel *m_commandLabel;
  QLineEdit *m_branchNameEdit;
  QLineEdit *m_worktreePathEdit;
  QPushButton *m_graphButton;
  QPushButton *m_executeButton;
  QPushButton *m_cancelButton;
};

#endif
