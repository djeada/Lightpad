#include "filetimelinedialog.h"
#include "../../git/gitdiffmodel.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int PAGE_SIZE = 100;
constexpr int REVISION_INDEX_ROLE = Qt::UserRole + 1;

} // namespace

FileTimelineDialog::FileTimelineDialog(GitIntegration *git,
                                       const QString &filePath,
                                       const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_filePath(filePath),
      m_hasWorkingTreePoint(false), m_exhausted(false), m_lineRangeStart(-1),
      m_lineRangeEnd(-1), m_headerLabel(nullptr), m_followRenamesCheck(nullptr),
      m_allBranchesCheck(nullptr), m_firstParentCheck(nullptr),
      m_lineRangeCheck(nullptr), m_authorEdit(nullptr), m_sinceEdit(nullptr),
      m_untilEdit(nullptr), m_timelineTree(nullptr), m_loadMoreButton(nullptr),
      m_previewHeader(nullptr), m_contentView(nullptr), m_diffView(nullptr),
      m_compareCurrentButton(nullptr), m_compareSelectedButton(nullptr),
      m_openCommitButton(nullptr), m_showInGraphButton(nullptr),
      m_refreshButton(nullptr), m_closeButton(nullptr) {
  setWindowTitle(tr("File Evolution Timeline"));
  setMinimumSize(900, 580);
  resize(1100, 700);

  buildUi();
  setKeyboardDefault(m_closeButton);
  applyTheme(theme);
  reload();
}

void FileTimelineDialog::setLineRange(int startLine, int endLine) {
  m_lineRangeStart = startLine;
  m_lineRangeEnd = endLine;
  if (m_lineRangeCheck) {
    m_lineRangeCheck->setEnabled(startLine > 0);
    m_lineRangeCheck->setText(
        startLine > 0 ? tr("Only commits changing lines %1–%2")
                            .arg(startLine)
                            .arg(endLine)
                      : tr("Only commits changing the selected lines"));
  }
  reload();
}

void FileTimelineDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  QHBoxLayout *header = new QHBoxLayout();
  m_headerLabel = new QLabel(this);
  m_headerLabel->setObjectName(QStringLiteral("timelineHeaderLabel"));
  header->addWidget(m_headerLabel);
  header->addStretch();
  m_refreshButton = new QPushButton(tr("Refresh"), this);
  m_refreshButton->setObjectName(QStringLiteral("timelineRefreshButton"));
  connect(m_refreshButton, &QPushButton::clicked, this,
          &FileTimelineDialog::reload);
  header->addWidget(m_refreshButton);
  layout->addLayout(header);

  buildFilterBar(layout);
  buildBody(layout);
  buildActions(layout);
}

void FileTimelineDialog::buildFilterBar(QVBoxLayout *layout) {
  QHBoxLayout *first = new QHBoxLayout();
  first->setSpacing(UiMetrics::ToolbarSpacing);

  m_followRenamesCheck = new QCheckBox(tr("Follow renames"), this);
  m_followRenamesCheck->setObjectName(QStringLiteral("timelineFollowCheck"));
  m_followRenamesCheck->setChecked(true);
  m_followRenamesCheck->setToolTip(
      tr("Git does not record file identity: a rename is inferred from "
         "content when the history is read.\ngit log --follow"));
  connect(m_followRenamesCheck, &QCheckBox::toggled, this,
          &FileTimelineDialog::reload);
  first->addWidget(m_followRenamesCheck);

  m_allBranchesCheck = new QCheckBox(tr("All branches"), this);
  m_allBranchesCheck->setObjectName(QStringLiteral("timelineAllBranchesCheck"));
  m_allBranchesCheck->setToolTip(
      tr("Include commits from every ref, not only the current branch."));
  connect(m_allBranchesCheck, &QCheckBox::toggled, this,
          &FileTimelineDialog::reload);
  first->addWidget(m_allBranchesCheck);

  m_firstParentCheck = new QCheckBox(tr("First parent only"), this);
  m_firstParentCheck->setObjectName(QStringLiteral("timelineFirstParentCheck"));
  connect(m_firstParentCheck, &QCheckBox::toggled, this,
          &FileTimelineDialog::reload);
  first->addWidget(m_firstParentCheck);

  m_lineRangeCheck =
      new QCheckBox(tr("Only commits changing the selected lines"), this);
  m_lineRangeCheck->setObjectName(QStringLiteral("timelineLineRangeCheck"));
  m_lineRangeCheck->setEnabled(false);
  connect(m_lineRangeCheck, &QCheckBox::toggled, this,
          &FileTimelineDialog::reload);
  first->addWidget(m_lineRangeCheck);

  first->addStretch();
  layout->addLayout(first);

  QHBoxLayout *second = new QHBoxLayout();
  second->setSpacing(UiMetrics::ToolbarSpacing);

  const auto addFilter = [&](QLineEdit **edit, const QString &labelText,
                             const QString &name, const QString &placeholder) {
    QLabel *label = new QLabel(labelText, this);
    label->setObjectName(name + QStringLiteral("Label"));
    second->addWidget(label);
    *edit = new QLineEdit(this);
    (*edit)->setObjectName(name);
    (*edit)->setPlaceholderText(placeholder);
    (*edit)->setClearButtonEnabled(true);
    connect(*edit, &QLineEdit::editingFinished, this,
            &FileTimelineDialog::reload);
    second->addWidget(*edit, 1);
  };

  addFilter(&m_authorEdit, tr("Author:"), QStringLiteral("timelineAuthorEdit"),
            tr("name or email"));
  addFilter(&m_sinceEdit, tr("Since:"), QStringLiteral("timelineSinceEdit"),
            tr("e.g. 3 months ago"));
  addFilter(&m_untilEdit, tr("Until:"), QStringLiteral("timelineUntilEdit"),
            tr("e.g. 2024-06-01"));

  layout->addLayout(second);
}

