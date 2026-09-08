#include "conflictstoryboarddialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "themedmessagebox.h"
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

ConflictStoryboardDialog::ConflictStoryboardDialog(GitIntegration *git,
                                                   const Theme &theme,
                                                   QWidget *parent)
    : StyledDialog(parent), m_git(git), m_operationLabel(nullptr),
      m_oursLabel(nullptr), m_theirsLabel(nullptr), m_baseLabel(nullptr),
      m_progressLabel(nullptr), m_classLabel(nullptr),
      m_classExplanation(nullptr), m_fileList(nullptr), m_baseView(nullptr),
      m_oursView(nullptr), m_theirsView(nullptr), m_resultView(nullptr),
      m_historyTree(nullptr), m_takeOursButton(nullptr),
      m_takeTheirsButton(nullptr), m_takeBothButton(nullptr),
      m_diffOursButton(nullptr), m_diffTheirsButton(nullptr),
      m_resolveButton(nullptr), m_continueButton(nullptr),
      m_abortButton(nullptr), m_closeButton(nullptr) {
  setWindowTitle(tr("Conflict storyboard"));
  setMinimumSize(960, 680);
  resize(1160, 780);

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  reload();
}

void ConflictStoryboardDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  buildIdentityHeader(layout);
  buildBody(layout);
  buildActions(layout);
}

void ConflictStoryboardDialog::buildIdentityHeader(QVBoxLayout *layout) {
  const auto makeLabel = [&](QLabel **label, const QString &name) {
    *label = new QLabel(this);
    (*label)->setObjectName(name);
    (*label)->setWordWrap(true);
    layout->addWidget(*label);
  };

  makeLabel(&m_operationLabel, QStringLiteral("conflictOperationLabel"));
  makeLabel(&m_oursLabel, QStringLiteral("conflictOursLabel"));
  makeLabel(&m_theirsLabel, QStringLiteral("conflictTheirsLabel"));
  makeLabel(&m_baseLabel, QStringLiteral("conflictBaseLabel"));
  makeLabel(&m_progressLabel, QStringLiteral("conflictProgressLabel"));
}

void ConflictStoryboardDialog::buildBody(QVBoxLayout *layout) {
  QSplitter *outer = new QSplitter(Qt::Horizontal, this);

  m_fileList = new QListWidget(outer);
  m_fileList->setObjectName(QStringLiteral("conflictFileList"));
  connect(m_fileList, &QListWidget::currentRowChanged, this,
          &ConflictStoryboardDialog::onFileSelected);
  outer->addWidget(m_fileList);

  QWidget *right = new QWidget(outer);
  QVBoxLayout *rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(UiMetrics::SpaceSm);

  m_classLabel = new QLabel(right);
  m_classLabel->setObjectName(QStringLiteral("conflictClassLabel"));
  rightLayout->addWidget(m_classLabel);

  m_classExplanation = new QLabel(right);
  m_classExplanation->setObjectName(QStringLiteral("conflictClassExplanation"));
  m_classExplanation->setWordWrap(true);
  rightLayout->addWidget(m_classExplanation);

  const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  QSplitter *panes = new QSplitter(Qt::Horizontal, right);

  const auto makePane = [&](QPlainTextEdit **view, const QString &title,
                            const QString &name, const QString &tip) {
    QWidget *column = new QWidget(panes);
    QVBoxLayout *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(UiMetrics::SpaceXs);

    QLabel *label = new QLabel(title, column);
    label->setObjectName(name + QStringLiteral("Label"));
    label->setToolTip(tip);
    columnLayout->addWidget(label);

    *view = new QPlainTextEdit(column);
    (*view)->setObjectName(name);
    (*view)->setFont(mono);
    (*view)->setReadOnly(true);
    (*view)->setLineWrapMode(QPlainTextEdit::NoWrap);
    columnLayout->addWidget(*view, 1);
    panes->addWidget(column);
  };

  makePane(&m_baseView, tr("BASE"), QStringLiteral("conflictBaseView"),
           tr("The last version both sides agreed on. Every conflict is a "
              "disagreement measured from here."));
  makePane(&m_oursView, tr("OURS"), QStringLiteral("conflictOursView"),
           tr("The side already checked out."));
  makePane(&m_theirsView, tr("THEIRS"), QStringLiteral("conflictTheirsView"),
           tr("The side being brought in."));
  panes->setSizes({300, 300, 300});
  rightLayout->addWidget(panes, 2);

  QLabel *resultLabel =
      new QLabel(tr("RESULT — edit this into what you want"), right);
  resultLabel->setObjectName(QStringLiteral("conflictResultLabel"));
  rightLayout->addWidget(resultLabel);

  m_resultView = new QPlainTextEdit(right);
  m_resultView->setObjectName(QStringLiteral("conflictResultView"));
  m_resultView->setFont(mono);
  m_resultView->setLineWrapMode(QPlainTextEdit::NoWrap);
  rightLayout->addWidget(m_resultView, 2);

  QLabel *historyLabel =
      new QLabel(tr("Commits that changed this file on each side"), right);
  historyLabel->setObjectName(QStringLiteral("conflictHistoryLabel"));
  rightLayout->addWidget(historyLabel);

  m_historyTree = new QTreeWidget(right);
  m_historyTree->setObjectName(QStringLiteral("conflictHistoryTree"));
  m_historyTree->setColumnCount(3);
  m_historyTree->setHeaderLabels({tr("Side"), tr("Commit"), tr("Subject")});
  m_historyTree->setRootIsDecorated(false);
  m_historyTree->setMaximumHeight(120);
  m_historyTree->header()->setSectionResizeMode(0,
                                                QHeaderView::ResizeToContents);
  m_historyTree->header()->setSectionResizeMode(1,
                                                QHeaderView::ResizeToContents);
  m_historyTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  rightLayout->addWidget(m_historyTree);

  outer->addWidget(right);
  outer->setSizes({240, 900});
  layout->addWidget(outer, 1);
}

