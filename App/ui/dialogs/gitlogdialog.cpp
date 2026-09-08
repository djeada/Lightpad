#include "gitlogdialog.h"
#include "../../git/gitintegration.h"
#include "../../ui/uistylehelper.h"
#include "../widgets/gitgraphwidget.h"

#include "../uimetrics.h"
#include "operationpreviewdialog.h"
#include "themedmessagebox.h"
#include <QCheckBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

GitLogDialog::GitLogDialog(GitIntegration *git, const Theme &theme,
                           QWidget *parent)
    : StyledDialog(parent), m_git(git), m_theme(theme), m_compareLabel(nullptr),
      m_firstParentCheck(nullptr), m_allBranchesCheck(nullptr),
      m_zoomInButton(nullptr), m_zoomOutButton(nullptr),
      m_zoomResetButton(nullptr), m_detailFiles(nullptr),
      m_detailDiffButton(nullptr), m_detailBranchButton(nullptr),
      m_detailTagButton(nullptr), m_detailCherryPickButton(nullptr),
      m_detailRevertButton(nullptr), m_detailResetButton(nullptr),
      m_detailCompareHeadButton(nullptr) {
  setWindowTitle(tr("Commit Graph"));
  setMinimumSize(860, 520);
  resize(1080, 660);
  buildUi();
  applyTheme(theme);
  loadCommits();
}

void GitLogDialog::setFilePath(const QString &filePath) {
  m_filePath = filePath;
  loadCommits();
}

void GitLogDialog::refresh() { loadCommits(); }

void GitLogDialog::selectCommit(const QString &hash) {
  if (hash.isEmpty()) {
    return;
  }
  m_graphWidget->selectCommit(hash, true);
  showCommitDetails(hash);
}

void GitLogDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    close();
    return;
  }
  QDialog::keyPressEvent(event);
}

