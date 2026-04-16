#include "gitrebasedialog.h"
#include "../../git/gitintegration.h"
#include "../../ui/uistylehelper.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include "themedmessagebox.h"
#include <QPushButton>
#include <QTemporaryFile>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

static const QStringList REBASE_ACTIONS = {"pick",   "reword", "edit",
                                           "squash", "fixup",  "drop"};

GitRebaseDialog::GitRebaseDialog(GitIntegration *git, const Theme &theme,
                                 QWidget *parent)
    : StyledDialog(parent), m_git(git), m_theme(theme) {
  setWindowTitle(tr("Interactive Rebase"));
  setMinimumSize(800, 520);
  resize(920, 600);
  buildUi();
  applyTheme(theme);
}

void GitRebaseDialog::loadCommits(const QString &upstream) {
  m_upstream = upstream;
  m_commitList->clear();
  m_entries.clear();

  if (!m_git || !m_git->isValidRepository())
    return;

  QList<GitCommitInfo> commits = m_git->getCommitLog(50);

  int count = qMin(commits.size(), 20);
  for (int i = 0; i < count; ++i) {
    RebaseEntry entry;
    entry.action = "pick";
    entry.hash = commits[i].shortHash;
    entry.subject = commits[i].subject;
    entry.author = commits[i].author;
    m_entries.append(entry);

    auto *item = new QTreeWidgetItem(m_commitList);
    rebuildComboForItem(item, i);
    item->setText(1, entry.hash);
    item->setText(2, commits[i].author);
    item->setText(3, entry.subject);
    item->setData(0, Qt::UserRole, i);
    item->setToolTip(3, commits[i].subject);
  }

  updateSummary();
}

void GitRebaseDialog::rebuildComboForItem(QTreeWidgetItem *item, int row) {
  auto *combo = new QComboBox(m_commitList);
  combo->addItems(REBASE_ACTIONS);
  QString action =
      (row >= 0 && row < m_entries.size()) ? m_entries[row].action : "pick";
  combo->setCurrentText(action);

  QString color = actionColor(action);
  combo->setStyleSheet(
      QString("QComboBox { background: %1; color: %2; border: 1px solid "
              "%3; border-radius: 4px; padding: 2px 6px; font-weight: "
              "bold; font-size: 11px; min-width: 70px; }"
              "QComboBox:hover { border-color: %4; }"
              "QComboBox::drop-down { border: none; width: 16px; }"
              "QComboBox::down-arrow { image: none; border-left: 4px solid "
              "transparent; border-right: 4px solid transparent; border-top: "
              "5px solid %2; }"
              "QComboBox QAbstractItemView { background: %5; color: "
              "%6; selection-background-color: %4; border: 1px solid "
              "%3; }")
          .arg(m_theme.surfaceAltColor.name(), color,
               m_theme.borderColor.name(), m_theme.accentColor.name(),
               m_theme.backgroundColor.name(), m_theme.foregroundColor.name()));

  m_commitList->setItemWidget(item, 0, combo);

  connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this, item](int index) {
            int r = item->data(0, Qt::UserRole).toInt();
            if (r >= 0 && r < m_entries.size()) {
              m_entries[r].action = REBASE_ACTIONS[index];
              rebuildComboForItem(item, r);
              updateSummary();
            }
          });
}

void GitRebaseDialog::onMoveUp() {
  int row = m_commitList->indexOfTopLevelItem(m_commitList->currentItem());
  if (row <= 0)
    return;
  auto *item = m_commitList->takeTopLevelItem(row);
  m_commitList->insertTopLevelItem(row - 1, item);
  m_commitList->setCurrentItem(item);

  m_entries.swapItemsAt(row, row - 1);
  rebuildComboForItem(item, row - 1);
  item->setData(0, Qt::UserRole, row - 1);

  auto *otherItem = m_commitList->topLevelItem(row);
  otherItem->setData(0, Qt::UserRole, row);
  rebuildComboForItem(otherItem, row);
  updateSummary();
}

void GitRebaseDialog::onMoveDown() {
  int row = m_commitList->indexOfTopLevelItem(m_commitList->currentItem());
  if (row < 0 || row >= m_commitList->topLevelItemCount() - 1)
    return;
  auto *item = m_commitList->takeTopLevelItem(row);
  m_commitList->insertTopLevelItem(row + 1, item);
  m_commitList->setCurrentItem(item);

  m_entries.swapItemsAt(row, row + 1);
  rebuildComboForItem(item, row + 1);
  item->setData(0, Qt::UserRole, row + 1);

  auto *otherItem = m_commitList->topLevelItem(row);
  otherItem->setData(0, Qt::UserRole, row);
  rebuildComboForItem(otherItem, row);
  updateSummary();
}

