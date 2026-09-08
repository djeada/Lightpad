#include "worktreemapdialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "themedmessagebox.h"
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int CARD_INDEX_ROLE = Qt::UserRole + 1;
}

WorktreeMapDialog::WorktreeMapDialog(GitIntegration *git, const Theme &theme,
                                     QWidget *parent)
    : StyledDialog(parent), m_git(git), m_worktreeTree(nullptr),
      m_detailLabel(nullptr), m_warningLabel(nullptr), m_createButton(nullptr),
      m_openButton(nullptr), m_graphButton(nullptr), m_pruneButton(nullptr),
      m_removeButton(nullptr), m_closeButton(nullptr) {
  setWindowTitle(tr("Worktrees"));
  setMinimumSize(840, 520);
  resize(960, 600);

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  reload();
}

void WorktreeMapDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  QLabel *header = new QLabel(
      tr("Each worktree is a separate working directory sharing this "
         "repository's history. A branch can only be checked out in one of "
         "them at a time."),
      this);
  header->setObjectName(QStringLiteral("worktreeHeaderLabel"));
  header->setWordWrap(true);
  layout->addWidget(header);

  m_worktreeTree = new QTreeWidget(this);
  m_worktreeTree->setObjectName(QStringLiteral("worktreeTree"));
  m_worktreeTree->setColumnCount(5);
  m_worktreeTree->setHeaderLabels({tr("Worktree"), tr("Checked out"),
                                   tr("State"), tr("Ahead/behind"),
                                   tr("Path")});
  m_worktreeTree->setRootIsDecorated(false);
  m_worktreeTree->header()->setSectionResizeMode(0,
                                                 QHeaderView::ResizeToContents);
  m_worktreeTree->header()->setSectionResizeMode(1,
                                                 QHeaderView::ResizeToContents);
  m_worktreeTree->header()->setSectionResizeMode(2,
                                                 QHeaderView::ResizeToContents);
  m_worktreeTree->header()->setSectionResizeMode(3,
                                                 QHeaderView::ResizeToContents);
  m_worktreeTree->header()->setSectionResizeMode(4, QHeaderView::Stretch);
  connect(m_worktreeTree, &QTreeWidget::itemSelectionChanged, this,
          &WorktreeMapDialog::onSelectionChanged);
  connect(m_worktreeTree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem *, int) { onOpen(); });
  layout->addWidget(m_worktreeTree, 1);

  m_detailLabel = new QLabel(this);
  m_detailLabel->setObjectName(QStringLiteral("worktreeDetailLabel"));
  m_detailLabel->setWordWrap(true);
  layout->addWidget(m_detailLabel);

  m_warningLabel = new QLabel(this);
  m_warningLabel->setObjectName(QStringLiteral("worktreeWarningLabel"));
  m_warningLabel->setWordWrap(true);
  layout->addWidget(m_warningLabel);

  QHBoxLayout *footer = new QHBoxLayout();
  const auto addButton = [&](QPushButton **button, const QString &text,
                             const QString &name, const QString &tip) {
    *button = new QPushButton(text, this);
    (*button)->setObjectName(name);
    (*button)->setToolTip(tip);
    footer->addWidget(*button);
  };

  addButton(&m_createButton, tr("New worktree…"),
            QStringLiteral("worktreeCreateButton"),
            tr("Check out another branch in its own directory, without "
               "disturbing this one."));
  addButton(&m_openButton, tr("Open in a new window"),
            QStringLiteral("worktreeOpenButton"),
            tr("Open this working directory in another Lightpad window."));
  addButton(&m_graphButton, tr("Show branch in graph"),
            QStringLiteral("worktreeGraphButton"),
            tr("Find the branch this worktree has checked out."));
  addButton(&m_pruneButton, tr("Prune stale entries"),
            QStringLiteral("worktreePruneButton"),
            tr("Removes administrative entries whose directory is gone. No "
               "files are touched."));
  addButton(&m_removeButton, tr("Remove worktree…"),
            QStringLiteral("worktreeRemoveButton"),
            tr("Deletes the working directory. Its uncommitted work is listed "
               "first."));

  connect(m_createButton, &QPushButton::clicked, this,
          &WorktreeMapDialog::onCreate);
  connect(m_openButton, &QPushButton::clicked, this,
          &WorktreeMapDialog::onOpen);
  connect(m_graphButton, &QPushButton::clicked, this,
          &WorktreeMapDialog::onShowInGraph);
  connect(m_pruneButton, &QPushButton::clicked, this,
          &WorktreeMapDialog::onPrune);
  connect(m_removeButton, &QPushButton::clicked, this,
          &WorktreeMapDialog::onRemove);

  footer->addStretch();
  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("worktreeCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  footer->addWidget(m_closeButton);
  layout->addLayout(footer);
}

void WorktreeMapDialog::reload() {
  m_cards = buildWorktreeMap(m_git);

  m_worktreeTree->clear();
  for (int i = 0; i < m_cards.size(); ++i) {
    const GitWorktreeCard &card = m_cards.at(i);
    QTreeWidgetItem *item = new QTreeWidgetItem(m_worktreeTree);
    item->setText(0, card.isCurrent ? tr("● %1 (this window)").arg(card.name)
                                    : card.name);
    item->setText(1, card.detached ? tr("detached %1").arg(card.shortHead())
                                   : card.branch);
    item->setText(2, card.prunable
                         ? tr("⚠ stale")
                         : (card.hasUncommittedWork() ? tr("uncommitted work")
                                                      : tr("clean")));
    item->setText(
        3, card.upstream.isEmpty()
               ? QString()
               : QStringLiteral("↑%1 ↓%2").arg(card.ahead).arg(card.behind));
    item->setText(4, card.path);
    item->setData(0, CARD_INDEX_ROLE, i);
  }

  if (m_worktreeTree->topLevelItemCount() > 0) {
    m_worktreeTree->setCurrentItem(m_worktreeTree->topLevelItem(0));
  }
  onSelectionChanged();

  bool anyPrunable = false;
  for (const GitWorktreeCard &card : m_cards) {
    anyPrunable = anyPrunable || card.prunable;
  }
  m_pruneButton->setEnabled(anyPrunable);
}