void FileTimelineDialog::buildBody(QVBoxLayout *layout) {
  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

  QWidget *left = new QWidget(splitter);
  QVBoxLayout *leftLayout = new QVBoxLayout(left);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(UiMetrics::SpaceXs);

  m_timelineTree = new QTreeWidget(left);
  m_timelineTree->setObjectName(QStringLiteral("timelineTree"));
  m_timelineTree->setColumnCount(3);
  m_timelineTree->setHeaderLabels(
      {tr("Revision"), tr("What happened"), tr("When")});
  m_timelineTree->setRootIsDecorated(false);
  m_timelineTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_timelineTree->header()->setSectionResizeMode(0,
                                                 QHeaderView::ResizeToContents);
  m_timelineTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_timelineTree->header()->setSectionResizeMode(2,
                                                 QHeaderView::ResizeToContents);
  connect(m_timelineTree, &QTreeWidget::itemSelectionChanged, this,
          &FileTimelineDialog::onSelectionChanged);
  connect(m_timelineTree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem *, int) { onOpenCommit(); });
  leftLayout->addWidget(m_timelineTree, 1);

  m_loadMoreButton = new QPushButton(tr("Load older revisions"), left);
  m_loadMoreButton->setObjectName(QStringLiteral("timelineLoadMoreButton"));
  connect(m_loadMoreButton, &QPushButton::clicked, this,
          &FileTimelineDialog::loadMore);
  leftLayout->addWidget(m_loadMoreButton);

  splitter->addWidget(left);

  QWidget *right = new QWidget(splitter);
  QVBoxLayout *rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(UiMetrics::SpaceXs);

  m_previewHeader = new QLabel(right);
  m_previewHeader->setObjectName(QStringLiteral("timelinePreviewHeader"));
  m_previewHeader->setWordWrap(true);
  rightLayout->addWidget(m_previewHeader);

  m_contentView = new QPlainTextEdit(right);
  m_contentView->setObjectName(QStringLiteral("timelineContentView"));
  m_contentView->setReadOnly(true);
  m_contentView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  rightLayout->addWidget(m_contentView, 1);

  m_diffView = new QListWidget(right);
  m_diffView->setObjectName(QStringLiteral("timelineDiffView"));
  m_diffView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_diffView->setUniformItemSizes(true);
  m_diffView->hide();
  rightLayout->addWidget(m_diffView, 1);

  splitter->addWidget(right);
  splitter->setSizes({540, 560});
  layout->addWidget(splitter, 1);
}