void GitRebaseDialog::onActionChanged(int) {}

void GitRebaseDialog::onSquashSelected() {
  auto selected = m_commitList->selectedItems();
  if (selected.size() < 2) {
    ThemedMessageBox::information(this, tr("Squash"),
                                  tr("Select at least 2 commits to squash."));
    return;
  }

  for (auto *item : selected) {
    int r = item->data(0, Qt::UserRole).toInt();
    if (r >= 0 && r < m_entries.size()) {
      if (item == selected.first()) {
        m_entries[r].action = "pick";
      } else {
        m_entries[r].action = "squash";
      }
      rebuildComboForItem(item, r);
    }
  }
  updateSummary();
}

void GitRebaseDialog::onDropSelected() {
  auto selected = m_commitList->selectedItems();
  if (selected.isEmpty())
    return;

  for (auto *item : selected) {
    int r = item->data(0, Qt::UserRole).toInt();
    if (r >= 0 && r < m_entries.size()) {
      m_entries[r].action = "drop";
      rebuildComboForItem(item, r);
    }
  }
  updateSummary();
}

void GitRebaseDialog::onPickAll() {
  for (int i = 0; i < m_entries.size(); ++i) {
    m_entries[i].action = "pick";
    auto *item = m_commitList->topLevelItem(i);
    if (item)
      rebuildComboForItem(item, i);
  }
  updateSummary();
}

void GitRebaseDialog::onSearchChanged(const QString &text) {
  for (int i = 0; i < m_commitList->topLevelItemCount(); ++i) {
    auto *item = m_commitList->topLevelItem(i);
    if (text.isEmpty()) {
      item->setHidden(false);
    } else {
      bool match = item->text(1).contains(text, Qt::CaseInsensitive) ||
                   item->text(2).contains(text, Qt::CaseInsensitive) ||
                   item->text(3).contains(text, Qt::CaseInsensitive);
      item->setHidden(!match);
    }
  }
}

void GitRebaseDialog::syncEntriesFromTree() {
  QList<RebaseEntry> reordered;
  for (int i = 0; i < m_commitList->topLevelItemCount(); ++i) {
    auto *item = m_commitList->topLevelItem(i);
    int origIdx = item->data(0, Qt::UserRole).toInt();
    if (origIdx >= 0 && origIdx < m_entries.size()) {
      reordered.append(m_entries[origIdx]);
    }
  }
  m_entries = reordered;
  for (int i = 0; i < m_commitList->topLevelItemCount(); ++i) {
    m_commitList->topLevelItem(i)->setData(0, Qt::UserRole, i);
  }
}

void GitRebaseDialog::updateSummary() {
  int pick = 0, reword = 0, edit = 0, squash = 0, fixup = 0, drop = 0;
  for (const auto &e : m_entries) {
    if (e.action == "pick")
      ++pick;
    else if (e.action == "reword")
      ++reword;
    else if (e.action == "edit")
      ++edit;
    else if (e.action == "squash")
      ++squash;
    else if (e.action == "fixup")
      ++fixup;
    else if (e.action == "drop")
      ++drop;
  }

  QStringList parts;
  if (pick > 0)
    parts << QString("<span style='color:%1'>%2 pick</span>").arg(m_theme.successColor.name()).arg(pick);
  if (reword > 0)
    parts
        << QString("<span style='color:%1'>%2 reword</span>").arg(m_theme.warningColor.name()).arg(reword);
  if (edit > 0)
    parts << QString("<span style='color:%1'>%2 edit</span>").arg(m_theme.accentColor.name()).arg(edit);
  if (squash > 0)
    parts
        << QString("<span style='color:" + m_theme.accentColor.lighter(130).name() + "'>%1 squash</span>").arg(squash);
  if (fixup > 0)
    parts << QString("<span style='color:%1'>%2 fixup</span>").arg(m_theme.singleLineCommentFormat.name()).arg(fixup);
  if (drop > 0)
    parts << QString("<span style='color:%1'>%2 drop</span>").arg(m_theme.errorColor.name()).arg(drop);

  if (m_summaryLabel) {
    m_summaryLabel->setText(
        tr("Plan: %1 commits — %2")
            .arg(m_entries.size())
            .arg(parts.isEmpty() ? tr("none") : parts.join(" · ")));
  }

  bool hasWork = false;
  for (const auto &e : m_entries) {
    if (e.action != "pick") {
      hasWork = true;
      break;
    }
  }
  m_startBtn->setEnabled(!m_entries.isEmpty());
  m_statusLabel->setText(
      hasWork
          ? tr("Ready — rebase plan has modifications")
          : tr("%1 commits loaded (all set to pick)").arg(m_entries.size()));
}

