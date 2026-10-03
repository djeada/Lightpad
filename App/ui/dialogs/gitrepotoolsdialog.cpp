#include "gitrepotoolsdialog.h"
#include "themedmessagebox.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QTableWidget *makeTable(const QStringList &headers, QWidget *parent) {
  auto *table = new QTableWidget(0, headers.size(), parent);
  table->setHorizontalHeaderLabels(headers);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->horizontalHeader()->setStretchLastSection(true);
  table->verticalHeader()->setVisible(false);
  return table;
}

void addRow(QTableWidget *table, const QStringList &cells) {
  const int row = table->rowCount();
  table->insertRow(row);
  for (int c = 0; c < cells.size(); ++c) {
    table->setItem(row, c, new QTableWidgetItem(cells.at(c)));
  }
}

QString cellAt(const QTableWidget *table, int column) {
  const int row = table->currentRow();
  if (row < 0 || !table->item(row, column)) {
    return QString();
  }
  return table->item(row, column)->text();
}

bool confirm(QWidget *parent, const QString &title, const QString &text) {
  return ThemedMessageBox::question(parent, title, text) ==
         ThemedMessageBox::Yes;
}

} // namespace

GitRepoToolsDialog::GitRepoToolsDialog(GitIntegration *git, QWidget *parent)
    : StyledDialog(parent), m_git(git) {
  setWindowTitle(tr("Repository Tools"));
  resize(760, 520);

  auto *layout = new QVBoxLayout(this);
  m_tabs = new QTabWidget(this);
  m_tabs->addTab(buildRemotesTab(), tr("Remotes"));
  m_tabs->addTab(buildTagsTab(), tr("Tags"));
  m_tabs->addTab(buildSubmodulesTab(), tr("Submodules"));
  m_tabs->addTab(buildCleanTab(), tr("Untracked Files"));
  m_tabs->addTab(buildSequencerTab(), tr("In Progress && Fixups"));
  layout->addWidget(m_tabs, 1);

  m_status = new QLabel(this);
  m_status->setWordWrap(true);
  layout->addWidget(m_status);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  auto *close = new QPushButton(tr("Close"), this);
  connect(close, &QPushButton::clicked, this, &QDialog::accept);
  buttons->addWidget(close);
  layout->addLayout(buttons);

  refreshAll();
}

void GitRepoToolsDialog::showTab(Tab tab) {
  m_tabs->setCurrentIndex(static_cast<int>(tab));
}

void GitRepoToolsDialog::refreshAll() {
  refreshRemotes();
  refreshTags();
  refreshSubmodules();
  refreshClean();
  refreshSequencer();
}

void GitRepoToolsDialog::report(bool ok, const QString &message) {
  m_status->setText(message);
  if (ok) {
    emit repositoryChanged();
  }
  refreshAll();
}

QWidget *GitRepoToolsDialog::buildRemotesTab() {
  auto *page = new QWidget(this);
  auto *v = new QVBoxLayout(page);
  m_remoteTable =
      makeTable({tr("Name"), tr("Fetch URL"), tr("Push URL")}, page);
  v->addWidget(m_remoteTable, 1);

  auto *row = new QHBoxLayout();
  auto *rename = new QPushButton(tr("Rename…"), page);
  auto *seturl = new QPushButton(tr("Change URL…"), page);
  auto *prune = new QPushButton(tr("Prune Stale Branches"), page);
  auto *remove = new QPushButton(tr("Remove"), page);
  for (auto *b : {rename, seturl, prune, remove}) {
    row->addWidget(b);
  }
  row->addStretch();
  v->addLayout(row);

  connect(rename, &QPushButton::clicked, this, [this]() {
    const QString name = selectedRemote();
    if (name.isEmpty())
      return;
    bool ok = false;
    const QString next = QInputDialog::getText(this, tr("Rename Remote"),
                                               tr("New name for %1:").arg(name),
                                               QLineEdit::Normal, name, &ok);
    if (ok && !next.trimmed().isEmpty() && next != name) {
      report(m_git->renameRemote(name, next.trimmed()),
             tr("Renamed remote %1 to %2").arg(name, next.trimmed()));
    }
  });
  connect(seturl, &QPushButton::clicked, this, [this]() {
    const QString name = selectedRemote();
    if (name.isEmpty())
      return;
    bool ok = false;
    const QString url = QInputDialog::getText(
        this, tr("Change URL"), tr("URL for %1:").arg(name), QLineEdit::Normal,
        cellAt(m_remoteTable, 1), &ok);
    if (ok && !url.trimmed().isEmpty()) {
      report(m_git->setRemoteUrl(name, url.trimmed()),
             tr("Updated URL of %1").arg(name));
    }
  });
  connect(prune, &QPushButton::clicked, this, [this]() {
    const QString name = selectedRemote();
    if (!name.isEmpty()) {
      report(m_git->pruneRemote(name), tr("Pruned %1").arg(name));
    }
  });
  connect(remove, &QPushButton::clicked, this, [this]() {
    const QString name = selectedRemote();
    if (!name.isEmpty() &&
        confirm(this, tr("Remove Remote"), tr("Remove remote %1?").arg(name))) {
      report(m_git->removeRemote(name), tr("Removed remote %1").arg(name));
    }
  });
  return page;
}

