#include "stagingcanvasdialog.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "themedmessagebox.h"
#include <QComboBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int HUNK_INDEX_ROLE = Qt::UserRole + 1;
constexpr int LINE_INDEX_ROLE = Qt::UserRole + 2;
constexpr int IS_HEADER_ROLE = Qt::UserRole + 3;
constexpr int PATH_ROLE = Qt::UserRole + 4;

constexpr int FLASH_DURATION_MS = 220;

QString statusLetter(GitFileStatus status) {
  switch (status) {
  case GitFileStatus::Untracked:
    return QStringLiteral("?");
  case GitFileStatus::Modified:
    return QStringLiteral("M");
  case GitFileStatus::Added:
    return QStringLiteral("A");
  case GitFileStatus::Deleted:
    return QStringLiteral("D");
  case GitFileStatus::Renamed:
    return QStringLiteral("R");
  case GitFileStatus::Copied:
    return QStringLiteral("C");
  case GitFileStatus::Unmerged:
    return QStringLiteral("U");
  case GitFileStatus::Ignored:
    return QStringLiteral("!");
  case GitFileStatus::Clean:
    break;
  }
  return QStringLiteral(" ");
}

QString markerFor(GitDiffLineType type) {
  switch (type) {
  case GitDiffLineType::Added:
    return QStringLiteral("+");
  case GitDiffLineType::Removed:
    return QStringLiteral("-");
  case GitDiffLineType::NoNewline:
    return QStringLiteral("\\");
  case GitDiffLineType::Context:
    break;
  }
  return QStringLiteral(" ");
}

} // namespace

StagingCanvasDialog::StagingCanvasDialog(GitIntegration *git,
                                         const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_fileTree(nullptr),
      m_compareSelector(nullptr), m_columnSplitter(nullptr),
      m_workingColumn(nullptr), m_workingHeader(nullptr),
      m_workingList(nullptr), m_indexColumn(nullptr), m_indexHeader(nullptr),
      m_indexList(nullptr), m_headColumn(nullptr), m_headHeader(nullptr),
      m_headList(nullptr), m_stageButton(nullptr), m_unstageButton(nullptr),
      m_discardButton(nullptr), m_restoreButton(nullptr),
      m_amendButton(nullptr), m_refreshButton(nullptr), m_closeButton(nullptr),
      m_statusLabel(nullptr), m_comparePair(ComparePair::WorkingIndex),
      m_reloading(false), m_flashTimer(new QTimer(this)),
      m_flashTarget(nullptr) {
  setWindowTitle(tr("Staging Canvas"));
  setMinimumSize(900, 560);
  resize(1100, 680);

  m_flashTimer->setSingleShot(true);
  m_flashTimer->setInterval(FLASH_DURATION_MS);
  connect(m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashTarget = nullptr;
    applyTheme(m_theme);
  });

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  reload();
}

void StagingCanvasDialog::buildUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                                 UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  mainLayout->setSpacing(UiMetrics::SpaceMd);

  QHBoxLayout *topBar = new QHBoxLayout();
  topBar->setSpacing(UiMetrics::ToolbarSpacing);

  QLabel *title = new QLabel(tr("Staging Canvas"), this);
  title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  styleTitleLabel(title);
  topBar->addWidget(title);

  topBar->addStretch();

  QLabel *compareLabel = new QLabel(tr("Compare:"), this);
  compareLabel->setObjectName(QStringLiteral("stagingCompareLabel"));
  topBar->addWidget(compareLabel);

  m_compareSelector = new QComboBox(this);
  m_compareSelector->setObjectName(QStringLiteral("stagingCompareSelector"));
  m_compareSelector->addItem(tr("Working ↔ Index"),
                             static_cast<int>(ComparePair::WorkingIndex));
  m_compareSelector->addItem(tr("Index ↔ HEAD"),
                             static_cast<int>(ComparePair::IndexHead));
  m_compareSelector->addItem(tr("Working ↔ HEAD"),
                             static_cast<int>(ComparePair::WorkingHead));
  m_compareSelector->setToolTip(
      tr("Which pair of states the canvas is diffing.\n"
         "Working ↔ Index is what you would stage next;\n"
         "Index ↔ HEAD is what the next commit contains;\n"
         "Working ↔ HEAD is everything since the last commit."));
  connect(m_compareSelector,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &StagingCanvasDialog::onComparePairChanged);
  topBar->addWidget(m_compareSelector);

  m_refreshButton = new QPushButton(tr("Refresh"), this);
  m_refreshButton->setObjectName(QStringLiteral("stagingRefreshButton"));
  m_refreshButton->setToolTip(tr("Re-read the repository (F5)"));
  connect(m_refreshButton, &QPushButton::clicked, this,
          &StagingCanvasDialog::reload);
  topBar->addWidget(m_refreshButton);

  mainLayout->addLayout(topBar);

  QSplitter *outerSplitter = new QSplitter(Qt::Horizontal, this);
  buildFileList(outerSplitter);
  buildColumns(outerSplitter);
  outerSplitter->setStretchFactor(0, 0);
  outerSplitter->setStretchFactor(1, 1);
  outerSplitter->setSizes({240, 860});
  mainLayout->addWidget(outerSplitter, 1);

  buildActionBar(mainLayout);
}

