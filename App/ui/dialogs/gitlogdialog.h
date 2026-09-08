#ifndef GITLOGDIALOG_H
#define GITLOGDIALOG_H

#include "../../git/gitintegration.h"
#include "styleddialog.h"

class QVBoxLayout;

class GitIntegration;
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QSplitter;
class QLabel;
class QLineEdit;
class QTabWidget;
class GitGraphWidget;
class QCheckBox;
class QPushButton;
class QToolButton;

class GitLogDialog : public StyledDialog {
  Q_OBJECT

public:
  explicit GitLogDialog(GitIntegration *git, const Theme &theme,
                        QWidget *parent = nullptr);

  void setFilePath(const QString &filePath);

  void refresh();

  void selectCommit(const QString &hash);

signals:

  void viewCommitDiff(const QString &commitHash);

  void compareRequested(const QString &from, const QString &to);

  void workingTreeRequested();

  void undoCommitRequested(const QString &hash);

  void timeTravelRequested(const QString &hash);

  void bisectRequested(const QString &goodRef, const QString &badRef);

  void repositoryChanged();

protected:
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void onCommitSelected(QTreeWidgetItem *current, QTreeWidgetItem *previous);
  void onGraphCommitSelected(const QString &hash);
  void onSearchChanged(const QString &text);
  void onGraphContextMenu(const QString &hash, const QPoint &globalPos);
  void onScopeChanged();
  void onWorkingTreeSelected();
  void onCompareAnchorChanged(const QString &hash);

private:
  void buildUi();
  void applyTheme(const Theme &theme) override;
  void loadCommits();
  void showContextMenuForCommit(const QString &hash, const QPoint &pos);
  void showCommitDetails(const QString &hash);
  void selectInList(const QString &hash, bool reveal = true);
  void buildControlBar(QVBoxLayout *layout);
  void buildDetailActions(QWidget *parent, QVBoxLayout *layout);
  void showWorkingTreeDetails();
  void updateDetailActions(bool commitSelected);
  GitLogOptions currentLogOptions() const;
  void createBranchAt(const QString &hash);
  void createTagAt(const QString &hash);
  void resetTo(const QString &hash);

  GitIntegration *m_git;
  QString m_filePath;
  Theme m_theme;

  QLineEdit *m_searchField;
  QTabWidget *m_tabWidget;
  QSplitter *m_splitter;
  QTreeWidget *m_commitTree;
  QTextEdit *m_detailView;
  GitGraphWidget *m_graphWidget;
  QLabel *m_statusLabel;
  QLabel *m_compareLabel;
  QCheckBox *m_firstParentCheck;
  QCheckBox *m_allBranchesCheck;
  QToolButton *m_zoomInButton;
  QToolButton *m_zoomOutButton;
  QToolButton *m_zoomResetButton;
  QTreeWidget *m_detailFiles;
  QPushButton *m_detailDiffButton;
  QPushButton *m_detailBranchButton;
  QPushButton *m_detailTagButton;
  QPushButton *m_detailCherryPickButton;
  QPushButton *m_detailRevertButton;
  QPushButton *m_detailResetButton;
  QPushButton *m_detailCompareHeadButton;
  QString m_selectedHash;
  bool m_syncingSelection = false;
  bool m_updatingScope = false;
};

#endif
