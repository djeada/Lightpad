#include "branchhygienedialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "operationpreviewdialog.h"
#include "themedmessagebox.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
const char *PINNED_KEY = "git/pinnedBranches";
constexpr int BRANCH_NAME_ROLE = Qt::UserRole + 1;
} // namespace

QStringList BranchHygieneDialog::pinnedBranches() {
  QSettings settings("Lightpad", "Lightpad");
  return settings.value(QString::fromLatin1(PINNED_KEY)).toStringList();
}

void BranchHygieneDialog::setPinnedBranches(const QStringList &names) {
  QSettings settings("Lightpad", "Lightpad");
  settings.setValue(QString::fromLatin1(PINNED_KEY), names);
}

BranchHygieneDialog::BranchHygieneDialog(GitIntegration *git,
                                         const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_baseCombo(nullptr),
      m_filterEdit(nullptr), m_branchTree(nullptr), m_detailLabel(nullptr),
      m_warningLabel(nullptr), m_stateList(nullptr), m_pinButton(nullptr),
      m_deleteButton(nullptr), m_cleanupButton(nullptr),
      m_upstreamButton(nullptr), m_graphButton(nullptr),
      m_closeButton(nullptr) {
  setWindowTitle(tr("Branch hygiene"));
  setMinimumSize(900, 600);
  resize(1040, 700);

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  reload();
}

void BranchHygieneDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  QHBoxLayout *bar = new QHBoxLayout();
  QLabel *baseLabel = new QLabel(tr("Merged into:"), this);
  baseLabel->setObjectName(QStringLiteral("hygieneBaseLabel"));
  baseLabel->setToolTip(tr(
      "\"Merged\" is always relative to something. This is that something."));
  bar->addWidget(baseLabel);

  m_baseCombo = new QComboBox(this);
  m_baseCombo->setObjectName(QStringLiteral("hygieneBaseCombo"));
  m_baseCombo->setEditable(true);
  connect(m_baseCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &BranchHygieneDialog::onBaseChanged);
  bar->addWidget(m_baseCombo);

  m_filterEdit = new QLineEdit(this);
  m_filterEdit->setObjectName(QStringLiteral("hygieneFilterEdit"));
  m_filterEdit->setPlaceholderText(tr("Filter branches…"));
  m_filterEdit->setClearButtonEnabled(true);
  connect(m_filterEdit, &QLineEdit::textChanged, this,
          [this](const QString &) { reload(); });
  bar->addWidget(m_filterEdit, 1);
  layout->addLayout(bar);

  m_branchTree = new QTreeWidget(this);
  m_branchTree->setObjectName(QStringLiteral("hygieneBranchTree"));
  m_branchTree->setColumnCount(6);
  m_branchTree->setHeaderLabels({tr("Branch"), tr("State"), tr("Upstream"),
                                 tr("Ahead/behind"), tr("Unique"),
                                 tr("Last activity")});
  m_branchTree->setRootIsDecorated(false);
  m_branchTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_branchTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int column = 1; column < 6; ++column) {
    m_branchTree->header()->setSectionResizeMode(column,
                                                 QHeaderView::ResizeToContents);
  }
  connect(m_branchTree, &QTreeWidget::itemSelectionChanged, this,
          &BranchHygieneDialog::onSelectionChanged);
  layout->addWidget(m_branchTree, 2);

  m_detailLabel = new QLabel(this);
  m_detailLabel->setObjectName(QStringLiteral("hygieneDetailLabel"));
  m_detailLabel->setWordWrap(true);
  layout->addWidget(m_detailLabel);

  m_warningLabel = new QLabel(this);
  m_warningLabel->setObjectName(QStringLiteral("hygieneWarningLabel"));
  m_warningLabel->setWordWrap(true);
  layout->addWidget(m_warningLabel);

  m_stateList = new QListWidget(this);
  m_stateList->setObjectName(QStringLiteral("hygieneStateList"));
  m_stateList->setSelectionMode(QAbstractItemView::NoSelection);
  m_stateList->setFocusPolicy(Qt::NoFocus);
  m_stateList->setMaximumHeight(110);
  layout->addWidget(m_stateList);

  QHBoxLayout *footer = new QHBoxLayout();
  const auto addButton = [&](QPushButton **button, const QString &text,
                             const QString &name, const QString &tip) {
    *button = new QPushButton(text, this);
    (*button)->setObjectName(name);
    (*button)->setToolTip(tip);
    footer->addWidget(*button);
  };

  addButton(&m_pinButton, tr("Pin / unpin"), QStringLiteral("hygienePinButton"),
            tr("Pinned branches never appear in cleanup suggestions."));
  addButton(&m_graphButton, tr("Show in graph"),
            QStringLiteral("hygieneGraphButton"),
            tr("Find this branch in the commit graph."));
  addButton(&m_upstreamButton, tr("Repair upstream…"),
            QStringLiteral("hygieneUpstreamButton"),
            tr("For a branch whose remote counterpart is gone: point it "
               "somewhere else, or delete it."));
  addButton(&m_cleanupButton, tr("Clean up merged branches…"),
            QStringLiteral("hygieneCleanupButton"),
            tr("Delete every branch proven merged into the chosen base, with "
               "the list shown first."));
  addButton(&m_deleteButton, tr("Delete selected…"),
            QStringLiteral("hygieneDeleteButton"),
            tr("Delete the selected branches, one preview each."));

  connect(m_pinButton, &QPushButton::clicked, this,
          &BranchHygieneDialog::onTogglePin);
  connect(m_graphButton, &QPushButton::clicked, this,
          &BranchHygieneDialog::onShowInGraph);
  connect(m_upstreamButton, &QPushButton::clicked, this,
          &BranchHygieneDialog::onSetUpstreamOrDelete);
  connect(m_cleanupButton, &QPushButton::clicked, this,
          &BranchHygieneDialog::onCleanupMerged);
  connect(m_deleteButton, &QPushButton::clicked, this,
          &BranchHygieneDialog::onDeleteSelected);

  footer->addStretch();
  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("hygieneCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  footer->addWidget(m_closeButton);
  layout->addLayout(footer);
}