int WorktreeMapDialog::currentCardIndex() const {
  QTreeWidgetItem *item =
      m_worktreeTree ? m_worktreeTree->currentItem() : nullptr;
  return item ? item->data(0, CARD_INDEX_ROLE).toInt() : -1;
}

const GitWorktreeCard *WorktreeMapDialog::currentCard() const {
  const int index = currentCardIndex();
  if (index < 0 || index >= m_cards.size()) {
    return nullptr;
  }
  return &m_cards.at(index);
}

void WorktreeMapDialog::onSelectionChanged() {
  const GitWorktreeCard *card = currentCard();
  const bool have = card != nullptr;

  m_openButton->setEnabled(have && !card->prunable && !card->isCurrent);
  m_graphButton->setEnabled(have && !card->branch.isEmpty());

  m_removeButton->setEnabled(have && !card->isMain && !card->isCurrent);

  if (!have) {
    m_detailLabel->setText(tr("No worktrees."));
    m_warningLabel->clear();
    return;
  }

  m_detailLabel->setText(
      tr("<b>%1</b> — %2<br><code>%3</code>%4")
          .arg(card->name.toHtmlEscaped(), gitWorktreeSummary(*card),
               card->path.toHtmlEscaped(),
               card->isMain ? tr("<br>This is the main working directory.")
                            : QString()));

  const QString warning = gitWorktreeRemovalWarning(*card);
  m_warningLabel->setText(
      warning.isEmpty()
          ? tr("✓ Nothing uncommitted here, so removing it would lose "
               "nothing.")
          : QStringLiteral("⚠ ") + warning);
  applyTheme(m_theme);
}

void WorktreeMapDialog::onCreate() {
  if (!m_git) {
    return;
  }

  bool ok = false;
  const QString branch = QInputDialog::getText(
      this, tr("New worktree"),
      tr("Branch to check out there (a new one is created if it does not "
         "exist):"),
      QLineEdit::Normal, QString(), &ok);
  if (!ok || branch.trimmed().isEmpty()) {
    return;
  }

  const QDir root(m_git->repositoryPath());
  const QString suggestion = root.absoluteFilePath(
      QStringLiteral("../%1-%2")
          .arg(root.dirName(), QString(branch).replace('/', '-')));
  const QString path = QInputDialog::getText(
      this, tr("New worktree"), tr("Folder for the new working directory:"),
      QLineEdit::Normal, suggestion, &ok);
  if (!ok || path.trimmed().isEmpty()) {
    return;
  }

  const bool exists =
      m_git->getBranches().end() !=
      std::find_if(m_git->getBranches().begin(), m_git->getBranches().end(),
                   [&](const GitBranchInfo &candidate) {
                     return candidate.name == branch.trimmed();
                   });

  if (m_git->addWorktree(path.trimmed(), branch.trimmed(), !exists)) {
    emit repositoryChanged();
    reload();
  }
}

void WorktreeMapDialog::onOpen() {
  if (const GitWorktreeCard *card = currentCard()) {
    if (!card->prunable) {
      emit openWorktreeRequested(card->path);
    }
  }
}

void WorktreeMapDialog::onRemove() {
  const GitWorktreeCard *card = currentCard();
  if (!card || !m_git || card->isMain || card->isCurrent) {
    return;
  }

  const QString warning = gitWorktreeRemovalWarning(*card);
  const QString body =
      warning.isEmpty()
          ? tr("Remove the worktree at <code>%1</code>?<br><br><i>The branch "
               "itself is not deleted; only this working directory goes.</i>")
                .arg(card->path.toHtmlEscaped())
          : tr("Remove the worktree at <code>%1</code>?<br><br><b>%2</b>")
                .arg(card->path.toHtmlEscaped(), warning.toHtmlEscaped());

  if (ThemedMessageBox::question(this, tr("Remove worktree"), body) !=
      ThemedMessageBox::Yes) {
    return;
  }

  if (m_git->removeWorktree(card->path)) {
    emit repositoryChanged();
    reload();
  }
}

void WorktreeMapDialog::onPrune() {
  if (m_git && m_git->pruneWorktrees()) {
    emit repositoryChanged();
    reload();
  }
}

void WorktreeMapDialog::onShowInGraph() {
  if (const GitWorktreeCard *card = currentCard()) {
    if (!card->branch.isEmpty()) {
      emit showBranchInGraphRequested(card->branch);
    }
  }
}

void WorktreeMapDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_worktreeTree) {
    m_worktreeTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  for (const char *name : {"worktreeHeaderLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  if (m_detailLabel) {
    m_detailLabel->setStyleSheet(
        QString("color: %1;").arg(theme.foregroundColor.name()));
  }
  if (m_warningLabel) {
    const GitWorktreeCard *card = currentCard();
    const bool safe = card && gitWorktreeRemovalWarning(*card).isEmpty();
    m_warningLabel->setStyleSheet(
        QString("color: %1;")
            .arg((safe ? theme.successColor : theme.warningColor).name()));
  }
  for (QPushButton *button : {m_createButton, m_openButton, m_graphButton,
                              m_pruneButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_removeButton) {
    styleDangerButton(m_removeButton);
  }
}
