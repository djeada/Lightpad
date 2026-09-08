#ifndef STASHSHELFDIALOG_H
#define STASHSHELFDIALOG_H

#include "../../git/gitstashshelf.h"
#include "styleddialog.h"

class GitIntegration;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTreeWidget;
class QVBoxLayout;

class StashShelfDialog : public StyledDialog {
  Q_OBJECT

public:
  StashShelfDialog(GitIntegration *git, const Theme &theme,
                   QWidget *parent = nullptr);

  const QList<GitStashCard> &cards() const { return m_cards; }
  int currentCardIndex() const;

signals:
  void repositoryChanged();
  void showBaseInGraphRequested(const QString &commitHash);
  void compareRequested(const QString &fromRef, const QString &toRef);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void reload();
  void onSearchChanged(const QString &text);
  void onCardSelected();
  void onApply();
  void onPop();
  void onDrop();
  void onRestoreSelectedFiles();
  void onBranchFromStash();
  void onShowBase();
  void onCompareWithWorkingTree();
  void onNewStash();

private:
  void buildUi();
  const GitStashCard *currentCard() const;
  QStringList selectedFilePaths() const;

  GitIntegration *m_git;
  QList<GitStashCard> m_cards;

  QLineEdit *m_searchEdit;
  QListWidget *m_shelfList;
  QLabel *m_detailLabel;
  QLabel *m_conflictLabel;
  QLabel *m_applyExplanation;
  QTreeWidget *m_fileTree;

  QPushButton *m_applyButton;
  QPushButton *m_popButton;
  QPushButton *m_dropButton;
  QPushButton *m_restoreFilesButton;
  QPushButton *m_branchButton;
  QPushButton *m_baseButton;
  QPushButton *m_compareButton;
  QPushButton *m_newStashButton;
  QPushButton *m_closeButton;
};

#endif
