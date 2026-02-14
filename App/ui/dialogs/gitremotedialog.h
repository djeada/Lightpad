#ifndef GITREMOTEDIALOG_H
#define GITREMOTEDIALOG_H

#include "../../git/gitintegration.h"
#include "../../settings/theme.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

class GitRemoteDialog : public QDialog {
  Q_OBJECT

public:
  enum class Mode { Push, Pull, Fetch, ManageRemotes };

  explicit GitRemoteDialog(GitIntegration *git, Mode mode = Mode::Push,
                           QWidget *parent = nullptr);
  ~GitRemoteDialog();

  void refresh();

signals:

  void operationCompleted(const QString &message);

private slots:
  void onPushClicked();
  void onPullClicked();
  void onFetchClicked();
  void onAddRemoteClicked();
  void onRemoveRemoteClicked();
  void onRemoteSelected(int index);
  void onCloseClicked();

private:
  void setupUI();
  void applyStyles();

public:
  void applyTheme(const Theme &theme);

private:
  void updateBranchList();
  void updateRemoteList();

  GitIntegration *m_git;
  Mode m_mode;

  QComboBox *m_remoteSelector;
  QComboBox *m_branchSelector;
  QCheckBox *m_setUpstreamCheckbox;
  QCheckBox *m_forceCheckbox;
  QPushButton *m_pushButton;
  QPushButton *m_pullButton;
  QPushButton *m_fetchButton;
  QProgressBar *m_progressBar;
  QLabel *m_statusLabel;

  QListWidget *m_remoteList;
  QLineEdit *m_remoteNameEdit;
  QLineEdit *m_remoteUrlEdit;
  QPushButton *m_addRemoteButton;
  QPushButton *m_removeRemoteButton;

  QPushButton *m_closeButton;
};

#endif
