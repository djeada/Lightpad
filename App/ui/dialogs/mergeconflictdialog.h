#ifndef MERGECONFLICTDIALOG_H
#define MERGECONFLICTDIALOG_H

#include "../../git/gitintegration.h"
#include "../../settings/theme.h"
#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>

class MergeConflictDialog : public QDialog {
  Q_OBJECT

public:
  explicit MergeConflictDialog(GitIntegration *git, QWidget *parent = nullptr);
  ~MergeConflictDialog();

  void setConflictedFiles(const QStringList &files);

  void refresh();

signals:

  void openFileRequested(const QString &filePath);

  void allConflictsResolved();

private slots:
  void onFileSelected(QListWidgetItem *item);
  void onAcceptOursClicked();
  void onAcceptTheirsClicked();
  void onOpenInEditorClicked();
  void onMarkResolvedClicked();
  void onAbortMergeClicked();
  void onContinueMergeClicked();

private:
  void setupUI();
  void applyStyles();
  void updateConflictPreview(const QString &filePath);
  void updateButtons();

public:
  void applyTheme(const Theme &theme);

private:
  GitIntegration *m_git;
  QListWidget *m_fileList;
  QTextEdit *m_oursPreview;
  QTextEdit *m_theirsPreview;
  QLabel *m_statusLabel;
  QLabel *m_conflictCountLabel;
  QPushButton *m_acceptOursButton;
  QPushButton *m_acceptTheirsButton;
  QPushButton *m_openEditorButton;
  QPushButton *m_markResolvedButton;
  QPushButton *m_abortButton;
  QPushButton *m_continueButton;
  QString m_currentFile;
};

#endif
