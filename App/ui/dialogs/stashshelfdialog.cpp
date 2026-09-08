#include "stashshelfdialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "../widgets/flowlayout.h"
#include "themedmessagebox.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int CARD_INDEX_ROLE = Qt::UserRole + 1;
constexpr int FILE_PATH_ROLE = Qt::UserRole + 2;
} // namespace

StashShelfDialog::StashShelfDialog(GitIntegration *git, const Theme &theme,
                                   QWidget *parent)
    : StyledDialog(parent), m_git(git), m_searchEdit(nullptr),
      m_shelfList(nullptr), m_detailLabel(nullptr), m_conflictLabel(nullptr),
      m_applyExplanation(nullptr), m_fileTree(nullptr), m_applyButton(nullptr),
      m_popButton(nullptr), m_dropButton(nullptr),
      m_restoreFilesButton(nullptr), m_branchButton(nullptr),
      m_baseButton(nullptr), m_compareButton(nullptr),
      m_newStashButton(nullptr), m_closeButton(nullptr) {
  setWindowTitle(tr("Stash shelf"));
  setMinimumSize(880, 600);
  resize(1000, 680);

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  reload();
}

void StashShelfDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setObjectName(QStringLiteral("stashSearchEdit"));
  m_searchEdit->setPlaceholderText(
      tr("Search stashes by message, branch or file…"));
  m_searchEdit->setClearButtonEnabled(true);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &StashShelfDialog::onSearchChanged);
  layout->addWidget(m_searchEdit);

  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

  m_shelfList = new QListWidget(splitter);
  m_shelfList->setObjectName(QStringLiteral("stashShelfList"));
  connect(m_shelfList, &QListWidget::currentRowChanged, this,
          &StashShelfDialog::onCardSelected);
  splitter->addWidget(m_shelfList);

  QWidget *right = new QWidget(splitter);
  QVBoxLayout *rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(UiMetrics::SpaceSm);

  m_detailLabel = new QLabel(right);
  m_detailLabel->setObjectName(QStringLiteral("stashDetailLabel"));
  m_detailLabel->setWordWrap(true);
  rightLayout->addWidget(m_detailLabel);

  m_conflictLabel = new QLabel(right);
  m_conflictLabel->setObjectName(QStringLiteral("stashConflictLabel"));
  m_conflictLabel->setWordWrap(true);
  rightLayout->addWidget(m_conflictLabel);

  m_fileTree = new QTreeWidget(right);
  m_fileTree->setObjectName(QStringLiteral("stashFileTree"));
  m_fileTree->setColumnCount(3);
  m_fileTree->setHeaderLabels({tr("File"), tr("Change"), tr("± lines")});
  m_fileTree->setRootIsDecorated(false);
  m_fileTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_fileTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_fileTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_fileTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  rightLayout->addWidget(m_fileTree, 1);

  m_applyExplanation = new QLabel(right);
  m_applyExplanation->setObjectName(QStringLiteral("stashApplyExplanation"));
  m_applyExplanation->setWordWrap(true);
  rightLayout->addWidget(m_applyExplanation);

  splitter->addWidget(right);
  splitter->setSizes({300, 700});
  layout->addWidget(splitter, 1);

  QWidget *entryActions = new QWidget(this);
  entryActions->setObjectName(QStringLiteral("stashEntryActions"));
  FlowLayout *actions =
      new FlowLayout(entryActions, 0, UiMetrics::ToolbarGroupSpacing,
                     UiMetrics::ToolbarGroupSpacing);

  const auto addButton = [&](QPushButton **button, const QString &text,
                             const QString &name, const QString &tip) {
    *button = new QPushButton(text, this);
    (*button)->setObjectName(name);
    (*button)->setToolTip(tip);
    actions->addWidget(*button);
  };

  addButton(&m_applyButton, tr("Apply (keep the stash)"),
            QStringLiteral("stashApplyButton"),
            gitStashApplyExplanation(false));
  addButton(&m_popButton, tr("Pop (apply, then remove)"),
            QStringLiteral("stashPopButton"), gitStashApplyExplanation(true));
  addButton(&m_restoreFilesButton, tr("Restore selected files"),
            QStringLiteral("stashRestoreFilesButton"),
            tr("Take only the files you selected out of the stash and leave "
               "the rest of it alone."));
  addButton(&m_branchButton, tr("Branch from this stash"),
            QStringLiteral("stashBranchButton"),
            tr("Creates a branch at the commit the stash was taken on and "
               "applies it there — the reliable way to replay a stash that no "
               "longer fits here."));
  addButton(&m_baseButton, tr("Show base commit"),
            QStringLiteral("stashBaseButton"),
            tr("A stash is tied to the commit it was made on; this is that "
               "commit."));
  addButton(&m_compareButton, tr("Compare with working tree"),
            QStringLiteral("stashCompareButton"),
            tr("See what this stash holds that your files do not."));
  addButton(&m_dropButton, tr("Drop"), QStringLiteral("stashDropButton"),
            tr("Deletes the stash entry. Its commit lingers in the reflog for "
               "a while, but nothing points at it."));

  connect(m_applyButton, &QPushButton::clicked, this,
          &StashShelfDialog::onApply);
  connect(m_popButton, &QPushButton::clicked, this, &StashShelfDialog::onPop);
  connect(m_restoreFilesButton, &QPushButton::clicked, this,
          &StashShelfDialog::onRestoreSelectedFiles);
  connect(m_branchButton, &QPushButton::clicked, this,
          &StashShelfDialog::onBranchFromStash);
  connect(m_baseButton, &QPushButton::clicked, this,
          &StashShelfDialog::onShowBase);
  connect(m_compareButton, &QPushButton::clicked, this,
          &StashShelfDialog::onCompareWithWorkingTree);
  connect(m_dropButton, &QPushButton::clicked, this, &StashShelfDialog::onDrop);

  layout->addWidget(entryActions);

  QHBoxLayout *shelfActions = new QHBoxLayout();
  shelfActions->setSpacing(UiMetrics::ToolbarGroupSpacing);
  shelfActions->addStretch();

  m_newStashButton = new QPushButton(tr("New stash…"), this);
  m_newStashButton->setObjectName(QStringLiteral("stashNewButton"));
  m_newStashButton->setToolTip(
      tr("Set the current changes aside under a name you will recognise "
         "later."));
  connect(m_newStashButton, &QPushButton::clicked, this,
          &StashShelfDialog::onNewStash);
  shelfActions->addWidget(m_newStashButton);

  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("stashCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  shelfActions->addWidget(m_closeButton);

  layout->addLayout(shelfActions);
}