void ConflictStoryboardDialog::buildActions(QVBoxLayout *layout) {
  QHBoxLayout *take = new QHBoxLayout();
  take->setSpacing(UiMetrics::ToolbarGroupSpacing);

  const auto makeButton = [&](QPushButton **button, const QString &text,
                              const QString &name, const QString &tip) {
    *button = new QPushButton(text, this);
    (*button)->setObjectName(name);
    (*button)->setToolTip(tip);
    take->addWidget(*button);
  };

  makeButton(&m_takeOursButton, tr("Take ours"),
             QStringLiteral("conflictTakeOursButton"),
             tr("Replace the result with this side's whole version."));
  makeButton(&m_takeTheirsButton, tr("Take theirs"),
             QStringLiteral("conflictTakeTheirsButton"),
             tr("Replace the result with the incoming side's whole version."));
  makeButton(&m_takeBothButton, tr("Take both"),
             QStringLiteral("conflictTakeBothButton"),
             tr("Keep both sides, ours first, with the markers removed."));
  makeButton(&m_diffOursButton, tr("Diff result vs ours"),
             QStringLiteral("conflictDiffOursButton"),
             tr("Check what your resolution changed relative to this side."));
  makeButton(&m_diffTheirsButton, tr("Diff result vs theirs"),
             QStringLiteral("conflictDiffTheirsButton"),
             tr("Check what your resolution changed relative to that side."));

  connect(m_takeOursButton, &QPushButton::clicked, this,
          &ConflictStoryboardDialog::onTakeOurs);
  connect(m_takeTheirsButton, &QPushButton::clicked, this,
          &ConflictStoryboardDialog::onTakeTheirs);
  connect(m_takeBothButton, &QPushButton::clicked, this,
          &ConflictStoryboardDialog::onTakeBoth);
  connect(m_diffOursButton, &QPushButton::clicked, this,
          &ConflictStoryboardDialog::onDiffAgainstOurs);
  connect(m_diffTheirsButton, &QPushButton::clicked, this,
          &ConflictStoryboardDialog::onDiffAgainstTheirs);

  take->addStretch();

  m_resolveButton = new QPushButton(tr("Mark this file resolved"), this);
  m_resolveButton->setObjectName(QStringLiteral("conflictResolveButton"));
  connect(m_resolveButton, &QPushButton::clicked, this,
          &ConflictStoryboardDialog::onMarkResolved);
  take->addWidget(m_resolveButton);

  layout->addLayout(take);

  QHBoxLayout *footer = new QHBoxLayout();
  m_continueButton = new QPushButton(this);
  m_continueButton->setObjectName(QStringLiteral("conflictContinueButton"));
  connect(m_continueButton, &QPushButton::clicked, this,
          &ConflictStoryboardDialog::onContinueOperation);
  footer->addWidget(m_continueButton);

  m_abortButton = new QPushButton(this);
  m_abortButton->setObjectName(QStringLiteral("conflictAbortButton"));
  connect(m_abortButton, &QPushButton::clicked, this,
          &ConflictStoryboardDialog::onAbortOperation);
  footer->addWidget(m_abortButton);

  footer->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("conflictCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
  footer->addWidget(m_closeButton);

  layout->addLayout(footer);
}

void ConflictStoryboardDialog::reload() {
  m_context = buildGitConflictContext(m_git);

  const QString operationName = gitOperationName(m_context.operation);
  m_operationLabel->setText(
      operationName.isEmpty()
          ? tr("<b>No operation is stuck.</b>")
          : tr("<b>%1 in progress</b> — Git could not combine these changes on "
               "its own.")
                .arg(operationName));
  m_oursLabel->setText(
      tr("<b>OURS</b>: %1 — <code>%2</code> %3<br><i>%4</i>")
          .arg(m_context.oursLabel.toHtmlEscaped(),
               m_context.files.isEmpty()
                   ? QString()
                   : m_context.files.first().ours.shortHash,
               m_context.files.isEmpty()
                   ? QString()
                   : m_context.files.first().ours.subject.toHtmlEscaped(),
               m_context.oursHint.toHtmlEscaped()));
  m_theirsLabel->setText(
      tr("<b>THEIRS</b>: %1 — <code>%2</code> %3<br><i>%4</i>")
          .arg(m_context.theirsLabel.toHtmlEscaped(),
               m_context.files.isEmpty()
                   ? QString()
                   : m_context.files.first().theirs.shortHash,
               m_context.files.isEmpty()
                   ? QString()
                   : m_context.files.first().theirs.subject.toHtmlEscaped(),
               m_context.theirsHint.toHtmlEscaped()));
  m_baseLabel->setText(
      m_context.mergeBase.isEmpty()
          ? tr("No common ancestor, so every difference is contested.")
          : tr("<b>Merge base</b>: <code>%1</code> %2 — the last point both "
               "sides agreed on.")
                .arg(m_context.mergeBase.left(7),
                     m_context.mergeBaseSubject.toHtmlEscaped()));

  m_fileList->clear();
  for (const GitConflictFile &file : m_context.files) {
    QListWidgetItem *item =
        new QListWidgetItem(QStringLiteral("%1 %2").arg(
                                file.resolved ? tr("✓") : tr("⚠"), file.path),
                            m_fileList);
    item->setToolTip(gitConflictClassName(file.conflictClass));
  }
  if (m_fileList->count() > 0) {
    m_fileList->setCurrentRow(0);
  } else {
    showFile(-1);
  }

  updateProgress();
}

int ConflictStoryboardDialog::currentFileIndex() const {
  return m_fileList ? m_fileList->currentRow() : -1;
}

const GitConflictFile *ConflictStoryboardDialog::currentFile() const {
  const int index = currentFileIndex();
  if (index < 0 || index >= m_context.files.size()) {
    return nullptr;
  }
  return &m_context.files.at(index);
}

void ConflictStoryboardDialog::onFileSelected() {
  showFile(currentFileIndex());
}

void ConflictStoryboardDialog::showFile(int index) {
  const bool have = index >= 0 && index < m_context.files.size();
  for (QPushButton *button :
       {m_takeOursButton, m_takeTheirsButton, m_takeBothButton,
        m_diffOursButton, m_diffTheirsButton, m_resolveButton}) {
    button->setEnabled(have);
  }
  m_resultView->setEnabled(have);

  if (!have) {
    m_classLabel->clear();
    m_classExplanation->clear();
    for (QPlainTextEdit *view :
         {m_baseView, m_oursView, m_theirsView, m_resultView}) {
      view->clear();
    }
    m_historyTree->clear();
    return;
  }

  const GitConflictFile &file = m_context.files.at(index);
  m_classLabel->setText(
      tr("<b>%1</b> — %2")
          .arg(gitConflictClassName(file.conflictClass), file.statusCode));
  m_classExplanation->setText(gitConflictClassExplanation(file.conflictClass));

  const auto fill = [](QPlainTextEdit *view, const GitConflictSide &side,
                       const QString &missingText) {
    view->setPlainText(side.exists ? side.content : missingText);
  };
  fill(m_baseView, file.base, tr("(the file did not exist at the merge base)"));
  fill(m_oursView, file.ours, tr("(this side does not have the file)"));
  fill(m_theirsView, file.theirs, tr("(that side does not have the file)"));
  m_resultView->setPlainText(file.merged);

  m_takeOursButton->setEnabled(file.ours.exists);
  m_takeTheirsButton->setEnabled(file.theirs.exists);
  m_takeBothButton->setEnabled(file.ours.exists && file.theirs.exists &&
                               file.conflictClass != GitConflictClass::Binary);
  m_resultView->setReadOnly(file.conflictClass == GitConflictClass::Binary);

  m_historyTree->clear();
  if (m_git && !m_context.mergeBase.isEmpty()) {
    const auto addSide = [&](const QString &label, const QString &range) {
      for (const GitCommitInfo &commit :
           m_git->getFileLogRange(file.path, range, 20)) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_historyTree);
        item->setText(0, label);
        item->setText(1, commit.shortHash);
        item->setText(
            2, QStringLiteral("%1 — %2").arg(commit.subject, commit.author));
      }
    };
    addSide(tr("ours"), QStringLiteral("%1..%2").arg(m_context.mergeBase,
                                                     file.ours.commitHash));
    addSide(tr("theirs"), QStringLiteral("%1..%2").arg(m_context.mergeBase,
                                                       file.theirs.commitHash));
  }
}