void GitLogDialog::buildUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(6);

  m_searchField = new QLineEdit(this);
  m_searchField->setObjectName(QStringLiteral("graphSearchField"));
  m_searchField->setPlaceholderText(
      tr("Filter by subject, author, hash, date, branch or tag…"));
  m_searchField->setClearButtonEnabled(true);
  connect(m_searchField, &QLineEdit::textChanged, this,
          &GitLogDialog::onSearchChanged);
  mainLayout->addWidget(m_searchField);

  buildControlBar(mainLayout);

  m_tabWidget = new QTabWidget(this);

  auto *graphTab = new QWidget(this);
  auto *graphLayout = new QVBoxLayout(graphTab);
  graphLayout->setContentsMargins(0, 0, 0, 0);

  m_graphWidget = new GitGraphWidget(m_git, m_theme, graphTab);
  connect(m_graphWidget, &GitGraphWidget::commitSelected, this,
          &GitLogDialog::onGraphCommitSelected);
  connect(m_graphWidget, &GitGraphWidget::commitDoubleClicked, this,
          [this](const QString &hash) { emit viewCommitDiff(hash); });
  connect(m_graphWidget, &GitGraphWidget::commitsAppended, this,
          [this](int totalLoaded) {
            m_statusLabel->setText(tr("%1 commits loaded").arg(totalLoaded));
          });
  connect(m_graphWidget, &GitGraphWidget::contextMenuRequested, this,
          &GitLogDialog::onGraphContextMenu);
  connect(m_graphWidget, &GitGraphWidget::workingTreeSelected, this,
          &GitLogDialog::onWorkingTreeSelected);
  connect(m_graphWidget, &GitGraphWidget::compareAnchorChanged, this,
          &GitLogDialog::onCompareAnchorChanged);
  connect(m_graphWidget, &GitGraphWidget::compareRequested, this,
          &GitLogDialog::compareRequested);

  auto *graphSplitter = new QSplitter(Qt::Vertical, graphTab);
  graphSplitter->addWidget(m_graphWidget);

  QWidget *detailPane = new QWidget(graphTab);
  QVBoxLayout *detailLayout = new QVBoxLayout(detailPane);
  detailLayout->setContentsMargins(0, 0, 0, 0);
  detailLayout->setSpacing(UiMetrics::SpaceSm);

  QSplitter *detailSplitter = new QSplitter(Qt::Horizontal, detailPane);
  m_detailView = new QTextEdit(detailSplitter);
  m_detailView->setObjectName(QStringLiteral("graphDetailView"));
  m_detailView->setReadOnly(true);
  detailSplitter->addWidget(m_detailView);

  m_detailFiles = new QTreeWidget(detailSplitter);
  m_detailFiles->setObjectName(QStringLiteral("graphDetailFiles"));
  m_detailFiles->setColumnCount(2);
  m_detailFiles->setHeaderLabels({tr("Changed file"), tr("± lines")});
  m_detailFiles->setRootIsDecorated(false);
  m_detailFiles->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_detailFiles->header()->setSectionResizeMode(1,
                                                QHeaderView::ResizeToContents);
  detailSplitter->addWidget(m_detailFiles);
  detailSplitter->setSizes({420, 320});
  detailLayout->addWidget(detailSplitter, 1);

  buildDetailActions(detailPane, detailLayout);

  m_splitter = graphSplitter;
  graphSplitter->addWidget(detailPane);
  graphSplitter->setStretchFactor(0, 3);
  graphSplitter->setStretchFactor(1, 2);
  m_commitTree = nullptr;

  graphLayout->addWidget(graphSplitter);
  m_tabWidget->addTab(graphTab, tr("Graph"));

  auto *listTab = new QWidget(this);
  auto *listLayout = new QVBoxLayout(listTab);
  listLayout->setContentsMargins(0, 0, 0, 0);

  m_commitTree = new QTreeWidget(listTab);
  m_commitTree->setHeaderLabels(
      {tr("Hash"), tr("Subject"), tr("Author"), tr("Date")});
  m_commitTree->setRootIsDecorated(false);
  m_commitTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_commitTree->setAlternatingRowColors(true);
  m_commitTree->setContextMenuPolicy(Qt::CustomContextMenu);
  m_commitTree->header()->setStretchLastSection(false);
  m_commitTree->header()->setSectionResizeMode(0,
                                               QHeaderView::ResizeToContents);
  m_commitTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_commitTree->header()->setSectionResizeMode(2,
                                               QHeaderView::ResizeToContents);
  m_commitTree->header()->setSectionResizeMode(3,
                                               QHeaderView::ResizeToContents);
  connect(m_commitTree, &QTreeWidget::currentItemChanged, this,
          &GitLogDialog::onCommitSelected);
  connect(m_commitTree, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            auto *item = m_commitTree->itemAt(pos);
            if (item) {
              QString hash = item->data(0, Qt::UserRole).toString();
              showContextMenuForCommit(
                  hash, m_commitTree->viewport()->mapToGlobal(pos));
            }
          });

  listLayout->addWidget(m_commitTree);
  m_tabWidget->addTab(listTab, tr("List"));
  m_tabWidget->setCurrentIndex(0);

  mainLayout->addWidget(m_tabWidget);

  m_statusLabel = new QLabel(this);
  m_statusLabel->setStyleSheet(
      QStringLiteral("color: palette(mid); font-size: 11px;"));
  mainLayout->addWidget(m_statusLabel);
}

