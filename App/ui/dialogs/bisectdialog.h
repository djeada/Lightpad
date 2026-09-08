#ifndef BISECTDIALOG_H
#define BISECTDIALOG_H

#include "../../git/gitbisect.h"
#include "styleddialog.h"

class GitIntegration;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

class BisectDialog : public StyledDialog {
  Q_OBJECT

public:
  BisectDialog(GitIntegration *git, const Theme &theme,
               QWidget *parent = nullptr);

  void setEndpoints(const QString &goodRef, const QString &badRef);

  const GitBisectState &state() const { return m_state; }

signals:
  void repositoryChanged();
  void inspectCommitRequested(const QString &hash);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void reload();
  void onStart();
  void onGood();
  void onBad();
  void onSkip();
  void onRunAutomated();
  void onReset();
  void onInspectSuspect();

private:
  void buildUi();
  void updateStartEnabled();
  void applyOutput(const QString &output);

  GitIntegration *m_git;
  GitBisectState m_state;

  QComboBox *m_goodCombo;
  QComboBox *m_badCombo;
  QLineEdit *m_commandEdit;
  QLabel *m_statusLabel;
  QLabel *m_guidanceLabel;
  QLabel *m_progressLabel;
  QPlainTextEdit *m_outputView;
  QListWidget *m_logList;

  QPushButton *m_startButton;
  QPushButton *m_goodButton;
  QPushButton *m_badButton;
  QPushButton *m_skipButton;
  QPushButton *m_runButton;
  QPushButton *m_inspectButton;
  QPushButton *m_resetButton;
  QPushButton *m_closeButton;
};

#endif
