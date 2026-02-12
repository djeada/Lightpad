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

/**
 * @brief A single entry in the interactive rebase todo list
 */
struct RebaseEntry {
  QString action; ///< pick, reword, edit, squash, fixup, drop
  QString hash;
  QString subject;
};

/**
 * @brief Interactive rebase UI dialog
 *
 * Lets users reorder, squash, fixup, edit, and drop commits
 * in an interactive rebase workflow.
 */
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

#endif // GITREBASEDIALOG_H
