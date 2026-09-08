#include "undochangeswizarddialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "operationpreviewdialog.h"
#include "themedmessagebox.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace {

QString objectNameFor(GitUndoGoal goal) {
  switch (goal) {
  case GitUndoGoal::DiscardWorkingEdits:
    return QStringLiteral("undoDiscardRadio");
  case GitUndoGoal::UnstageKeepEdits:
    return QStringLiteral("undoUnstageRadio");
  case GitUndoGoal::MoveBranchKeepStaged:
    return QStringLiteral("undoSoftResetRadio");
  case GitUndoGoal::MoveBranchKeepUnstaged:
    return QStringLiteral("undoMixedResetRadio");
  case GitUndoGoal::ResetEverything:
    return QStringLiteral("undoHardResetRadio");
  case GitUndoGoal::RevertPublishedCommit:
    return QStringLiteral("undoRevertRadio");
  case GitUndoGoal::AmendLastCommit:
    return QStringLiteral("undoAmendRadio");
  }
  return QString();
}

const QList<GitUndoGoal> &allGoals() {
  static const QList<GitUndoGoal> goals{
      GitUndoGoal::DiscardWorkingEdits,  GitUndoGoal::UnstageKeepEdits,
      GitUndoGoal::MoveBranchKeepStaged, GitUndoGoal::MoveBranchKeepUnstaged,
      GitUndoGoal::ResetEverything,      GitUndoGoal::RevertPublishedCommit,
      GitUndoGoal::AmendLastCommit};
  return goals;
}

} // namespace

UndoChangesWizardDialog::UndoChangesWizardDialog(GitIntegration *git,
                                                 const Theme &theme,
                                                 QWidget *parent)
    : StyledDialog(parent), m_git(git), m_headerLabel(nullptr),
      m_recommendationLabel(nullptr), m_matrixLabel(nullptr),
      m_commandLabel(nullptr), m_riskList(nullptr), m_previewButton(nullptr),
      m_executeButton(nullptr), m_cancelButton(nullptr), m_updating(false) {
  setWindowTitle(tr("Undo changes"));
  setMinimumSize(780, 640);
  resize(900, 720);

  buildUi();
  setKeyboardDefault(m_executeButton);
  applyTheme(theme);
  reload();
}

void UndoChangesWizardDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  m_headerLabel = new QLabel(this);
  m_headerLabel->setObjectName(QStringLiteral("undoHeaderLabel"));
  m_headerLabel->setWordWrap(true);
  layout->addWidget(m_headerLabel);

  m_recommendationLabel = new QLabel(this);
  m_recommendationLabel->setObjectName(
      QStringLiteral("undoRecommendationLabel"));
  m_recommendationLabel->setWordWrap(true);
  layout->addWidget(m_recommendationLabel);

  buildOptions(layout);
  buildMatrix(layout);
  buildFooter(layout);
}

void UndoChangesWizardDialog::buildOptions(QVBoxLayout *layout) {
  for (GitUndoGoal goal : allGoals()) {
    QRadioButton *radio = new QRadioButton(this);
    radio->setObjectName(objectNameFor(goal));
    connect(radio, &QRadioButton::toggled, this,
            &UndoChangesWizardDialog::onGoalChanged);
    layout->addWidget(radio);

    QLabel *explanation = new QLabel(this);
    explanation->setObjectName(objectNameFor(goal) +
                               QStringLiteral("Explanation"));
    explanation->setWordWrap(true);
    explanation->setIndent(24);
    layout->addWidget(explanation);

    m_radios.insert(goal, radio);
    m_explanations.insert(goal, explanation);
  }
}

void UndoChangesWizardDialog::buildMatrix(QVBoxLayout *layout) {
  m_matrixLabel = new QLabel(this);
  m_matrixLabel->setObjectName(QStringLiteral("undoMatrixLabel"));
  m_matrixLabel->setWordWrap(true);
  m_matrixLabel->setToolTip(
      tr("The four things Git can move independently. Which cells change is "
         "the whole difference between restore, reset, revert and amend."));
  layout->addWidget(m_matrixLabel);

  m_riskList = new QListWidget(this);
  m_riskList->setObjectName(QStringLiteral("undoRiskList"));
  m_riskList->setMaximumHeight(110);
  m_riskList->setSelectionMode(QAbstractItemView::NoSelection);
  m_riskList->setFocusPolicy(Qt::NoFocus);
  layout->addWidget(m_riskList);

  m_commandLabel = new QLabel(this);
  m_commandLabel->setObjectName(QStringLiteral("undoCommandLabel"));
  m_commandLabel->setWordWrap(true);
  m_commandLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(m_commandLabel);
}