void GitRebaseDialog::onStartRebase() {
  if (m_entries.isEmpty())
    return;

  int dropCount = 0;
  int squashCount = 0;
  for (const auto &e : m_entries) {
    if (e.action == "drop")
      ++dropCount;
    if (e.action == "squash" || e.action == "fixup")
      ++squashCount;
  }

  QString warning;
  if (dropCount > 0 && squashCount > 0) {
    warning = tr("This rebase will drop %1 commit(s) and squash %2 commit(s).\n"
                 "This rewrites history and cannot be easily undone.")
                  .arg(dropCount)
                  .arg(squashCount);
  } else if (dropCount > 0) {
    warning = tr("This rebase will drop %1 commit(s).\n"
                 "This rewrites history and cannot be easily undone.")
                  .arg(dropCount);
  } else if (squashCount > 0) {
    warning = tr("This rebase will squash %1 commit(s).\n"
                 "This rewrites history and cannot be easily undone.")
                  .arg(squashCount);
  } else {
    warning = tr("This will rewrite commit history.\n"
                 "Are you sure you want to proceed?");
  }

  ThemedMessageBox confirmBox(this);
  confirmBox.setIcon(ThemedMessageBox::Warning);
  confirmBox.setWindowTitle(tr("Confirm Rebase"));
  confirmBox.setText(warning);
  confirmBox.setStandardButtons(ThemedMessageBox::Yes | ThemedMessageBox::No);
  confirmBox.setDefaultButton(ThemedMessageBox::No);
  if (confirmBox.exec() != ThemedMessageBox::Yes)
    return;

  QString todoScript;

  for (int i = m_entries.size() - 1; i >= 0; --i) {
    todoScript += m_entries[i].action + " " + m_entries[i].hash + " " +
                  m_entries[i].subject + "\n";
  }

  QTemporaryFile todoFile;
  todoFile.setAutoRemove(false);
  if (!todoFile.open()) {
    m_statusLabel->setText(tr("Failed to create rebase script"));
    return;
  }
  QTextStream out(&todoFile);
  out << todoScript;
  todoFile.close();

  QString scriptPath = todoFile.fileName();

  QProcess proc;
  proc.setWorkingDirectory(m_git->repositoryPath());
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GIT_SEQUENCE_EDITOR", QString("cp %1").arg(scriptPath));
  proc.setProcessEnvironment(env);
  proc.start("git", {"rebase", "-i", m_upstream});
  proc.waitForFinished(10000);

  QFile::remove(scriptPath);

  QString output = proc.readAllStandardOutput() + proc.readAllStandardError();
  if (proc.exitCode() == 0) {
    m_statusLabel->setText(tr("Rebase completed successfully"));
    ThemedMessageBox::information(this, tr("Rebase"),
                                  tr("Interactive rebase completed."));
    accept();
  } else {
    m_statusLabel->setText(tr("Rebase failed or needs conflict resolution"));
    ThemedMessageBox::warning(this, tr("Rebase"),
                              tr("Rebase encountered issues:\n%1").arg(output));
  }
}

