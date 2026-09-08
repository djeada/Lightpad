#include "commitcraftingdialog.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "themedmessagebox.h"
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QString countOf(int count, const QString &singular, const QString &plural) {
  return count == 1 ? singular : plural.arg(count);
}

constexpr int PROMPT_WARNING_ROLE = Qt::UserRole + 10;

constexpr int CHANGE_KIND_ROLE = Qt::UserRole + 1;
constexpr int CHANGE_PATH_ROLE = Qt::UserRole + 2;
constexpr int CHANGE_HUNK_ROLE = Qt::UserRole + 3;
constexpr int CHANGE_FINGERPRINT_ROLE = Qt::UserRole + 4;
constexpr int CHANGE_ADDITIONS_ROLE = Qt::UserRole + 5;
constexpr int CHANGE_DELETIONS_ROLE = Qt::UserRole + 6;
constexpr int BUCKET_INDEX_ROLE = Qt::UserRole + 7;
constexpr int IS_CHANGE_ROLE = Qt::UserRole + 8;

void storeChange(QTreeWidgetItem *item, const CommitChangeRef &change) {
  item->setData(0, IS_CHANGE_ROLE, true);
  item->setData(0, CHANGE_KIND_ROLE, static_cast<int>(change.kind));
  item->setData(0, CHANGE_PATH_ROLE, change.filePath);
  item->setData(0, CHANGE_HUNK_ROLE, change.hunkIndex);
  item->setData(0, CHANGE_FINGERPRINT_ROLE, change.fingerprint);
  item->setData(0, CHANGE_ADDITIONS_ROLE, change.additions);
  item->setData(0, CHANGE_DELETIONS_ROLE, change.deletions);
}

CommitChangeRef readChange(const QTreeWidgetItem *item) {
  CommitChangeRef change;
  change.kind = static_cast<CommitChangeRef::Kind>(
      item->data(0, CHANGE_KIND_ROLE).toInt());
  change.filePath = item->data(0, CHANGE_PATH_ROLE).toString();
  change.hunkIndex = item->data(0, CHANGE_HUNK_ROLE).toInt();
  change.fingerprint = item->data(0, CHANGE_FINGERPRINT_ROLE).toString();
  change.additions = item->data(0, CHANGE_ADDITIONS_ROLE).toInt();
  change.deletions = item->data(0, CHANGE_DELETIONS_ROLE).toInt();
  return change;
}

} // namespace