void FileTimelineDialog::buildActions(QVBoxLayout *layout) {
  QHBoxLayout *actions = new QHBoxLayout();
  actions->setSpacing(UiMetrics::ToolbarGroupSpacing);

  const auto addButton = [&](QPushButton **button, const QString &text,
                             const QString &name, const QString &tip) {
    *button = new QPushButton(text, this);
    (*button)->setObjectName(name);
    (*button)->setToolTip(tip);
    actions->addWidget(*button);
  };

  addButton(&m_compareCurrentButton, tr("Compare to current"),
            QStringLiteral("timelineCompareCurrentButton"),
            tr("Diff this revision against the file as it is now."));
  addButton(&m_compareSelectedButton, tr("Compare the two selected"),
            QStringLiteral("timelineCompareSelectedButton"),
            tr("Select two points to see what changed between them."));
  addButton(&m_openCommitButton, tr("Open full commit"),
            QStringLiteral("timelineOpenCommitButton"),
            tr("A snapshot belongs to a commit; this shows everything that "
               "commit changed."));
  addButton(&m_showInGraphButton, tr("Show in graph"),
            QStringLiteral("timelineShowInGraphButton"),
            tr("Jump to this commit in the commit graph."));

  connect(m_compareCurrentButton, &QPushButton::clicked, this,
          &FileTimelineDialog::onCompareToCurrent);
  connect(m_compareSelectedButton, &QPushButton::clicked, this,
          &FileTimelineDialog::onCompareSelected);
  connect(m_openCommitButton, &QPushButton::clicked, this,
          &FileTimelineDialog::onOpenCommit);
  connect(m_showInGraphButton, &QPushButton::clicked, this,
          &FileTimelineDialog::onShowInGraph);

  actions->addStretch();
  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("timelineCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  actions->addWidget(m_closeButton);

  layout->addLayout(actions);
}

GitFileTimelineOptions FileTimelineDialog::currentOptions() const {
  GitFileTimelineOptions options;
  options.followRenames = m_followRenamesCheck->isChecked();
  options.allBranches = m_allBranchesCheck->isChecked();
  options.firstParentOnly = m_firstParentCheck->isChecked();
  options.author = m_authorEdit->text().trimmed();
  options.since = m_sinceEdit->text().trimmed();
  options.until = m_untilEdit->text().trimmed();
  if (m_lineRangeCheck->isChecked() && m_lineRangeStart > 0) {
    options.lineRangeStart = m_lineRangeStart;
    options.lineRangeEnd = m_lineRangeEnd;
  }
  return options;
}

void FileTimelineDialog::reload() {
  if (!m_git || !m_git->isValidRepository() || m_filePath.isEmpty()) {
    return;
  }

  m_revisions =
      m_git->getFileTimeline(m_filePath, currentOptions(), 0, PAGE_SIZE);
  m_exhausted = m_revisions.size() < PAGE_SIZE;

  const GitFileInfo status = m_git->getFileStatus(m_filePath);
  m_hasWorkingTreePoint = status.workTreeStatus != GitFileStatus::Clean ||
                          status.indexStatus != GitFileStatus::Clean;

  rebuildTimeline();
}

void FileTimelineDialog::loadMore() {
  if (!m_git || m_exhausted) {
    return;
  }
  const QList<GitFileRevision> page = m_git->getFileTimeline(
      m_filePath, currentOptions(), m_revisions.size(), PAGE_SIZE);
  if (page.isEmpty() || page.size() < PAGE_SIZE) {
    m_exhausted = true;
  }
  m_revisions += page;
  rebuildTimeline();
}

void FileTimelineDialog::rebuildTimeline() {
  m_timelineTree->clear();

  if (m_hasWorkingTreePoint) {
    GitFileRevision working;
    working.isWorkingTree = true;
    working.pathAtRevision = m_filePath;

    QTreeWidgetItem *item = new QTreeWidgetItem(m_timelineTree);
    item->setText(0, tr("● working tree"));
    item->setText(1, gitFileRevisionSummary(working));
    item->setText(2, tr("now"));
    item->setData(0, REVISION_INDEX_ROLE, -1);
  }

  for (int i = 0; i < m_revisions.size(); ++i) {
    const GitFileRevision &revision = m_revisions.at(i);
    QTreeWidgetItem *item = new QTreeWidgetItem(m_timelineTree);

    item->setText(0, (revision.isRename() ? tr("⤳ ") : tr("│ ")) +
                         revision.commit.shortHash);
    item->setText(
        1, QStringLiteral("%1 — %2").arg(revision.commit.subject,
                                         gitFileRevisionSummary(revision)));
    item->setText(2, revision.commit.relativeDate);
    item->setData(0, REVISION_INDEX_ROLE, i);
    item->setToolTip(
        1, QStringLiteral("%1\n%2 <%3>\n%4")
               .arg(revision.commit.subject, revision.commit.author,
                    revision.commit.authorEmail, revision.commit.date));
  }

  m_headerLabel->setText(
      tr("%1 — %2 revisions%3")
          .arg(m_filePath)
          .arg(m_revisions.size())
          .arg(m_exhausted ? QString() : tr(" (more available)")));
  m_loadMoreButton->setVisible(!m_exhausted);

  if (m_timelineTree->topLevelItemCount() > 0) {
    m_timelineTree->setCurrentItem(m_timelineTree->topLevelItem(0));
  } else {
    m_previewHeader->setText(
        tr("No revisions match these filters. File history is a projection of "
           "commit history, so a filter that hides the commits hides the "
           "file's past too."));
    m_contentView->clear();
  }
}

QList<GitFileRevision> FileTimelineDialog::selectedRevisions() const {
  QList<GitFileRevision> selected;
  for (QTreeWidgetItem *item : m_timelineTree->selectedItems()) {
    const int index = item->data(0, REVISION_INDEX_ROLE).toInt();
    if (index < 0) {
      GitFileRevision working;
      working.isWorkingTree = true;
      working.pathAtRevision = m_filePath;
      selected.append(working);
    } else if (index < m_revisions.size()) {
      selected.append(m_revisions.at(index));
    }
  }
  return selected;
}

void FileTimelineDialog::onSelectionChanged() {
  const QList<GitFileRevision> selected = selectedRevisions();

  m_compareCurrentButton->setEnabled(selected.size() == 1 &&
                                     !selected.first().isWorkingTree);
  m_compareSelectedButton->setEnabled(selected.size() == 2);
  m_openCommitButton->setEnabled(selected.size() == 1 &&
                                 !selected.first().isWorkingTree);
  m_showInGraphButton->setEnabled(selected.size() == 1 &&
                                  !selected.first().isWorkingTree);

  if (selected.size() == 2) {

    showDiffBetween(selected.at(1), selected.at(0));
    return;
  }
  if (selected.size() == 1) {
    showContentFor(selected.first());
  }
}

void FileTimelineDialog::showContentFor(const GitFileRevision &revision) {
  m_diffView->hide();
  m_contentView->show();

  if (revision.isWorkingTree) {
    m_previewHeader->setText(
        tr("<b>Working tree</b> — uncommitted, not part of any commit yet."));

    QString content;
    if (m_git) {
      QFile file(QDir(m_git->repositoryPath()).filePath(m_filePath));
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(file.readAll());
      }
    }
    m_contentView->setPlainText(content);
    return;
  }

  m_previewHeader->setText(
      tr("<b>Read-only snapshot</b> of <code>%1</code> at <code>%2</code> — "
         "%3, %4. Read it alongside everything else that commit changed.")
          .arg(revision.pathAtRevision.isEmpty() ? m_filePath
                                                 : revision.pathAtRevision,
               revision.commit.shortHash,
               revision.commit.author.toHtmlEscaped(), revision.commit.date));

  const QString path =
      revision.pathAtRevision.isEmpty() ? m_filePath : revision.pathAtRevision;
  m_contentView->setPlainText(
      m_git ? m_git->getFileAtRevision(path, revision.commit.hash) : QString());
}

