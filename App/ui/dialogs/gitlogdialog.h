#ifndef GITLOGDIALOG_H
#define GITLOGDIALOG_H

#include "../../settings/theme.h"
#include <QDialog>

class GitIntegration;
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QSplitter;
class QLabel;
class QLineEdit;
class QTabWidget;
class GitGraphWidget;

class GitLogDialog : public QDialog {
  Q_OBJECT

public:
  explicit GitLogDialog(GitIntegration *git, const Theme &theme,
                        QWidget *parent = nullptr);

  void setFilePath(const QString &filePath);

  void refresh();

signals:

  void viewCommitDiff(const QString &commitHash);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void onCommitSelected(QTreeWidgetItem *current, QTreeWidgetItem *previous);
  void onGraphCommitSelected(const QString &hash);
  void onSearchChanged(const QString &text);

private:
  void buildUi();
  void applyTheme(const Theme &theme);
  void loadCommits();
  void showContextMenuForCommit(const QString &hash, const QPoint &pos);

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
};

#endif