void GitLogDialog::buildControlBar(QVBoxLayout *layout) {
  QHBoxLayout *bar = new QHBoxLayout();
  bar->setSpacing(UiMetrics::ToolbarSpacing);

  m_allBranchesCheck = new QCheckBox(tr("All branches"), this);
  m_allBranchesCheck->setObjectName(QStringLiteral("graphAllBranchesCheck"));
  m_allBranchesCheck->setChecked(true);
  m_allBranchesCheck->setToolTip(
      tr("Show every ref's history, not only what is reachable from HEAD.\n"
         "Unchecking focuses the current branch.\ngit log --all"));
  connect(m_allBranchesCheck, &QCheckBox::toggled, this,
          &GitLogDialog::onScopeChanged);
  bar->addWidget(m_allBranchesCheck);

  m_firstParentCheck = new QCheckBox(tr("First parent only"), this);
  m_firstParentCheck->setObjectName(QStringLiteral("graphFirstParentCheck"));
  m_firstParentCheck->setToolTip(
      tr("Follow only the first parent of each merge, which collapses "
         "merged-in side branches into a single line.\n"
         "git log --first-parent"));
  connect(m_firstParentCheck, &QCheckBox::toggled, this,
          &GitLogDialog::onScopeChanged);
  bar->addWidget(m_firstParentCheck);

  bar->addStretch();

  m_compareLabel = new QLabel(this);
  m_compareLabel->setObjectName(QStringLiteral("graphCompareLabel"));
  bar->addWidget(m_compareLabel);

  const auto makeZoom = [&](QToolButton **button, const QString &text,
                            const QString &name, const QString &tip) {
    *button = new QToolButton(this);
    (*button)->setObjectName(name);
    (*button)->setText(text);
    (*button)->setToolTip(tip);
    bar->addWidget(*button);
  };
  makeZoom(&m_zoomOutButton, QStringLiteral("−"),
           QStringLiteral("graphZoomOutButton"),
           tr("Show more commits in the same space (Ctrl+scroll)"));
  makeZoom(&m_zoomResetButton, QStringLiteral("⌾"),
           QStringLiteral("graphZoomResetButton"), tr("Reset row height"));
  makeZoom(&m_zoomInButton, QStringLiteral("+"),
           QStringLiteral("graphZoomInButton"),
           tr("Give each commit more room"));
  connect(m_zoomOutButton, &QToolButton::clicked, this,
          [this]() { m_graphWidget->zoomOut(); });
  connect(m_zoomResetButton, &QToolButton::clicked, this,
          [this]() { m_graphWidget->resetZoom(); });
  connect(m_zoomInButton, &QToolButton::clicked, this,
          [this]() { m_graphWidget->zoomIn(); });

  layout->addLayout(bar);
}