CommitCraftingDialog::CommitCraftingDialog(GitIntegration *git,
                                           const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_headerLabel(nullptr),
      m_unassignedLabel(nullptr), m_bucketStatsLabel(nullptr),
      m_unassignedTree(nullptr), m_bucketTree(nullptr), m_promptList(nullptr),
      m_bucketNameEdit(nullptr), m_messageEdit(nullptr),
      m_addBucketButton(nullptr), m_removeBucketButton(nullptr),
      m_assignButton(nullptr), m_returnButton(nullptr), m_commitButton(nullptr),
      m_refreshButton(nullptr), m_closeButton(nullptr), m_updating(false) {
  setWindowTitle(tr("Commit Crafting Workspace"));
  setMinimumSize(900, 620);
  resize(1080, 740);

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  reload();
}

void CommitCraftingDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  QHBoxLayout *header = new QHBoxLayout();
  m_headerLabel = new QLabel(this);
  m_headerLabel->setObjectName(QStringLiteral("craftHeaderLabel"));
  header->addWidget(m_headerLabel);
  header->addStretch();

  m_refreshButton = new QPushButton(tr("Refresh"), this);
  m_refreshButton->setObjectName(QStringLiteral("craftRefreshButton"));
  m_refreshButton->setToolTip(
      tr("Re-read the working tree. Assignments whose code changed since you "
         "made them are dropped rather than silently re-pointed."));
  connect(m_refreshButton, &QPushButton::clicked, this,
          &CommitCraftingDialog::reload);
  header->addWidget(m_refreshButton);
  layout->addLayout(header);

  buildColumns(layout);
  buildBucketEditor(layout);

  m_promptList = new QListWidget(this);
  m_promptList->setObjectName(QStringLiteral("craftPromptList"));
  m_promptList->setMaximumHeight(96);
  m_promptList->setToolTip(
      tr("Review prompts. These are suggestions only — nothing here stages or "
         "groups anything by itself."));
  layout->addWidget(m_promptList);

  QHBoxLayout *footer = new QHBoxLayout();
  footer->addStretch();
  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("craftCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  footer->addWidget(m_closeButton);
  layout->addLayout(footer);
}

void CommitCraftingDialog::buildColumns(QVBoxLayout *layout) {
  QHBoxLayout *columns = new QHBoxLayout();
  columns->setSpacing(UiMetrics::SpaceMd);

  QWidget *left = new QWidget(this);
  QVBoxLayout *leftLayout = new QVBoxLayout(left);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(UiMetrics::SpaceXs);

  m_unassignedLabel = new QLabel(this);
  m_unassignedLabel->setObjectName(QStringLiteral("craftUnassignedLabel"));
  leftLayout->addWidget(m_unassignedLabel);

  m_unassignedTree = new QTreeWidget(left);
  m_unassignedTree->setObjectName(QStringLiteral("craftUnassignedTree"));
  m_unassignedTree->setColumnCount(2);
  m_unassignedTree->setHeaderLabels({tr("Change"), tr("± lines")});
  m_unassignedTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_unassignedTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_unassignedTree->header()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  leftLayout->addWidget(m_unassignedTree, 1);
  columns->addWidget(left, 1);

  QWidget *middle = new QWidget(this);
  QVBoxLayout *middleLayout = new QVBoxLayout(middle);
  middleLayout->setContentsMargins(0, 0, 0, 0);
  middleLayout->addStretch();

  m_assignButton = new QPushButton(tr("Assign →"), middle);
  m_assignButton->setObjectName(QStringLiteral("craftAssignButton"));
  m_assignButton->setToolTip(
      tr("Put the selected changes in the selected bucket (Ctrl+Right)."));
  connect(m_assignButton, &QPushButton::clicked, this,
          &CommitCraftingDialog::onAssign);
  middleLayout->addWidget(m_assignButton);

  m_returnButton = new QPushButton(tr("← Return"), middle);
  m_returnButton->setObjectName(QStringLiteral("craftReturnButton"));
  m_returnButton->setToolTip(
      tr("Take the selected changes back out of their bucket (Ctrl+Left)."));
  connect(m_returnButton, &QPushButton::clicked, this,
          &CommitCraftingDialog::onReturnToUnassigned);
  middleLayout->addWidget(m_returnButton);

  middleLayout->addStretch();
  columns->addWidget(middle);

  QWidget *right = new QWidget(this);
  QVBoxLayout *rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(UiMetrics::SpaceXs);

  QHBoxLayout *bucketHeader = new QHBoxLayout();
  QLabel *bucketsLabel = new QLabel(tr("Planned commits"), right);
  bucketsLabel->setObjectName(QStringLiteral("craftBucketsLabel"));
  bucketHeader->addWidget(bucketsLabel);
  bucketHeader->addStretch();

  m_addBucketButton = new QPushButton(tr("+ New bucket"), right);
  m_addBucketButton->setObjectName(QStringLiteral("craftAddBucketButton"));
  connect(m_addBucketButton, &QPushButton::clicked, this,
          &CommitCraftingDialog::onAddBucket);
  bucketHeader->addWidget(m_addBucketButton);

  m_removeBucketButton = new QPushButton(tr("Remove"), right);
  m_removeBucketButton->setObjectName(
      QStringLiteral("craftRemoveBucketButton"));
  m_removeBucketButton->setToolTip(
      tr("Drop this bucket. Its changes return to the unassigned list; "
         "nothing on disk is touched."));
  connect(m_removeBucketButton, &QPushButton::clicked, this,
          &CommitCraftingDialog::onRemoveBucket);
  bucketHeader->addWidget(m_removeBucketButton);
  rightLayout->addLayout(bucketHeader);

  m_bucketTree = new QTreeWidget(right);
  m_bucketTree->setObjectName(QStringLiteral("craftBucketTree"));
  m_bucketTree->setColumnCount(2);
  m_bucketTree->setHeaderLabels({tr("Bucket"), tr("± lines")});
  m_bucketTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_bucketTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_bucketTree->header()->setSectionResizeMode(1,
                                               QHeaderView::ResizeToContents);
  connect(m_bucketTree, &QTreeWidget::itemSelectionChanged, this,
          &CommitCraftingDialog::onBucketSelectionChanged);
  rightLayout->addWidget(m_bucketTree, 1);
  columns->addWidget(right, 1);

  layout->addLayout(columns, 1);
}

void CommitCraftingDialog::buildBucketEditor(QVBoxLayout *layout) {
  QHBoxLayout *nameRow = new QHBoxLayout();
  QLabel *nameLabel = new QLabel(tr("Bucket:"), this);
  nameLabel->setObjectName(QStringLiteral("craftNameLabel"));
  nameRow->addWidget(nameLabel);

  m_bucketNameEdit = new QLineEdit(this);
  m_bucketNameEdit->setObjectName(QStringLiteral("craftBucketNameEdit"));
  m_bucketNameEdit->setPlaceholderText(tr("bug fix, tests, refactor…"));
  connect(m_bucketNameEdit, &QLineEdit::textEdited, this,
          &CommitCraftingDialog::onBucketNameEdited);
  nameRow->addWidget(m_bucketNameEdit, 1);

  m_bucketStatsLabel = new QLabel(this);
  m_bucketStatsLabel->setObjectName(QStringLiteral("craftBucketStatsLabel"));
  nameRow->addWidget(m_bucketStatsLabel);
  layout->addLayout(nameRow);

  m_messageEdit = new QTextEdit(this);
  m_messageEdit->setObjectName(QStringLiteral("craftMessageEdit"));
  m_messageEdit->setPlaceholderText(
      tr("Commit message for this bucket. A good one explains the intent the "
         "bucket was built around."));
  m_messageEdit->setMaximumHeight(96);
  connect(m_messageEdit, &QTextEdit::textChanged, this,
          &CommitCraftingDialog::onMessageEdited);
  layout->addWidget(m_messageEdit);

  QHBoxLayout *commitRow = new QHBoxLayout();
  commitRow->addStretch();
  m_commitButton = new QPushButton(tr("Commit this bucket"), this);
  m_commitButton->setObjectName(QStringLiteral("craftCommitButton"));
  m_commitButton->setToolTip(
      tr("Stage exactly this bucket's changes and commit them.\n"
         "The index is reset first, so only what is in the bucket is "
         "committed."));
  connect(m_commitButton, &QPushButton::clicked, this,
          &CommitCraftingDialog::onCommitBucket);
  commitRow->addWidget(m_commitButton);
  layout->addLayout(commitRow);
}

void CommitCraftingDialog::reload() {
  if (!m_git || !m_git->isValidRepository()) {
    return;
  }

  m_diffs.clear();
  QList<CommitChangeRef> available;

  for (const GitFileInfo &info : m_git->getStatus()) {
    if (info.workTreeStatus == GitFileStatus::Untracked ||
        info.indexStatus == GitFileStatus::Untracked) {
      CommitChangeRef change;
      change.kind = CommitChangeRef::Kind::UntrackedFile;
      change.filePath = info.filePath;
      change.fingerprint = QStringLiteral("untracked");

      QFile file(QDir(m_git->repositoryPath()).filePath(info.filePath));
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        change.additions =
            QString::fromUtf8(file.readAll()).count(QLatin1Char('\n'));
      }
      available.append(change);
      continue;
    }

    const GitDiffFile diff =
        parseUnifiedDiff(m_git->getWorkingVsHeadDiff(info.filePath));
    if (!diff.valid) {
      continue;
    }
    m_diffs.insert(info.filePath, diff);

    for (int i = 0; i < diff.hunks.size(); ++i) {
      CommitChangeRef change;
      change.kind = CommitChangeRef::Kind::Hunk;
      change.filePath = info.filePath;
      change.hunkIndex = i;
      change.fingerprint = hunkFingerprint(diff.hunks.at(i));
      change.additions = diff.hunks.at(i).additions();
      change.deletions = diff.hunks.at(i).deletions();
      available.append(change);
    }
  }

  m_plan.setAvailableChanges(available);
  refreshTrees();
  refreshPrompts();
  updateBucketEditor();

  QSet<QString> files;
  for (const CommitChangeRef &change : available) {
    files.insert(change.filePath);
  }
  m_headerLabel->setText(
      tr("Working tree vs HEAD — %1 across %2")
          .arg(countOf(available.size(), tr("1 change"), tr("%1 changes")),
               countOf(files.size(), tr("1 file"), tr("%1 files"))));
}

