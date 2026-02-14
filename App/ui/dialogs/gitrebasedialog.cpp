#include "gitrebasedialog.h"
#include "../../git/gitintegration.h"
#include "../../ui/uistylehelper.h"

#include <QComboBox>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
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
    : QDialog(parent), m_git(git), m_theme(theme) {
  setWindowTitle(tr("Interactive Rebase"));
  setMinimumSize(700, 450);
  resize(800, 500);
  buildUi();
  applyTheme(theme);
}

void GitRebaseDialog::loadCommits(const QString &upstream) {
  m_upstream = upstream;
  m_commitList->clear();
  m_entries.clear();

  if (!m_git || !m_git->isValidRepository())
    return;

  // Get commits between upstream and HEAD
  QList<GitCommitInfo> commits = m_git->getCommitLog(50);

  // For interactive rebase, we only show commits from HEAD back to upstream
  // Limit to a reasonable number
  int count = qMin(commits.size(), 20);
  for (int i = 0; i < count; ++i) {
    RebaseEntry entry;
    entry.action = "pick";
    entry.hash = commits[i].shortHash;
    entry.subject = commits[i].subject;
    m_entries.append(entry);

    auto *item = new QTreeWidgetItem(m_commitList);
    auto *combo = new QComboBox(m_commitList);
    combo->addItems(REBASE_ACTIONS);
    combo->setCurrentText("pick");
    m_commitList->setItemWidget(item, 0, combo);
    item->setText(1, entry.hash);
    item->setText(2, commits[i].author);
    item->setText(3, entry.subject);
    item->setData(0, Qt::UserRole, i);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, item](int index) {
              int row = item->data(0, Qt::UserRole).toInt();
              if (row >= 0 && row < m_entries.size())
                m_entries[row].action = REBASE_ACTIONS[index];
            });
  }

  m_statusLabel->setText(tr("%1 commits loaded").arg(count));
}

void GitRebaseDialog::onMoveUp() {
  int row = m_commitList->indexOfTopLevelItem(m_commitList->currentItem());
  if (row <= 0)
    return;
  auto *item = m_commitList->takeTopLevelItem(row);
  m_commitList->insertTopLevelItem(row - 1, item);
  m_commitList->setCurrentItem(item);

  // Swap entries
  m_entries.swapItemsAt(row, row - 1);
  // Re-create combo for moved item
  auto *combo = new QComboBox(m_commitList);
  combo->addItems(REBASE_ACTIONS);
  combo->setCurrentText(m_entries[row - 1].action);
  m_commitList->setItemWidget(item, 0, combo);
  item->setData(0, Qt::UserRole, row - 1);

  // Fix the swapped item's index too
  auto *otherItem = m_commitList->topLevelItem(row);
  otherItem->setData(0, Qt::UserRole, row);
  auto *otherCombo = new QComboBox(m_commitList);
  otherCombo->addItems(REBASE_ACTIONS);
  otherCombo->setCurrentText(m_entries[row].action);
  m_commitList->setItemWidget(otherItem, 0, otherCombo);
}

void GitRebaseDialog::onMoveDown() {
  int row = m_commitList->indexOfTopLevelItem(m_commitList->currentItem());
  if (row < 0 || row >= m_commitList->topLevelItemCount() - 1)
    return;
  auto *item = m_commitList->takeTopLevelItem(row);
  m_commitList->insertTopLevelItem(row + 1, item);
  m_commitList->setCurrentItem(item);

  m_entries.swapItemsAt(row, row + 1);
  auto *combo = new QComboBox(m_commitList);
  combo->addItems(REBASE_ACTIONS);
  combo->setCurrentText(m_entries[row + 1].action);
  m_commitList->setItemWidget(item, 0, combo);
  item->setData(0, Qt::UserRole, row + 1);

  auto *otherItem = m_commitList->topLevelItem(row);
  otherItem->setData(0, Qt::UserRole, row);
  auto *otherCombo = new QComboBox(m_commitList);
  otherCombo->addItems(REBASE_ACTIONS);
  otherCombo->setCurrentText(m_entries[row].action);
  m_commitList->setItemWidget(otherItem, 0, otherCombo);
}

void GitRebaseDialog::onActionChanged(int /*index*/) {
  // Handled per-item via lambda connections
}