void StagingCanvasDialog::buildFileList(QWidget *parent) {
  QWidget *container = new QWidget(parent);
  QVBoxLayout *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(UiMetrics::SpaceSm);

  QLabel *header = new QLabel(tr("Changed files"), container);
  header->setObjectName(QStringLiteral("stagingFilesLabel"));
  styleSubduedLabel(header);
  layout->addWidget(header);

  m_fileTree = new QTreeWidget(container);
  m_fileTree->setObjectName(QStringLiteral("stagingFileTree"));
  m_fileTree->setColumnCount(2);
  m_fileTree->setHeaderLabels({tr("File"), tr("State")});
  m_fileTree->setRootIsDecorated(false);
  m_fileTree->setUniformRowHeights(true);
  m_fileTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_fileTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  connect(m_fileTree, &QTreeWidget::currentItemChanged, this,
          &StagingCanvasDialog::onFileSelectionChanged);
  connect(m_fileTree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem *item, int) {
            if (item) {
              emit fileOpenRequested(item->data(0, PATH_ROLE).toString());
            }
          });
  layout->addWidget(m_fileTree, 1);
}

void StagingCanvasDialog::buildColumns(QWidget *parent) {
  QWidget *container = new QWidget(parent);
  QVBoxLayout *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(UiMetrics::SpaceSm);

  m_columnSplitter = new QSplitter(Qt::Horizontal, container);

  const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

  const auto makeColumn = [&](QWidget **column, QLabel **header,
                              QListWidget **list, const QString &objectName,
                              const QString &titleText, const QString &tip,
                              bool selectable) {
    *column = new QWidget(m_columnSplitter);
    QVBoxLayout *columnLayout = new QVBoxLayout(*column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(UiMetrics::SpaceXs);

    *header = new QLabel(titleText, *column);
    (*header)->setObjectName(objectName + QStringLiteral("Header"));
    (*header)->setToolTip(tip);
    (*header)->setAlignment(Qt::AlignCenter);
    columnLayout->addWidget(*header);

    *list = new QListWidget(*column);
    (*list)->setObjectName(objectName);
    (*list)->setFont(mono);
    (*list)->setUniformItemSizes(true);
    (*list)->setToolTip(tip);
    (*list)->setSelectionMode(selectable ? QAbstractItemView::ExtendedSelection
                                         : QAbstractItemView::NoSelection);
    (*list)->setFocusPolicy(selectable ? Qt::StrongFocus : Qt::NoFocus);
    if (selectable) {
      (*list)->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(*list, &QListWidget::itemSelectionChanged, this,
              &StagingCanvasDialog::onColumnSelectionChanged);
      connect(*list, &QListWidget::currentRowChanged, this,
              &StagingCanvasDialog::onColumnSelectionChanged);
    }
    columnLayout->addWidget(*list, 1);
  };

  makeColumn(&m_workingColumn, &m_workingHeader, &m_workingList,
             QStringLiteral("stagingWorkingColumn"), tr("Working Tree"),
             tr("Your files on disk. What is here but not in the Index will "
                "not be committed.\ngit diff"),
             true);
  makeColumn(&m_indexColumn, &m_indexHeader, &m_indexList,
             QStringLiteral("stagingIndexColumn"), tr("Index (Staged)"),
             tr("Exactly what the next commit will contain. Git commits from "
                "here.\ngit diff --cached"),
             true);
  makeColumn(&m_headColumn, &m_headHeader, &m_headList,
             QStringLiteral("stagingHeadColumn"), tr("HEAD"),
             tr("The last commit, shown read-only. Nothing you do here can "
                "change it.\ngit show HEAD:<file>"),
             false);

  connect(m_workingList, &QListWidget::customContextMenuRequested, this,
          &StagingCanvasDialog::onWorkingContextMenu);
  connect(m_indexList, &QListWidget::customContextMenuRequested, this,
          &StagingCanvasDialog::onIndexContextMenu);

  m_columnSplitter->setSizes({300, 300, 260});
  layout->addWidget(m_columnSplitter, 1);
}

void StagingCanvasDialog::buildActionBar(QVBoxLayout *layout) {
  QHBoxLayout *actions = new QHBoxLayout();
  actions->setSpacing(UiMetrics::ToolbarGroupSpacing);

  m_stageButton = new QPushButton(tr("Stage →"), this);
  m_stageButton->setObjectName(QStringLiteral("stagingStageButton"));
  m_stageButton->setToolTip(
      tr("Move the selection from the Working Tree into the Index "
         "(Ctrl+Right).\nYour files on disk are not touched."));
  connect(m_stageButton, &QPushButton::clicked, this,
          &StagingCanvasDialog::onStageClicked);
  actions->addWidget(m_stageButton);

  m_unstageButton = new QPushButton(tr("← Unstage"), this);
  m_unstageButton->setObjectName(QStringLiteral("stagingUnstageButton"));
  m_unstageButton->setToolTip(
      tr("Take the selection back out of the Index (Ctrl+Left).\n"
         "This is not the same as discarding: your edits stay on disk."));
  connect(m_unstageButton, &QPushButton::clicked, this,
          &StagingCanvasDialog::onUnstageClicked);
  actions->addWidget(m_unstageButton);

  actions->addSpacing(UiMetrics::SpaceLg);

  m_discardButton = new QPushButton(tr("Discard…"), this);
  m_discardButton->setObjectName(QStringLiteral("stagingDiscardButton"));
  m_discardButton->setToolTip(
      tr("Throw away the selected working-tree changes.\n"
         "This cannot be undone by Git — it asks first."));
  connect(m_discardButton, &QPushButton::clicked, this,
          &StagingCanvasDialog::onDiscardClicked);
  actions->addWidget(m_discardButton);

  m_restoreButton = new QPushButton(tr("Restore file from HEAD…"), this);
  m_restoreButton->setObjectName(QStringLiteral("stagingRestoreButton"));
  m_restoreButton->setToolTip(
      tr("Replace the whole file with its committed version.\n"
         "Discards every staged and unstaged change to it."));
  connect(m_restoreButton, &QPushButton::clicked, this, [this]() {
    const QString path = currentPath();
    if (path.isEmpty() || !m_git) {
      return;
    }
    if (!confirmDestructive(
            tr("Restore file from HEAD"),
            tr("Replace <b>%1</b> with its committed version?").arg(path),
            tr("Every staged and unstaged change to this file is lost. "
               "Git cannot bring them back."))) {
      return;
    }
    if (m_git->discardChanges(path)) {
      reload();
      emit repositoryChanged();
    }
  });
  actions->addWidget(m_restoreButton);

  actions->addStretch();

  m_amendButton = new QPushButton(tr("Amend into last commit…"), this);
  m_amendButton->setObjectName(QStringLiteral("stagingAmendButton"));
  m_amendButton->setToolTip(
      tr("Fold the staged changes into the previous commit instead of "
         "creating a new one."));
  connect(m_amendButton, &QPushButton::clicked, this,
          &StagingCanvasDialog::onAmendClicked);
  actions->addWidget(m_amendButton);

  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("stagingCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  actions->addWidget(m_closeButton);

  layout->addLayout(actions);

  m_statusLabel = new QLabel(this);
  m_statusLabel->setObjectName(QStringLiteral("stagingStatusLabel"));
  m_statusLabel->setWordWrap(true);
  layout->addWidget(m_statusLabel);
}

QString StagingCanvasDialog::currentPath() const {
  QTreeWidgetItem *item = m_fileTree ? m_fileTree->currentItem() : nullptr;
  return item ? item->data(0, PATH_ROLE).toString() : QString();
}

bool StagingCanvasDialog::stagingEnabled() const {

  return m_comparePair != ComparePair::WorkingHead;
}

void StagingCanvasDialog::reload() {
  if (!m_git || !m_git->isValidRepository()) {
    return;
  }
  reloadFiles();
  reloadColumns();
}

void StagingCanvasDialog::reloadFiles() {
  const QString previous =
      m_selectedPath.isEmpty() ? currentPath() : m_selectedPath;
  m_reloading = true;
  m_fileTree->clear();

  const QList<GitFileInfo> status = m_git->getStatus();
  QTreeWidgetItem *toSelect = nullptr;

  for (const GitFileInfo &info : status) {
    QTreeWidgetItem *item = new QTreeWidgetItem(m_fileTree);
    item->setText(0, info.filePath);
    item->setData(0, PATH_ROLE, info.filePath);

    const bool staged = info.indexStatus != GitFileStatus::Clean &&
                        info.indexStatus != GitFileStatus::Untracked;
    const bool dirty = info.workTreeStatus != GitFileStatus::Clean;
    QString state;
    if (staged) {
      state += tr("▲%1").arg(statusLetter(info.indexStatus));
    }
    if (dirty) {
      if (!state.isEmpty()) {
        state += QStringLiteral(" ");
      }
      state += tr("●%1").arg(statusLetter(info.workTreeStatus));
    }
    item->setText(1, state);
    item->setToolTip(
        1, tr("▲ staged in the Index%1\n● changed in the Working Tree%2")
               .arg(staged ? QString() : tr(" (no)"),
                    dirty ? QString() : tr(" (no)")));
    item->setToolTip(0, info.filePath);

    if (info.filePath == previous) {
      toSelect = item;
    }
  }

  m_reloading = false;

  if (!toSelect && m_fileTree->topLevelItemCount() > 0) {
    toSelect = m_fileTree->topLevelItem(0);
  }
  if (toSelect) {
    m_fileTree->setCurrentItem(toSelect);
  } else {
    m_selectedPath.clear();
    reloadColumns();
  }
}

void StagingCanvasDialog::selectFile(const QString &filePath) {
  for (int i = 0; i < m_fileTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_fileTree->topLevelItem(i);
    if (item->data(0, PATH_ROLE).toString() == filePath) {
      m_fileTree->setCurrentItem(item);
      return;
    }
  }
}

void StagingCanvasDialog::onFileSelectionChanged() {
  if (m_reloading) {
    return;
  }
  m_selectedPath = currentPath();
  reloadColumns();
}

void StagingCanvasDialog::onComparePairChanged(int index) {
  m_comparePair =
      static_cast<ComparePair>(m_compareSelector->itemData(index).toInt());
  reloadColumns();
}

void StagingCanvasDialog::reloadColumns() {
  const QString path = currentPath();

  m_workingDiff = GitDiffFile();
  m_indexDiff = GitDiffFile();

  if (path.isEmpty() || !m_git) {
    m_workingList->clear();
    m_indexList->clear();
    m_headList->clear();
    updateActionState();
    return;
  }

  const QString workingText = m_comparePair == ComparePair::WorkingHead
                                  ? m_git->getWorkingVsHeadDiff(path)
                                  : m_git->getFileDiff(path, false);
  m_workingDiff = parseUnifiedDiff(workingText);
  m_indexDiff = parseUnifiedDiff(m_git->getFileDiff(path, true));

  m_workingHeader->setText(m_comparePair == ComparePair::WorkingHead
                               ? tr("Working Tree (vs HEAD)")
                               : tr("Working Tree (vs Index)"));
  m_indexHeader->setText(tr("Index (vs HEAD)"));

  populateDiffColumn(m_workingList, m_workingDiff,
                     m_comparePair == ComparePair::WorkingHead
                         ? tr("No changes since the last commit.")
                         : tr("Nothing unstaged — the working tree matches "
                              "the index."));
  populateDiffColumn(m_indexList, m_indexDiff,
                     tr("Nothing staged — the index matches HEAD."));
  populateHeadColumn();
  updateActionState();
}

void StagingCanvasDialog::populateDiffColumn(QListWidget *list,
                                             const GitDiffFile &diff,
                                             const QString &emptyText) {
  list->clear();

  if (diff.isBinary) {
    QListWidgetItem *item = new QListWidgetItem(tr("Binary file — Git cannot "
                                                   "show or split its lines."),
                                                list);
    item->setFlags(Qt::ItemIsEnabled);
    return;
  }

  if (diff.hunks.isEmpty()) {
    QListWidgetItem *item = new QListWidgetItem(emptyText, list);
    item->setFlags(Qt::ItemIsEnabled);
    return;
  }

  for (int hunkIndex = 0; hunkIndex < diff.hunks.size(); ++hunkIndex) {
    const GitHunk &hunk = diff.hunks.at(hunkIndex);

    QListWidgetItem *header =
        new QListWidgetItem(tr("── hunk %1 of %2 · +%3 −%4 ──")
                                .arg(hunkIndex + 1)
                                .arg(diff.hunks.size())
                                .arg(hunk.additions())
                                .arg(hunk.deletions()),
                            list);
    header->setData(HUNK_INDEX_ROLE, hunkIndex);
    header->setData(IS_HEADER_ROLE, true);
    header->setToolTip(hunk.header);

    header->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    for (int lineIndex = 0; lineIndex < hunk.lines.size(); ++lineIndex) {
      const GitDiffLine &line = hunk.lines.at(lineIndex);
      QListWidgetItem *item =
          new QListWidgetItem(markerFor(line.type) + line.text, list);
      item->setData(HUNK_INDEX_ROLE, hunkIndex);
      item->setData(LINE_INDEX_ROLE, lineIndex);
      item->setData(IS_HEADER_ROLE, false);

      if (line.isChange()) {
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      } else {

        item->setFlags(Qt::ItemIsEnabled);
      }
    }
  }
}

void StagingCanvasDialog::populateHeadColumn() {
  m_headList->clear();
  const QString path = currentPath();
  if (path.isEmpty() || !m_git) {
    return;
  }

  const QString content =
      m_git->getFileAtRevision(path, QStringLiteral("HEAD"));
  if (content.isEmpty()) {
    QListWidgetItem *item = new QListWidgetItem(
        tr("Not in HEAD — this file is new since the last commit."),
        m_headList);
    item->setFlags(Qt::ItemIsEnabled);
    return;
  }

  const QStringList lines = content.split(QLatin1Char('\n'));
  for (int i = 0; i < lines.size(); ++i) {
    if (i == lines.size() - 1 && lines.at(i).isEmpty()) {
      break;
    }
    QListWidgetItem *item =
        new QListWidgetItem(QStringLiteral(" ") + lines.at(i), m_headList);
    item->setFlags(Qt::ItemIsEnabled);
  }
}

StagingCanvasDialog::ColumnTarget
StagingCanvasDialog::targetFor(QListWidget *list,
                               const GitDiffFile &diff) const {
  ColumnTarget target;
  if (!list || diff.hunks.isEmpty()) {
    return target;
  }

  QMap<int, QSet<int>> byHunk;
  bool sawHeader = false;
  int headerHunk = -1;

  for (QListWidgetItem *item : list->selectedItems()) {
    const int hunkIndex = item->data(HUNK_INDEX_ROLE).toInt();
    if (item->data(IS_HEADER_ROLE).toBool()) {
      sawHeader = true;
      headerHunk = hunkIndex;
      continue;
    }
    byHunk[hunkIndex].insert(item->data(LINE_INDEX_ROLE).toInt());
  }

  if (!byHunk.isEmpty()) {
    target.granularity = Granularity::Selection;
    for (auto it = byHunk.constBegin(); it != byHunk.constEnd(); ++it) {
      target.lines.append({it.key(), it.value()});
      target.lineCount += it.value().size();
    }
    return target;
  }

  int hunkIndex = headerHunk;
  if (!sawHeader) {
    QListWidgetItem *current = list->currentItem();
    if (current && current->data(HUNK_INDEX_ROLE).isValid()) {
      hunkIndex = current->data(HUNK_INDEX_ROLE).toInt();
    }
  }

  if (hunkIndex >= 0 && hunkIndex < diff.hunks.size()) {
    target.granularity = Granularity::Hunk;
    target.hunkIndex = hunkIndex;
    target.lineCount = diff.hunks.at(hunkIndex).changedLineCount();
    return target;
  }

  target.granularity = Granularity::File;
  target.lineCount = diff.additions() + diff.deletions();
  return target;
}

QString StagingCanvasDialog::describeTarget(const ColumnTarget &target,
                                            const QString &verb) const {
  switch (target.granularity) {
  case Granularity::Nothing:
    return verb;
  case Granularity::Selection:
    return target.lineCount == 1
               ? tr("%1 1 line").arg(verb)
               : tr("%1 %2 lines").arg(verb).arg(target.lineCount);
  case Granularity::Hunk:
    return tr("%1 hunk %2").arg(verb).arg(target.hunkIndex + 1);
  case Granularity::File:
    return tr("%1 file").arg(verb);
  }
  return verb;
}

QString StagingCanvasDialog::patchFor(const ColumnTarget &target,
                                      const GitDiffFile &diff,
                                      bool reverse) const {
  switch (target.granularity) {
  case Granularity::Nothing:
    return QString();
  case Granularity::Selection:
    return buildLinePatchMulti(diff, target.lines, reverse);
  case Granularity::Hunk:
    return buildHunkPatch(diff, {target.hunkIndex});
  case Granularity::File: {
    QList<int> all;
    for (int i = 0; i < diff.hunks.size(); ++i) {
      all.append(i);
    }
    return buildHunkPatch(diff, all);
  }
  }
  return QString();
}

void StagingCanvasDialog::onColumnSelectionChanged() { updateActionState(); }

void StagingCanvasDialog::updateActionState() {
  const bool haveFile = !currentPath().isEmpty();
  const bool canStage = stagingEnabled();

  const ColumnTarget stageTarget = targetFor(m_workingList, m_workingDiff);
  const ColumnTarget unstageTarget = targetFor(m_indexList, m_indexDiff);

  const bool hasWorking = !m_workingDiff.hunks.isEmpty();
  const bool hasStaged = !m_indexDiff.hunks.isEmpty();

  m_stageButton->setText(describeTarget(stageTarget, tr("Stage")) +
                         QStringLiteral(" →"));
  m_stageButton->setEnabled(haveFile && canStage && hasWorking);

  m_unstageButton->setText(QStringLiteral("← ") +
                           describeTarget(unstageTarget, tr("Unstage")));
  m_unstageButton->setEnabled(haveFile && hasStaged);

  m_discardButton->setText(describeTarget(stageTarget, tr("Discard")) +
                           QStringLiteral("…"));
  m_discardButton->setEnabled(haveFile && canStage && hasWorking);

  m_restoreButton->setEnabled(haveFile);
  m_amendButton->setEnabled(hasStaged);

  if (!haveFile) {
    m_statusLabel->setText(tr("No changed files. The working tree, the index "
                              "and HEAD all agree."));
    return;
  }

  if (!canStage) {
    m_statusLabel->setText(
        tr("Working ↔ HEAD is a read-only view of everything since the last "
           "commit. Switch to Working ↔ Index to stage or discard."));
    return;
  }

  QStringList parts;
  parts << tr("%1: %2 unstaged, %3 staged")
               .arg(currentPath())
               .arg(m_workingDiff.additions() + m_workingDiff.deletions())
               .arg(m_indexDiff.additions() + m_indexDiff.deletions());
  parts << tr("Ctrl+Right stages, Ctrl+Left unstages, Delete discards.");
  m_statusLabel->setText(parts.join(QStringLiteral("  ·  ")));
}

bool StagingCanvasDialog::applyAndRefresh(const QString &patch, bool cached,
                                          bool reverse) {
  if (patch.isEmpty() || !m_git) {
    return false;
  }
  if (!m_git->applyPatch(patch, cached, reverse)) {
    return false;
  }
  reload();
  emit repositoryChanged();
  return true;
}

void StagingCanvasDialog::onStageClicked() {
  if (!stagingEnabled()) {
    return;
  }
  const ColumnTarget target = targetFor(m_workingList, m_workingDiff);
  if (applyAndRefresh(patchFor(target, m_workingDiff, false), true, false)) {
    flashColumn(m_indexList);
  }
}

void StagingCanvasDialog::onUnstageClicked() {
  const ColumnTarget target = targetFor(m_indexList, m_indexDiff);

  if (applyAndRefresh(patchFor(target, m_indexDiff, true), true, true)) {
    flashColumn(m_workingList);
  }
}

void StagingCanvasDialog::onDiscardClicked() {
  if (!stagingEnabled()) {
    return;
  }
  const ColumnTarget target = targetFor(m_workingList, m_workingDiff);
  const QString patch = patchFor(target, m_workingDiff, true);
  if (patch.isEmpty()) {
    return;
  }

  if (!confirmDestructive(
          tr("Discard working-tree changes"),
          tr("Throw away %1 in <b>%2</b>?")
              .arg(describeTarget(target, tr("")).trimmed(), currentPath()),
          tr("These edits are not in the index and not in any commit, so Git "
             "has no copy of them. This cannot be undone."))) {
    return;
  }

  if (applyAndRefresh(patch, false, true)) {
    flashColumn(m_workingList);
  }
}

void StagingCanvasDialog::onAmendClicked() {
  if (!m_git) {
    return;
  }
  if (!confirmDestructive(
          tr("Amend the last commit"),
          tr("Fold the staged changes into the previous commit?"),
          tr("This rewrites that commit, so it gets a new hash. If you have "
             "already pushed it, the remote will reject a plain push."))) {
    return;
  }
  if (m_git->commitAmend()) {
    reload();
    emit repositoryChanged();
  }
}

bool StagingCanvasDialog::confirmDestructive(const QString &title,
                                             const QString &body,
                                             const QString &detail) {
  return ThemedMessageBox::question(
             this, title,
             QStringLiteral("%1<br><br><i>%2</i>").arg(body, detail)) ==
         QMessageBox::Yes;
}

void StagingCanvasDialog::flashColumn(QListWidget *list) {
  m_flashTarget = list;
  applyTheme(m_theme);
  m_flashTimer->start();
}

void StagingCanvasDialog::onWorkingContextMenu(const QPoint &pos) {
  QMenu menu(this);
  menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));
  QAction *stageHunk = menu.addAction(tr("Stage this hunk"));
  QAction *stageFile = menu.addAction(tr("Stage whole file"));
  menu.addSeparator();
  QAction *discard = menu.addAction(tr("Discard selection…"));

  stageHunk->setEnabled(stagingEnabled() && !m_workingDiff.hunks.isEmpty());
  stageFile->setEnabled(stagingEnabled() && !m_workingDiff.hunks.isEmpty());
  discard->setEnabled(stagingEnabled() && !m_workingDiff.hunks.isEmpty());

  QAction *chosen = menu.exec(m_workingList->viewport()->mapToGlobal(pos));
  if (!chosen) {
    return;
  }
  if (chosen == discard) {
    onDiscardClicked();
    return;
  }

  ColumnTarget target;
  if (chosen == stageHunk) {
    QListWidgetItem *item = m_workingList->itemAt(pos);
    target.granularity = Granularity::Hunk;
    target.hunkIndex = item ? item->data(HUNK_INDEX_ROLE).toInt() : 0;
  } else {
    target.granularity = Granularity::File;
  }
  if (applyAndRefresh(patchFor(target, m_workingDiff, false), true, false)) {
    flashColumn(m_indexList);
  }
}

