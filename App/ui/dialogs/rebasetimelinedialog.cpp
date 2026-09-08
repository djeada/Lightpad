#include "rebasetimelinedialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "themedmessagebox.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int ROW_ROLE = Qt::UserRole + 1;

const QList<GitRebaseAction> &selectableActions() {
  static const QList<GitRebaseAction> actions{
      GitRebaseAction::Pick,   GitRebaseAction::Reword, GitRebaseAction::Edit,
      GitRebaseAction::Squash, GitRebaseAction::Fixup,  GitRebaseAction::Drop};
  return actions;
}

} // namespace

RebaseTimelineDialog::RebaseTimelineDialog(GitIntegration *git,
                                           const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_rebaseRunning(false),
      m_updating(false), m_countSpin(nullptr), m_ontoLabel(nullptr),
      m_publishedLabel(nullptr), m_timelineTree(nullptr),
      m_previewList(nullptr), m_problemList(nullptr), m_progressLabel(nullptr),
      m_upButton(nullptr), m_downButton(nullptr), m_autosquashButton(nullptr),
      m_startButton(nullptr), m_continueButton(nullptr), m_skipButton(nullptr),
      m_abortButton(nullptr), m_closeButton(nullptr) {
  setWindowTitle(tr("Rebase timeline"));
  setMinimumSize(880, 620);
  resize(1020, 720);

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  reload();
}

void RebaseTimelineDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  buildRangeBar(layout);
  buildTimeline(layout);
  buildPreview(layout);
  buildFooter(layout);
}

void RebaseTimelineDialog::buildRangeBar(QVBoxLayout *layout) {
  QHBoxLayout *bar = new QHBoxLayout();
  bar->setSpacing(UiMetrics::ToolbarSpacing);

  QLabel *countLabel = new QLabel(tr("Rewrite the last"), this);
  countLabel->setObjectName(QStringLiteral("rebaseCountLabel"));
  bar->addWidget(countLabel);

  m_countSpin = new QSpinBox(this);
  m_countSpin->setObjectName(QStringLiteral("rebaseCountSpin"));
  m_countSpin->setRange(1, 200);
  m_countSpin->setMinimumHeight(UiMetrics::ControlHeight);
  m_countSpin->setValue(5);
  connect(m_countSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &RebaseTimelineDialog::onRangeChanged);
  bar->addWidget(m_countSpin);

  QLabel *commitsLabel = new QLabel(tr("commits"), this);
  commitsLabel->setObjectName(QStringLiteral("rebaseCommitsLabel"));
  bar->addWidget(commitsLabel);

  m_ontoLabel = new QLabel(this);
  m_ontoLabel->setObjectName(QStringLiteral("rebaseOntoLabel"));
  bar->addWidget(m_ontoLabel);
  bar->addStretch();

  m_autosquashButton = new QPushButton(tr("Apply fixup!/squash!"), this);
  m_autosquashButton->setObjectName(QStringLiteral("rebaseAutosquashButton"));
  m_autosquashButton->setToolTip(
      tr("Move every fixup!/squash! commit next to the commit it names and "
         "set its action, the way --autosquash would."));
  connect(m_autosquashButton, &QPushButton::clicked, this,
          &RebaseTimelineDialog::onAutosquash);
  bar->addWidget(m_autosquashButton);

  layout->addLayout(bar);

  m_publishedLabel = new QLabel(this);
  m_publishedLabel->setObjectName(QStringLiteral("rebasePublishedLabel"));
  m_publishedLabel->setWordWrap(true);
  layout->addWidget(m_publishedLabel);
}