void ConflictStoryboardDialog::updateProgress() {
  m_progressLabel->setText(m_context.files.isEmpty()
                               ? tr("Nothing is conflicted.")
                               : tr("%1 of %2 files resolved — %3 left.")
                                     .arg(m_context.resolvedCount())
                                     .arg(m_context.files.size())
                                     .arg(m_context.remainingCount()));

  const QString operationName = gitOperationName(m_context.operation);
  m_continueButton->setText(
      operationName.isEmpty() ? tr("Continue")
                              : tr("Continue %1").arg(operationName.toLower()));
  m_abortButton->setText(operationName.isEmpty()
                             ? tr("Abort")
                             : tr("Abort %1").arg(operationName.toLower()));
  m_continueButton->setEnabled(m_context.remainingCount() == 0 &&
                               !m_context.files.isEmpty());
  m_abortButton->setEnabled(m_context.operation != GitOperation::None);
}

void ConflictStoryboardDialog::onTakeOurs() {
  const GitConflictFile *file = currentFile();
  if (file && file->ours.exists) {
    m_resultView->setPlainText(file->ours.content);
  }
}

void ConflictStoryboardDialog::onTakeTheirs() {
  const GitConflictFile *file = currentFile();
  if (file && file->theirs.exists) {
    m_resultView->setPlainText(file->theirs.content);
  }
}