QWidget *GitRepoToolsDialog::buildTagsTab() {
  auto *page = new QWidget(this);
  auto *v = new QVBoxLayout(page);
  m_tagTable = makeTable({tr("Tag"), tr("Commit"), tr("Type")}, page);
  v->addWidget(m_tagTable, 1);

  auto *row = new QHBoxLayout();
  m_tagRemote = new QComboBox(page);
  auto *create = new QPushButton(tr("New Tag…"), page);
  auto *del = new QPushButton(tr("Delete Local"), page);
  auto *push = new QPushButton(tr("Push to Remote"), page);
  auto *delRemote = new QPushButton(tr("Delete on Remote"), page);
  row->addWidget(create);
  row->addWidget(del);
  row->addStretch();
  row->addWidget(m_tagRemote);
  row->addWidget(push);
  row->addWidget(delRemote);
  v->addLayout(row);

  connect(create, &QPushButton::clicked, this, [this]() {
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("New Tag"), tr("Tag name (on HEAD):"),
                              QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
      return;
    const QString message = QInputDialog::getText(
        this, tr("New Tag"), tr("Message (leave empty for a lightweight tag):"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
      return;
    report(m_git->createTag(name.trimmed(), QString(), message.trimmed()),
           tr("Created tag %1").arg(name.trimmed()));
  });
  connect(del, &QPushButton::clicked, this, [this]() {
    const QString tag = selectedTag();
    if (!tag.isEmpty() &&
        confirm(this, tr("Delete Tag"), tr("Delete local tag %1?").arg(tag))) {
      report(m_git->deleteTag(tag), tr("Deleted tag %1").arg(tag));
    }
  });
  connect(push, &QPushButton::clicked, this, [this]() {
    const QString tag = selectedTag();
    const QString remote = m_tagRemote->currentText();
    if (!tag.isEmpty() && !remote.isEmpty()) {
      report(m_git->pushTag(remote, tag),
             tr("Pushed %1 to %2").arg(tag, remote));
    }
  });
  connect(delRemote, &QPushButton::clicked, this, [this]() {
    const QString tag = selectedTag();
    const QString remote = m_tagRemote->currentText();
    if (!tag.isEmpty() && !remote.isEmpty() &&
        confirm(
            this, tr("Delete Remote Tag"),
            tr("Delete tag %1 on %2? Others who fetched it keep their copy.")
                .arg(tag, remote))) {
      report(m_git->deleteRemoteTag(remote, tag),
             tr("Deleted %1 on %2").arg(tag, remote));
    }
  });
  return page;
}

QWidget *GitRepoToolsDialog::buildSubmodulesTab() {
  auto *page = new QWidget(this);
  auto *v = new QVBoxLayout(page);
  m_submoduleSummary = new QLabel(page);
  v->addWidget(m_submoduleSummary);
  m_submoduleTable =
      makeTable({tr("Path"), tr("Commit"), tr("State"), tr("Describe")}, page);
  v->addWidget(m_submoduleTable, 1);

  auto *row = new QHBoxLayout();
  auto *add = new QPushButton(tr("Add…"), page);
  auto *update = new QPushButton(tr("Init / Update All"), page);
  auto *sync = new QPushButton(tr("Sync URLs"), page);
  auto *remove = new QPushButton(tr("Remove"), page);
  for (auto *b : {add, update, sync, remove}) {
    row->addWidget(b);
  }
  row->addStretch();
  v->addLayout(row);

  connect(add, &QPushButton::clicked, this, [this]() {
    bool ok = false;
    const QString url =
        QInputDialog::getText(this, tr("Add Submodule"), tr("Repository URL:"),
                              QLineEdit::Normal, QString(), &ok);
    if (!ok || url.trimmed().isEmpty())
      return;
    const QString path = QInputDialog::getText(
        this, tr("Add Submodule"), tr("Path inside the repository:"),
        QLineEdit::Normal, QString(), &ok);
    if (ok && !path.trimmed().isEmpty()) {
      report(m_git->addSubmodule(url.trimmed(), path.trimmed()),
             tr("Added submodule %1").arg(path.trimmed()));
    }
  });
  connect(update, &QPushButton::clicked, this, [this]() {
    report(m_git->initUpdateSubmodules(), tr("Submodules updated"));
  });
  connect(sync, &QPushButton::clicked, this, [this]() {
    report(m_git->syncSubmodules(), tr("Submodule URLs synced"));
  });
  connect(remove, &QPushButton::clicked, this, [this]() {
    const QString path = cellAt(m_submoduleTable, 0);
    if (!path.isEmpty() &&
        confirm(this, tr("Remove Submodule"),
                tr("Remove submodule %1 from the working tree and index?")
                    .arg(path))) {
      report(m_git->removeSubmodule(path),
             tr("Removed submodule %1").arg(path));
    }
  });
  return page;
}

QWidget *GitRepoToolsDialog::buildCleanTab() {
  auto *page = new QWidget(this);
  auto *v = new QVBoxLayout(page);
  v->addWidget(new QLabel(
      tr("Untracked items that would be deleted. Select the ones to remove; "
         "this cannot be undone."),
      page));
  m_cleanList = new QListWidget(page);
  m_cleanList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  v->addWidget(m_cleanList, 1);

  auto *row = new QHBoxLayout();
  m_cleanIgnored = new QCheckBox(tr("Include ignored files"), page);
  auto *selectAll = new QPushButton(tr("Select All"), page);
  auto *remove = new QPushButton(tr("Delete Selected"), page);
  row->addWidget(m_cleanIgnored);
  row->addStretch();
  row->addWidget(selectAll);
  row->addWidget(remove);
  v->addLayout(row);

  connect(m_cleanIgnored, &QCheckBox::toggled, this,
          [this]() { refreshClean(); });
  connect(selectAll, &QPushButton::clicked, m_cleanList,
          &QListWidget::selectAll);
  connect(remove, &QPushButton::clicked, this, [this]() {
    QStringList paths;
    for (QListWidgetItem *item : m_cleanList->selectedItems()) {
      paths << item->text();
    }
    if (paths.isEmpty() ||
        !confirm(this, tr("Delete Untracked Files"),
                 tr("Permanently delete %1 item(s)?").arg(paths.size()))) {
      return;
    }
    report(m_git->cleanPaths(paths, m_cleanIgnored->isChecked()),
           tr("Deleted %1 item(s)").arg(paths.size()));
  });
  return page;
}

QWidget *GitRepoToolsDialog::buildSequencerTab() {
  auto *page = new QWidget(this);
  auto *v = new QVBoxLayout(page);

  m_sequencerLabel = new QLabel(page);
  m_sequencerLabel->setWordWrap(true);
  v->addWidget(m_sequencerLabel);
  auto *row = new QHBoxLayout();
  m_seqContinue = new QPushButton(tr("Continue"), page);
  m_seqSkip = new QPushButton(tr("Skip Commit"), page);
  m_seqAbort = new QPushButton(tr("Abort"), page);
  row->addWidget(m_seqContinue);
  row->addWidget(m_seqSkip);
  row->addWidget(m_seqAbort);
  row->addStretch();
  v->addLayout(row);
  connect(m_seqContinue, &QPushButton::clicked, this,
          [this]() { report(m_git->sequencerContinue(), tr("Continued")); });
  connect(m_seqSkip, &QPushButton::clicked, this, [this]() {
    report(m_git->sequencerSkip(), tr("Skipped the current commit"));
  });
  connect(m_seqAbort, &QPushButton::clicked, this, [this]() {
    if (confirm(this, tr("Abort"),
                tr("Abort and return to the state before it started?"))) {
      report(m_git->sequencerAbort(), tr("Aborted"));
    }
  });

  v->addSpacing(16);
  auto *title = new QLabel(tr("Autosquash fixup commits"), page);
  QFont f = title->font();
  f.setBold(true);
  title->setFont(f);
  v->addWidget(title);
  v->addWidget(new QLabel(
      tr("Fold every fixup! / squash! commit above the base into its "
         "target. Create fixups from a commit's context menu."),
      page));
  auto *baseRow = new QHBoxLayout();
  baseRow->addWidget(new QLabel(tr("Base:"), page));
  m_autosquashBase = new QLineEdit(page);
  m_autosquashBase->setPlaceholderText(
      tr("branch or commit, e.g. origin/main"));
  auto *check = new QPushButton(tr("Check"), page);
  auto *run = new QPushButton(tr("Autosquash"), page);
  baseRow->addWidget(m_autosquashBase, 1);
  baseRow->addWidget(check);
  baseRow->addWidget(run);
  v->addLayout(baseRow);
  m_fixupLabel = new QLabel(page);
  m_fixupLabel->setWordWrap(true);
  v->addWidget(m_fixupLabel);
  v->addStretch();

  connect(check, &QPushButton::clicked, this, [this]() {
    const QStringList subjects =
        m_git->pendingFixupSubjects(m_autosquashBase->text().trimmed());
    m_fixupLabel->setText(subjects.isEmpty()
                              ? tr("No fixup commits found above the base.")
                              : subjects.join('\n'));
  });
  connect(run, &QPushButton::clicked, this, [this]() {
    const QString base = m_autosquashBase->text().trimmed();
    if (base.isEmpty() ||
        !confirm(this, tr("Autosquash"),
                 tr("Rewrite history above %1 by folding fixup commits?")
                     .arg(base))) {
      return;
    }
    report(m_git->autosquashRebase(base),
           tr("Autosquashed above %1").arg(base));
  });
  return page;
}

QString GitRepoToolsDialog::selectedRemote() const {
  return cellAt(m_remoteTable, 0);
}

QString GitRepoToolsDialog::selectedTag() const {
  return cellAt(m_tagTable, 0);
}

void GitRepoToolsDialog::refreshRemotes() {
  m_remoteTable->setRowCount(0);
  m_tagRemote->clear();
  for (const GitRemoteInfo &r : m_git->getRemotes()) {
    addRow(m_remoteTable, {r.name, r.fetchUrl, r.pushUrl});
    m_tagRemote->addItem(r.name);
  }
}

void GitRepoToolsDialog::refreshTags() {
  m_tagTable->setRowCount(0);
  for (const GitTagInfo &t : m_git->getTags()) {
    addRow(m_tagTable, {t.name, t.hash.left(8),
                        t.isAnnotated ? tr("annotated") : tr("lightweight")});
  }
}

void GitRepoToolsDialog::refreshSubmodules() {
  const QList<GitSubmoduleInfo> modules = m_git->submodules();
  m_submoduleTable->setRowCount(0);
  for (const GitSubmoduleInfo &m : modules) {
    addRow(m_submoduleTable, {m.path, m.hash.left(8),
                              gitSubmoduleStateName(m.state), m.describe});
  }
  m_submoduleSummary->setText(gitSubmoduleSummary(modules));
}

void GitRepoToolsDialog::refreshClean() {
  m_cleanList->clear();
  m_cleanList->addItems(m_git->cleanPreview(m_cleanIgnored->isChecked()));
}

void GitRepoToolsDialog::refreshSequencer() {
  const auto kind = m_git->sequencerKind();
  const bool active = kind != GitIntegration::SequencerKind::None;
  m_seqContinue->setEnabled(active);
  m_seqSkip->setEnabled(active);
  m_seqAbort->setEnabled(active);
  if (!active) {
    m_sequencerLabel->setText(tr("No cherry-pick or revert is in progress."));
  } else {
    m_sequencerLabel->setText(
        kind == GitIntegration::SequencerKind::Revert
            ? tr("A revert is in progress. Resolve conflicts and stage the "
                 "files, then continue.")
            : tr("A cherry-pick is in progress. Resolve conflicts and stage "
                 "the files, then continue."));
  }
}
