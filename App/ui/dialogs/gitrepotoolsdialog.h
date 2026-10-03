#ifndef GITREPOTOOLSDIALOG_H
#define GITREPOTOOLSDIALOG_H

#include "../../git/gitintegration.h"
#include "styleddialog.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTableWidget;

class GitRepoToolsDialog : public StyledDialog {
  Q_OBJECT

public:
  enum class Tab { Remotes, Tags, Submodules, Clean, Sequencer };

  explicit GitRepoToolsDialog(GitIntegration *git, QWidget *parent = nullptr);

  void showTab(Tab tab);
  void refreshAll();

signals:
  void repositoryChanged();

private:
  QWidget *buildRemotesTab();
  QWidget *buildTagsTab();
  QWidget *buildSubmodulesTab();
  QWidget *buildCleanTab();
  QWidget *buildSequencerTab();

  void refreshRemotes();
  void refreshTags();
  void refreshSubmodules();
  void refreshClean();
  void refreshSequencer();
  void report(bool ok, const QString &message);
  QString selectedRemote() const;
  QString selectedTag() const;

  GitIntegration *m_git;
  QTabWidget *m_tabs = nullptr;
  QLabel *m_status = nullptr;

  QTableWidget *m_remoteTable = nullptr;
  QTableWidget *m_tagTable = nullptr;
  QComboBox *m_tagRemote = nullptr;
  QTableWidget *m_submoduleTable = nullptr;
  QLabel *m_submoduleSummary = nullptr;
  QListWidget *m_cleanList = nullptr;
  QCheckBox *m_cleanIgnored = nullptr;
  QLabel *m_sequencerLabel = nullptr;
  QPushButton *m_seqContinue = nullptr;
  QPushButton *m_seqSkip = nullptr;
  QPushButton *m_seqAbort = nullptr;
  QLineEdit *m_autosquashBase = nullptr;
  QLabel *m_fixupLabel = nullptr;
};

#endif
