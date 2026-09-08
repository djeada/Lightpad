#include "integrationadvisordialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "operationpreviewdialog.h"
#include "themedmessagebox.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

QString objectNameFor(GitIntegrationIntent intent) {
  switch (intent) {
  case GitIntegrationIntent::BringEverythingIn:
    return QStringLiteral("advisorMergeRadio");
  case GitIntegrationIntent::ReplayMineOnTop:
    return QStringLiteral("advisorRebaseRadio");
  case GitIntegrationIntent::BringOneCommit:
    return QStringLiteral("advisorCherryPickRadio");
  case GitIntegrationIntent::ApplySelectedChanges:
    return QStringLiteral("advisorApplyRadio");
  }
  return QString();
}

QString glyphFor(PreviewNode::State state) {
  switch (state) {
  case PreviewNode::State::Unchanged:
    return QStringLiteral("│");
  case PreviewNode::State::Added:
    return QStringLiteral("+");
  case PreviewNode::State::Rewritten:
    return QStringLiteral("↻");
  case PreviewNode::State::Unreachable:
    return QStringLiteral("✕");
  }
  return QStringLiteral(" ");
}

} // namespace

IntegrationAdvisorDialog::IntegrationAdvisorDialog(GitIntegration *git,
                                                   const Theme &theme,
                                                   QWidget *parent)
    : StyledDialog(parent), m_git(git), m_sourceCombo(nullptr),
      m_commitCombo(nullptr), m_targetLabel(nullptr),
      m_recommendationLabel(nullptr), m_riskLabel(nullptr),
      m_commandLabel(nullptr), m_topologyList(nullptr), m_expertCheck(nullptr),
      m_questionnaire(nullptr), m_expertBar(nullptr), m_previewButton(nullptr),
      m_executeButton(nullptr), m_cancelButton(nullptr), m_updating(false) {
  setWindowTitle(tr("Bring changes over"));
  setMinimumSize(820, 620);
  resize(940, 700);

  buildUi();
  setKeyboardDefault(m_executeButton);
  applyTheme(theme);
  reload();
}

void IntegrationAdvisorDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  buildSourceBar(layout);
  buildOptions(layout);
  buildFooter(layout);
}

void IntegrationAdvisorDialog::buildSourceBar(QVBoxLayout *layout) {
  QHBoxLayout *bar = new QHBoxLayout();
  bar->setSpacing(UiMetrics::ToolbarSpacing);

  QLabel *fromLabel = new QLabel(tr("From:"), this);
  fromLabel->setObjectName(QStringLiteral("advisorFromLabel"));
  bar->addWidget(fromLabel);

  m_sourceCombo = new QComboBox(this);
  m_sourceCombo->setObjectName(QStringLiteral("advisorSourceCombo"));
  connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) {
            if (!m_updating) {
              reload();
            }
          });
  bar->addWidget(m_sourceCombo, 1);

  QLabel *commitLabel = new QLabel(tr("Single commit:"), this);
  commitLabel->setObjectName(QStringLiteral("advisorCommitLabel"));
  bar->addWidget(commitLabel);

  m_commitCombo = new QComboBox(this);
  m_commitCombo->setObjectName(QStringLiteral("advisorCommitCombo"));
  m_commitCombo->setToolTip(
      tr("Which commit the copy-one-commit options would take."));
  connect(m_commitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) {
            if (!m_updating) {
              reload();
            }
          });
  bar->addWidget(m_commitCombo, 1);

  m_targetLabel = new QLabel(this);
  m_targetLabel->setObjectName(QStringLiteral("advisorTargetLabel"));

  m_targetLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  bar->addWidget(m_targetLabel);

  layout->addLayout(bar);

  m_recommendationLabel = new QLabel(this);
  m_recommendationLabel->setObjectName(
      QStringLiteral("advisorRecommendationLabel"));
  m_recommendationLabel->setWordWrap(true);
  layout->addWidget(m_recommendationLabel);
}