void StashShelfDialog::reload() {
  m_cards = buildStashShelf(m_git);

  m_shelfList->clear();
  for (int i = 0; i < m_cards.size(); ++i) {
    const GitStashCard &card = m_cards.at(i);
    QListWidgetItem *item = new QListWidgetItem(
        tr("%1  %2\n%3 · %4 files · +%5 −%6%7")
            .arg(card.selector,
                 card.message.isEmpty() ? tr("(no message)") : card.message,
                 card.branch.isEmpty() ? tr("unknown branch") : card.branch)
            .arg(card.files.size())
            .arg(card.additions())
            .arg(card.deletions())
            .arg(card.includesUntracked ? tr(" · includes untracked")
                                        : QString()),
        m_shelfList);
    item->setData(CARD_INDEX_ROLE, i);
  }

  onSearchChanged(m_searchEdit->text());
  if (m_shelfList->count() > 0) {
    m_shelfList->setCurrentRow(0);
  } else {
    onCardSelected();
  }
}

void StashShelfDialog::onSearchChanged(const QString &text) {
  const QString needle = text.trimmed();
  for (int i = 0; i < m_shelfList->count(); ++i) {
    QListWidgetItem *item = m_shelfList->item(i);
    if (needle.isEmpty()) {
      item->setHidden(false);
      continue;
    }
    const GitStashCard &card = m_cards.at(item->data(CARD_INDEX_ROLE).toInt());
    bool matches = card.message.contains(needle, Qt::CaseInsensitive) ||
                   card.branch.contains(needle, Qt::CaseInsensitive) ||
                   card.selector.contains(needle, Qt::CaseInsensitive);
    if (!matches) {
      for (const GitComparisonFile &file : card.files) {
        if (file.path.contains(needle, Qt::CaseInsensitive)) {
          matches = true;
          break;
        }
      }
    }
    item->setHidden(!matches);
  }
}