void GitLogDialog::buildDetailActions(QWidget *parent, QVBoxLayout *layout) {
  QHBoxLayout *actions = new QHBoxLayout();
  actions->setSpacing(UiMetrics::ToolbarGroupSpacing);

  const auto makeButton = [&](QPushButton **button, const QString &text,
                              const QString &name, const QString &tip) {
    *button = new QPushButton(text, parent);
    (*button)->setObjectName(name);
    (*button)->setToolTip(tip);
    actions->addWidget(*button);
  };

  makeButton(&m_detailDiffButton, tr("View diff"),
             QStringLiteral("graphViewDiffButton"),
             tr("Show what this commit changed"));
  makeButton(&m_detailCompareHeadButton, tr("Compare with HEAD"),
             QStringLiteral("graphCompareHeadButton"),
             tr("Diff this commit against the current checkout"));
  makeButton(&m_detailBranchButton, tr("Branch here…"),
             QStringLiteral("graphBranchButton"),
             tr("Create a branch pointing at this commit.\n"
                "A branch is just a movable name for a commit."));
  makeButton(&m_detailTagButton, tr("Tag here…"),
             QStringLiteral("graphTagButton"),
             tr("Create a tag pointing at this commit"));
  makeButton(&m_detailCherryPickButton, tr("Cherry-pick"),
             QStringLiteral("graphCherryPickButton"),
             tr("Replay this commit's change on the current branch"));
  makeButton(&m_detailRevertButton, tr("Revert"),
             QStringLiteral("graphRevertButton"),
             tr("Add a new commit that undoes this one"));
  makeButton(&m_detailResetButton, tr("Reset branch here…"),
             QStringLiteral("graphResetButton"),
             tr("Move the current branch to this commit"));

  actions->addStretch();
  layout->addLayout(actions);

  connect(m_detailDiffButton, &QPushButton::clicked, this, [this]() {
    if (!m_selectedHash.isEmpty()) {
      emit viewCommitDiff(m_selectedHash);
    }
  });
  connect(m_detailCompareHeadButton, &QPushButton::clicked, this, [this]() {
    if (!m_selectedHash.isEmpty()) {
      emit compareRequested(m_selectedHash, QStringLiteral("HEAD"));
    }
  });
  connect(m_detailBranchButton, &QPushButton::clicked, this,
          [this]() { createBranchAt(m_selectedHash); });
  connect(m_detailTagButton, &QPushButton::clicked, this,
          [this]() { createTagAt(m_selectedHash); });
  connect(m_detailCherryPickButton, &QPushButton::clicked, this, [this]() {
    if (m_selectedHash.isEmpty() || !m_git) {
      return;
    }
    if (ThemedMessageBox::question(this, tr("Cherry-pick"),
                                   tr("Replay %1 on the current branch?")
                                       .arg(m_selectedHash.left(7))) ==
        ThemedMessageBox::Yes) {
      m_git->cherryPick(m_selectedHash);
      emit repositoryChanged();
      loadCommits();
    }
  });
  connect(m_detailRevertButton, &QPushButton::clicked, this, [this]() {
    if (m_selectedHash.isEmpty() || !m_git) {
      return;
    }
    GitOperationRequest request;
    request.kind = GitOperationKind::Revert;
    request.target = m_selectedHash;
    if (OperationPreviewDialog::confirm(m_git, request, m_theme, this)) {
      m_git->revertCommit(m_selectedHash);
      emit repositoryChanged();
      loadCommits();
    }
  });
  connect(m_detailResetButton, &QPushButton::clicked, this,
          [this]() { resetTo(m_selectedHash); });

  updateDetailActions(false);
}

GitLogOptions GitLogDialog::currentLogOptions() const {
  GitLogOptions options;
  options.pathFilter = m_filePath;
  options.allRefs = m_allBranchesCheck && m_allBranchesCheck->isChecked();
  options.firstParentOnly =
      m_firstParentCheck && m_firstParentCheck->isChecked();
  return options;
}

void GitLogDialog::onScopeChanged() {
  if (m_updatingScope) {
    return;
  }
  loadCommits();
}

void GitLogDialog::onCompareAnchorChanged(const QString &hash) {
  if (!m_compareLabel) {
    return;
  }
  m_compareLabel->setText(
      hash.isEmpty()
          ? tr("Ctrl+click or Space marks two commits to compare")
          : tr("Comparing from %1 — pick the second commit").arg(hash.left(7)));
}

void GitLogDialog::onWorkingTreeSelected() {
  m_selectedHash.clear();
  showWorkingTreeDetails();
  updateDetailActions(false);
  emit workingTreeRequested();
}

void GitLogDialog::showWorkingTreeDetails() {
  if (!m_git) {
    return;
  }
  const GitRepositoryState state = m_git->repositoryState();

  QString html = QStringLiteral("<b>%1</b><br>%2")
                     .arg(tr("Uncommitted changes").toHtmlEscaped(),
                          gitRepositoryStateSummary(state).toHtmlEscaped());
  html += QStringLiteral("<br><br><i>%1</i>")
              .arg(tr("These changes are not in the commit graph yet. They "
                      "become a node only when you commit them.")
                       .toHtmlEscaped());
  m_detailView->setHtml(html);

  m_detailFiles->clear();
  m_detailFiles->setHeaderLabels({tr("Changed file"), tr("State")});
  for (const GitFileInfo &info : m_git->getStatus()) {
    QTreeWidgetItem *item = new QTreeWidgetItem(m_detailFiles);
    item->setText(0, info.filePath);

    const bool staged = info.indexStatus != GitFileStatus::Clean &&
                        info.indexStatus != GitFileStatus::Untracked;
    item->setText(1, staged ? tr("staged") : tr("unstaged"));
    item->setToolTip(0, info.filePath);
  }
}