void RebaseTimelineDialog::buildTimeline(QVBoxLayout *layout) {
  QHBoxLayout *row = new QHBoxLayout();
  row->setSpacing(UiMetrics::SpaceMd);

  m_timelineTree = new QTreeWidget(this);
  m_timelineTree->setObjectName(QStringLiteral("rebaseTimelineTree"));
  m_timelineTree->setColumnCount(4);
  m_timelineTree->setHeaderLabels(
      {tr("Action"), tr("Commit"), tr("Subject"), tr("Shared?")});
  m_timelineTree->setRootIsDecorated(false);
  m_timelineTree->header()->setSectionResizeMode(0,
                                                 QHeaderView::ResizeToContents);
  m_timelineTree->header()->setSectionResizeMode(1,
                                                 QHeaderView::ResizeToContents);
  m_timelineTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  m_timelineTree->header()->setSectionResizeMode(3,
                                                 QHeaderView::ResizeToContents);
  connect(m_timelineTree, &QTreeWidget::itemSelectionChanged, this,
          [this]() { rebuildPreview(); });
  row->addWidget(m_timelineTree, 2);

  QWidget *buttons = new QWidget(this);
  QVBoxLayout *buttonLayout = new QVBoxLayout(buttons);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->addStretch();

  m_upButton = new QPushButton(tr("▲ Move up"), buttons);
  m_upButton->setObjectName(QStringLiteral("rebaseMoveUpButton"));
  m_upButton->setToolTip(tr("Replay this commit earlier (Ctrl+Up)."));
  connect(m_upButton, &QPushButton::clicked, this,
          &RebaseTimelineDialog::onMoveUp);
  buttonLayout->addWidget(m_upButton);

  m_downButton = new QPushButton(tr("▼ Move down"), buttons);
  m_downButton->setObjectName(QStringLiteral("rebaseMoveDownButton"));
  m_downButton->setToolTip(tr("Replay this commit later (Ctrl+Down)."));
  connect(m_downButton, &QPushButton::clicked, this,
          &RebaseTimelineDialog::onMoveDown);
  buttonLayout->addWidget(m_downButton);

  buttonLayout->addStretch();
  row->addWidget(buttons);

  layout->addLayout(row, 2);
}

void RebaseTimelineDialog::buildPreview(QVBoxLayout *layout) {
  QLabel *previewLabel = new QLabel(tr("After the rebase"), this);
  previewLabel->setObjectName(QStringLiteral("rebasePreviewLabel"));
  layout->addWidget(previewLabel);

  m_previewList = new QListWidget(this);
  m_previewList->setObjectName(QStringLiteral("rebasePreviewList"));
  m_previewList->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_previewList->setSelectionMode(QAbstractItemView::NoSelection);
  m_previewList->setFocusPolicy(Qt::NoFocus);
  m_previewList->setMaximumHeight(150);
  layout->addWidget(m_previewList);

  m_problemList = new QListWidget(this);
  m_problemList->setObjectName(QStringLiteral("rebaseProblemList"));
  m_problemList->setSelectionMode(QAbstractItemView::NoSelection);
  m_problemList->setFocusPolicy(Qt::NoFocus);
  m_problemList->setMaximumHeight(70);
  layout->addWidget(m_problemList);
}