int StashShelfDialog::currentCardIndex() const {
  QListWidgetItem *item = m_shelfList ? m_shelfList->currentItem() : nullptr;
  return item ? item->data(CARD_INDEX_ROLE).toInt() : -1;
}

const GitStashCard *StashShelfDialog::currentCard() const {
  const int index = currentCardIndex();
  if (index < 0 || index >= m_cards.size()) {
    return nullptr;
  }
  return &m_cards.at(index);
}

QStringList StashShelfDialog::selectedFilePaths() const {
  QStringList paths;
  for (QTreeWidgetItem *item : m_fileTree->selectedItems()) {
    paths << item->data(0, FILE_PATH_ROLE).toString();
  }
  return paths;
}

void StashShelfDialog::onCardSelected() {
  const GitStashCard *card = currentCard();
  const bool have = card != nullptr;

  for (QPushButton *button :
       {m_applyButton, m_popButton, m_dropButton, m_restoreFilesButton,
        m_branchButton, m_baseButton, m_compareButton}) {
    button->setEnabled(have);
  }

  m_fileTree->clear();
  if (!have) {
    m_detailLabel->setText(
        tr("No stashes. A stash sets work aside without committing it."));
    m_conflictLabel->clear();
    m_applyExplanation->clear();
    return;
  }

  m_detailLabel->setText(
      tr("<b>%1</b> — %2<br>taken from <b>%3</b> %4, on top of <code>%5</code> "
         "%6<br>%7")
          .arg(card->selector,
               card->message.isEmpty() ? tr("(no message)")
                                       : card->message.toHtmlEscaped(),
               card->branch.toHtmlEscaped(), card->relativeDate,
               card->baseHash.left(7), card->baseSubject.toHtmlEscaped(),
               card->includesUntracked
                   ? tr("Untracked files are included in this stash.")
                   : tr("Untracked files were not included.")));

  for (const GitComparisonFile &file : card->files) {
    QTreeWidgetItem *item = new QTreeWidgetItem(m_fileTree);
    item->setText(0, file.path);
    item->setText(1, file.statusText());
    item->setText(
        2, QStringLiteral("+%1 −%2").arg(file.additions).arg(file.deletions));
    item->setData(0, FILE_PATH_ROLE, file.path);
  }

  const QStringList conflicts = predictStashConflicts(m_git, *card);
  const bool baseIsCurrent =
      m_git &&
      m_git->getCommitDetails(QStringLiteral("HEAD")).hash == card->baseHash;
  m_conflictLabel->setText(gitStashConflictSummary(conflicts, baseIsCurrent));
  m_applyExplanation->setText(gitStashApplyExplanation(false) +
                              QStringLiteral("<br>") +
                              gitStashApplyExplanation(true));
  applyTheme(m_theme);
}

void StashShelfDialog::onApply() {
  const GitStashCard *card = currentCard();
  if (card && m_git && m_git->stashApply(card->index)) {
    emit repositoryChanged();
    reload();
  }
}

void StashShelfDialog::onPop() {
  const GitStashCard *card = currentCard();
  if (!card || !m_git) {
    return;
  }
  if (ThemedMessageBox::question(
          this, tr("Pop this stash?"),
          QStringLiteral("%1<br><br><i>%2</i>")
              .arg(tr("Apply %1 and then remove it from the shelf?")
                       .arg(card->selector),
                   gitStashApplyExplanation(true))) != ThemedMessageBox::Yes) {
    return;
  }
  if (m_git->stashPop(card->index)) {
    emit repositoryChanged();
    reload();
  }
}

