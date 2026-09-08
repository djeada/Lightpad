#include "provenancelensdialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include <QDir>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int HASH_ROLE = Qt::UserRole + 1;
}

ProvenanceLensDialog::ProvenanceLensDialog(GitIntegration *git,
                                           const QString &filePath,
                                           int startLine, int endLine,
                                           const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_headerLabel(nullptr),
      m_latestLabel(nullptr), m_churnLabel(nullptr), m_caveatLabel(nullptr),
      m_stepsTree(nullptr), m_previousView(nullptr), m_currentView(nullptr),
      m_openCommitButton(nullptr), m_parentDiffButton(nullptr),
      m_fileHistoryButton(nullptr), m_graphButton(nullptr),
      m_closeButton(nullptr) {
  m_provenance = buildLineProvenance(git, filePath, startLine, endLine);

  setWindowTitle(tr("Where this code came from"));
  setMinimumSize(820, 560);
  resize(960, 660);

  buildUi();
  setKeyboardDefault(m_closeButton);
  applyTheme(theme);
  onSelectionChanged();
}

void ProvenanceLensDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  m_headerLabel = new QLabel(this);
  m_headerLabel->setObjectName(QStringLiteral("provenanceHeaderLabel"));
  m_headerLabel->setWordWrap(true);

  QString shownPath = m_provenance.filePath;
  if (QDir::isAbsolutePath(shownPath) && m_git &&
      !m_git->repositoryPath().isEmpty()) {
    const QString relative =
        QDir(m_git->repositoryPath()).relativeFilePath(shownPath);
    if (!relative.startsWith(QLatin1String(".."))) {
      shownPath = relative;
    }
  }
  m_headerLabel->setText(m_provenance.startLine == m_provenance.endLine
                             ? tr("<b>%1</b>, line %2")
                                   .arg(shownPath.toHtmlEscaped())
                                   .arg(m_provenance.startLine)
                             : tr("<b>%1</b>, lines %2–%3")
                                   .arg(shownPath.toHtmlEscaped())
                                   .arg(m_provenance.startLine)
                                   .arg(m_provenance.endLine));
  layout->addWidget(m_headerLabel);

  m_latestLabel = new QLabel(this);
  m_latestLabel->setObjectName(QStringLiteral("provenanceLatestLabel"));
  m_latestLabel->setWordWrap(true);
  m_latestLabel->setText(
      m_provenance.latest.hash.isEmpty()
          ? tr("No commit in the loaded history touched these lines.")
          : tr("Last changed by <b>%1</b> in <code>%2</code> %3 — "
               "<i>%4</i>")
                .arg(m_provenance.latest.author.toHtmlEscaped(),
                     m_provenance.latest.shortHash,
                     m_provenance.latest.relativeDate,
                     m_provenance.latest.subject.toHtmlEscaped()));
  layout->addWidget(m_latestLabel);

  m_churnLabel = new QLabel(gitProvenanceChurnSummary(m_provenance), this);
  m_churnLabel->setObjectName(QStringLiteral("provenanceChurnLabel"));
  m_churnLabel->setWordWrap(true);
  m_churnLabel->setToolTip(
      tr("How often this region changed. This describes the history; it is "
         "not a judgement about the code."));
  layout->addWidget(m_churnLabel);

  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

  m_stepsTree = new QTreeWidget(splitter);
  m_stepsTree->setObjectName(QStringLiteral("provenanceStepsTree"));
  m_stepsTree->setColumnCount(3);
  m_stepsTree->setHeaderLabels({tr("Commit"), tr("Author"), tr("Subject")});
  m_stepsTree->setRootIsDecorated(false);
  m_stepsTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  m_stepsTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_stepsTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  connect(m_stepsTree, &QTreeWidget::itemSelectionChanged, this,
          &ProvenanceLensDialog::onSelectionChanged);
  for (const GitProvenanceStep &step : m_provenance.steps) {
    QTreeWidgetItem *item = new QTreeWidgetItem(m_stepsTree);
    item->setText(0, step.commit.shortHash);
    item->setText(1, step.commit.author);
    item->setText(2, step.commit.subject);
    item->setData(0, HASH_ROLE, step.commit.hash);
    item->setToolTip(
        2, QStringLiteral("%1\n%2").arg(step.commit.subject, step.commit.date));
  }
  splitter->addWidget(m_stepsTree);

  QWidget *texts = new QWidget(splitter);
  QVBoxLayout *textLayout = new QVBoxLayout(texts);
  textLayout->setContentsMargins(0, 0, 0, 0);
  textLayout->setSpacing(UiMetrics::SpaceXs);

  const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

  QLabel *previousLabel = new QLabel(tr("What it replaced"), texts);
  previousLabel->setObjectName(QStringLiteral("provenancePreviousLabel"));
  textLayout->addWidget(previousLabel);

  m_previousView = new QPlainTextEdit(texts);
  m_previousView->setObjectName(QStringLiteral("provenancePreviousView"));
  m_previousView->setReadOnly(true);
  m_previousView->setFont(mono);
  m_previousView->setPlainText(
      m_provenance.previousText.isEmpty()
          ? tr("(nothing — these lines were introduced by that commit)")
          : m_provenance.previousText);
  textLayout->addWidget(m_previousView, 1);

  QLabel *currentLabel = new QLabel(tr("What is there now"), texts);
  currentLabel->setObjectName(QStringLiteral("provenanceCurrentLabel"));
  textLayout->addWidget(currentLabel);

  m_currentView = new QPlainTextEdit(texts);
  m_currentView->setObjectName(QStringLiteral("provenanceCurrentView"));
  m_currentView->setReadOnly(true);
  m_currentView->setFont(mono);
  m_currentView->setPlainText(m_provenance.currentText);
  textLayout->addWidget(m_currentView, 1);

  splitter->addWidget(texts);
  splitter->setSizes({420, 520});
  layout->addWidget(splitter, 1);

  m_caveatLabel = new QLabel(gitProvenanceCaveat(m_provenance), this);
  m_caveatLabel->setObjectName(QStringLiteral("provenanceCaveatLabel"));
  m_caveatLabel->setWordWrap(true);
  layout->addWidget(m_caveatLabel);

  QHBoxLayout *footer = new QHBoxLayout();
  const auto addButton = [&](QPushButton **button, const QString &text,
                             const QString &name, const QString &tip) {
    *button = new QPushButton(text, this);
    (*button)->setObjectName(name);
    (*button)->setToolTip(tip);
    footer->addWidget(*button);
  };
  addButton(&m_openCommitButton, tr("Open the commit"),
            QStringLiteral("provenanceOpenCommitButton"),
            tr("Reading the commit and its diff is usually what explains a "
               "change; the author name rarely does."));
  addButton(&m_parentDiffButton, tr("Diff against its parent"),
            QStringLiteral("provenanceParentDiffButton"),
            tr("Exactly what that commit changed."));
  addButton(&m_fileHistoryButton, tr("File timeline"),
            QStringLiteral("provenanceFileHistoryButton"),
            tr("How the whole file got here, renames included."));
  addButton(&m_graphButton, tr("Show in graph"),
            QStringLiteral("provenanceGraphButton"),
            tr("Where this commit sits in the history."));

  connect(m_openCommitButton, &QPushButton::clicked, this,
          &ProvenanceLensDialog::onOpenCommit);
  connect(m_parentDiffButton, &QPushButton::clicked, this,
          &ProvenanceLensDialog::onOpenParentDiff);
  connect(m_fileHistoryButton, &QPushButton::clicked, this,
          &ProvenanceLensDialog::onOpenFileHistory);
  connect(m_graphButton, &QPushButton::clicked, this,
          &ProvenanceLensDialog::onShowInGraph);

  footer->addStretch();
  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("provenanceCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  footer->addWidget(m_closeButton);
  layout->addLayout(footer);

  if (m_stepsTree->topLevelItemCount() > 0) {
    m_stepsTree->setCurrentItem(m_stepsTree->topLevelItem(0));
  }
}

QString ProvenanceLensDialog::selectedCommitHash() const {
  QTreeWidgetItem *item = m_stepsTree ? m_stepsTree->currentItem() : nullptr;
  if (item) {
    return item->data(0, HASH_ROLE).toString();
  }
  return m_provenance.latest.hash;
}

void ProvenanceLensDialog::onSelectionChanged() {
  const bool have = !selectedCommitHash().isEmpty();
  for (QPushButton *button :
       {m_openCommitButton, m_parentDiffButton, m_graphButton}) {
    if (button) {
      button->setEnabled(have);
    }
  }
}

void ProvenanceLensDialog::onOpenCommit() {
  const QString hash = selectedCommitHash();
  if (!hash.isEmpty()) {
    emit openCommitRequested(hash);
  }
}

void ProvenanceLensDialog::onOpenParentDiff() {
  const QString hash = selectedCommitHash();
  if (!hash.isEmpty()) {
    emit openParentDiffRequested(hash);
  }
}

void ProvenanceLensDialog::onOpenFileHistory() {
  emit openFileHistoryRequested(m_provenance.filePath, m_provenance.startLine,
                                m_provenance.endLine);
}

void ProvenanceLensDialog::onShowInGraph() {
  const QString hash = selectedCommitHash();
  if (!hash.isEmpty()) {
    emit showInGraphRequested(hash);
  }
}

void ProvenanceLensDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_headerLabel) {
    styleTitleLabel(m_headerLabel);
  }
  if (m_latestLabel) {
    m_latestLabel->setStyleSheet(
        QString("color: %1;").arg(theme.foregroundColor.name()));
  }
  for (const char *name : {"provenanceChurnLabel", "provenancePreviousLabel",
                           "provenanceCurrentLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  if (m_caveatLabel) {
    if (m_provenance.attributionUncertain) {

      m_caveatLabel->setStyleSheet(
          QString("color: %1;").arg(theme.warningColor.name()));
    } else {
      styleSubduedLabel(m_caveatLabel);
    }
  }
  if (m_stepsTree) {
    m_stepsTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  for (QPlainTextEdit *view : {m_previousView, m_currentView}) {
    if (view) {
      view->setStyleSheet(
          QString("QPlainTextEdit { background: %1; color: %2; border: 1px "
                  "solid %3; }")
              .arg(theme.surfaceColor.name(), theme.foregroundColor.name(),
                   theme.borderColor.name()));
    }
  }
  for (QPushButton *button :
       {m_openCommitButton, m_parentDiffButton, m_fileHistoryButton,
        m_graphButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
}
