#ifndef PROVENANCELENSDIALOG_H
#define PROVENANCELENSDIALOG_H

#include "../../git/gitprovenance.h"
#include "styleddialog.h"

class GitIntegration;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QVBoxLayout;

class ProvenanceLensDialog : public StyledDialog {
  Q_OBJECT

public:
  ProvenanceLensDialog(GitIntegration *git, const QString &filePath,
                       int startLine, int endLine, const Theme &theme,
                       QWidget *parent = nullptr);

  const GitLineProvenance &provenance() const { return m_provenance; }
  QString selectedCommitHash() const;

signals:
  void openCommitRequested(const QString &hash);
  void openParentDiffRequested(const QString &hash);
  void openFileHistoryRequested(const QString &filePath, int startLine,
                                int endLine);
  void showInGraphRequested(const QString &hash);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void onSelectionChanged();
  void onOpenCommit();
  void onOpenParentDiff();
  void onOpenFileHistory();
  void onShowInGraph();

private:
  void buildUi();

  GitIntegration *m_git;
  GitLineProvenance m_provenance;

  QLabel *m_headerLabel;
  QLabel *m_latestLabel;
  QLabel *m_churnLabel;
  QLabel *m_caveatLabel;
  QTreeWidget *m_stepsTree;
  QPlainTextEdit *m_previousView;
  QPlainTextEdit *m_currentView;

  QPushButton *m_openCommitButton;
  QPushButton *m_parentDiffButton;
  QPushButton *m_fileHistoryButton;
  QPushButton *m_graphButton;
  QPushButton *m_closeButton;
};

#endif
