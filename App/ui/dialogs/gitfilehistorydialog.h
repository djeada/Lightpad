#ifndef GITFILEHISTORYDIALOG_H
#define GITFILEHISTORYDIALOG_H

#include <QDialog>

class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QLabel;
class QSplitter;
class GitIntegration;
struct GitCommitInfo;

/**
 * @brief Dialog showing commit history for a specific file
 *
 * GitLens-style file history view showing all commits that modified
 * a given file, with commit details and ability to open diffs.
 */
class GitFileHistoryDialog : public QDialog {
  Q_OBJECT

public:
  explicit GitFileHistoryDialog(GitIntegration *git, const QString &filePath,
                                QWidget *parent = nullptr);

signals:
  void viewCommitDiff(const QString &commitHash);
  void openFileAtRevision(const QString &filePath, const QString &commitHash);

private slots:
  void onCommitSelected(QTreeWidgetItem *current, QTreeWidgetItem *previous);
  void onCommitDoubleClicked(QTreeWidgetItem *item, int column);

private:
  void loadHistory();
  void showCommitDetails(const GitCommitInfo &info);

  GitIntegration *m_git;
  QString m_filePath;
  QTreeWidget *m_commitTree;
  QTextEdit *m_detailView;
  QLabel *m_titleLabel;
};

#endif // GITFILEHISTORYDIALOG_H
