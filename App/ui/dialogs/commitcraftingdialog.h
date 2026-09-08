#ifndef COMMITCRAFTINGDIALOG_H
#define COMMITCRAFTINGDIALOG_H

#include "../../git/commitplan.h"
#include "../../git/gitintegration.h"
#include "styleddialog.h"
#include <QColor>
#include <QMap>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class CommitCraftingDialog : public StyledDialog {
  Q_OBJECT

public:
  CommitCraftingDialog(GitIntegration *git, const Theme &theme,
                       QWidget *parent = nullptr);

  const CommitPlan &plan() const { return m_plan; }
  QList<ReviewPrompt> reviewPrompts() const { return m_prompts; }

signals:
  void repositoryChanged();

protected:
  void applyTheme(const Theme &theme) override;
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void reload();
  void onAddBucket();
  void onRemoveBucket();
  void onAssign();
  void onReturnToUnassigned();
  void onBucketSelectionChanged();
  void onBucketNameEdited(const QString &name);
  void onMessageEdited();
  void onCommitBucket();

private:
  void buildUi();
  void buildColumns(QVBoxLayout *layout);
  void buildBucketEditor(QVBoxLayout *layout);
  void refreshTrees();
  void refreshPrompts();
  void stylePromptItems();
  void updateBucketEditor();
  int currentBucketIndex() const;
  QList<CommitChangeRef> selectedUnassigned() const;
  QList<CommitChangeRef> selectedAssigned() const;
  QString patchForBucket(const CommitBucket &bucket, const QString &filePath,
                         const QList<int> &hunkIndices) const;

  GitIntegration *m_git;
  CommitPlan m_plan;

  QMap<QString, GitDiffFile> m_diffs;
  QList<ReviewPrompt> m_prompts;

  QLabel *m_headerLabel;
  QLabel *m_unassignedLabel;
  QLabel *m_bucketStatsLabel;
  QTreeWidget *m_unassignedTree;
  QTreeWidget *m_bucketTree;
  QListWidget *m_promptList;
  QColor m_promptWarningColor;
  QColor m_promptNormalColor;

  QLineEdit *m_bucketNameEdit;
  QTextEdit *m_messageEdit;

  QPushButton *m_addBucketButton;
  QPushButton *m_removeBucketButton;
  QPushButton *m_assignButton;
  QPushButton *m_returnButton;
  QPushButton *m_commitButton;
  QPushButton *m_refreshButton;
  QPushButton *m_closeButton;

  bool m_updating;
};

#endif