void IntegrationAdvisorDialog::buildOptions(QVBoxLayout *layout) {
  m_questionnaire = new QWidget(this);
  m_questionnaire->setObjectName(QStringLiteral("advisorQuestionnaire"));
  QVBoxLayout *options = new QVBoxLayout(m_questionnaire);
  options->setContentsMargins(0, 0, 0, 0);
  options->setSpacing(UiMetrics::SpaceSm);

  const QList<GitIntegrationIntent> intents{
      GitIntegrationIntent::BringEverythingIn,
      GitIntegrationIntent::ReplayMineOnTop,
      GitIntegrationIntent::BringOneCommit,
      GitIntegrationIntent::ApplySelectedChanges};

  for (GitIntegrationIntent intent : intents) {
    QRadioButton *radio = new QRadioButton(m_questionnaire);
    radio->setObjectName(objectNameFor(intent));
    connect(radio, &QRadioButton::toggled, this,
            &IntegrationAdvisorDialog::onIntentChanged);
    options->addWidget(radio);

    QLabel *consequence = new QLabel(m_questionnaire);
    consequence->setObjectName(objectNameFor(intent) +
                               QStringLiteral("Consequence"));
    consequence->setWordWrap(true);
    consequence->setIndent(24);
    options->addWidget(consequence);

    m_radios.insert(intent, radio);
    m_consequences.insert(intent, consequence);
  }

  layout->addWidget(m_questionnaire);

  m_expertBar = new QWidget(this);
  m_expertBar->setObjectName(QStringLiteral("advisorExpertBar"));
  QHBoxLayout *expertLayout = new QHBoxLayout(m_expertBar);
  expertLayout->setContentsMargins(0, 0, 0, 0);
  expertLayout->setSpacing(UiMetrics::ToolbarGroupSpacing);
  for (GitIntegrationIntent intent : intents) {
    QPushButton *button =
        new QPushButton(gitIntegrationIntentName(intent), m_expertBar);
    button->setObjectName(objectNameFor(intent) +
                          QStringLiteral("ExpertButton"));
    connect(button, &QPushButton::clicked, this, [this, intent]() {
      if (QRadioButton *radio = m_radios.value(intent, nullptr)) {
        radio->setChecked(true);
      }
      onExecute();
    });
    expertLayout->addWidget(button);
  }
  expertLayout->addStretch();
  m_expertBar->hide();
  layout->addWidget(m_expertBar);

  m_riskLabel = new QLabel(this);
  m_riskLabel->setObjectName(QStringLiteral("advisorRiskLabel"));
  m_riskLabel->setWordWrap(true);
  layout->addWidget(m_riskLabel);

  m_topologyList = new QListWidget(this);
  m_topologyList->setObjectName(QStringLiteral("advisorTopologyList"));
  m_topologyList->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_topologyList->setSelectionMode(QAbstractItemView::NoSelection);
  m_topologyList->setFocusPolicy(Qt::NoFocus);
  m_topologyList->setToolTip(
      tr("The shape this option would leave the branch in."));
  layout->addWidget(m_topologyList, 1);

  m_commandLabel = new QLabel(this);
  m_commandLabel->setObjectName(QStringLiteral("advisorCommandLabel"));
  m_commandLabel->setWordWrap(true);
  m_commandLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(m_commandLabel);
}

void IntegrationAdvisorDialog::buildFooter(QVBoxLayout *layout) {
  QHBoxLayout *footer = new QHBoxLayout();

  m_expertCheck = new QCheckBox(tr("Expert mode"), this);
  m_expertCheck->setObjectName(QStringLiteral("advisorExpertCheck"));
  m_expertCheck->setToolTip(
      tr("Skip the questions and show the four commands directly."));
  connect(m_expertCheck, &QCheckBox::toggled, this,
          &IntegrationAdvisorDialog::onExpertModeToggled);
  footer->addWidget(m_expertCheck);
  footer->addStretch();

  m_previewButton = new QPushButton(tr("Show full preview…"), this);
  m_previewButton->setObjectName(QStringLiteral("advisorPreviewButton"));
  connect(m_previewButton, &QPushButton::clicked, this,
          &IntegrationAdvisorDialog::onShowFullPreview);
  footer->addWidget(m_previewButton);

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setObjectName(QStringLiteral("advisorCancelButton"));
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  footer->addWidget(m_cancelButton);

  m_executeButton = new QPushButton(tr("Do it"), this);
  m_executeButton->setObjectName(QStringLiteral("advisorExecuteButton"));
  connect(m_executeButton, &QPushButton::clicked, this,
          &IntegrationAdvisorDialog::onExecute);
  footer->addWidget(m_executeButton);

  layout->addLayout(footer);
}