void FileTimelineDialog::showDiffBetween(const GitFileRevision &older,
                                         const GitFileRevision &newer) {
  m_contentView->hide();
  m_diffView->show();

  const QString olderRef = older.isWorkingTree ? QString() : older.commit.hash;
  const QString newerRef = newer.isWorkingTree ? QString() : newer.commit.hash;
  const QString olderPath =
      older.pathAtRevision.isEmpty() ? m_filePath : older.pathAtRevision;
  const QString newerPath =
      newer.pathAtRevision.isEmpty() ? m_filePath : newer.pathAtRevision;

  QStringList args{QStringLiteral("diff")};
  if (olderRef.isEmpty()) {
    args << newerRef << QStringLiteral("--") << newerPath;
  } else if (newerRef.isEmpty()) {
    args << olderRef << QStringLiteral("--") << olderPath;
  } else {
    args << olderRef << newerRef << QStringLiteral("--") << olderPath;
    if (olderPath != newerPath) {
      args << newerPath;
    }
  }

  m_previewHeader->setText(
      tr("<b>%1 → %2</b> — what changed between these two points.")
          .arg(older.isWorkingTree ? tr("working tree")
                                   : older.commit.shortHash,
               newer.isWorkingTree ? tr("working tree")
                                   : newer.commit.shortHash));

  renderDiff(m_git ? m_git->executeWordDiff(args) : QString());
}