void CommitCraftingDialog::refreshTrees() {
  m_updating = true;

  m_unassignedTree->clear();
  QMap<QString, QTreeWidgetItem *> fileItems;
  const QList<CommitChangeRef> unassigned = m_plan.unassigned();
  for (const CommitChangeRef &change : unassigned) {
    QTreeWidgetItem *parent = fileItems.value(change.filePath, nullptr);
    if (!parent) {
      parent = new QTreeWidgetItem(m_unassignedTree);
      parent->setText(0, change.filePath);
      parent->setFlags(Qt::ItemIsEnabled);
      parent->setExpanded(true);
      fileItems.insert(change.filePath, parent);
    }
    QTreeWidgetItem *item = new QTreeWidgetItem(parent);
    item->setText(0, change.kind == CommitChangeRef::Kind::UntrackedFile
                         ? tr("whole file (new)")
                         : tr("hunk %1").arg(change.hunkIndex + 1));
    item->setText(
        1,
        QStringLiteral("+%1 −%2").arg(change.additions).arg(change.deletions));
    storeChange(item, change);
  }
  m_unassignedLabel->setText(
      tr("Unassigned changes (%1)").arg(unassigned.size()));

  const int previousBucket = currentBucketIndex();
  m_bucketTree->clear();
  const QList<CommitBucket> &buckets = m_plan.buckets();
  for (int i = 0; i < buckets.size(); ++i) {
    const CommitBucket &bucket = buckets.at(i);
    QTreeWidgetItem *bucketItem = new QTreeWidgetItem(m_bucketTree);
    bucketItem->setText(
        0,
        tr("%1 (%2)").arg(bucket.name.isEmpty() ? tr("untitled") : bucket.name,
                          QString::number(bucket.changes.size())));
    bucketItem->setText(1, QStringLiteral("+%1 −%2")
                               .arg(bucket.additions())
                               .arg(bucket.deletions()));
    bucketItem->setData(0, BUCKET_INDEX_ROLE, i);
    bucketItem->setData(0, IS_CHANGE_ROLE, false);
    bucketItem->setExpanded(true);

    for (const CommitChangeRef &change : bucket.changes) {
      QTreeWidgetItem *item = new QTreeWidgetItem(bucketItem);
      item->setText(0, change.label());
      item->setText(1, QStringLiteral("+%1 −%2")
                           .arg(change.additions)
                           .arg(change.deletions));
      item->setData(0, BUCKET_INDEX_ROLE, i);
      storeChange(item, change);
    }
  }

  if (previousBucket >= 0 &&
      previousBucket < m_bucketTree->topLevelItemCount()) {
    m_bucketTree->setCurrentItem(m_bucketTree->topLevelItem(previousBucket));
  } else if (m_bucketTree->topLevelItemCount() > 0) {
    m_bucketTree->setCurrentItem(m_bucketTree->topLevelItem(0));
  }

  m_updating = false;
}