void GitLogDialog::updateDetailActions(bool commitSelected) {
  for (QPushButton *button :
       {m_detailDiffButton, m_detailCompareHeadButton, m_detailBranchButton,
        m_detailTagButton, m_detailCherryPickButton, m_detailRevertButton,
        m_detailResetButton}) {
    if (button) {
      button->setEnabled(commitSelected);
    }
  }
}

void GitLogDialog::createBranchAt(const QString &hash) {
  if (hash.isEmpty() || !m_git) {
    return;
  }
  bool ok = false;
  const QString name = QInputDialog::getText(
      this, tr("Branch here"), tr("New branch at %1:").arg(hash.left(7)),
      QLineEdit::Normal, QString(), &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }
  if (m_git->createBranchFromCommit(name.trimmed(), hash, false)) {
    emit repositoryChanged();
    loadCommits();
  }
}

void GitLogDialog::createTagAt(const QString &hash) {
  if (hash.isEmpty() || !m_git) {
    return;
  }
  bool ok = false;
  const QString name = QInputDialog::getText(
      this, tr("Tag here"), tr("New tag at %1:").arg(hash.left(7)),
      QLineEdit::Normal, QString(), &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }
  if (m_git->createTag(name.trimmed(), hash)) {
    emit repositoryChanged();
    loadCommits();
  }
}

void GitLogDialog::resetTo(const QString &hash) {
  if (hash.isEmpty() || !m_git) {
    return;
  }

  QStringList modes{tr("Soft — keep the index and working tree"),
                    tr("Mixed — keep the working tree, clear the index"),
                    tr("Hard — discard everything since this commit")};
  bool ok = false;
  const QString chosen = QInputDialog::getItem(
      this, tr("Reset branch here"),
      tr("Move the current branch to %1 how?").arg(hash.left(7)), modes, 1,
      false, &ok);
  if (!ok) {
    return;
  }

  const int index = modes.indexOf(chosen);
  const QString mode = index == 0   ? QStringLiteral("soft")
                       : index == 2 ? QStringLiteral("hard")
                                    : QStringLiteral("mixed");

  GitOperationRequest request;
  request.kind = GitOperationKind::Reset;
  request.target = hash;
  request.resetMode = mode;
  if (!OperationPreviewDialog::confirm(m_git, request, m_theme, this)) {
    return;
  }

  if (m_git->resetToCommit(hash, mode)) {
    emit repositoryChanged();
    loadCommits();
  }
}

void GitLogDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);

  if (m_commitTree) {
    m_commitTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  if (m_detailFiles) {
    m_detailFiles->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  if (m_detailView) {
    m_detailView->setStyleSheet(
        QString(
            "QTextEdit { background: %1; color: %2; border: 1px solid %3; }")
            .arg(theme.surfaceColor.name(), theme.foregroundColor.name(),
                 theme.borderColor.name()));
  }
  if (m_compareLabel) {
    styleSubduedLabel(m_compareLabel);
  }
  for (QPushButton *button :
       {m_detailDiffButton, m_detailCompareHeadButton, m_detailBranchButton,
        m_detailTagButton, m_detailCherryPickButton, m_detailRevertButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_detailResetButton) {
    styleDangerButton(m_detailResetButton);
  }

  if (m_splitter) {
    m_splitter->setStyleSheet(QString("QSplitter::handle { background: %1; }")
                                  .arg(theme.borderColor.name()));
  }
}

void GitLogDialog::loadCommits() {
  if (m_commitTree) {
    m_commitTree->clear();
  }

  if (!m_git || !m_git->isValidRepository()) {
    m_statusLabel->setText(tr("No valid repository"));
    return;
  }

  QList<GitCommitInfo> commits;
  if (!m_filePath.isEmpty()) {
    commits = m_git->getCommitLog(100, m_filePath);
  } else {
    commits = m_git->getCommitLog(100);
  }

  for (const GitCommitInfo &commit : commits) {
    auto *item = new QTreeWidgetItem(m_commitTree);
    item->setText(0, commit.shortHash);
    item->setText(1, commit.subject);
    item->setText(2, commit.author);
    item->setText(3, commit.relativeDate);
    item->setData(0, Qt::UserRole, commit.hash);
    item->setToolTip(1, commit.subject);
  }

  m_graphWidget->setLogOptions(currentLogOptions());
  m_graphWidget->setWorkingTreeState(m_git->repositoryState());
  onCompareAnchorChanged(m_graphWidget->compareAnchor());

  if (m_graphWidget->hasWorkingTreeNode()) {
    showWorkingTreeDetails();
    updateDetailActions(false);
  } else if (!commits.isEmpty()) {
    m_graphWidget->selectCommit(commits.first().hash);
    showCommitDetails(commits.first().hash);
  }

  m_statusLabel->setText(tr("%1 commits").arg(commits.size()));
}

void GitLogDialog::showCommitDetails(const QString &hash) {
  if (!m_git || hash.isEmpty()) {
    return;
  }

  GitCommitInfo details = m_git->getCommitDetails(hash);

  QString html = QStringLiteral("<table width=\"100%\" cellspacing=\"0\">");
  html +=
      QStringLiteral("<tr><td><b>Commit</b></td><td><code>%1</code></td></tr>")
          .arg(details.hash.left(12).toHtmlEscaped());
  html +=
      QStringLiteral("<tr><td><b>Author</b></td><td>%1 &lt;%2&gt;</td></tr>")
          .arg(details.author.toHtmlEscaped(),
               details.authorEmail.toHtmlEscaped());
  html += QStringLiteral("<tr><td><b>Date</b></td><td>%1 (%2)</td></tr>")
              .arg(details.date.toHtmlEscaped(),
                   details.relativeDate.toHtmlEscaped());

  if (!details.parents.isEmpty()) {
    QStringList shortParents;
    for (const QString &parent : details.parents) {
      shortParents << parent.left(7);
    }
    html += QStringLiteral("<tr><td><b>%1</b></td><td><code>%2</code>%3</td>"
                           "</tr>")
                .arg(details.parents.size() > 1 ? tr("Parents") : tr("Parent"),
                     shortParents.join(QStringLiteral(", ")),
                     details.parents.size() > 1
                         ? QStringLiteral(" — %1").arg(tr("a merge commit"))
                         : QString());
  }

  const QStringList refs = m_git->getCommitRefs(hash);
  if (!refs.isEmpty()) {
    html += QStringLiteral("<tr><td><b>Refs</b></td><td>%1</td></tr>")
                .arg(refs.join(QStringLiteral(", ")).toHtmlEscaped());
  }
  html += QLatin1String("</table>");

  html += QStringLiteral("<br><b>%1</b>").arg(details.subject.toHtmlEscaped());

  if (!details.body.trimmed().isEmpty()) {
    html += QStringLiteral("<br><pre style=\"margin-top:4px;\">%1</pre>")
                .arg(details.body.toHtmlEscaped());
  }

  m_selectedHash = hash;
  updateDetailActions(true);

  const QList<GitCommitFileStat> stats = m_git->getCommitFileStats(hash);
  if (m_detailFiles) {
    m_detailFiles->clear();
    m_detailFiles->setHeaderLabels({tr("Changed file"), tr("± lines")});
    for (const GitCommitFileStat &stat : stats) {
      QTreeWidgetItem *item = new QTreeWidgetItem(m_detailFiles);
      item->setText(0, stat.filePath);
      item->setText(
          1, QStringLiteral("+%1 −%2").arg(stat.additions).arg(stat.deletions));
      item->setToolTip(0, stat.filePath);
    }
  }

  if (!stats.isEmpty()) {
    html +=
        QStringLiteral("<br><b>%1 changed %2</b><ul style=\"margin-top:2px;\">")
            .arg(stats.size())
            .arg(stats.size() == 1 ? tr("file") : tr("files"));
    int shown = 0;
    for (const GitCommitFileStat &stat : stats) {
      if (++shown > 50) {
        html += QStringLiteral("<li>… +%1 more</li>").arg(stats.size() - 50);
        break;
      }
      html += QStringLiteral("<li><code>%1</code>&nbsp;&nbsp;"
                             "<span style=\"color:#3fb950;\">+%2</span> "
                             "<span style=\"color:#f85149;\">−%3</span></li>")
                  .arg(stat.filePath.toHtmlEscaped())
                  .arg(stat.additions)
                  .arg(stat.deletions);
    }
    html += QLatin1String("</ul>");
  }

  m_detailView->setHtml(html);
}

void GitLogDialog::selectInList(const QString &hash, bool reveal) {
  if (!m_commitTree || m_syncingSelection) {
    return;
  }

  for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
    if (item->data(0, Qt::UserRole).toString() == hash) {
      m_syncingSelection = true;
      m_commitTree->setCurrentItem(item);
      if (reveal) {
        m_commitTree->scrollToItem(item);
      }
      m_syncingSelection = false;
      return;
    }
  }
}

void GitLogDialog::onCommitSelected(QTreeWidgetItem *current,
                                    QTreeWidgetItem *) {
  if (!current || !m_git || m_syncingSelection) {
    return;
  }

  const QString hash = current->data(0, Qt::UserRole).toString();

  m_syncingSelection = true;
  m_graphWidget->selectCommit(hash);
  m_syncingSelection = false;

  showCommitDetails(hash);
}

void GitLogDialog::onGraphCommitSelected(const QString &hash) {
  selectInList(hash);
  showCommitDetails(hash);
}

void GitLogDialog::onSearchChanged(const QString &text) {

  if (m_commitTree) {
    for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
      QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
      bool matches = text.isEmpty();
      if (!matches) {
        for (int col = 0; col < 4; ++col) {
          if (item->text(col).contains(text, Qt::CaseInsensitive)) {
            matches = true;
            break;
          }
        }
      }
      item->setHidden(!matches);
    }
  }

  m_graphWidget->setFilter(text);

  if (m_statusLabel) {
    m_statusLabel->setText(
        text.trimmed().isEmpty()
            ? tr("%1 commits").arg(m_graphWidget->loadedCommitCount())
            : tr("%1 of %2 commits match")
                  .arg(m_graphWidget->matchingCommitCount())
                  .arg(m_graphWidget->loadedCommitCount()));
  }
}