void UndoChangesWizardDialog::buildFooter(QVBoxLayout *layout) {
  QHBoxLayout *footer = new QHBoxLayout();
  footer->addStretch();

  m_previewButton = new QPushButton(tr("Show full preview…"), this);
  m_previewButton->setObjectName(QStringLiteral("undoPreviewButton"));
  connect(m_previewButton, &QPushButton::clicked, this,
          &UndoChangesWizardDialog::onShowPreview);
  footer->addWidget(m_previewButton);

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setObjectName(QStringLiteral("undoCancelButton"));
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  footer->addWidget(m_cancelButton);

  m_executeButton = new QPushButton(tr("Undo"), this);
  m_executeButton->setObjectName(QStringLiteral("undoExecuteButton"));
  connect(m_executeButton, &QPushButton::clicked, this,
          &UndoChangesWizardDialog::onExecute);
  footer->addWidget(m_executeButton);

  layout->addLayout(footer);
}

void UndoChangesWizardDialog::setTarget(const QString &commit,
                                        const QStringList &paths) {
  m_targetCommit = commit;
  m_targetPaths = paths;
  reload();
}

void UndoChangesWizardDialog::reload() {
  m_plan = buildGitUndoPlan(m_git, m_targetCommit, m_targetPaths);

  m_headerLabel->setText(
      m_targetCommit.isEmpty()
          ? tr("What would you like to undo?")
          : tr("What would you like to undo about <code>%1</code>?")
                .arg(m_targetCommit.left(12)));
  m_recommendationLabel->setText(
      m_plan.valid ? tr("<b>Suggested:</b> %1 — %2")
                         .arg(gitUndoGoalName(m_plan.recommended),
                              m_plan.recommendationReason)
                   : m_plan.error);

  m_updating = true;
  for (const GitUndoOption &option : m_plan.options) {
    QRadioButton *radio = m_radios.value(option.goal, nullptr);
    QLabel *explanation = m_explanations.value(option.goal, nullptr);
    if (!radio || !explanation) {
      continue;
    }
    radio->setText(option.question);
    radio->setEnabled(option.isAvailable());
    explanation->setText(
        option.isAvailable()
            ? option.explanation
            : tr("Not available: %1").arg(option.unavailableReason));
  }
  m_updating = false;

  if (QRadioButton *radio = m_radios.value(m_plan.recommended, nullptr)) {
    if (radio->isEnabled()) {
      radio->setChecked(true);
    }
  }
  updateSelectedOption();
}

const GitUndoOption *
UndoChangesWizardDialog::optionFor(GitUndoGoal goal) const {
  for (const GitUndoOption &option : m_plan.options) {
    if (option.goal == goal) {
      return &option;
    }
  }
  return nullptr;
}

GitUndoGoal UndoChangesWizardDialog::selectedGoal() const {
  for (auto it = m_radios.constBegin(); it != m_radios.constEnd(); ++it) {
    if (it.value()->isChecked()) {
      return it.key();
    }
  }
  return GitUndoGoal::DiscardWorkingEdits;
}

void UndoChangesWizardDialog::onGoalChanged() {
  if (m_updating) {
    return;
  }
  updateSelectedOption();
}

void UndoChangesWizardDialog::updateSelectedOption() {
  const GitUndoOption *option = optionFor(selectedGoal());
  if (!option) {
    return;
  }

  m_matrixLabel->setText(
      tr("<b>Working Tree</b>: %1 &nbsp;·&nbsp; <b>Index</b>: %2 "
         "&nbsp;·&nbsp; <b>HEAD</b>: %3 &nbsp;·&nbsp; <b>Branch</b>: %4")
          .arg(option->matrix.workingTree, option->matrix.index,
               option->matrix.head, option->matrix.branch));
  m_commandLabel->setText(
      tr("Command: <code>%1</code>").arg(option->command.toHtmlEscaped()));

  m_riskList->clear();
  if (option->destructive && !option->atRiskPaths.isEmpty()) {
    for (const QString &path : option->atRiskPaths) {
      QListWidgetItem *item = new QListWidgetItem(
          tr("⚠ %1 — its uncommitted content is lost").arg(path), m_riskList);
      item->setFlags(Qt::ItemIsEnabled);
    }
  } else if (option->destructive) {
    QListWidgetItem *item = new QListWidgetItem(
        tr("Nothing uncommitted is at risk right now."), m_riskList);
    item->setFlags(Qt::ItemIsEnabled);
  } else {
    QListWidgetItem *item = new QListWidgetItem(
        tr("✓ Nothing is thrown away: every change ends up somewhere you can "
           "get it back from."),
        m_riskList);
    item->setFlags(Qt::ItemIsEnabled);
  }

  m_previewButton->setEnabled(option->hasOperationPreview &&
                              option->isAvailable());
  m_executeButton->setEnabled(option->isAvailable());
  applyTheme(m_theme);
}