void IntegrationAdvisorDialog::setSource(const QString &sourceRef,
                                         const QString &singleCommit) {
  m_updating = true;
  const int index = m_sourceCombo->findText(sourceRef);
  if (index >= 0) {
    m_sourceCombo->setCurrentIndex(index);
  }
  if (!singleCommit.isEmpty()) {
    const int commitIndex = m_commitCombo->findData(singleCommit);
    if (commitIndex >= 0) {
      m_commitCombo->setCurrentIndex(commitIndex);
    }
  }
  m_updating = false;
  reload();
}

void IntegrationAdvisorDialog::reload() {
  if (!m_git || !m_git->isValidRepository()) {
    return;
  }

  m_updating = true;
  const QString previousSource = m_sourceCombo->currentText();
  const QString previousCommit = m_commitCombo->currentData().toString();

  m_sourceCombo->clear();
  const GitRepositoryState state = m_git->repositoryState();
  for (const GitBranchInfo &branch : m_git->getBranches()) {
    if (branch.name != state.branch) {
      m_sourceCombo->addItem(branch.name);
    }
  }
  if (!previousSource.isEmpty()) {
    const int index = m_sourceCombo->findText(previousSource);
    if (index >= 0) {
      m_sourceCombo->setCurrentIndex(index);
    }
  }

  const QString source = m_sourceCombo->currentText();

  m_commitCombo->clear();
  m_commitCombo->addItem(tr("(none)"), QString());
  for (const GitCommitInfo &commit :
       m_git->getCommitLogPage(QStringLiteral("HEAD..%1").arg(source), 0, 50)) {
    m_commitCombo->addItem(
        QStringLiteral("%1 %2").arg(commit.shortHash, commit.subject),
        commit.hash);
  }
  const int commitIndex = m_commitCombo->findData(previousCommit);
  if (commitIndex >= 0) {
    m_commitCombo->setCurrentIndex(commitIndex);
  }
  m_updating = false;

  m_advice = adviseGitIntegration(m_git, source,
                                  m_commitCombo->currentData().toString());

  m_targetLabel->setText(
      tr("into %1 (current branch)").arg(m_advice.targetBranch));
  m_recommendationLabel->setText(
      m_advice.valid ? tr("<b>Suggested:</b> %1 — %2")
                           .arg(gitIntegrationIntentName(m_advice.recommended),
                                m_advice.recommendationReason)
                     : m_advice.error);

  for (const GitIntegrationOption &option : m_advice.options) {
    QRadioButton *radio = m_radios.value(option.intent, nullptr);
    QLabel *consequence = m_consequences.value(option.intent, nullptr);
    if (!radio || !consequence) {
      continue;
    }
    radio->setText(
        QStringLiteral("%1  →  %2")
            .arg(option.question, gitIntegrationIntentName(option.intent)));
    radio->setEnabled(option.isAvailable());
    consequence->setText(
        option.isAvailable()
            ? option.consequence
            : tr("Not available: %1").arg(option.unavailableReason));
  }

  if (QRadioButton *radio = m_radios.value(m_advice.recommended, nullptr)) {
    if (radio->isEnabled()) {
      radio->setChecked(true);
    }
  }
  updateSelectedOption();
}

const GitIntegrationOption *
IntegrationAdvisorDialog::optionFor(GitIntegrationIntent intent) const {
  for (const GitIntegrationOption &option : m_advice.options) {
    if (option.intent == intent) {
      return &option;
    }
  }
  return nullptr;
}