void GitLogDialog::onGraphContextMenu(const QString &hash,
                                      const QPoint &globalPos) {
  showContextMenuForCommit(hash, globalPos);
}

void GitLogDialog::showContextMenuForCommit(const QString &hash,
                                            const QPoint &pos) {
  QMenu menu(this);
  menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));

  QAction *viewDiff = menu.addAction(tr("View diff"));
  QAction *compareHead = menu.addAction(tr("Compare with HEAD"));
  QAction *compareFrom = menu.addAction(tr("Compare from here…"));
  menu.addSeparator();
  QAction *branchHere = menu.addAction(tr("Branch here…"));
  QAction *tagHere = menu.addAction(tr("Tag here…"));
  menu.addSeparator();
  QAction *cherryPick = menu.addAction(tr("Cherry-pick onto current branch"));
  QAction *revert = menu.addAction(tr("Revert this commit"));
  QAction *bisectGood = menu.addAction(tr("Bisect: it was good here…"));
  QAction *bisectBad = menu.addAction(tr("Bisect: it was bad here…"));
  QAction *timeTravel = menu.addAction(tr("Time travel to this commit…"));
  QAction *undoHere = menu.addAction(tr("Undo this commit…"));
  QAction *resetHere = menu.addAction(tr("Reset current branch here…"));
  QAction *rebaseOnto = menu.addAction(tr("Rebase current branch onto here"));
  menu.addSeparator();

  QMenu *parents = menu.addMenu(tr("Inspect parent"));
  const GitCommitInfo info =
      m_git ? m_git->getCommitDetails(hash) : GitCommitInfo();
  QList<QAction *> parentActions;
  for (int i = 0; i < info.parents.size(); ++i) {
    QAction *action = parents->addAction(
        tr("Parent %1 — %2").arg(i + 1).arg(info.parents.at(i).left(7)));
    action->setData(info.parents.at(i));
    parentActions.append(action);
  }
  parents->setEnabled(!parentActions.isEmpty());

  QAction *copyHash = menu.addAction(tr("Copy hash"));

  QAction *chosen = menu.exec(pos);
  if (!chosen) {
    return;
  }

  if (parentActions.contains(chosen)) {
    const QString parentHash = chosen->data().toString();
    m_graphWidget->selectCommit(parentHash, true);
    showCommitDetails(parentHash);
    return;
  }

  if (chosen == viewDiff) {
    emit viewCommitDiff(hash);
  } else if (chosen == compareHead) {
    emit compareRequested(hash, QStringLiteral("HEAD"));
  } else if (chosen == compareFrom) {
    m_graphWidget->selectCommit(hash, false);
    onCompareAnchorChanged(hash);
    m_compareLabel->setText(
        tr("Comparing from %1 — Ctrl+click the second commit")
            .arg(hash.left(7)));
  } else if (chosen == branchHere) {
    createBranchAt(hash);
  } else if (chosen == tagHere) {
    createTagAt(hash);
  } else if (chosen == bisectGood) {
    emit bisectRequested(hash, m_graphWidget->compareAnchor().isEmpty()
                                   ? QStringLiteral("HEAD")
                                   : m_graphWidget->compareAnchor());
  } else if (chosen == bisectBad) {
    emit bisectRequested(m_graphWidget->compareAnchor(), hash);
  } else if (chosen == timeTravel) {
    emit timeTravelRequested(hash);
  } else if (chosen == undoHere) {
    emit undoCommitRequested(hash);
  } else if (chosen == resetHere) {
    resetTo(hash);
  } else if (chosen == rebaseOnto) {
    GitOperationRequest request;
    request.kind = GitOperationKind::Rebase;
    request.target = hash;
    if (m_git &&
        OperationPreviewDialog::confirm(m_git, request, m_theme, this)) {
      m_git->rebaseBranch(hash);
      emit repositoryChanged();
      loadCommits();
    }
  } else if (chosen == cherryPick) {
    if (m_git &&
        ThemedMessageBox::question(
            this, tr("Cherry-pick"),
            tr("Cherry-pick commit %1 into current branch?").arg(hash.left(7)),
            ThemedMessageBox::Yes | ThemedMessageBox::No) ==
            ThemedMessageBox::Yes) {
      m_git->cherryPick(hash);
      emit repositoryChanged();
      loadCommits();
    }
  } else if (chosen == revert) {
    if (m_git && ThemedMessageBox::question(
                     this, tr("Revert"),
                     tr("Add a commit that undoes %1?").arg(hash.left(7))) ==
                     ThemedMessageBox::Yes) {
      m_git->revertCommit(hash);
      emit repositoryChanged();
      loadCommits();
    }
  } else if (chosen == copyHash) {
    QGuiApplication::clipboard()->setText(hash);
  }
}
