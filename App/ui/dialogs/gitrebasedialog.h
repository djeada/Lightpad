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

struct RebaseEntry {
  QString action;
  QString hash;
  QString subject;
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

private:
  void buildUi();
  void applyTheme(const Theme &theme);
  void updateActionForItem(QTreeWidgetItem *item, const QString &action);

  GitIntegration *m_git;
  Theme m_theme;
  QString m_upstream;

  QTreeWidget *m_commitList;
  QPushButton *m_moveUpBtn;
  QPushButton *m_moveDownBtn;
  QPushButton *m_startBtn;
  QPushButton *m_cancelBtn;
  QLabel *m_statusLabel;

  QList<RebaseEntry> m_entries;
};

#endif
