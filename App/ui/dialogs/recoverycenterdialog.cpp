#include "recoverycenterdialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "themedmessagebox.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int EVENT_INDEX_ROLE = Qt::UserRole + 1;
}

RecoveryCenterDialog::RecoveryCenterDialog(GitIntegration *git,
                                           const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_headerLabel(nullptr),
      m_timelineTree(nullptr), m_descriptionLabel(nullptr),
      m_guaranteeLabel(nullptr), m_detailList(nullptr),
      m_recoverButton(nullptr), m_inspectButton(nullptr),
      m_compareButton(nullptr), m_closeButton(nullptr) {
  setWindowTitle(tr("Recovery Center"));
  setMinimumSize(880, 600);
  resize(1000, 680);

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  reload();
}

void RecoveryCenterDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  m_headerLabel = new QLabel(this);
  m_headerLabel->setObjectName(QStringLiteral("recoveryHeaderLabel"));
  m_headerLabel->setWordWrap(true);
  m_headerLabel->setText(
      tr("Everywhere your refs have been. This is your local reflog — it is "
         "not shared with anyone, and it expires, so recovering something "
         "means giving it a name."));
  layout->addWidget(m_headerLabel);

  m_timelineTree = new QTreeWidget(this);
  m_timelineTree->setObjectName(QStringLiteral("recoveryTimelineTree"));
  m_timelineTree->setColumnCount(4);
  m_timelineTree->setHeaderLabels(
      {tr("When"), tr("What happened"), tr("Landed on"), tr("Still named?")});
  m_timelineTree->setRootIsDecorated(false);
  m_timelineTree->header()->setSectionResizeMode(0,
                                                 QHeaderView::ResizeToContents);
  m_timelineTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_timelineTree->header()->setSectionResizeMode(2,
                                                 QHeaderView::ResizeToContents);
  m_timelineTree->header()->setSectionResizeMode(3,
                                                 QHeaderView::ResizeToContents);
  connect(m_timelineTree, &QTreeWidget::itemSelectionChanged, this,
          &RecoveryCenterDialog::onEventSelected);
  layout->addWidget(m_timelineTree, 2);

  m_descriptionLabel = new QLabel(this);
  m_descriptionLabel->setObjectName(QStringLiteral("recoveryDescriptionLabel"));
  m_descriptionLabel->setWordWrap(true);
  layout->addWidget(m_descriptionLabel);

  m_guaranteeLabel = new QLabel(this);
  m_guaranteeLabel->setObjectName(QStringLiteral("recoveryGuaranteeLabel"));
  m_guaranteeLabel->setWordWrap(true);
  layout->addWidget(m_guaranteeLabel);

  m_detailList = new QListWidget(this);
  m_detailList->setObjectName(QStringLiteral("recoveryDetailList"));
  m_detailList->setSelectionMode(QAbstractItemView::NoSelection);
  m_detailList->setFocusPolicy(Qt::NoFocus);
  m_detailList->setMaximumHeight(120);
  m_detailList->setToolTip(
      tr("The raw reflog entries this card was built from."));
  layout->addWidget(m_detailList);

  QHBoxLayout *footer = new QHBoxLayout();
  m_recoverButton = new QPushButton(tr("Create recovery branch here"), this);
  m_recoverButton->setObjectName(QStringLiteral("recoveryBranchButton"));
  m_recoverButton->setToolTip(
      tr("Gives this commit a name without moving anything you are on."));
  connect(m_recoverButton, &QPushButton::clicked, this,
          &RecoveryCenterDialog::onCreateRecoveryBranch);
  footer->addWidget(m_recoverButton);

  m_inspectButton = new QPushButton(tr("Inspect commit"), this);
  m_inspectButton->setObjectName(QStringLiteral("recoveryInspectButton"));
  connect(m_inspectButton, &QPushButton::clicked, this,
          &RecoveryCenterDialog::onInspect);
  footer->addWidget(m_inspectButton);

  m_compareButton = new QPushButton(tr("Compare with where I am"), this);
  m_compareButton->setObjectName(QStringLiteral("recoveryCompareButton"));
  m_compareButton->setToolTip(
      tr("Check what this candidate holds that your checkout does not, before "
         "restoring anything."));
  connect(m_compareButton, &QPushButton::clicked, this,
          &RecoveryCenterDialog::onCompareWithHead);
  footer->addWidget(m_compareButton);

  footer->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("recoveryCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  footer->addWidget(m_closeButton);

  layout->addLayout(footer);
}

void RecoveryCenterDialog::reload() {
  m_events = buildRecoveryTimeline(m_git);

  m_timelineTree->clear();
  for (int i = 0; i < m_events.size(); ++i) {
    const GitRecoveryEvent &event = m_events.at(i);
    QTreeWidgetItem *item = new QTreeWidgetItem(m_timelineTree);
    item->setText(0, event.relativeDate);
    item->setText(1, gitRecoveryEventDescription(event));
    item->setText(2, event.shortTo());
    item->setText(3, event.reachable ? tr("named") : tr("⚠ unnamed"));
    item->setData(0, EVENT_INDEX_ROLE, i);
    item->setToolTip(1, event.rawAction);
  }

  if (m_timelineTree->topLevelItemCount() > 0) {
    m_timelineTree->setCurrentItem(m_timelineTree->topLevelItem(0));
  } else {
    onEventSelected();
  }
}

int RecoveryCenterDialog::currentEventIndex() const {
  QTreeWidgetItem *item = m_timelineTree->currentItem();
  return item ? item->data(0, EVENT_INDEX_ROLE).toInt() : -1;
}

const GitRecoveryEvent *RecoveryCenterDialog::currentEvent() const {
  const int index = currentEventIndex();
  if (index < 0 || index >= m_events.size()) {
    return nullptr;
  }
  return &m_events.at(index);
}

void RecoveryCenterDialog::onEventSelected() {
  const GitRecoveryEvent *event = currentEvent();
  const bool have = event != nullptr;

  for (QPushButton *button :
       {m_recoverButton, m_inspectButton, m_compareButton}) {
    button->setEnabled(have);
  }

  if (!have) {
    m_descriptionLabel->setText(
        tr("Nothing in the reflog yet. It fills up as you commit, switch "
           "branches and rewrite history."));
    m_guaranteeLabel->clear();
    m_detailList->clear();
    return;
  }

  m_descriptionLabel->setText(
      tr("<b>%1</b> — %2<br>%3 → %4")
          .arg(gitRecoveryEventKindName(event->kind),
               gitRecoveryEventDescription(*event),
               event->fromHash.isEmpty() ? tr("(nothing)") : event->shortFrom(),
               event->shortTo()));
  m_guaranteeLabel->setText(gitRecoveryGuarantee(*event));

  m_detailList->clear();
  for (const QString &raw : event->rawEntries) {
    QListWidgetItem *item = new QListWidgetItem(raw, m_detailList);
    item->setFlags(Qt::ItemIsEnabled);
  }

  applyTheme(m_theme);
}

void RecoveryCenterDialog::onCreateRecoveryBranch() {
  const GitRecoveryEvent *event = currentEvent();
  if (!event || !m_git) {
    return;
  }

  bool ok = false;
  const QString name = QInputDialog::getText(
      this, tr("Create recovery branch"),
      tr("Name for a branch at %1:").arg(event->shortTo()), QLineEdit::Normal,
      QStringLiteral("recovered/%1").arg(event->shortTo()), &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }

  if (m_git->createBranchFromCommit(name.trimmed(), event->toHash, false)) {
    ThemedMessageBox::information(
        this, tr("Recovered"),
        tr("%1 now points at %2. Nothing you were working on was touched — "
           "switch to it when you are ready.")
            .arg(name.trimmed(), event->shortTo()));
    emit repositoryChanged();
    reload();
  }
}

void RecoveryCenterDialog::onInspect() {
  if (const GitRecoveryEvent *event = currentEvent()) {
    emit inspectCommitRequested(event->toHash);
  }
}

void RecoveryCenterDialog::onCompareWithHead() {
  if (const GitRecoveryEvent *event = currentEvent()) {
    emit compareRequested(QStringLiteral("HEAD"), event->toHash);
  }
}

void RecoveryCenterDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_headerLabel) {
    styleSubduedLabel(m_headerLabel);
  }
  if (m_timelineTree) {
    m_timelineTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  if (m_descriptionLabel) {
    m_descriptionLabel->setStyleSheet(
        QString("color: %1;").arg(theme.foregroundColor.name()));
  }
  if (m_guaranteeLabel) {
    const GitRecoveryEvent *event = currentEvent();
    const bool unnamed = event && !event->reachable;
    m_guaranteeLabel->setStyleSheet(
        QString("color: %1;")
            .arg((unnamed ? theme.warningColor : theme.successColor).name()));
  }
  if (m_detailList) {
    m_detailList->setStyleSheet(
        QString("QListWidget { background: %1; color: %2; border: 1px solid "
                "%3; }")
            .arg(theme.surfaceColor.name(),
                 theme.singleLineCommentFormat.name(),
                 theme.borderColor.name()));
  }
  for (QPushButton *button :
       {m_inspectButton, m_compareButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_recoverButton) {
    stylePrimaryButton(m_recoverButton);
  }
}