void RebaseTimelineDialog::buildFooter(QVBoxLayout *layout) {
  m_progressLabel = new QLabel(this);
  m_progressLabel->setObjectName(QStringLiteral("rebaseProgressLabel"));
  m_progressLabel->setWordWrap(true);
  layout->addWidget(m_progressLabel);

  QHBoxLayout *footer = new QHBoxLayout();

  m_continueButton = new QPushButton(tr("Continue"), this);
  m_continueButton->setObjectName(QStringLiteral("rebaseContinueButton"));
  connect(m_continueButton, &QPushButton::clicked, this,
          &RebaseTimelineDialog::onContinue);
  footer->addWidget(m_continueButton);

  m_skipButton = new QPushButton(tr("Skip this commit"), this);
  m_skipButton->setObjectName(QStringLiteral("rebaseSkipButton"));
  connect(m_skipButton, &QPushButton::clicked, this,
          &RebaseTimelineDialog::onSkip);
  footer->addWidget(m_skipButton);

  m_abortButton = new QPushButton(tr("Abort"), this);
  m_abortButton->setObjectName(QStringLiteral("rebaseAbortButton"));
  m_abortButton->setToolTip(
      tr("Put the branch back exactly as it was before the rebase started."));
  connect(m_abortButton, &QPushButton::clicked, this,
          &RebaseTimelineDialog::onAbort);
  footer->addWidget(m_abortButton);

  footer->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("rebaseCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
  footer->addWidget(m_closeButton);

  m_startButton = new QPushButton(tr("Start rebase"), this);
  m_startButton->setObjectName(QStringLiteral("rebaseStartButton"));
  connect(m_startButton, &QPushButton::clicked, this,
          &RebaseTimelineDialog::onStartRebase);
  footer->addWidget(m_startButton);

  layout->addLayout(footer);
}

void RebaseTimelineDialog::reload() {
  if (!m_git || !m_git->isValidRepository()) {
    return;
  }

  m_rebaseRunning = m_git->isRebaseInProgress();
  if (!m_rebaseRunning) {
    onRangeChanged();
  }
  updateProgressUi();
}

void RebaseTimelineDialog::onRangeChanged() {
  if (!m_git || m_rebaseRunning) {
    return;
  }

  const int count = m_countSpin->value();
  m_onto = QStringLiteral("HEAD~%1").arg(count);

  QList<GitCommitInfo> commits =
      m_git->getCommitLogPage(QStringLiteral("HEAD"), 0, count);
  std::reverse(commits.begin(), commits.end());

  const GitSyncState sync = m_git->syncState();
  QSet<QString> publishedHashes;
  if (sync.hasUpstream) {
    QSet<QString> localOnly;
    for (const GitCommitInfo &commit : sync.outgoing) {
      localOnly.insert(commit.hash);
    }
    for (const GitCommitInfo &commit : commits) {
      if (!localOnly.contains(commit.hash)) {
        publishedHashes.insert(commit.hash);
      }
    }
  }

  QList<GitRebaseEntry> entries;
  for (const GitCommitInfo &commit : commits) {
    GitRebaseEntry entry;
    entry.commit = commit;
    entry.published = publishedHashes.contains(commit.hash);
    entries.append(entry);
  }

  m_plan.setEntries(entries);
  m_ontoLabel->setText(tr("onto %1").arg(m_onto));
  rebuildTimeline();
}

void RebaseTimelineDialog::rebuildTimeline() {
  m_updating = true;
  const int previousRow = currentRow();
  m_timelineTree->clear();

  const QList<GitRebaseEntry> &entries = m_plan.entries();
  for (int i = 0; i < entries.size(); ++i) {
    const GitRebaseEntry &entry = entries.at(i);
    QTreeWidgetItem *item = new QTreeWidgetItem(m_timelineTree);
    item->setData(0, ROW_ROLE, i);
    item->setText(1, entry.commit.shortHash);
    item->setText(2, entry.commit.subject);
    item->setText(3, entry.published ? tr("⚠ already pushed") : tr("local"));
    item->setToolTip(2, gitRebaseActionExplanation(entry.action));

    QComboBox *combo = new QComboBox(m_timelineTree);
    combo->setObjectName(QStringLiteral("rebaseActionCombo%1").arg(i));
    for (GitRebaseAction action : selectableActions()) {
      combo->addItem(gitRebaseActionName(action), static_cast<int>(action));
    }
    combo->setCurrentIndex(selectableActions().indexOf(entry.action));
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, i, combo](int index) {
              if (m_updating) {
                return;
              }
              onActionChanged(i, static_cast<GitRebaseAction>(
                                     combo->itemData(index).toInt()));
            });
    m_timelineTree->setItemWidget(item, 0, combo);
  }

  if (previousRow >= 0 && previousRow < m_timelineTree->topLevelItemCount()) {
    m_timelineTree->setCurrentItem(m_timelineTree->topLevelItem(previousRow));
  } else if (m_timelineTree->topLevelItemCount() > 0) {
    m_timelineTree->setCurrentItem(m_timelineTree->topLevelItem(0));
  }
  m_updating = false;

  m_publishedLabel->setText(
      m_plan.touchesPublishedHistory()
          ? tr("⚠ Part of this range is already on your upstream. Rewriting "
               "those commits gives them new hashes, so everyone else has to "
               "reconcile.")
          : tr("✓ Every commit in this range is only here, so rewriting it "
               "affects nobody else."));

  rebuildPreview();

  applyTheme(m_theme);
}