void CommitCraftingDialog::refreshPrompts() {
  m_prompts = buildReviewPrompts(m_plan.available(), m_diffs);
  m_promptList->clear();

  for (const CommitChangeRef &stale : m_plan.staleAssignments()) {
    QListWidgetItem *item = new QListWidgetItem(
        tr("⚠ %1 changed since you assigned it, so it was returned to the "
           "unassigned list.")
            .arg(stale.filePath),
        m_promptList);
    item->setFlags(Qt::ItemIsEnabled);
  }

  for (const ReviewPrompt &prompt : m_prompts) {
    const QString glyph = prompt.kind == ReviewPrompt::Kind::CoupledChange
                              ? QStringLiteral("💡")
                              : QStringLiteral("⚠");
    QListWidgetItem *item = new QListWidgetItem(
        glyph + QStringLiteral(" ") + prompt.text, m_promptList);
    item->setFlags(Qt::ItemIsEnabled);
    item->setData(PROMPT_WARNING_ROLE,
                  prompt.kind != ReviewPrompt::Kind::CoupledChange);
  }

  if (m_promptList->count() == 0) {
    QListWidgetItem *item =
        new QListWidgetItem(tr("Nothing flagged for review."), m_promptList);
    item->setFlags(Qt::ItemIsEnabled);
    item->setData(PROMPT_WARNING_ROLE, false);
  }
  stylePromptItems();
}

void CommitCraftingDialog::stylePromptItems() {
  if (!m_promptList) {
    return;
  }
  for (int i = 0; i < m_promptList->count(); ++i) {
    QListWidgetItem *item = m_promptList->item(i);
    item->setForeground(item->data(PROMPT_WARNING_ROLE).toBool()
                            ? m_promptWarningColor
                            : m_promptNormalColor);
  }
}