GitIntegrationIntent IntegrationAdvisorDialog::selectedIntent() const {
  for (auto it = m_radios.constBegin(); it != m_radios.constEnd(); ++it) {
    if (it.value()->isChecked()) {
      return it.key();
    }
  }
  return GitIntegrationIntent::BringEverythingIn;
}

bool IntegrationAdvisorDialog::expertMode() const {
  return m_expertCheck && m_expertCheck->isChecked();
}

void IntegrationAdvisorDialog::onIntentChanged() {
  if (m_updating) {
    return;
  }
  updateSelectedOption();
}

void IntegrationAdvisorDialog::updateSelectedOption() {
  const GitIntegrationOption *option = optionFor(selectedIntent());
  if (!option) {
    return;
  }

  QStringList risk;
  if (option->rewritesHistory) {
    risk << tr("⚠ Rewrites history — the commits you have now are replaced by "
               "new ones");
  }
  if (option->affectsSharedHistory) {
    risk << tr("⚠ These commits are already on your upstream, so rewriting "
               "them forces everyone else to reconcile");
  }
  if (!option->createsCommit) {
    risk << tr("No commit is created; the change lands in your working tree "
               "for you to sort out");
  }
  if (risk.isEmpty()) {
    risk << tr("✓ Nothing existing is rewritten");
  }
  m_riskLabel->setText(risk.join(QStringLiteral("<br>")));

  m_commandLabel->setText(
      tr("Command: <code>%1</code>").arg(option->commandName.toHtmlEscaped()));
  m_previewButton->setEnabled(option->hasOperationPreview &&
                              option->isAvailable());
  m_executeButton->setEnabled(option->isAvailable());

  fillTopology(*option);
}

void IntegrationAdvisorDialog::fillTopology(
    const GitIntegrationOption &option) {
  m_topologyList->clear();
  if (!option.hasOperationPreview || !option.isAvailable()) {
    QListWidgetItem *item = new QListWidgetItem(
        option.isAvailable()
            ? tr("This option does not change any commits, so there is no "
                 "topology to preview.")
            : tr("Not available: %1").arg(option.unavailableReason),
        m_topologyList);
    item->setFlags(Qt::ItemIsEnabled);
    return;
  }

  GitOperationRequest request;
  request.kind = option.operationKind;
  request.target = option.intent == GitIntegrationIntent::BringOneCommit
                       ? m_advice.singleCommit
                       : m_advice.sourceRef;
  const GitOperationPreview preview = previewGitOperation(m_git, request, 6);

  for (const PreviewNode &node : preview.after) {
    QListWidgetItem *item = new QListWidgetItem(
        QStringLiteral("%1%2 %3  %4%5")
            .arg(node.lane > 0 ? QStringLiteral("    ") : QString(),
                 glyphFor(node.state), node.shortHash, node.subject,
                 node.isHead ? tr("   ← HEAD") : QString()),
        m_topologyList);
    item->setFlags(Qt::ItemIsEnabled);
    item->setData(Qt::UserRole, static_cast<int>(node.state));
  }
  applyTheme(m_theme);
}

void IntegrationAdvisorDialog::onExpertModeToggled(bool expert) {
  m_questionnaire->setVisible(!expert);
  m_expertBar->setVisible(expert);
}

void IntegrationAdvisorDialog::onShowFullPreview() {
  const GitIntegrationOption *option = optionFor(selectedIntent());
  if (!option || !option->hasOperationPreview) {
    return;
  }
  GitOperationRequest request;
  request.kind = option->operationKind;
  request.target = option->intent == GitIntegrationIntent::BringOneCommit
                       ? m_advice.singleCommit
                       : m_advice.sourceRef;
  OperationPreviewDialog preview(m_git, request, m_theme, this);
  preview.exec();
}