void RebaseTimelineDialog::rebuildPreview() {
  m_previewList->clear();
  for (const GitCommitInfo &commit : m_plan.resultingCommits()) {
    QListWidgetItem *item = new QListWidgetItem(
        QStringLiteral("+ %1  %2").arg(commit.shortHash, commit.subject),
        m_previewList);
    item->setFlags(Qt::ItemIsEnabled);
  }
  if (m_previewList->count() == 0) {
    QListWidgetItem *item =
        new QListWidgetItem(tr("(nothing would be left)"), m_previewList);
    item->setFlags(Qt::ItemIsEnabled);
  }

  m_problemList->clear();
  const QStringList problems = m_plan.validationProblems();
  for (const QString &problem : problems) {
    QListWidgetItem *item =
        new QListWidgetItem(QStringLiteral("⚠ ") + problem, m_problemList);
    item->setFlags(Qt::ItemIsEnabled);
  }
  m_problemList->setVisible(!problems.isEmpty());
  if (m_startButton) {
    m_startButton->setEnabled(!m_rebaseRunning && problems.isEmpty() &&
                              !m_plan.isEmpty());
  }
}

int RebaseTimelineDialog::currentRow() const {
  QTreeWidgetItem *item = m_timelineTree->currentItem();
  return item ? item->data(0, ROW_ROLE).toInt() : -1;
}

void RebaseTimelineDialog::onActionChanged(int row, GitRebaseAction action) {
  m_plan.setAction(row, action);
  if (QTreeWidgetItem *item = m_timelineTree->topLevelItem(row)) {
    item->setToolTip(2, gitRebaseActionExplanation(action));
  }
  rebuildPreview();
}

void RebaseTimelineDialog::onMoveUp() {
  const int row = currentRow();
  if (m_plan.moveUp(row)) {
    rebuildTimeline();
    m_timelineTree->setCurrentItem(m_timelineTree->topLevelItem(row - 1));
  }
}

void RebaseTimelineDialog::onMoveDown() {
  const int row = currentRow();
  if (m_plan.moveDown(row)) {
    rebuildTimeline();
    m_timelineTree->setCurrentItem(m_timelineTree->topLevelItem(row + 1));
  }
}

void RebaseTimelineDialog::onAutosquash() {
  const int moved = m_plan.applyAutosquash();
  rebuildTimeline();
  if (moved == 0) {
    ThemedMessageBox::information(
        this, tr("Nothing to autosquash"),
        tr("No commit in this range has a subject starting with \"fixup!\" or "
           "\"squash!\" that names another commit here."));
  }
}

void RebaseTimelineDialog::onStartRebase() {
  if (!m_git || m_rebaseRunning) {
    return;
  }

  if (m_plan.touchesPublishedHistory() &&
      ThemedMessageBox::question(
          this, tr("Rewrite shared history?"),
          tr("Some of these commits are already on your upstream. Rewriting "
             "them replaces them with new commits, so anyone who has the old "
             "ones will have to reconcile.<br><br>Continue?")) !=
          ThemedMessageBox::Yes) {
    return;
  }

  m_git->startInteractiveRebase(m_onto, m_plan);
  m_rebaseRunning = m_git->isRebaseInProgress();
  updateProgressUi();
  emit repositoryChanged();

  if (!m_rebaseRunning) {
    accept();
  }
}

void RebaseTimelineDialog::onContinue() {
  if (!m_git) {
    return;
  }
  if (m_git->hasMergeConflicts()) {
    emit resolveConflictsRequested();
    return;
  }
  m_git->rebaseContinue();
  m_rebaseRunning = m_git->isRebaseInProgress();
  updateProgressUi();
  emit repositoryChanged();
  if (!m_rebaseRunning) {
    accept();
  }
}

void RebaseTimelineDialog::onSkip() {
  if (!m_git) {
    return;
  }
  m_git->rebaseSkip();
  m_rebaseRunning = m_git->isRebaseInProgress();
  updateProgressUi();
  emit repositoryChanged();
}

void RebaseTimelineDialog::onAbort() {
  if (!m_git) {
    return;
  }
  m_git->rebaseAbort();
  m_rebaseRunning = m_git->isRebaseInProgress();
  updateProgressUi();
  emit repositoryChanged();
  if (!m_rebaseRunning) {
    reload();
  }
}