void BranchHygieneDialog::reload() {
  if (!m_git || !m_git->isValidRepository()) {
    return;
  }

  const QString previousBase = m_baseCombo->currentText();
  if (m_baseCombo->count() == 0) {
    QStringList candidates;
    for (const GitBranchInfo &branch : m_git->getBranches()) {
      candidates << branch.name;
    }

    QString preferred;
    for (const QString &trunk :
         {QStringLiteral("main"), QStringLiteral("master"),
          QStringLiteral("develop")}) {
      if (candidates.contains(trunk)) {
        preferred = trunk;
        break;
      }
    }
    if (preferred.isEmpty()) {
      preferred = m_git->currentBranch();
    }
    m_baseCombo->addItems(candidates);
    const int index = m_baseCombo->findText(preferred);
    if (index >= 0) {
      m_baseCombo->setCurrentIndex(index);
    }
  } else if (!previousBase.isEmpty()) {
    m_baseCombo->setCurrentText(previousBase);
  }

  const QStringList pinned = pinnedBranches();
  m_report = buildBranchHygieneReport(
      m_git, m_baseCombo->currentText(),
      QSet<QString>(pinned.constBegin(), pinned.constEnd()));

  const QString filter = m_filterEdit->text().trimmed();
  m_branchTree->clear();
  for (const GitBranchHealth &branch : m_report.branches) {
    if (!filter.isEmpty() &&
        !branch.name.contains(filter, Qt::CaseInsensitive)) {
      continue;
    }

    QStringList states;
    for (GitBranchState state : branch.states) {
      states << gitBranchStateName(state);
    }

    QTreeWidgetItem *item = new QTreeWidgetItem(m_branchTree);
    item->setText(0, branch.name);
    item->setText(1, states.join(QStringLiteral(", ")));
    item->setText(2, branch.upstreamGone ? tr("%1 (gone)").arg(branch.upstream)
                                         : branch.upstream);
    item->setText(
        3,
        branch.upstream.isEmpty()
            ? QString()
            : QStringLiteral("↑%1 ↓%2").arg(branch.ahead).arg(branch.behind));
    item->setText(4, branch.mergedIntoBase
                         ? tr("0")
                         : QString::number(branch.uniqueCommits));
    item->setText(5, branch.lastActivity);
    item->setData(0, BRANCH_NAME_ROLE, branch.name);
  }

  if (m_branchTree->topLevelItemCount() > 0) {
    m_branchTree->setCurrentItem(m_branchTree->topLevelItem(0));
  }
  onSelectionChanged();

  m_cleanupButton->setText(tr("Clean up %1 merged branches…")
                               .arg(m_report.cleanupCandidates().size()));
  m_cleanupButton->setEnabled(!m_report.cleanupCandidates().isEmpty());
}