void StashShelfDialog::onDrop() {
  const GitStashCard *card = currentCard();
  if (!card || !m_git) {
    return;
  }
  if (ThemedMessageBox::question(
          this, tr("Drop this stash?"),
          tr("Delete %1 without applying it?<br><br><i>Its commit stays in "
             "the reflog for a while, but nothing will point at it.</i>")
              .arg(card->selector)) != ThemedMessageBox::Yes) {
    return;
  }
  if (m_git->stashDrop(card->index)) {
    emit repositoryChanged();
    reload();
  }
}

void StashShelfDialog::onRestoreSelectedFiles() {
  const GitStashCard *card = currentCard();
  const QStringList paths = selectedFilePaths();
  if (!card || !m_git || paths.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Nothing selected"),
        tr("Select the files you want out of this stash first."));
    return;
  }

  if (m_git->restoreStashPaths(card->index, paths)) {
    ThemedMessageBox::information(
        this, tr("Restored"),
        tr("%1 file(s) came out of %2. The stash itself is untouched.")
            .arg(paths.size())
            .arg(card->selector));
    emit repositoryChanged();
    reload();
  }
}

void StashShelfDialog::onBranchFromStash() {
  const GitStashCard *card = currentCard();
  if (!card || !m_git) {
    return;
  }

  bool ok = false;
  const QString name = QInputDialog::getText(
      this, tr("Branch from stash"),
      tr("Name for a branch at %1 with this stash applied:")
          .arg(card->baseHash.left(7)),
      QLineEdit::Normal, QStringLiteral("stash/%1").arg(card->index), &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }

  if (m_git->stashBranch(name.trimmed(), card->index)) {
    emit repositoryChanged();
    reload();
  }
}

void StashShelfDialog::onNewStash() {
  if (!m_git) {
    return;
  }

  bool ok = false;
  const QString message = QInputDialog::getText(
      this, tr("New stash"),
      tr("What is this work about? A name is what makes a stash findable "
         "later:"),
      QLineEdit::Normal, QString(), &ok);
  if (!ok) {
    return;
  }

  if (m_git->stash(message.trimmed(), true)) {
    emit repositoryChanged();
    reload();
  }
}

void StashShelfDialog::onShowBase() {
  if (const GitStashCard *card = currentCard()) {
    emit showBaseInGraphRequested(card->baseHash);
  }
}

void StashShelfDialog::onCompareWithWorkingTree() {
  if (const GitStashCard *card = currentCard()) {
    emit compareRequested(card->selector, QString());
  }
}

void StashShelfDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_searchEdit) {
    m_searchEdit->setStyleSheet(UIStyleHelper::searchBoxStyle(theme));
  }
  if (m_shelfList) {
    m_shelfList->setStyleSheet(UIStyleHelper::resultListStyle(theme));
  }
  if (m_fileTree) {
    m_fileTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  if (m_detailLabel) {
    m_detailLabel->setStyleSheet(
        QString("color: %1;").arg(theme.foregroundColor.name()));
  }
  if (m_applyExplanation) {
    styleSubduedLabel(m_applyExplanation);
  }
  if (m_conflictLabel) {
    const GitStashCard *card = currentCard();
    const bool risky = card && !predictStashConflicts(m_git, *card).isEmpty();
    m_conflictLabel->setStyleSheet(
        QString("color: %1;")
            .arg((risky ? theme.warningColor : theme.successColor).name()));
  }
  for (QPushButton *button :
       {m_popButton, m_restoreFilesButton, m_branchButton, m_baseButton,
        m_compareButton, m_newStashButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_applyButton) {
    stylePrimaryButton(m_applyButton);
  }
  if (m_dropButton) {
    styleDangerButton(m_dropButton);
  }
}