void StagingCanvasDialog::onIndexContextMenu(const QPoint &pos) {
  QMenu menu(this);
  menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));
  QAction *unstageHunk = menu.addAction(tr("Unstage this hunk"));
  QAction *unstageFile = menu.addAction(tr("Unstage whole file"));

  unstageHunk->setEnabled(!m_indexDiff.hunks.isEmpty());
  unstageFile->setEnabled(!m_indexDiff.hunks.isEmpty());

  QAction *chosen = menu.exec(m_indexList->viewport()->mapToGlobal(pos));
  if (!chosen) {
    return;
  }

  ColumnTarget target;
  if (chosen == unstageHunk) {
    QListWidgetItem *item = m_indexList->itemAt(pos);
    target.granularity = Granularity::Hunk;
    target.hunkIndex = item ? item->data(HUNK_INDEX_ROLE).toInt() : 0;
  } else {
    target.granularity = Granularity::File;
  }
  if (applyAndRefresh(patchFor(target, m_indexDiff, true), true, true)) {
    flashColumn(m_workingList);
  }
}

void StagingCanvasDialog::keyPressEvent(QKeyEvent *event) {

  if (event->modifiers().testFlag(Qt::ControlModifier)) {
    if (event->key() == Qt::Key_Right) {
      onStageClicked();
      return;
    }
    if (event->key() == Qt::Key_Left) {
      onUnstageClicked();
      return;
    }
  }
  if (event->key() == Qt::Key_F5) {
    reload();
    return;
  }
  if (event->key() == Qt::Key_Delete && m_workingList->hasFocus()) {
    onDiscardClicked();
    return;
  }
  StyledDialog::keyPressEvent(event);
}

void StagingCanvasDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);

  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_fileTree) {
    m_fileTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  if (m_compareSelector) {
    m_compareSelector->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }

  const auto styleColumn = [&](QLabel *header, QListWidget *list,
                               const QColor &accent, bool active) {
    if (!header || !list) {
      return;
    }
    const bool flashing = m_flashTarget == list;
    const QColor border = flashing ? theme.successColor : accent;
    header->setStyleSheet(
        QString("QLabel { color: %1; font-weight: bold; padding: 4px; "
                "border-bottom: 2px solid %2; }")
            .arg(active ? theme.foregroundColor.name()
                        : theme.singleLineCommentFormat.name(),
                 border.name()));
    list->setStyleSheet(
        QString("QListWidget { background: %1; color: %2; border: 1px solid "
                "%3; }"
                "QListWidget::item:selected { background: %4; color: %5; }")
            .arg(theme.surfaceColor.name(),
                 active ? theme.foregroundColor.name()
                        : theme.singleLineCommentFormat.name(),
                 border.name(), theme.accentColor.name(),
                 theme.backgroundColor.name()));
  };

  const bool workingActive = m_comparePair != ComparePair::IndexHead;
  const bool indexActive = m_comparePair != ComparePair::WorkingHead;
  styleColumn(m_workingHeader, m_workingList, theme.gitModifiedColor,
              workingActive);
  styleColumn(m_indexHeader, m_indexList, theme.gitAddedColor, indexActive);
  styleColumn(m_headHeader, m_headList, theme.borderColor,
              m_comparePair != ComparePair::WorkingIndex);

  for (QPushButton *button : {m_stageButton, m_unstageButton, m_amendButton}) {
    if (button) {
      stylePrimaryButton(button);
    }
  }
  for (QPushButton *button : {m_refreshButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  for (QPushButton *button : {m_discardButton, m_restoreButton}) {
    if (button) {
      styleDangerButton(button);
    }
  }
  if (m_statusLabel) {
    styleSubduedLabel(m_statusLabel);
  }

  for (const char *name : {"stagingCompareLabel", "stagingFilesLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
}
