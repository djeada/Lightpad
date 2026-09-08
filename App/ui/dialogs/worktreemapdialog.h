#ifndef WORKTREEMAPDIALOG_H
#define WORKTREEMAPDIALOG_H

#include "../../git/gitworktreemap.h"
#include "styleddialog.h"

class GitIntegration;
class QLabel;
class QListWidget;
class QPushButton;
class QTreeWidget;
class QVBoxLayout;

class WorktreeMapDialog : public StyledDialog {
  Q_OBJECT

public:
  WorktreeMapDialog(GitIntegration *git, const Theme &theme,
                    QWidget *parent = nullptr);

  const QList<GitWorktreeCard> &cards() const { return m_cards; }
  int currentCardIndex() const;

signals:
  void repositoryChanged();
  void openWorktreeRequested(const QString &path);
  void showBranchInGraphRequested(const QString &branchName);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void reload();
  void onSelectionChanged();
  void onCreate();
  void onOpen();
  void onRemove();
  void onPrune();
  void onShowInGraph();

private:
  void buildUi();
  const GitWorktreeCard *currentCard() const;

  GitIntegration *m_git;
  QList<GitWorktreeCard> m_cards;

  QTreeWidget *m_worktreeTree;
  QLabel *m_detailLabel;
  QLabel *m_warningLabel;

  QPushButton *m_createButton;
  QPushButton *m_openButton;
  QPushButton *m_graphButton;
  QPushButton *m_pruneButton;
  QPushButton *m_removeButton;
  QPushButton *m_closeButton;
};

#endif
