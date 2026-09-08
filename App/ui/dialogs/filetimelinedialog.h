#ifndef FILETIMELINEDIALOG_H
#define FILETIMELINEDIALOG_H

#include "../../git/gitintegration.h"
#include "styleddialog.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class FileTimelineDialog : public StyledDialog {
  Q_OBJECT

public:
  FileTimelineDialog(GitIntegration *git, const QString &filePath,
                     const Theme &theme, QWidget *parent = nullptr);

  void setLineRange(int startLine, int endLine);

  int revisionCount() const { return m_revisions.size(); }
  QList<GitFileRevision> revisions() const { return m_revisions; }

signals:
  void openCommitRequested(const QString &hash);
  void compareRequested(const QString &fromRef, const QString &toRef,
                        const QString &filePath);
  void showInGraphRequested(const QString &hash, const QString &filePath);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void reload();
  void loadMore();
  void onSelectionChanged();
  void onCompareToCurrent();
  void onCompareSelected();
  void onOpenCommit();
  void onShowInGraph();

private:
  void buildUi();
  void buildFilterBar(QVBoxLayout *layout);
  void buildBody(QVBoxLayout *layout);
  void buildActions(QVBoxLayout *layout);
  void rebuildTimeline();
  void showContentFor(const GitFileRevision &revision);
  void showDiffBetween(const GitFileRevision &older,
                       const GitFileRevision &newer);
  void renderDiff(const QString &diffText);
  GitFileTimelineOptions currentOptions() const;
  QList<GitFileRevision> selectedRevisions() const;

  GitIntegration *m_git;
  QString m_filePath;
  QList<GitFileRevision> m_revisions;
  bool m_hasWorkingTreePoint;
  bool m_exhausted;
  int m_lineRangeStart;
  int m_lineRangeEnd;

  QLabel *m_headerLabel;
  QCheckBox *m_followRenamesCheck;
  QCheckBox *m_allBranchesCheck;
  QCheckBox *m_firstParentCheck;
  QCheckBox *m_lineRangeCheck;
  QLineEdit *m_authorEdit;
  QLineEdit *m_sinceEdit;
  QLineEdit *m_untilEdit;

  QTreeWidget *m_timelineTree;
  QPushButton *m_loadMoreButton;

  QLabel *m_previewHeader;
  QPlainTextEdit *m_contentView;
  QListWidget *m_diffView;

  QPushButton *m_compareCurrentButton;
  QPushButton *m_compareSelectedButton;
  QPushButton *m_openCommitButton;
  QPushButton *m_showInGraphButton;
  QPushButton *m_refreshButton;
  QPushButton *m_closeButton;
};

#endif
