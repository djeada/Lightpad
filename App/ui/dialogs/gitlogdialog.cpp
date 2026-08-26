#include "gitlogdialog.h"
#include "../../git/gitintegration.h"
#include "../../ui/uistylehelper.h"
#include "../widgets/gitgraphwidget.h"

#include "themedmessagebox.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

GitLogDialog::GitLogDialog(GitIntegration *git, const Theme &theme,
                           QWidget *parent)
    : StyledDialog(parent), m_git(git), m_theme(theme) {
  setWindowTitle(tr("Git Log"));
  setMinimumSize(760, 480);
  resize(900, 580);
  buildUi();
  applyTheme(theme);
  loadCommits();
}

void GitLogDialog::setFilePath(const QString &filePath) {
  m_filePath = filePath;
  loadCommits();
}

void GitLogDialog::refresh() { loadCommits(); }

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
  m_searchField->setPlaceholderText(tr("Filter by subject, author or hash…"));
  m_searchField->setClearButtonEnabled(true);
  connect(m_searchField, &QLineEdit::textChanged, this,
          &GitLogDialog::onSearchChanged);
  mainLayout->addWidget(m_searchField);

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

  auto *graphSplitter = new QSplitter(Qt::Vertical, graphTab);
  graphSplitter->addWidget(m_graphWidget);

  m_detailView = new QTextEdit(graphTab);
  m_detailView->setReadOnly(true);
  m_splitter = graphSplitter;
  graphSplitter->addWidget(m_detailView);
  graphSplitter->setStretchFactor(0, 3);
  graphSplitter->setStretchFactor(1, 1);
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

void GitLogDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);

  if (m_commitTree) {
    m_commitTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
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

  m_graphWidget->loadGraph(200, m_filePath);

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

  const QList<GitCommitFileStat> stats = m_git->getCommitFileStats(hash);
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
}

void GitLogDialog::onGraphContextMenu(const QString &hash,
                                      const QPoint &globalPos) {
  showContextMenuForCommit(hash, globalPos);
}

void GitLogDialog::showContextMenuForCommit(const QString &hash,
                                            const QPoint &pos) {
  QMenu menu(this);

  QAction *viewDiff = menu.addAction(tr("View Diff"));
  QAction *cherryPick = menu.addAction(tr("Cherry-pick"));
  menu.addSeparator();
  QAction *copyHash = menu.addAction(tr("Copy Hash"));

  QAction *chosen = menu.exec(pos);
  if (chosen == viewDiff) {
    emit viewCommitDiff(hash);
  } else if (chosen == cherryPick) {
    if (m_git) {
      int ret = ThemedMessageBox::question(
          this, tr("Cherry-pick"),
          tr("Cherry-pick commit %1 into current branch?").arg(hash.left(7)),
          ThemedMessageBox::Yes | ThemedMessageBox::No);
      if (ret == ThemedMessageBox::Yes) {
        m_git->cherryPick(hash);
      }
    }
  } else if (chosen == copyHash) {
    QGuiApplication::clipboard()->setText(hash);
  }
}
