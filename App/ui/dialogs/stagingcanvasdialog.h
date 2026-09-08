#ifndef STAGINGCANVASDIALOG_H
#define STAGINGCANVASDIALOG_H

#include "../../git/gitdiffmodel.h"
#include "../../git/gitintegration.h"
#include "styleddialog.h"
#include <QSet>

class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSplitter;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class StagingCanvasDialog : public StyledDialog {
  Q_OBJECT

public:
  enum class ComparePair {
    WorkingIndex,
    IndexHead,
    WorkingHead,
  };
  Q_ENUM(ComparePair)

  StagingCanvasDialog(GitIntegration *git, const Theme &theme,
                      QWidget *parent = nullptr);

  void selectFile(const QString &filePath);

  void reload();

signals:
  void fileOpenRequested(const QString &filePath);
  void repositoryChanged();

protected:
  void applyTheme(const Theme &theme) override;
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void onFileSelectionChanged();
  void onComparePairChanged(int index);
  void onColumnSelectionChanged();
  void onStageClicked();
  void onUnstageClicked();
  void onDiscardClicked();
  void onAmendClicked();
  void onWorkingContextMenu(const QPoint &pos);
  void onIndexContextMenu(const QPoint &pos);

private:
  enum class Granularity { Selection, Hunk, File, Nothing };

  struct ColumnTarget {
    Granularity granularity = Granularity::Nothing;

    QList<QPair<int, QSet<int>>> lines;
    int hunkIndex = -1;
    int lineCount = 0;
  };

  void buildUi();
  void buildFileList(QWidget *parent);
  void buildColumns(QWidget *parent);
  void buildActionBar(QVBoxLayout *layout);

  void reloadFiles();
  void reloadColumns();
  void populateDiffColumn(QListWidget *list, const GitDiffFile &diff,
                          const QString &emptyText);
  void populateHeadColumn();
  void updateActionState();
  void flashColumn(QListWidget *list);

  ColumnTarget targetFor(QListWidget *list, const GitDiffFile &diff) const;
  QString describeTarget(const ColumnTarget &target, const QString &verb) const;
  QString patchFor(const ColumnTarget &target, const GitDiffFile &diff,
                   bool reverse) const;

  bool applyAndRefresh(const QString &patch, bool cached, bool reverse);
  bool confirmDestructive(const QString &title, const QString &body,
                          const QString &detail);

  QString currentPath() const;
  bool stagingEnabled() const;

  GitIntegration *m_git;

  QTreeWidget *m_fileTree;
  QComboBox *m_compareSelector;
  QSplitter *m_columnSplitter;

  QWidget *m_workingColumn;
  QLabel *m_workingHeader;
  QListWidget *m_workingList;

  QWidget *m_indexColumn;
  QLabel *m_indexHeader;
  QListWidget *m_indexList;

  QWidget *m_headColumn;
  QLabel *m_headHeader;
  QListWidget *m_headList;

  QPushButton *m_stageButton;
  QPushButton *m_unstageButton;
  QPushButton *m_discardButton;
  QPushButton *m_restoreButton;
  QPushButton *m_amendButton;
  QPushButton *m_refreshButton;
  QPushButton *m_closeButton;
  QLabel *m_statusLabel;

  GitDiffFile m_workingDiff;
  GitDiffFile m_indexDiff;
  QString m_selectedPath;
  ComparePair m_comparePair;
  bool m_reloading;
  QTimer *m_flashTimer;
  QListWidget *m_flashTarget;
};

#endif
