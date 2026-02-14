#include "gitlogdialog.h"
#include "../../git/gitintegration.h"
#include "../../ui/uistylehelper.h"
#include "../widgets/gitgraphwidget.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

GitLogDialog::GitLogDialog(GitIntegration *git, const Theme &theme,
                           QWidget *parent)
    : QDialog(parent), m_git(git), m_theme(theme) {
  setWindowTitle(tr("Git Log"));
  setMinimumSize(700, 450);
  resize(850, 550);
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
  m_searchField->setPlaceholderText(tr("Filter commits..."));
  m_searchField->setClearButtonEnabled(true);
  connect(m_searchField, &QLineEdit::textChanged, this,
          &GitLogDialog::onSearchChanged);
  mainLayout->addWidget(m_searchField);

  m_tabWidget = new QTabWidget(this);

  auto *listTab = new QWidget(this);
  auto *listLayout = new QVBoxLayout(listTab);
  listLayout->setContentsMargins(0, 0, 0, 0);

  m_splitter = new QSplitter(Qt::Vertical, listTab);

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

  m_splitter->addWidget(m_commitTree);

  m_detailView = new QTextEdit(listTab);
  m_detailView->setReadOnly(true);
  m_splitter->addWidget(m_detailView);

  m_splitter->setStretchFactor(0, 3);
  m_splitter->setStretchFactor(1, 1);
  listLayout->addWidget(m_splitter);

  m_tabWidget->addTab(listTab, tr("List"));

  m_graphWidget = new GitGraphWidget(m_git, m_theme, this);
  connect(m_graphWidget, &GitGraphWidget::commitSelected, this,
          &GitLogDialog::onGraphCommitSelected);
  connect(m_graphWidget, &GitGraphWidget::commitDoubleClicked, this,
          [this](const QString &hash) { emit viewCommitDiff(hash); });
  m_tabWidget->addTab(m_graphWidget, tr("Graph"));

  mainLayout->addWidget(m_tabWidget);

  m_statusLabel = new QLabel(this);
  mainLayout->addWidget(m_statusLabel);
}

void GitLogDialog::applyTheme(const Theme &theme) {
  QString bg = theme.backgroundColor.name();
  QString fg = theme.foregroundColor.name();
  QString highlight = theme.highlightColor.name();

  setStyleSheet(
      QString("QDialog { background-color: %1; color: %2; }"
              "QTreeWidget { background-color: %1; color: %2; "
              "alternate-background-color: %3; }"
              "QTextEdit { background-color: %1; color: %2; }"
              "QLineEdit { background-color: %3; color: %2; "
              "border: 1px solid %4; padding: 4px; }"
              "QHeaderView::section { background-color: %3; color: %2; }"
              "QTabWidget::pane { border: 1px solid %4; }"
              "QTabBar::tab { background: %3; color: %2; padding: 6px 16px; }"
              "QTabBar::tab:selected { background: %1; }")
          .arg(bg, fg, theme.lineNumberAreaColor.name(), highlight));
}

void GitLogDialog::loadCommits() {
  m_commitTree->clear();

  if (!m_git || !m_git->isValidRepository()) {
    m_statusLabel->setText(tr("No valid repository"));
    return;
  }

  QList<GitCommitInfo> commits;
  if (m_filePath.isEmpty()) {
    commits = m_git->getCommitLog(100);
  } else {
    commits = m_git->getCommitLog(100, m_filePath);
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

  m_graphWidget->loadGraph(200);

  m_statusLabel->setText(tr("%1 commits").arg(commits.size()));
}

void GitLogDialog::onCommitSelected(QTreeWidgetItem *current,
                                    QTreeWidgetItem *) {
  if (!current || !m_git)
    return;

  QString hash = current->data(0, Qt::UserRole).toString();
  GitCommitInfo details = m_git->getCommitDetails(hash);

  QString html =
      QString("<b>Commit:</b> %1<br>"
              "<b>Author:</b> %2 &lt;%3&gt;<br>"
              "<b>Date:</b> %4<br><br>"
              "<b>%5</b>")
          .arg(details.hash.left(12), details.author, details.authorEmail,
               details.date, details.subject.toHtmlEscaped());

  if (!details.body.isEmpty()) {
    html += "<br><pre>" + details.body.toHtmlEscaped() + "</pre>";
  }

  m_detailView->setHtml(html);
}

void GitLogDialog::onSearchChanged(const QString &text) {
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

void GitLogDialog::onGraphCommitSelected(const QString &hash) {
  if (!m_git)
    return;

  GitCommitInfo details = m_git->getCommitDetails(hash);
  QString html =
      QString("<b>Commit:</b> %1<br>"
              "<b>Author:</b> %2 &lt;%3&gt;<br>"
              "<b>Date:</b> %4<br><br>"
              "<b>%5</b>")
          .arg(details.hash.left(12), details.author, details.authorEmail,
               details.date, details.subject.toHtmlEscaped());

  if (!details.body.isEmpty()) {
    html += "<br><pre>" + details.body.toHtmlEscaped() + "</pre>";
  }

  m_detailView->setHtml(html);
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
      int ret = QMessageBox::question(
          this, tr("Cherry-pick"),
          tr("Cherry-pick commit %1 into current branch?").arg(hash.left(7)),
          QMessageBox::Yes | QMessageBox::No);
      if (ret == QMessageBox::Yes) {
        m_git->cherryPick(hash);
      }
    }
  } else if (chosen == copyHash) {
    QGuiApplication::clipboard()->setText(hash);
  }
}