void GitRebaseDialog::buildUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(12, 12, 12, 12);
  mainLayout->setSpacing(8);

  auto *titleLabel = new QLabel(tr("🔀 Interactive Rebase"), this);
  titleLabel->setStyleSheet(UIStyleHelper::titleLabelStyle(m_theme));
  mainLayout->addWidget(titleLabel);

  auto *headerLabel = new QLabel(
      tr("Reorder, squash, or drop commits. Use ▲/▼ buttons to reorder, or "
         "select multiple commits for batch operations."),
      this);
  headerLabel->setWordWrap(true);
  headerLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(m_theme));
  mainLayout->addWidget(headerLabel);

  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setPlaceholderText(
      tr("🔍 Filter commits by hash, author, or message..."));
  m_searchEdit->setClearButtonEnabled(true);
  mainLayout->addWidget(m_searchEdit);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &GitRebaseDialog::onSearchChanged);

  auto *toolbar = new QHBoxLayout();
  toolbar->setSpacing(6);

  m_moveUpBtn = new QPushButton(tr("▲ Up"), this);
  m_moveDownBtn = new QPushButton(tr("▼ Down"), this);
  m_squashBtn = new QPushButton(tr("📦 Squash"), this);
  m_dropBtn = new QPushButton(tr("🗑 Drop"), this);
  m_pickAllBtn = new QPushButton(tr("↺ Reset All"), this);

  m_moveUpBtn->setToolTip(tr("Move selected commit up (earlier in history)"));
  m_moveDownBtn->setToolTip(tr("Move selected commit down (later in history)"));
  m_squashBtn->setToolTip(
      tr("Mark selected commits for squashing (combine into one)"));
  m_dropBtn->setToolTip(
      tr("Mark selected commits for dropping (remove from history)"));
  m_pickAllBtn->setToolTip(tr("Reset all actions back to 'pick'"));

  toolbar->addWidget(m_moveUpBtn);
  toolbar->addWidget(m_moveDownBtn);
  toolbar->addSpacing(8);
  toolbar->addWidget(m_squashBtn);
  toolbar->addWidget(m_dropBtn);
  toolbar->addStretch();
  toolbar->addWidget(m_pickAllBtn);
  mainLayout->addLayout(toolbar);

  connect(m_moveUpBtn, &QPushButton::clicked, this, &GitRebaseDialog::onMoveUp);
  connect(m_moveDownBtn, &QPushButton::clicked, this,
          &GitRebaseDialog::onMoveDown);
  connect(m_squashBtn, &QPushButton::clicked, this,
          &GitRebaseDialog::onSquashSelected);
  connect(m_dropBtn, &QPushButton::clicked, this,
          &GitRebaseDialog::onDropSelected);
  connect(m_pickAllBtn, &QPushButton::clicked, this,
          &GitRebaseDialog::onPickAll);

  m_commitList = new QTreeWidget(this);
  m_commitList->setHeaderLabels(
      {tr("Action"), tr("Hash"), tr("Author"), tr("Subject")});
  m_commitList->setRootIsDecorated(false);
  m_commitList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_commitList->setDragDropMode(QAbstractItemView::InternalMove);
  m_commitList->setDragEnabled(true);
  m_commitList->setAcceptDrops(true);
  m_commitList->setDropIndicatorShown(true);
  m_commitList->setDefaultDropAction(Qt::MoveAction);
  m_commitList->setAlternatingRowColors(true);
  m_commitList->header()->setSectionResizeMode(0, QHeaderView::Fixed);
  m_commitList->header()->resizeSection(0, 110);
  m_commitList->header()->setSectionResizeMode(1,
                                               QHeaderView::ResizeToContents);
  m_commitList->header()->setSectionResizeMode(2,
                                               QHeaderView::ResizeToContents);
  m_commitList->header()->setSectionResizeMode(3, QHeaderView::Stretch);

  connect(m_commitList->model(), &QAbstractItemModel::rowsMoved, [this]() {
    syncEntriesFromTree();
    for (int i = 0; i < m_commitList->topLevelItemCount(); ++i) {
      rebuildComboForItem(m_commitList->topLevelItem(i), i);
    }
    updateSummary();
  });

  mainLayout->addWidget(m_commitList, 1);

  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setTextFormat(Qt::RichText);
  mainLayout->addWidget(m_summaryLabel);

  m_statusLabel = new QLabel(this);
  mainLayout->addWidget(m_statusLabel);

  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();
  m_cancelBtn = new QPushButton(tr("Cancel"), this);
  m_startBtn = new QPushButton(tr("▶ Start Rebase"), this);
  buttonLayout->addWidget(m_cancelBtn);
  buttonLayout->addWidget(m_startBtn);
  mainLayout->addLayout(buttonLayout);

  connect(m_startBtn, &QPushButton::clicked, this,
          &GitRebaseDialog::onStartRebase);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void GitRebaseDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);

  stylePrimaryButton(m_startBtn);
  styleDangerButton(m_dropBtn);

  if (m_commitList) {
    m_commitList->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }

  styleSubduedLabel(m_summaryLabel);
  styleSubduedLabel(m_statusLabel);
}

void GitRebaseDialog::updateActionForItem(QTreeWidgetItem *, const QString &) {}

QString GitRebaseDialog::actionColor(const QString &action) const {
  if (action == "pick") return m_theme.successColor.name();
  if (action == "reword") return m_theme.warningColor.name();
  if (action == "edit") return m_theme.accentColor.name();
  if (action == "squash") return m_theme.accentColor.lighter(130).name();
  if (action == "fixup") return m_theme.singleLineCommentFormat.name();
  if (action == "drop") return m_theme.errorColor.name();
  return m_theme.foregroundColor.name();
}
