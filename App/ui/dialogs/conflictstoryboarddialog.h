#ifndef CONFLICTSTORYBOARDDIALOG_H
#define CONFLICTSTORYBOARDDIALOG_H

#include "../../git/gitconflictmodel.h"
#include "styleddialog.h"

class GitIntegration;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QVBoxLayout;

class ConflictStoryboardDialog : public StyledDialog {
  Q_OBJECT

public:
  ConflictStoryboardDialog(GitIntegration *git, const Theme &theme,
                           QWidget *parent = nullptr);

  const GitConflictContext &context() const { return m_context; }
  int currentFileIndex() const;

signals:
  void repositoryChanged();
  void fileOpenRequested(const QString &filePath);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void reload();
  void onFileSelected();
  void onTakeOurs();
  void onTakeTheirs();
  void onTakeBoth();
  void onMarkResolved();
  void onDiffAgainstOurs();
  void onDiffAgainstTheirs();
  void onContinueOperation();
  void onAbortOperation();

private:
  void buildUi();
  void buildIdentityHeader(QVBoxLayout *layout);
  void buildBody(QVBoxLayout *layout);
  void buildActions(QVBoxLayout *layout);
  void showFile(int index);
  void updateProgress();
  void showDiff(const QString &title, const QString &other);
  const GitConflictFile *currentFile() const;

  GitIntegration *m_git;
  GitConflictContext m_context;

  QLabel *m_operationLabel;
  QLabel *m_oursLabel;
  QLabel *m_theirsLabel;
  QLabel *m_baseLabel;
  QLabel *m_progressLabel;
  QLabel *m_classLabel;
  QLabel *m_classExplanation;

  QListWidget *m_fileList;
  QPlainTextEdit *m_baseView;
  QPlainTextEdit *m_oursView;
  QPlainTextEdit *m_theirsView;
  QPlainTextEdit *m_resultView;
  QTreeWidget *m_historyTree;

  QPushButton *m_takeOursButton;
  QPushButton *m_takeTheirsButton;
  QPushButton *m_takeBothButton;
  QPushButton *m_diffOursButton;
  QPushButton *m_diffTheirsButton;
  QPushButton *m_resolveButton;
  QPushButton *m_continueButton;
  QPushButton *m_abortButton;
  QPushButton *m_closeButton;
};

#endif