void BranchHygieneDialog::onBaseChanged() { reload(); }

QString BranchHygieneDialog::selectedBranchName() const {
  QTreeWidgetItem *item = m_branchTree ? m_branchTree->currentItem() : nullptr;
  return item ? item->data(0, BRANCH_NAME_ROLE).toString() : QString();
}

const GitBranchHealth *BranchHygieneDialog::currentBranch() const {
  const QString name = selectedBranchName();
  for (const GitBranchHealth &branch : m_report.branches) {
    if (branch.name == name) {
      return &branch;
    }
  }
  return nullptr;
}

QList<GitBranchHealth> BranchHygieneDialog::selectedBranches() const {
  QList<GitBranchHealth> selected;
  for (QTreeWidgetItem *item : m_branchTree->selectedItems()) {
    const QString name = item->data(0, BRANCH_NAME_ROLE).toString();
    for (const GitBranchHealth &branch : m_report.branches) {
      if (branch.name == name) {
        selected.append(branch);
        break;
      }
    }
  }
  return selected;
}

void BranchHygieneDialog::onSelectionChanged() {
  const GitBranchHealth *branch = currentBranch();

  m_stateList->clear();
  if (!branch) {

    if (m_branchTree->topLevelItemCount() > 0) {
      m_detailLabel->setText(tr("Select a branch."));
    } else if (!m_filterEdit->text().trimmed().isEmpty()) {
      m_detailLabel->setText(
          tr("No branch matches “%1”.").arg(m_filterEdit->text().trimmed()));
    } else {
      m_detailLabel->setText(
          tr("This repository has no branches to review yet."));
    }
    m_warningLabel->clear();
    for (QPushButton *button :
         {m_pinButton, m_deleteButton, m_upstreamButton, m_graphButton}) {
      button->setEnabled(false);
    }
    return;
  }

  m_detailLabel->setText(
      tr("<b>%1</b> — tip <code>%2</code>, last activity %3.")
          .arg(branch->name.toHtmlEscaped(), branch->tipHash.left(7),
               branch->lastActivity));

  const QString warning = branch->deleteWarning();
  m_warningLabel->setText(
      warning.isEmpty()
          ? tr("✓ Fully merged into %1, so deleting it only removes a name.")
                .arg(m_report.baseRef)
          : QStringLiteral("⚠ ") + warning);

  for (GitBranchState state : branch->states) {
    QListWidgetItem *item = new QListWidgetItem(
        QStringLiteral("%1 — %2").arg(gitBranchStateName(state),
                                      gitBranchStateExplanation(state)),
        m_stateList);
    item->setFlags(Qt::ItemIsEnabled);
  }

  m_pinButton->setEnabled(!branch->isRemote);
  m_graphButton->setEnabled(true);
  m_upstreamButton->setEnabled(branch->upstreamGone);

  m_deleteButton->setEnabled(!branch->isCurrent &&
                             branch->worktreePath.isEmpty());
  applyTheme(m_theme);
}

void BranchHygieneDialog::onTogglePin() {
  const GitBranchHealth *branch = currentBranch();
  if (!branch) {
    return;
  }

  QStringList pinned = pinnedBranches();
  if (pinned.contains(branch->name)) {
    pinned.removeAll(branch->name);
  } else {
    pinned.append(branch->name);
  }
  setPinnedBranches(pinned);
  reload();
}

void BranchHygieneDialog::onDeleteSelected() {
  if (!m_git) {
    return;
  }

  for (const GitBranchHealth &branch : selectedBranches()) {
    if (branch.isCurrent || !branch.worktreePath.isEmpty()) {
      continue;
    }

    GitOperationRequest request;
    request.kind = GitOperationKind::BranchDelete;
    request.target = branch.name;
    if (!OperationPreviewDialog::confirm(m_git, request, m_theme, this)) {
      continue;
    }
    m_git->deleteBranch(branch.name, !branch.mergedIntoBase);
  }

  emit repositoryChanged();
  reload();
}

