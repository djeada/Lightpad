#ifndef GITREBASEDIALOG_H
#define GITREBASEDIALOG_H

#include "../../settings/theme.h"
#include <QDialog>

class GitIntegration;
class QTreeWidget;
class QTreeWidgetItem;
class QComboBox;
class QLabel;
class QPushButton;
class QLineEdit;

struct RebaseEntry {
  QString action;
  QString hash;
  QString subject;
  QString author;
};

class GitRebaseDialog : public QDialog {
  Q_OBJECT

public:
  explicit GitRebaseDialog(GitIntegration *git, const Theme &theme,
                           QWidget *parent = nullptr);

  void loadCommits(const QString &upstream = "HEAD~10");

private slots:
  void onMoveUp();
  void onMoveDown();
  void onActionChanged(int index);
  void onStartRebase();
  void onSquashSelected();
  void onDropSelected();
  void onPickAll();
  void onSearchChanged(const QString &text);

private:
  void buildUi();
  void applyTheme(const Theme &theme);
  void updateActionForItem(QTreeWidgetItem *item, const QString &action);
  void rebuildComboForItem(QTreeWidgetItem *item, int row);
  void syncEntriesFromTree();
  void updateSummary();

  GitIntegration *m_git;
  Theme m_theme;
  QString m_upstream;

  QTreeWidget *m_commitList;
  QPushButton *m_moveUpBtn;
  QPushButton *m_moveDownBtn;
  QPushButton *m_squashBtn;
  QPushButton *m_dropBtn;
  QPushButton *m_pickAllBtn;
  QPushButton *m_startBtn;
  QPushButton *m_cancelBtn;
  QLabel *m_statusLabel;
  QLabel *m_summaryLabel;
  QLineEdit *m_searchEdit;

  QList<RebaseEntry> m_entries;
};

#endif
