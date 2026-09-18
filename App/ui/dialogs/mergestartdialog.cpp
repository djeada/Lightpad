#include "mergestartdialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"

#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int kConflictPreviewLimit = 12;

QString copyDescription(const GitMergeSource &source) {
  QString text = source.whereLabel();
  if (!source.summary.shortHash.isEmpty()) {
    text += QStringLiteral("  -  %1").arg(source.summary.shortHash);
  }
  if (!source.summary.subject.isEmpty()) {
    text += QStringLiteral("  \"%1\"").arg(source.summary.subject);
  }
  if (!source.summary.relativeDate.isEmpty()) {
    text += QStringLiteral("  (%1)").arg(source.summary.relativeDate);
  }
  return text;
}

} // namespace

MergeStartDialog::MergeStartDialog(GitIntegration *git, const Theme &theme,
                                   QWidget *parent)
    : StyledDialog(parent), m_git(git) {
  setWindowTitle(tr("Combine two branches"));
  setObjectName(QStringLiteral("mergeStartDialog"));
  setMinimumSize(620, 560);
  resize(720, 700);

  buildUi();
  loadChoices();
  applyTheme(theme);
  refreshPlan();
}

void MergeStartDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceXl, UiMetrics::SpaceXl,
                             UiMetrics::SpaceXl, UiMetrics::SpaceXl);
  layout->setSpacing(UiMetrics::SpaceLg);

  QLabel *title = new QLabel(tr("Combine two branches"), this);
  title->setObjectName(QStringLiteral("mergeTitleLabel"));
  layout->addWidget(title);

  QLabel *subtitle = new QLabel(
      tr("Choose a source and destination. Review the result before merging."),
      this);
  subtitle->setObjectName(QStringLiteral("mergeSubtitleLabel"));
  subtitle->setWordWrap(true);
  layout->addWidget(subtitle);

  QScrollArea *scroll = new QScrollArea(this);
  scroll->setObjectName(QStringLiteral("mergeStepsScrollArea"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QWidget *steps = new QWidget(scroll);
  steps->setObjectName(QStringLiteral("mergeStepsContainer"));
  QVBoxLayout *stepsLayout = new QVBoxLayout(steps);
  stepsLayout->setContentsMargins(0, UiMetrics::SpaceMd, UiMetrics::SpaceMd, 0);
  stepsLayout->setSpacing(UiMetrics::SpaceLg);

  buildSourceStep(stepsLayout);
  buildTargetStep(stepsLayout);
  buildOutcomeStep(stepsLayout);
  buildPreferenceStep(stepsLayout);
  stepsLayout->addStretch();

  scroll->setWidget(steps);
  layout->addWidget(scroll, 1);

  buildActions(layout);
}

void MergeStartDialog::buildSourceStep(QVBoxLayout *layout) {
  QGroupBox *group = new QGroupBox(tr("Source branch"), this);
  group->setObjectName(QStringLiteral("mergeSourceGroup"));
  QVBoxLayout *groupLayout = new QVBoxLayout(group);
  groupLayout->setSpacing(UiMetrics::SpaceMd);

  m_branchCombo = new QComboBox(group);
  m_branchCombo->setObjectName(QStringLiteral("mergeSourceCombo"));
  m_branchCombo->setMinimumContentsLength(16);
  m_branchCombo->setSizeAdjustPolicy(
      QComboBox::AdjustToMinimumContentsLengthWithIcon);
  connect(m_branchCombo, &QComboBox::currentIndexChanged, this,
          &MergeStartDialog::onBranchChanged);
  groupLayout->addWidget(m_branchCombo);

  m_copyGroup = new QGroupBox(tr("Which copy of it?"), group);
  m_copyGroup->setObjectName(QStringLiteral("mergeCopyGroup"));
  m_copyLayout = new QVBoxLayout(m_copyGroup);
  m_copyLayout->setSpacing(UiMetrics::SpaceSm);
  groupLayout->addWidget(m_copyGroup);

  m_copyWarningLabel = new QLabel(group);
  m_copyWarningLabel->setObjectName(QStringLiteral("mergeCopyWarningLabel"));
  m_copyWarningLabel->setWordWrap(true);
  m_copyWarningLabel->hide();
  groupLayout->addWidget(m_copyWarningLabel);

  layout->addWidget(group);

  QLabel *arrow = new QLabel(tr("goes into"), this);
  arrow->setObjectName(QStringLiteral("mergeArrowLabel"));
  arrow->setAlignment(Qt::AlignCenter);
  layout->addWidget(arrow);
}

void MergeStartDialog::buildTargetStep(QVBoxLayout *layout) {
  QGroupBox *group = new QGroupBox(tr("Destination branch"), this);
  group->setObjectName(QStringLiteral("mergeTargetGroup"));
  QVBoxLayout *groupLayout = new QVBoxLayout(group);

  m_targetCombo = new QComboBox(group);
  m_targetCombo->setObjectName(QStringLiteral("mergeTargetCombo"));
  m_targetCombo->setMinimumContentsLength(16);
  m_targetCombo->setSizeAdjustPolicy(
      QComboBox::AdjustToMinimumContentsLengthWithIcon);
  if (m_git) {
    for (const GitBranchInfo &branch : m_git->getBranches()) {
      if (!branch.isRemote) {
        m_targetCombo->addItem(branch.name, branch.name);
      }
    }
    m_targetCombo->setCurrentIndex(
        m_targetCombo->findData(m_git->currentBranch()));
  }
  connect(m_targetCombo, &QComboBox::currentIndexChanged, this, [this]() {
    const QString source = m_branchCombo->currentData().toString();
    loadChoices();
    selectBranch(source);
    refreshPlan();
  });
  groupLayout->addWidget(m_targetCombo);

  m_targetLabel = new QLabel(group);
  m_targetLabel->setObjectName(QStringLiteral("mergeTargetLabel"));
  groupLayout->addWidget(m_targetLabel);
  m_targetLabel->hide();

  m_targetHintLabel = new QLabel(
      tr("This is the branch you are standing on. It is the one that changes; "
         "the other branch is left exactly as it is."),
      group);
  m_targetHintLabel->setObjectName(QStringLiteral("mergeTargetHintLabel"));
  m_targetHintLabel->setWordWrap(true);
  groupLayout->addWidget(m_targetHintLabel);

  layout->addWidget(group);
}

void MergeStartDialog::buildOutcomeStep(QVBoxLayout *layout) {
  QGroupBox *group = new QGroupBox(tr("Merge preview"), this);
  group->setObjectName(QStringLiteral("mergeOutcomeGroup"));
  QVBoxLayout *groupLayout = new QVBoxLayout(group);
  groupLayout->setSpacing(UiMetrics::SpaceSm);

  m_headlineLabel = new QLabel(group);
  m_headlineLabel->setObjectName(QStringLiteral("mergeHeadlineLabel"));
  m_headlineLabel->setWordWrap(true);
  groupLayout->addWidget(m_headlineLabel);

  m_outcomeLabel = new QLabel(group);
  m_outcomeLabel->setObjectName(QStringLiteral("mergeOutcomeLabel"));
  m_outcomeLabel->setWordWrap(true);
  groupLayout->addWidget(m_outcomeLabel);

  m_filesLabel = new QLabel(group);
  m_filesLabel->setObjectName(QStringLiteral("mergeFilesLabel"));
  m_filesLabel->setWordWrap(true);
  groupLayout->addWidget(m_filesLabel);

  m_conflictLabel = new QLabel(group);
  m_conflictLabel->setObjectName(QStringLiteral("mergeConflictLabel"));
  m_conflictLabel->setWordWrap(true);
  groupLayout->addWidget(m_conflictLabel);

  m_conflictList = new QListWidget(group);
  m_conflictList->setObjectName(QStringLiteral("mergeConflictList"));
  m_conflictList->setMinimumHeight(64);
  m_conflictList->setMaximumHeight(150);
  m_conflictList->hide();
  groupLayout->addWidget(m_conflictList);

  layout->addWidget(group);
}

void MergeStartDialog::buildPreferenceStep(QVBoxLayout *layout) {
  m_preferenceGroup = new QGroupBox(tr("Conflict handling"), this);
  m_preferenceGroup->setObjectName(QStringLiteral("mergePreferenceGroup"));
  QVBoxLayout *groupLayout = new QVBoxLayout(m_preferenceGroup);
  groupLayout->setSpacing(UiMetrics::SpaceSm);

  m_manualRadio = new QRadioButton(
      tr("Show me each disagreement and let me choose"), m_preferenceGroup);
  m_manualRadio->setObjectName(QStringLiteral("mergePreferenceManual"));
  m_manualRadio->setChecked(true);
  groupLayout->addWidget(m_manualRadio);

  m_keepOursRadio = new QRadioButton(m_preferenceGroup);
  m_keepOursRadio->setObjectName(QStringLiteral("mergePreferenceKeepOurs"));
  groupLayout->addWidget(m_keepOursRadio);

  m_keepTheirsRadio = new QRadioButton(m_preferenceGroup);
  m_keepTheirsRadio->setObjectName(QStringLiteral("mergePreferenceKeepTheirs"));
  groupLayout->addWidget(m_keepTheirsRadio);

  m_preferenceExplanation = new QLabel(m_preferenceGroup);
  m_preferenceExplanation->setObjectName(
      QStringLiteral("mergePreferenceExplanation"));
  m_preferenceExplanation->setWordWrap(true);
  groupLayout->addWidget(m_preferenceExplanation);

  for (QRadioButton *button :
       {m_manualRadio, m_keepOursRadio, m_keepTheirsRadio}) {
    connect(button, &QRadioButton::toggled, this,
            &MergeStartDialog::onPreferenceChanged);
  }

  layout->addWidget(m_preferenceGroup);
}

void MergeStartDialog::buildActions(QVBoxLayout *layout) {
  m_blockerLabel = new QLabel(this);
  m_blockerLabel->setObjectName(QStringLiteral("mergeBlockerLabel"));
  m_blockerLabel->setWordWrap(true);
  m_blockerLabel->hide();
  layout->addWidget(m_blockerLabel);

  QHBoxLayout *actions = new QHBoxLayout();
  actions->addStretch();

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setObjectName(QStringLiteral("mergeCancelButton"));
  connect(m_cancelButton, &QPushButton::clicked, this,
          &MergeStartDialog::reject);
  actions->addWidget(m_cancelButton);

  m_startButton = new QPushButton(tr("Start the merge"), this);
  m_startButton->setObjectName(QStringLiteral("mergeStartButton"));
  connect(m_startButton, &QPushButton::clicked, this,
          &MergeStartDialog::accept);
  actions->addWidget(m_startButton);

  layout->addLayout(actions);
  setKeyboardDefault(m_startButton);
}

void MergeStartDialog::loadChoices() {
  m_choices = gitMergeChoices(m_git, selectedTarget());

  const QSignalBlocker blocker(m_branchCombo);
  m_branchCombo->clear();
  m_branchCombo->setEnabled(!m_choices.isEmpty());

  for (const GitMergeChoice &choice : m_choices) {
    m_branchCombo->addItem(choice.name, choice.name);
  }

  if (m_choices.isEmpty()) {
    m_branchCombo->addItem(tr("There is no other branch to bring in"));
    m_branchCombo->setEnabled(false);
  }

  rebuildCopyOptions();
}

const GitMergeChoice *MergeStartDialog::currentChoice() const {
  const int index = m_branchCombo->currentIndex();
  if (index < 0 || index >= m_choices.size()) {
    return nullptr;
  }
  return &m_choices.at(index);
}

void MergeStartDialog::rebuildCopyOptions() {
  qDeleteAll(m_copyButtons);
  m_copyButtons.clear();

  const GitMergeChoice *choice = currentChoice();
  if (!choice) {
    m_copyGroup->hide();
    m_copyWarningLabel->hide();
    return;
  }

  for (const GitMergeSource &source : choice->copies) {
    QRadioButton *button = new QRadioButton(source.whereLabel(), m_copyGroup);
    button->setToolTip(copyDescription(source));
    button->setProperty("mergeRef", source.ref);
    button->setObjectName(QStringLiteral("mergeCopyOption"));
    connect(button, &QRadioButton::toggled, this,
            &MergeStartDialog::onCopyChanged);
    m_copyLayout->addWidget(button);
    m_copyButtons.append(button);
  }

  if (!m_copyButtons.isEmpty()) {
    const QSignalBlocker blocker(m_copyButtons.first());
    m_copyButtons.first()->setChecked(true);
  }

  m_copyGroup->setVisible(choice->isSplit() && choice->copiesDisagree());

  if (choice->isSplit() && choice->copiesDisagree()) {
    m_copyWarningLabel->setText(
        tr("Heads up: these two copies are not at the same commit. Whichever "
           "one you pick is the work that comes in."));
    m_copyWarningLabel->show();
  } else {
    m_copyWarningLabel->hide();
  }
}

QString MergeStartDialog::selectedRef() const {
  for (QRadioButton *button : m_copyButtons) {
    if (button->isChecked()) {
      return button->property("mergeRef").toString();
    }
  }
  const GitMergeChoice *choice = currentChoice();
  const GitMergeSource *preferred = choice ? choice->preferred() : nullptr;
  return preferred ? preferred->ref : QString();
}

QString MergeStartDialog::selectedTarget() const {
  return m_targetCombo ? m_targetCombo->currentData().toString() : QString();
}

GitMergeOptions MergeStartDialog::mergeOptions() const {
  GitMergeOptions options;
  if (m_keepOursRadio && m_keepOursRadio->isChecked()) {
    options.preference = GitMergeOptions::Preference::PreferOurs;
  } else if (m_keepTheirsRadio && m_keepTheirsRadio->isChecked()) {
    options.preference = GitMergeOptions::Preference::PreferTheirs;
  }
  return options;
}

void MergeStartDialog::selectBranch(const QString &name) {
  for (int i = 0; i < m_choices.size(); ++i) {
    if (m_choices.at(i).name == name ||
        (!m_choices.at(i).copies.isEmpty() &&
         m_choices.at(i).copies.first().ref == name)) {
      m_branchCombo->setCurrentIndex(i);
      return;
    }
  }
}

void MergeStartDialog::onBranchChanged() {
  rebuildCopyOptions();
  refreshPlan();
}

void MergeStartDialog::onCopyChanged() { refreshPlan(); }

void MergeStartDialog::onPreferenceChanged() {
  const QString ours =
      m_plan.targetRef.isEmpty() ? tr("this branch") : m_plan.targetRef;
  const QString theirs =
      m_plan.sourceRef.isEmpty() ? tr("the other branch") : m_plan.sourceRef;

  m_keepOursRadio->setText(tr("Always keep my version (%1)").arg(ours));
  m_keepTheirsRadio->setText(tr("Always keep their version (%1)").arg(theirs));
  m_preferenceExplanation->setText(
      gitMergePreferenceExplanation(mergeOptions().preference, ours, theirs));
}

void MergeStartDialog::refreshPlan() {
  const QString ref = selectedRef();
  m_plan = ref.isEmpty() || selectedTarget().isEmpty()
               ? GitMergePlan()
               : buildGitMergePlan(m_git, ref, selectedTarget());
  m_targetHintLabel->setText(
      m_git && selectedTarget() != m_git->currentBranch()
          ? tr("On merge, switch to %1 and merge the source into it. You will "
               "remain on %1.")
                .arg(selectedTarget())
          : tr("The source will be merged into this branch. The source branch "
               "stays unchanged."));

  const QString target = m_plan.targetRef.isEmpty()
                             ? (m_git ? m_git->currentBranch() : QString())
                             : m_plan.targetRef;
  m_targetLabel->setText(target.isEmpty() ? tr("(not on a branch)") : target);

  m_headlineLabel->setText(m_plan.headline());
  m_outcomeLabel->setText(m_plan.outcomeSentence());

  if (m_plan.valid && !m_plan.changedFiles.isEmpty()) {
    m_filesLabel->setText(m_plan.changedFiles.size() == 1
                              ? tr("1 file is touched by this merge.")
                              : tr("%1 files are touched by this merge.")
                                    .arg(m_plan.changedFiles.size()));
    m_filesLabel->show();
  } else {
    m_filesLabel->hide();
  }

  m_conflictLabel->setText(m_plan.conflictSentence());

  m_conflictList->clear();
  if (!m_plan.likelyConflicts.isEmpty()) {
    for (const QString &file :
         m_plan.likelyConflicts.mid(0, kConflictPreviewLimit)) {
      m_conflictList->addItem(QStringLiteral("!  %1").arg(file));
    }
    if (m_plan.likelyConflicts.size() > kConflictPreviewLimit) {
      m_conflictList->addItem(
          tr("... and %1 more")
              .arg(m_plan.likelyConflicts.size() - kConflictPreviewLimit));
    }
    m_conflictList->show();
  } else {
    m_conflictList->hide();
  }

  const bool conflictsPossible =
      m_plan.relation == GitMergeRelation::Diverged ||
      m_plan.relation == GitMergeRelation::Unrelated;
  m_preferenceGroup->setVisible(conflictsPossible);

  onPreferenceChanged();
  updateStartButton();
}

void MergeStartDialog::updateStartButton() {
  const bool canStart = m_plan.canStart();
  m_startButton->setEnabled(canStart);

  if (!m_plan.blockers.isEmpty()) {
    QStringList bullets;
    for (const QString &blocker : m_plan.blockers) {
      bullets << QStringLiteral("- %1").arg(blocker);
    }
    m_blockerLabel->setText(
        tr("This merge cannot start yet:\n%1").arg(bullets.join('\n')));
    m_blockerLabel->show();
  } else {
    m_blockerLabel->hide();
  }

  if (m_plan.relation == GitMergeRelation::AlreadyUpToDate) {
    m_startButton->setText(tr("Nothing to merge"));
  } else if (m_plan.relation == GitMergeRelation::FastForward) {
    m_startButton->setText(tr("Fast-forward"));
  } else {
    m_startButton->setText(tr("Merge"));
  }
}

void MergeStartDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);

  const QString scrollRules =
      QStringLiteral(
          "QScrollArea#mergeStepsScrollArea { background: transparent; border: "
          "none; }"
          "QScrollArea#mergeStepsScrollArea > QWidget { background: "
          "transparent; }"
          "QWidget#mergeStepsContainer { background: %1; }")
          .arg(theme.backgroundColor.name());

  setStyleSheet(
      UIStyleHelper::formDialogStyle(theme) +
      UIStyleHelper::groupBoxStyle(theme) +
      UIStyleHelper::comboBoxStyle(theme) +
      UIStyleHelper::resultListStyle(theme) + scrollRules +
      QStringLiteral("QGroupBox { padding: 8px; padding-top: 16px; }"
                     "QGroupBox::title { color: %1; font-size: 12px; }"
                     "QRadioButton { color: %1; background: transparent; "
                     "spacing: 8px; padding: 4px; }"
                     "QRadioButton::indicator { width: 14px; height: 14px; "
                     "border-radius: 7px; "
                     "border: 1px solid %2; background: %3; }"
                     "QRadioButton::indicator:checked { background: %4; "
                     "border-color: %4; }")
          .arg(theme.foregroundColor.name(), theme.borderColor.name(),
               theme.surfaceColor.name(), theme.accentColor.name()));

  if (QScrollArea *scroll =
          findChild<QScrollArea *>(QStringLiteral("mergeStepsScrollArea"))) {
    scroll->viewport()->setAutoFillBackground(false);
  }

  if (QLabel *title = findChild<QLabel *>(QStringLiteral("mergeTitleLabel"))) {
    title->setStyleSheet(
        QStringLiteral("font-size: 19px; font-weight: bold; color: %1;")
            .arg(theme.foregroundColor.name()));
  }
  if (QLabel *subtitle =
          findChild<QLabel *>(QStringLiteral("mergeSubtitleLabel"))) {
    styleSubduedLabel(subtitle);
  }
  if (QLabel *arrow = findChild<QLabel *>(QStringLiteral("mergeArrowLabel"))) {
    arrow->setStyleSheet(
        QStringLiteral("font-size: 13px; font-weight: bold; color: %1;")
            .arg(theme.accentColor.name()));
  }

  m_targetLabel->setStyleSheet(
      QStringLiteral("font-size: 16px; font-weight: bold; color: %1;")
          .arg(theme.accentColor.name()));
  styleSubduedLabel(m_targetHintLabel);

  m_headlineLabel->setStyleSheet(
      QStringLiteral("font-size: 14px; font-weight: bold; color: %1;")
          .arg(theme.foregroundColor.name()));
  styleSubduedLabel(m_outcomeLabel);
  styleSubduedLabel(m_filesLabel);
  styleSubduedLabel(m_preferenceExplanation);

  for (QLabel *label :
       {m_targetHintLabel, m_outcomeLabel, m_filesLabel,
        m_preferenceExplanation,
        findChild<QLabel *>(QStringLiteral("mergeSubtitleLabel"))}) {
    if (label) {
      label->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                               .arg(theme.foregroundColor.name()));
    }
  }

  m_conflictLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: %1;")
                                     .arg(theme.foregroundColor.name()));
  m_copyWarningLabel->setStyleSheet(
      QStringLiteral("color: %1;").arg(theme.warningColor.name()));
  m_blockerLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;")
                                    .arg(theme.errorColor.name()));

  stylePrimaryButton(m_startButton);
  styleSecondaryButton(m_cancelButton);
}