void RebaseTimelineDialog::updateProgressUi() {
  int current = 0;
  int total = 0;
  const bool haveProgress = m_git && m_git->rebaseProgress(current, total);

  if (m_rebaseRunning) {
    QString text = haveProgress ? tr("Rebase in progress — commit %1 of %2.")
                                      .arg(current)
                                      .arg(total)
                                : tr("Rebase in progress.");
    if (m_git && m_git->hasMergeConflicts()) {
      text += tr(" Git stopped on a conflict; resolve it, then continue.");
    } else {
      text += tr(" Git stopped so you can change this commit, then continue.");
    }
    m_progressLabel->setText(text);
  } else {
    m_progressLabel->setText(
        tr("Nothing is running yet. The plan above is only applied when you "
           "start the rebase."));
  }

  m_continueButton->setVisible(m_rebaseRunning);
  m_skipButton->setVisible(m_rebaseRunning);
  m_abortButton->setVisible(m_rebaseRunning);
  m_startButton->setVisible(!m_rebaseRunning);
  m_countSpin->setEnabled(!m_rebaseRunning);
  m_timelineTree->setEnabled(!m_rebaseRunning);
  m_autosquashButton->setEnabled(!m_rebaseRunning);
  m_upButton->setEnabled(!m_rebaseRunning);
  m_downButton->setEnabled(!m_rebaseRunning);
}

void RebaseTimelineDialog::keyPressEvent(QKeyEvent *event) {

  if (event->modifiers().testFlag(Qt::ControlModifier)) {
    if (event->key() == Qt::Key_Up) {
      onMoveUp();
      return;
    }
    if (event->key() == Qt::Key_Down) {
      onMoveDown();
      return;
    }
  }

  const int row = currentRow();
  if (row >= 0 && event->modifiers() == Qt::NoModifier) {
    const QMap<int, GitRebaseAction> keys{{Qt::Key_P, GitRebaseAction::Pick},
                                          {Qt::Key_R, GitRebaseAction::Reword},
                                          {Qt::Key_E, GitRebaseAction::Edit},
                                          {Qt::Key_S, GitRebaseAction::Squash},
                                          {Qt::Key_F, GitRebaseAction::Fixup},
                                          {Qt::Key_D, GitRebaseAction::Drop}};
    const auto it = keys.constFind(event->key());
    if (it != keys.constEnd()) {
      onActionChanged(row, it.value());
      rebuildTimeline();
      m_timelineTree->setCurrentItem(m_timelineTree->topLevelItem(row));
      return;
    }
  }

  StyledDialog::keyPressEvent(event);
}

void RebaseTimelineDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_timelineTree) {
    m_timelineTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  for (QListWidget *list : {m_previewList, m_problemList}) {
    if (list) {
      list->setStyleSheet(
          QString("QListWidget { background: %1; border: 1px solid %2; "
                  "color: %3; }")
              .arg(theme.surfaceColor.name(), theme.borderColor.name(),
                   list == m_problemList ? theme.errorColor.name()
                                         : theme.successColor.name()));
    }
  }
  if (m_countSpin) {

    m_countSpin->setStyleSheet(
        QString("QSpinBox { background: %1; color: %2; border: 1px solid %3; "
                "border-radius: %4px; padding: 0px; min-height: %5px; "
                "min-width: 56px; }"
                "QSpinBox::up-button, QSpinBox::down-button { width: 16px; }")
            .arg(theme.surfaceColor.name(), theme.foregroundColor.name(),
                 theme.borderColor.name())
            .arg(UiMetrics::RadiusSm)
            .arg(UiMetrics::ControlHeight + 4));
  }

  for (QComboBox *combo : m_timelineTree->findChildren<QComboBox *>()) {
    combo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }
  for (const char *name :
       {"rebaseCountLabel", "rebaseCommitsLabel", "rebaseOntoLabel",
        "rebasePreviewLabel", "rebaseProgressLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  if (m_publishedLabel) {
    m_publishedLabel->setStyleSheet(
        QString("color: %1;")
            .arg((m_plan.touchesPublishedHistory() ? theme.warningColor
                                                   : theme.successColor)
                     .name()));
  }
  for (QPushButton *button : {m_upButton, m_downButton, m_autosquashButton,
                              m_closeButton, m_skipButton, m_continueButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_abortButton) {
    styleDangerButton(m_abortButton);
  }
  if (m_startButton) {
    if (m_plan.touchesPublishedHistory()) {
      styleDangerButton(m_startButton);
    } else {
      stylePrimaryButton(m_startButton);
    }
  }
}