void FileTimelineDialog::renderDiff(const QString &diffText) {
  m_diffView->clear();
  if (diffText.trimmed().isEmpty()) {
    QListWidgetItem *item = new QListWidgetItem(
        tr("No difference between these two points."), m_diffView);
    item->setFlags(Qt::ItemIsEnabled);
    return;
  }

  for (const QString &line : diffText.split(QLatin1Char('\n'))) {
    QListWidgetItem *item = new QListWidgetItem(line, m_diffView);
    if (line.startsWith(QLatin1Char('+')) &&
        !line.startsWith(QLatin1String("+++"))) {
      item->setForeground(m_theme.diffAddedColor);
    } else if (line.startsWith(QLatin1Char('-')) &&
               !line.startsWith(QLatin1String("---"))) {
      item->setForeground(m_theme.diffRemovedColor);
    } else if (line.startsWith(QLatin1String("@@"))) {
      item->setForeground(m_theme.accentColor);
    } else {
      item->setForeground(m_theme.singleLineCommentFormat);
    }
  }
}

void FileTimelineDialog::onCompareToCurrent() {
  const QList<GitFileRevision> selected = selectedRevisions();
  if (selected.size() != 1 || selected.first().isWorkingTree) {
    return;
  }
  emit compareRequested(selected.first().commit.hash, QString(), m_filePath);
}

void FileTimelineDialog::onCompareSelected() {
  const QList<GitFileRevision> selected = selectedRevisions();
  if (selected.size() != 2) {
    return;
  }
  emit compareRequested(
      selected.at(1).isWorkingTree ? QString() : selected.at(1).commit.hash,
      selected.at(0).isWorkingTree ? QString() : selected.at(0).commit.hash,
      m_filePath);
}

void FileTimelineDialog::onOpenCommit() {
  const QList<GitFileRevision> selected = selectedRevisions();
  if (selected.size() != 1 || selected.first().isWorkingTree) {
    return;
  }
  emit openCommitRequested(selected.first().commit.hash);
}

void FileTimelineDialog::onShowInGraph() {
  const QList<GitFileRevision> selected = selectedRevisions();
  if (selected.size() != 1 || selected.first().isWorkingTree) {
    return;
  }
  emit showInGraphRequested(selected.first().commit.hash, m_filePath);
}

void FileTimelineDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_timelineTree) {
    m_timelineTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  for (QCheckBox *check : {m_followRenamesCheck, m_allBranchesCheck,
                           m_firstParentCheck, m_lineRangeCheck}) {
    if (check) {
      check->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
    }
  }
  for (QLineEdit *edit : {m_authorEdit, m_sinceEdit, m_untilEdit}) {
    if (edit) {
      edit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
    }
  }
  if (m_contentView) {
    m_contentView->setStyleSheet(
        QString("QPlainTextEdit { background: %1; color: %2; border: 1px "
                "solid %3; }")
            .arg(theme.surfaceColor.name(), theme.foregroundColor.name(),
                 theme.borderColor.name()));
  }
  if (m_diffView) {
    m_diffView->setStyleSheet(
        QString("QListWidget { background: %1; border: 1px solid %2; }")
            .arg(theme.surfaceColor.name(), theme.borderColor.name()));
  }
  if (m_headerLabel) {
    styleTitleLabel(m_headerLabel);
  }
  if (m_previewHeader) {
    styleSubduedLabel(m_previewHeader);
  }
  for (QPushButton *button :
       {m_compareCurrentButton, m_compareSelectedButton, m_openCommitButton,
        m_showInGraphButton, m_refreshButton, m_closeButton,
        m_loadMoreButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  for (const char *name : {"timelineAuthorEditLabel", "timelineSinceEditLabel",
                           "timelineUntilEditLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
}