void ConflictStoryboardDialog::onTakeBoth() {
  const GitConflictFile *file = currentFile();
  if (!file || !file->ours.exists || !file->theirs.exists) {
    return;
  }
  QString combined = file->ours.content;
  if (!combined.endsWith(QLatin1Char('\n'))) {
    combined += QLatin1Char('\n');
  }
  combined += file->theirs.content;
  m_resultView->setPlainText(combined);
}

void ConflictStoryboardDialog::showDiff(const QString &title,
                                        const QString &other) {
  const QString result = m_resultView->toPlainText();
  const QStringList resultLines = result.split(QLatin1Char('\n'));
  const QStringList otherLines = other.split(QLatin1Char('\n'));

  QStringList report;
  const int count = qMax(resultLines.size(), otherLines.size());
  for (int i = 0; i < count; ++i) {
    const QString a = i < otherLines.size() ? otherLines.at(i) : QString();
    const QString b = i < resultLines.size() ? resultLines.at(i) : QString();
    if (a != b) {
      report
          << tr("line %1:\n  %2: %3\n  result: %4").arg(i + 1).arg(title, a, b);
    }
  }

  ThemedMessageBox::information(
      this, tr("Result vs %1").arg(title),
      report.isEmpty() ? tr("Your result is identical to %1.").arg(title)
                       : report.join(QStringLiteral("\n")).left(4000));
}

