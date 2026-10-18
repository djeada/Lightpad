#ifndef GITINITDIALOG_H
#define GITINITDIALOG_H

#include "gitdialog.h"
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

class GitInitDialog : public GitDialog {
  Q_OBJECT

public:
  explicit GitInitDialog(const QString &projectPath, QWidget *parent = nullptr);
  ~GitInitDialog();

  QString repositoryPath() const;

  bool createInitialCommit() const;

  bool addGitIgnore() const;

  QString remoteUrl() const;

signals:

  void initializeRequested(const QString &path);

private slots:
  void onInitClicked();
  void onCancelClicked();
  void onBrowseClicked();

private:
  void setupUI();
  void applyStyles();

public:
  void applyTheme(const Theme &theme) override;

private:
  QString m_projectPath;
  QLineEdit *m_pathEdit;
  QPushButton *m_browseButton;
  QCheckBox *m_initialCommitCheckbox;
  QCheckBox *m_gitIgnoreCheckbox;
  QLineEdit *m_remoteEdit;
  QPushButton *m_initButton;
  QPushButton *m_cancelButton;
  QLabel *m_titleLabel = nullptr;
  QLabel *m_subtitleLabel = nullptr;
};

#endif