void GitRebaseDialog::onStartRebase() {
  if (m_entries.isEmpty())
    return;

  // Build the rebase todo script
  QString todoScript;
  // Reverse order: git rebase -i lists oldest first
  for (int i = m_entries.size() - 1; i >= 0; --i) {
    todoScript += m_entries[i].action + " " + m_entries[i].hash + " " +
                  m_entries[i].subject + "\n";
  }

  // Write to a temporary script file
  QTemporaryFile todoFile;
  todoFile.setAutoRemove(false);
  if (!todoFile.open()) {
    m_statusLabel->setText(tr("Failed to create rebase script"));
    return;
  }
  QTextStream out(&todoFile);
  out << todoScript;
  todoFile.close();

  // Use GIT_SEQUENCE_EDITOR to inject the todo script
  QString scriptPath = todoFile.fileName();

  // Execute rebase with custom sequence editor
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
    QMessageBox::information(this, tr("Rebase"),
                             tr("Interactive rebase completed."));
    accept();
  } else {
    m_statusLabel->setText(tr("Rebase failed or needs conflict resolution"));
    QMessageBox::warning(this, tr("Rebase"),
                         tr("Rebase encountered issues:\n%1").arg(output));
  }
}

void GitRebaseDialog::buildUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(6);

  auto *headerLabel = new QLabel(
      tr("Reorder, squash, or drop commits. Drag items or use ▲/▼ buttons."),
      this);
  mainLayout->addWidget(headerLabel);

  // Toolbar
  auto *toolbar = new QHBoxLayout();
  m_moveUpBtn = new QPushButton(tr("▲ Move Up"), this);
  m_moveDownBtn = new QPushButton(tr("▼ Move Down"), this);
  toolbar->addWidget(m_moveUpBtn);
  toolbar->addWidget(m_moveDownBtn);
  toolbar->addStretch();
  mainLayout->addLayout(toolbar);

  connect(m_moveUpBtn, &QPushButton::clicked, this, &GitRebaseDialog::onMoveUp);
  connect(m_moveDownBtn, &QPushButton::clicked, this,
          &GitRebaseDialog::onMoveDown);

  // Commit list
  m_commitList = new QTreeWidget(this);
  m_commitList->setHeaderLabels(
      {tr("Action"), tr("Hash"), tr("Author"), tr("Subject")});
  m_commitList->setRootIsDecorated(false);
  m_commitList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_commitList->header()->setSectionResizeMode(0, QHeaderView::Fixed);
  m_commitList->header()->resizeSection(0, 100);
  m_commitList->header()->setSectionResizeMode(1,
                                               QHeaderView::ResizeToContents);
  m_commitList->header()->setSectionResizeMode(2,
                                               QHeaderView::ResizeToContents);
  m_commitList->header()->setSectionResizeMode(3, QHeaderView::Stretch);
  mainLayout->addWidget(m_commitList);

  // Status + buttons
  m_statusLabel = new QLabel(this);
  mainLayout->addWidget(m_statusLabel);

  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();
  m_startBtn = new QPushButton(tr("Start Rebase"), this);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);
  buttonLayout->addWidget(m_startBtn);
  buttonLayout->addWidget(m_cancelBtn);
  mainLayout->addLayout(buttonLayout);

  connect(m_startBtn, &QPushButton::clicked, this,
          &GitRebaseDialog::onStartRebase);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void GitRebaseDialog::applyTheme(const Theme &theme) {
  QString bg = theme.backgroundColor.name();
  QString fg = theme.foregroundColor.name();
  QString highlight = theme.highlightColor.name();

  setStyleSheet(
      QString("QDialog { background-color: %1; color: %2; }"
              "QTreeWidget { background-color: %1; color: %2; }"
              "QLabel { color: %2; }"
              "QPushButton { background-color: %3; color: %2; "
              "border: 1px solid %4; padding: 4px 12px; }"
              "QPushButton:hover { background-color: %4; }"
              "QHeaderView::section { background-color: %3; color: %2; }")
          .arg(bg, fg, theme.lineNumberAreaColor.name(), highlight));
}

void GitRebaseDialog::updateActionForItem(QTreeWidgetItem * /*item*/,
                                          const QString & /*action*/) {
  // Reserved for drag-drop implementation
}