void ConflictStoryboardDialog::onDiffAgainstOurs() {
  if (const GitConflictFile *file = currentFile()) {
    showDiff(m_context.oursLabel, file->ours.content);
  }
}

void ConflictStoryboardDialog::onDiffAgainstTheirs() {
  if (const GitConflictFile *file = currentFile()) {
    showDiff(m_context.theirsLabel, file->theirs.content);
  }
}

void ConflictStoryboardDialog::onMarkResolved() {
  const GitConflictFile *file = currentFile();
  if (!file || !m_git) {
    return;
  }

  const QString result = m_resultView->toPlainText();
  if (result.contains(QStringLiteral("<<<<<<<")) ||
      result.contains(QStringLiteral(">>>>>>>"))) {
    if (ThemedMessageBox::question(
            this, tr("Conflict markers are still there"),
            tr("The result still contains <code>&lt;&lt;&lt;&lt;&lt;&lt;&lt;"
               "</code> markers. Committing them would put them in the "
               "file.<br><br>Mark it resolved anyway?")) !=
        ThemedMessageBox::Yes) {
      return;
    }
  }

  const int index = currentFileIndex();
  if (m_git->resolveConflictWith(file->path, result)) {
    reload();
    if (index < m_fileList->count()) {
      m_fileList->setCurrentRow(index);
    }
    emit repositoryChanged();
  }
}

void ConflictStoryboardDialog::onContinueOperation() {
  if (!m_git) {
    return;
  }

  bool ok = false;
  switch (m_context.operation) {
  case GitOperation::Rebase:
    ok = m_git->rebaseContinue();
    break;
  case GitOperation::Merge:
    ok = m_git->continueMerge();
    break;
  default:
    ok = m_git->continueMerge();
    break;
  }

  emit repositoryChanged();
  if (ok && !m_git->hasMergeConflicts()) {
    accept();
  } else {
    reload();
  }
}

void ConflictStoryboardDialog::onAbortOperation() {
  if (!m_git) {
    return;
  }
  if (ThemedMessageBox::question(
          this, tr("Abort"),
          tr("Put the repository back the way it was before this operation "
             "started?<br><br><i>Your resolutions so far are thrown away; "
             "nothing that was committed before is affected.</i>")) !=
      ThemedMessageBox::Yes) {
    return;
  }

  if (m_context.operation == GitOperation::Rebase) {
    m_git->rebaseAbort();
  } else {
    m_git->abortMerge();
  }
  emit repositoryChanged();
  accept();
}

void ConflictStoryboardDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_operationLabel) {
    styleTitleLabel(m_operationLabel);
  }
  for (const char *name :
       {"conflictOursLabel", "conflictTheirsLabel", "conflictBaseLabel",
        "conflictProgressLabel", "conflictClassExplanation",
        "conflictResultLabel", "conflictHistoryLabel", "conflictBaseViewLabel",
        "conflictOursViewLabel", "conflictTheirsViewLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  if (m_classLabel) {
    m_classLabel->setStyleSheet(QString("color: %1; font-weight: bold;")
                                    .arg(theme.warningColor.name()));
  }
  if (m_fileList) {
    m_fileList->setStyleSheet(UIStyleHelper::resultListStyle(theme));
  }
  if (m_historyTree) {
    m_historyTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }

  const auto stylePane = [&](QPlainTextEdit *view, const QColor &border) {
    if (!view) {
      return;
    }
    view->setStyleSheet(
        QString("QPlainTextEdit { background: %1; color: %2; border: 1px "
                "solid %3; }")
            .arg(theme.surfaceColor.name(), theme.foregroundColor.name(),
                 border.name()));
  };
  stylePane(m_baseView, theme.borderColor);
  stylePane(m_oursView, theme.infoColor);
  stylePane(m_theirsView, theme.warningColor);
  stylePane(m_resultView, theme.successColor);

  for (QPushButton *button :
       {m_takeOursButton, m_takeTheirsButton, m_takeBothButton,
        m_diffOursButton, m_diffTheirsButton, m_closeButton,
        m_continueButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_resolveButton) {
    stylePrimaryButton(m_resolveButton);
  }
  if (m_abortButton) {
    styleDangerButton(m_abortButton);
  }
}