void IntegrationAdvisorDialog::onExecute() {
  const GitIntegrationOption *option = optionFor(selectedIntent());
  if (!option || !option->isAvailable() || !m_git) {
    return;
  }

  bool ok = false;
  switch (option->intent) {
  case GitIntegrationIntent::BringEverythingIn: {
    GitOperationRequest request;
    request.kind = GitOperationKind::Merge;
    request.target = m_advice.sourceRef;
    if (!OperationPreviewDialog::confirm(m_git, request, m_theme, this)) {
      return;
    }
    ok = m_git->mergeBranch(m_advice.sourceRef);
    break;
  }
  case GitIntegrationIntent::ReplayMineOnTop: {
    GitOperationRequest request;
    request.kind = GitOperationKind::Rebase;
    request.target = m_advice.sourceRef;
    if (!OperationPreviewDialog::confirm(m_git, request, m_theme, this)) {
      return;
    }
    ok = m_git->rebaseBranch(m_advice.sourceRef);
    break;
  }
  case GitIntegrationIntent::BringOneCommit: {
    GitOperationRequest request;
    request.kind = GitOperationKind::CherryPick;
    request.target = m_advice.singleCommit;
    if (!OperationPreviewDialog::confirm(m_git, request, m_theme, this)) {
      return;
    }
    ok = m_git->cherryPick(m_advice.singleCommit);
    break;
  }
  case GitIntegrationIntent::ApplySelectedChanges:
    ok = m_git->cherryPickNoCommit(m_advice.singleCommit);
    if (ok) {
      emit reviewAppliedChangesRequested();
    }
    break;
  }

  if (ok) {
    emit repositoryChanged();
    accept();
  }
}

void IntegrationAdvisorDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  for (QComboBox *combo : {m_sourceCombo, m_commitCombo}) {
    if (combo) {
      combo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
    }
  }
  for (QRadioButton *radio : m_radios) {
    radio->setStyleSheet(UIStyleHelper::checkBoxStyle(theme) +
                         QString("QRadioButton { color: %1; }"
                                 "QRadioButton:disabled { color: %2; }")
                             .arg(theme.foregroundColor.name(),
                                  theme.singleLineCommentFormat.name()));
  }
  for (QLabel *label : m_consequences) {
    styleSubduedLabel(label);
  }
  for (const char *name :
       {"advisorFromLabel", "advisorCommitLabel", "advisorTargetLabel",
        "advisorCommandLabel", "advisorRecommendationLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }

  const GitIntegrationOption *option = optionFor(selectedIntent());
  const bool risky =
      option && (option->rewritesHistory || option->affectsSharedHistory);
  if (m_riskLabel) {
    m_riskLabel->setStyleSheet(
        QString("color: %1;")
            .arg((option && option->affectsSharedHistory ? theme.errorColor
                  : risky                                ? theme.warningColor
                                                         : theme.successColor)
                     .name()));
  }
  if (m_topologyList) {
    m_topologyList->setStyleSheet(
        QString("QListWidget { background: %1; border: 1px solid %2; }")
            .arg(theme.surfaceColor.name(), theme.borderColor.name()));
    for (int i = 0; i < m_topologyList->count(); ++i) {
      QListWidgetItem *item = m_topologyList->item(i);
      switch (
          static_cast<PreviewNode::State>(item->data(Qt::UserRole).toInt())) {
      case PreviewNode::State::Added:
        item->setForeground(theme.successColor);
        break;
      case PreviewNode::State::Rewritten:
        item->setForeground(theme.warningColor);
        break;
      case PreviewNode::State::Unreachable:
        item->setForeground(theme.errorColor);
        break;
      case PreviewNode::State::Unchanged:
        item->setForeground(theme.foregroundColor);
        break;
      }
    }
  }
  if (m_expertCheck) {
    m_expertCheck->setStyleSheet(
        UIStyleHelper::checkBoxStyle(theme) +
        QString("QCheckBox { color: %1; }").arg(theme.foregroundColor.name()));
  }
  for (QPushButton *button : {m_previewButton, m_cancelButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_executeButton) {
    if (risky) {
      styleDangerButton(m_executeButton);
    } else {
      stylePrimaryButton(m_executeButton);
    }
  }
}
