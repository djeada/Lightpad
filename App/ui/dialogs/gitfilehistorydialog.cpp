#include "gitfilehistorydialog.h"
#include "../../git/gitintegration.h"

#include <QBoxLayout>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QTreeWidget>

GitFileHistoryDialog::GitFileHistoryDialog(GitIntegration *git,
                                           const QString &filePath,
                                           QWidget *parent)
    : QDialog(parent), m_git(git), m_filePath(filePath) {
  setWindowTitle(tr("File History — %1").arg(QFileInfo(filePath).fileName()));
  resize(750, 500);

  auto *layout = new QVBoxLayout(this);

  m_titleLabel = new QLabel(tr("History for: <b>%1</b>").arg(filePath), this);
  layout->addWidget(m_titleLabel);

  auto *splitter = new QSplitter(Qt::Vertical, this);

  m_commitTree = new QTreeWidget(this);
  m_commitTree->setHeaderLabels(
      {tr("Hash"), tr("Author"), tr("Date"), tr("Message")});
  m_commitTree->setRootIsDecorated(false);
  m_commitTree->setAlternatingRowColors(true);
  m_commitTree->header()->setStretchLastSection(true);
  m_commitTree->header()->setSectionResizeMode(0,
                                               QHeaderView::ResizeToContents);
  m_commitTree->header()->setSectionResizeMode(1,
                                               QHeaderView::ResizeToContents);
  m_commitTree->header()->setSectionResizeMode(2,
                                               QHeaderView::ResizeToContents);
  splitter->addWidget(m_commitTree);

  m_detailView = new QTextEdit(this);
  m_detailView->setReadOnly(true);
  m_detailView->setPlaceholderText(tr("Select a commit to see details"));
  splitter->addWidget(m_detailView);

  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);
  layout->addWidget(splitter);

  auto *buttonLayout = new QHBoxLayout;
  auto *closeBtn = new QPushButton(tr("Close"), this);
  buttonLayout->addStretch();
  buttonLayout->addWidget(closeBtn);
  layout->addLayout(buttonLayout);

  connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_commitTree, &QTreeWidget::currentItemChanged, this,
          &GitFileHistoryDialog::onCommitSelected);
  connect(m_commitTree, &QTreeWidget::itemDoubleClicked, this,
          &GitFileHistoryDialog::onCommitDoubleClicked);

  loadHistory();
}

void GitFileHistoryDialog::loadHistory() {
  if (!m_git)
    return;

  QList<GitCommitInfo> commits = m_git->getFileLog(m_filePath, 100);
  for (const auto &commit : commits) {
    auto *item = new QTreeWidgetItem(m_commitTree);
    item->setText(0, commit.shortHash);
    item->setText(1, commit.author);
    item->setText(2, commit.relativeDate);
    item->setText(3, commit.subject);
    item->setData(0, Qt::UserRole, commit.hash);
  }
}

void GitFileHistoryDialog::onCommitSelected(QTreeWidgetItem *current,
                                            QTreeWidgetItem *) {
  if (!current || !m_git)
    return;

  QString hash = current->data(0, Qt::UserRole).toString();
  GitCommitInfo info = m_git->getCommitDetails(hash);
  showCommitDetails(info);
}

void GitFileHistoryDialog::onCommitDoubleClicked(QTreeWidgetItem *item, int) {
  if (!item)
    return;
  QString hash = item->data(0, Qt::UserRole).toString();
  emit viewCommitDiff(hash);
}

void GitFileHistoryDialog::showCommitDetails(const GitCommitInfo &info) {
  QString html = QStringLiteral(
      "<div style='font-family: monospace;'>"
      "<div style='font-size: 14px; font-weight: bold;'>%1</div>"
      "<div style='color: #aaa; margin: 4px 0;'>%2 &lt;%3&gt;</div>"
      "<div style='color: #888;'>%4 (%5)</div>"
      "<hr>"
      "<div style='margin-top: 8px;'>%6</div>");

  if (!info.body.isEmpty()) {
    html +=
        QStringLiteral("<div style='margin-top: 8px; color: #ccc;'>%1</div>")
            .arg(info.body.toHtmlEscaped().replace('\n', "<br>"));
  }

  QList<GitCommitFileStat> stats = m_git->getCommitFileStats(info.hash);
  if (!stats.isEmpty()) {
    html += QStringLiteral(
                "<div style='margin-top: 10px; border-top: 1px solid #555; "
                "padding-top: 6px;'><b>Changed files (%1):</b></div>")
                .arg(stats.size());
    for (const auto &stat : stats) {
      html += QStringLiteral("<div><span style='color:#4caf50;'>+%1</span> "
                             "<span style='color:#f44336;'>-%2</span> %3</div>")
                  .arg(stat.additions)
                  .arg(stat.deletions)
                  .arg(stat.filePath.toHtmlEscaped());
    }
  }

  html += QStringLiteral("</div>");

  m_detailView->setHtml(html.arg(info.shortHash.toHtmlEscaped())
                            .arg(info.author.toHtmlEscaped())
                            .arg(info.authorEmail.toHtmlEscaped())
                            .arg(info.date.toHtmlEscaped())
                            .arg(info.relativeDate.toHtmlEscaped())
                            .arg(info.subject.toHtmlEscaped()));
}