int CommitCraftingDialog::currentBucketIndex() const {
  QTreeWidgetItem *item = m_bucketTree ? m_bucketTree->currentItem() : nullptr;
  if (!item) {
    return -1;
  }
  return item->data(0, BUCKET_INDEX_ROLE).toInt();
}

QList<CommitChangeRef> CommitCraftingDialog::selectedUnassigned() const {
  QList<CommitChangeRef> changes;
  for (QTreeWidgetItem *item : m_unassignedTree->selectedItems()) {
    if (item->data(0, IS_CHANGE_ROLE).toBool()) {
      changes.append(readChange(item));
    }
  }
  return changes;
}

QList<CommitChangeRef> CommitCraftingDialog::selectedAssigned() const {
  QList<CommitChangeRef> changes;
  for (QTreeWidgetItem *item : m_bucketTree->selectedItems()) {
    if (item->data(0, IS_CHANGE_ROLE).toBool()) {
      changes.append(readChange(item));
    }
  }
  return changes;
}

void CommitCraftingDialog::onAddBucket() {
  const int index =
      m_plan.addBucket(tr("commit %1").arg(m_plan.buckets().size() + 1));
  refreshTrees();
  if (index < m_bucketTree->topLevelItemCount()) {
    m_bucketTree->setCurrentItem(m_bucketTree->topLevelItem(index));
  }
  updateBucketEditor();
  m_bucketNameEdit->setFocus();
  m_bucketNameEdit->selectAll();
}

void CommitCraftingDialog::onRemoveBucket() {
  const int index = currentBucketIndex();
  if (index < 0) {
    return;
  }
  m_plan.removeBucket(index);
  refreshTrees();
  updateBucketEditor();
}

void CommitCraftingDialog::onAssign() {
  const int bucket = currentBucketIndex();
  if (bucket < 0) {
    return;
  }
  for (const CommitChangeRef &change : selectedUnassigned()) {
    m_plan.assign(bucket, change);
  }
  refreshTrees();
  updateBucketEditor();
}

void CommitCraftingDialog::onReturnToUnassigned() {
  for (const CommitChangeRef &change : selectedAssigned()) {
    m_plan.unassign(change);
  }
  refreshTrees();
  updateBucketEditor();
}

void CommitCraftingDialog::onBucketSelectionChanged() {
  if (m_updating) {
    return;
  }
  updateBucketEditor();
}

void CommitCraftingDialog::updateBucketEditor() {
  const int index = currentBucketIndex();
  const bool haveBucket = index >= 0 && index < m_plan.buckets().size();

  m_updating = true;
  if (haveBucket) {
    const CommitBucket &bucket = m_plan.buckets().at(index);
    m_bucketNameEdit->setText(bucket.name);
    m_messageEdit->setPlainText(bucket.message);
    m_bucketStatsLabel->setText(tr("%1 changes · +%2 −%3")
                                    .arg(bucket.changes.size())
                                    .arg(bucket.additions())
                                    .arg(bucket.deletions()));
  } else {
    m_bucketNameEdit->clear();
    m_messageEdit->clear();
    m_bucketStatsLabel->setText(tr("No bucket selected"));
  }
  m_updating = false;

  m_bucketNameEdit->setEnabled(haveBucket);
  m_messageEdit->setEnabled(haveBucket);
  m_removeBucketButton->setEnabled(haveBucket);
  m_assignButton->setEnabled(haveBucket);
  m_returnButton->setEnabled(haveBucket);

  const bool commitable =
      haveBucket && !m_plan.buckets().at(index).changes.isEmpty() &&
      !m_plan.buckets().at(index).message.trimmed().isEmpty();
  m_commitButton->setEnabled(commitable);
}

void CommitCraftingDialog::onBucketNameEdited(const QString &name) {
  if (m_updating) {
    return;
  }
  const int index = currentBucketIndex();
  if (index < 0) {
    return;
  }
  m_plan.renameBucket(index, name);
  refreshTrees();
}

void CommitCraftingDialog::onMessageEdited() {
  if (m_updating) {
    return;
  }
  const int index = currentBucketIndex();
  if (index < 0) {
    return;
  }
  m_plan.setMessage(index, m_messageEdit->toPlainText());
  updateBucketEditor();
}