void UndoChangesWizardDialog::onShowPreview() {
  const GitUndoOption *option = optionFor(selectedGoal());
  if (!option || !option->hasOperationPreview) {
    return;
  }
  GitOperationRequest request;
  request.kind = option->previewKind;
  request.target = option->previewTarget;
  request.resetMode = option->resetMode;
  OperationPreviewDialog preview(m_git, request, m_theme, this);
  preview.exec();
}

void UndoChangesWizardDialog::onExecute() {
  const GitUndoOption *option = optionFor(selectedGoal());
  if (!option || !option->isAvailable() || !m_git) {
    return;
  }

  if (option->hasOperationPreview) {
    GitOperationRequest request;
    request.kind = option->previewKind;
    request.target = option->previewTarget;
    request.resetMode = option->resetMode;
    if (!OperationPreviewDialog::confirm(m_git, request, m_theme, this)) {
      return;
    }
  } else if (option->destructive) {

    if (ThemedMessageBox::question(
            this, tr("Discard uncommitted edits"),
            tr("Throw away the uncommitted content of %1 file(s)?<br><br>"
               "<i>%2</i>")
                .arg(option->atRiskPaths.size())
                .arg(option->explanation)) != ThemedMessageBox::Yes) {
      return;
    }
  }

  bool ok = false;
  switch (option->goal) {
  case GitUndoGoal::DiscardWorkingEdits:
    if (m_targetPaths.isEmpty()) {
      ok = m_git->discardAllChanges();
    } else {
      ok = true;
      for (const QString &path : m_targetPaths) {
        ok = m_git->discardChanges(path) && ok;
      }
    }
    break;
  case GitUndoGoal::UnstageKeepEdits:
    if (m_targetPaths.isEmpty()) {
      ok = m_git->unstageAll();
    } else {
      ok = true;
      for (const QString &path : m_targetPaths) {
        ok = m_git->unstageFile(path) && ok;
      }
    }
    break;
  case GitUndoGoal::MoveBranchKeepStaged:
    ok = m_git->resetToCommit(option->previewTarget, QStringLiteral("soft"));
    break;
  case GitUndoGoal::MoveBranchKeepUnstaged:
    ok = m_git->resetToCommit(option->previewTarget, QStringLiteral("mixed"));
    break;
  case GitUndoGoal::ResetEverything:
    ok = m_git->resetToCommit(option->previewTarget, QStringLiteral("hard"));
    break;
  case GitUndoGoal::RevertPublishedCommit:
    ok = m_git->revertCommit(option->previewTarget);
    break;
  case GitUndoGoal::AmendLastCommit:

    emit amendRequested();
    accept();
    return;
  }

  if (ok) {
    emit repositoryChanged();
    accept();
  }
}

void UndoChangesWizardDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_headerLabel) {
    styleTitleLabel(m_headerLabel);
  }
  for (const char *name :
       {"undoRecommendationLabel", "undoMatrixLabel", "undoCommandLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  for (QRadioButton *radio : m_radios) {
    radio->setStyleSheet(UIStyleHelper::checkBoxStyle(theme) +
                         QString("QRadioButton { color: %1; }"
                                 "QRadioButton:disabled { color: %2; }")
                             .arg(theme.foregroundColor.name(),
                                  theme.singleLineCommentFormat.name()));
  }
  for (QLabel *label : m_explanations) {
    styleSubduedLabel(label);
  }

  const GitUndoOption *option = optionFor(selectedGoal());
  const bool destructive = option && option->destructive;
  if (m_riskList) {
    m_riskList->setStyleSheet(
        QString("QListWidget { background: %1; color: %2; border: 1px solid "
                "%3; }")
            .arg(theme.surfaceColor.name(),
                 (destructive ? theme.errorColor : theme.successColor).name(),
                 theme.borderColor.name()));
  }
  for (QPushButton *button : {m_previewButton, m_cancelButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_executeButton) {
    if (destructive) {
      styleDangerButton(m_executeButton);
    } else {
      stylePrimaryButton(m_executeButton);
    }
  }
}