void BranchHygieneDialog::onCleanupMerged() {
  const QList<GitBranchHealth> candidates = m_report.cleanupCandidates();
  if (candidates.isEmpty() || !m_git) {
    return;
  }

  QStringList names;
  for (const GitBranchHealth &branch : candidates) {
    names << branch.name;
  }

  if (ThemedMessageBox::question(
          this, tr("Clean up merged branches"),
          tr("Delete these %1 branches?<br><br><code>%2</code><br><br><i>Every "
             "one of them is fully merged into %3, so no commit loses its "
             "last name.</i>")
              .arg(names.size())
              .arg(names.join(QStringLiteral("<br>")))
              .arg(m_report.baseRef)) != ThemedMessageBox::Yes) {
    return;
  }

  for (const QString &name : names) {
    m_git->deleteBranch(name, false);
  }
  emit repositoryChanged();
  reload();
}

void BranchHygieneDialog::onSetUpstreamOrDelete() {
  const GitBranchHealth *branch = currentBranch();
  if (!branch || !branch->upstreamGone || !m_git) {
    return;
  }

  ThemedMessageBox box(this);
  box.setIcon(ThemedMessageBox::Question);
  box.setWindowTitle(tr("Upstream is gone"));
  box.setText(tr("%1 tracks %2, which no longer exists on the remote.")
                  .arg(branch->name, branch->upstream));
  box.setInformativeText(
      tr("This usually means the branch was merged and the remote copy was "
         "deleted. You can stop tracking it and keep the branch, or delete "
         "the branch too."));
  box.setStandardButtons(ThemedMessageBox::Yes | ThemedMessageBox::No |
                         ThemedMessageBox::Cancel);
  box.setButtonText(ThemedMessageBox::Yes, tr("Stop tracking, keep branch"));
  box.setButtonText(ThemedMessageBox::No, tr("Delete the branch…"));
  box.setDefaultButton(ThemedMessageBox::Yes);

  const int choice = box.exec();
  if (choice == ThemedMessageBox::Yes) {
    m_git->unsetUpstream(branch->name);
    emit repositoryChanged();
    reload();
  } else if (choice == ThemedMessageBox::No) {
    GitOperationRequest request;
    request.kind = GitOperationKind::BranchDelete;
    request.target = branch->name;
    if (OperationPreviewDialog::confirm(m_git, request, m_theme, this)) {
      m_git->deleteBranch(branch->name, !branch->mergedIntoBase);
      emit repositoryChanged();
      reload();
    }
  }
}

void BranchHygieneDialog::onShowInGraph() {
  if (const GitBranchHealth *branch = currentBranch()) {
    emit showBranchInGraphRequested(branch->name);
  }
}

void BranchHygieneDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_branchTree) {
    m_branchTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  if (m_baseCombo) {
    m_baseCombo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }
  if (m_filterEdit) {
    m_filterEdit->setStyleSheet(UIStyleHelper::searchBoxStyle(theme));
  }
  if (m_stateList) {
    m_stateList->setStyleSheet(
        QString("QListWidget { background: %1; color: %2; border: 1px solid "
                "%3; }")
            .arg(theme.surfaceColor.name(),
                 theme.singleLineCommentFormat.name(),
                 theme.borderColor.name()));
  }
  if (m_detailLabel) {
    m_detailLabel->setStyleSheet(
        QString("color: %1;").arg(theme.foregroundColor.name()));
  }
  if (m_warningLabel) {
    const GitBranchHealth *branch = currentBranch();
    const bool safe = branch && branch->deleteWarning().isEmpty();
    m_warningLabel->setStyleSheet(
        QString("color: %1;")
            .arg((safe ? theme.successColor : theme.warningColor).name()));
  }
  for (QPushButton *button :
       {m_pinButton, m_graphButton, m_upstreamButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  for (QPushButton *button : {m_deleteButton, m_cleanupButton}) {
    if (button) {
      styleDangerButton(button);
    }
  }
}