QString
CommitCraftingDialog::patchForBucket(const CommitBucket &bucket,
                                     const QString &filePath,
                                     const QList<int> &hunkIndices) const {
  Q_UNUSED(bucket);
  const auto it = m_diffs.constFind(filePath);
  if (it == m_diffs.constEnd()) {
    return QString();
  }
  return buildHunkPatch(*it, hunkIndices);
}

void CommitCraftingDialog::onCommitBucket() {
  const int index = currentBucketIndex();
  if (index < 0 || !m_git) {
    return;
  }
  const CommitBucket bucket = m_plan.buckets().at(index);
  if (bucket.changes.isEmpty() || bucket.message.trimmed().isEmpty()) {
    return;
  }

  if (ThemedMessageBox::question(
          this, tr("Commit bucket"),
          tr("Commit <b>%1</b> — %2 changes, +%3 −%4?<br><br><i>The index is "
             "reset first, so the commit contains exactly this bucket and "
             "nothing else. Your files on disk are not touched.</i>")
              .arg(bucket.name.isEmpty() ? tr("untitled") : bucket.name)
              .arg(bucket.changes.size())
              .arg(bucket.additions())
              .arg(bucket.deletions())) != ThemedMessageBox::Yes) {
    return;
  }

  if (!m_git->unstageAll()) {
    ThemedMessageBox::warning(
        this, tr("Could not prepare the index"),
        tr("Resetting the index failed, so nothing was committed."));
    return;
  }

  QMap<QString, QList<int>> hunksByFile;
  QStringList untracked;
  for (const CommitChangeRef &change : bucket.changes) {
    if (change.kind == CommitChangeRef::Kind::UntrackedFile) {
      untracked << change.filePath;
    } else {
      hunksByFile[change.filePath].append(change.hunkIndex);
    }
  }

  for (auto it = hunksByFile.constBegin(); it != hunksByFile.constEnd(); ++it) {
    const QString patch = patchForBucket(bucket, it.key(), it.value());
    if (patch.isEmpty() || !m_git->applyPatch(patch, true, false)) {
      ThemedMessageBox::warning(
          this, tr("Could not stage the bucket"),
          tr("Staging %1 failed, so nothing was committed. Refresh and try "
             "again.")
              .arg(it.key()));
      return;
    }
  }
  for (const QString &path : untracked) {
    m_git->stageFile(path);
  }

  if (m_git->commit(bucket.message)) {
    m_plan.removeBucket(index);
    reload();
    emit repositoryChanged();
  }
}

void CommitCraftingDialog::keyPressEvent(QKeyEvent *event) {
  if (event->modifiers().testFlag(Qt::ControlModifier)) {
    if (event->key() == Qt::Key_Right) {
      onAssign();
      return;
    }
    if (event->key() == Qt::Key_Left) {
      onReturnToUnassigned();
      return;
    }
  }
  if (event->key() == Qt::Key_F5) {
    reload();
    return;
  }
  StyledDialog::keyPressEvent(event);
}

void CommitCraftingDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  for (QTreeWidget *tree : {m_unassignedTree, m_bucketTree}) {
    if (tree) {
      tree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
    }
  }
  if (m_promptList) {
    m_promptList->setStyleSheet(
        QString("QListWidget { background: %1; color: %2; border: 1px solid "
                "%3; }")
            .arg(theme.surfaceColor.name(), theme.foregroundColor.name(),
                 theme.borderColor.name()));
    m_promptWarningColor = theme.warningColor;
    m_promptNormalColor = theme.foregroundColor;
    stylePromptItems();
  }
  if (m_bucketNameEdit) {
    m_bucketNameEdit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  }
  if (m_messageEdit) {
    m_messageEdit->setStyleSheet(
        QString("QTextEdit { background: %1; color: %2; border: 1px solid %3; "
                "}")
            .arg(theme.surfaceColor.name(), theme.foregroundColor.name(),
                 theme.borderColor.name()));
  }
  if (m_headerLabel) {
    styleTitleLabel(m_headerLabel);
  }
  for (const char *name : {"craftUnassignedLabel", "craftBucketsLabel",
                           "craftNameLabel", "craftBucketStatsLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  if (m_commitButton) {
    stylePrimaryButton(m_commitButton);
  }
  for (QPushButton *button :
       {m_addBucketButton, m_removeBucketButton, m_assignButton, m_returnButton,
        m_refreshButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
}
