#include "whatifsandboxdialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "operationpreviewdialog.h"
#include "themedmessagebox.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int COMMIT_ID_ROLE = Qt::UserRole + 1;
}

WhatIfSandboxDialog::WhatIfSandboxDialog(GitIntegration *git,
                                         const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_bannerLabel(nullptr),
      m_leftRefCombo(nullptr), m_rightRefCombo(nullptr), m_graphList(nullptr),
      m_journalList(nullptr), m_unreachableLabel(nullptr),
      m_limitationList(nullptr), m_mergeButton(nullptr),
      m_rebaseButton(nullptr), m_resetButton(nullptr),
      m_cherryPickButton(nullptr), m_deleteButton(nullptr),
      m_resetSandboxButton(nullptr), m_applyButton(nullptr),
      m_closeButton(nullptr) {
  setWindowTitle(tr("What-if sandbox"));
  setMinimumSize(900, 640);
  resize(1040, 720);

  m_sandbox.loadFrom(git);

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  refreshViews();
}

void WhatIfSandboxDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  m_bannerLabel = new QLabel(
      tr("SANDBOX — a copy of the graph. Nothing here runs a Git command, and "
         "nothing you do can change the repository."),
      this);
  m_bannerLabel->setObjectName(QStringLiteral("sandboxBannerLabel"));
  m_bannerLabel->setWordWrap(true);
  layout->addWidget(m_bannerLabel);

  QHBoxLayout *refs = new QHBoxLayout();
  QLabel *leftLabel = new QLabel(tr("This ref:"), this);
  leftLabel->setObjectName(QStringLiteral("sandboxLeftLabel"));
  refs->addWidget(leftLabel);
  m_leftRefCombo = new QComboBox(this);
  m_leftRefCombo->setObjectName(QStringLiteral("sandboxLeftRefCombo"));
  refs->addWidget(m_leftRefCombo, 1);

  QLabel *rightLabel = new QLabel(tr("and this one:"), this);
  rightLabel->setObjectName(QStringLiteral("sandboxRightLabel"));
  refs->addWidget(rightLabel);
  m_rightRefCombo = new QComboBox(this);
  m_rightRefCombo->setObjectName(QStringLiteral("sandboxRightRefCombo"));
  refs->addWidget(m_rightRefCombo, 1);
  layout->addLayout(refs);

  QHBoxLayout *actions = new QHBoxLayout();
  const auto addButton = [&](QPushButton **button, const QString &text,
                             const QString &name, const QString &tip) {
    *button = new QPushButton(text, this);
    (*button)->setObjectName(name);
    (*button)->setToolTip(tip);
    actions->addWidget(*button);
  };
  addButton(&m_mergeButton, tr("Merge second into first"),
            QStringLiteral("sandboxMergeButton"),
            tr("What the topology would look like after a merge."));
  addButton(&m_rebaseButton, tr("Rebase first onto second"),
            QStringLiteral("sandboxRebaseButton"),
            tr("Rebase recreates commits; the sandbox shows them as new "
               "nodes."));
  addButton(&m_cherryPickButton, tr("Cherry-pick selected commit"),
            QStringLiteral("sandboxCherryPickButton"),
            tr("Copies the selected commit onto the first ref."));
  addButton(&m_resetButton, tr("Move first ref to selected commit"),
            QStringLiteral("sandboxResetButton"),
            tr("What a reset would do to reachability."));
  addButton(&m_deleteButton, tr("Delete first ref"),
            QStringLiteral("sandboxDeleteButton"),
            tr("Shows whether any commit would be left with no name."));
  connect(m_mergeButton, &QPushButton::clicked, this,
          &WhatIfSandboxDialog::onMerge);
  connect(m_rebaseButton, &QPushButton::clicked, this,
          &WhatIfSandboxDialog::onRebase);
  connect(m_cherryPickButton, &QPushButton::clicked, this,
          &WhatIfSandboxDialog::onCherryPick);
  connect(m_resetButton, &QPushButton::clicked, this,
          &WhatIfSandboxDialog::onReset);
  connect(m_deleteButton, &QPushButton::clicked, this,
          &WhatIfSandboxDialog::onDeleteRef);
  actions->addStretch();
  layout->addLayout(actions);

  QHBoxLayout *body = new QHBoxLayout();
  m_graphList = new QListWidget(this);
  m_graphList->setObjectName(QStringLiteral("sandboxGraphList"));
  m_graphList->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  body->addWidget(m_graphList, 2);

  QWidget *side = new QWidget(this);
  QVBoxLayout *sideLayout = new QVBoxLayout(side);
  sideLayout->setContentsMargins(0, 0, 0, 0);
  sideLayout->setSpacing(UiMetrics::SpaceXs);

  QLabel *journalLabel = new QLabel(tr("What you have simulated"), side);
  journalLabel->setObjectName(QStringLiteral("sandboxJournalLabel"));
  sideLayout->addWidget(journalLabel);

  m_journalList = new QListWidget(side);
  m_journalList->setObjectName(QStringLiteral("sandboxJournalList"));
  m_journalList->setSelectionMode(QAbstractItemView::NoSelection);
  m_journalList->setFocusPolicy(Qt::NoFocus);
  m_journalList->setWordWrap(true);
  sideLayout->addWidget(m_journalList, 1);

  m_unreachableLabel = new QLabel(side);
  m_unreachableLabel->setObjectName(QStringLiteral("sandboxUnreachableLabel"));
  m_unreachableLabel->setWordWrap(true);
  sideLayout->addWidget(m_unreachableLabel);

  m_limitationList = new QListWidget(side);
  m_limitationList->setObjectName(QStringLiteral("sandboxLimitationList"));
  m_limitationList->setSelectionMode(QAbstractItemView::NoSelection);
  m_limitationList->setFocusPolicy(Qt::NoFocus);

  m_limitationList->setWordWrap(true);
  m_limitationList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_limitationList->setMaximumHeight(130);
  sideLayout->addWidget(m_limitationList);

  body->addWidget(side, 1);
  layout->addLayout(body, 1);

  QHBoxLayout *footer = new QHBoxLayout();
  m_resetSandboxButton = new QPushButton(tr("Start over"), this);
  m_resetSandboxButton->setObjectName(
      QStringLiteral("sandboxResetSandboxButton"));
  m_resetSandboxButton->setToolTip(
      tr("Puts the sandbox back to the real graph. Nothing to undo — nothing "
         "was ever changed."));
  connect(m_resetSandboxButton, &QPushButton::clicked, this,
          &WhatIfSandboxDialog::onResetSandbox);
  footer->addWidget(m_resetSandboxButton);
  footer->addStretch();

  m_applyButton = new QPushButton(tr("Do this for real…"), this);
  m_applyButton->setObjectName(QStringLiteral("sandboxApplyButton"));
  m_applyButton->setToolTip(
      tr("Turns the simulation into real operations, each with its own "
         "preview before it runs."));
  connect(m_applyButton, &QPushButton::clicked, this,
          &WhatIfSandboxDialog::onApplyPlan);
  footer->addWidget(m_applyButton);

  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("sandboxCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  footer->addWidget(m_closeButton);
  layout->addLayout(footer);

  for (const QString &limitation : m_sandbox.limitations()) {
    QListWidgetItem *item = new QListWidgetItem(
        QStringLiteral("• ") + limitation, m_limitationList);
    item->setFlags(Qt::ItemIsEnabled);
  }
}

void WhatIfSandboxDialog::refreshViews() {
  const QString previousLeft = m_leftRefCombo->currentText();
  const QString previousRight = m_rightRefCombo->currentText();

  m_leftRefCombo->clear();
  m_rightRefCombo->clear();
  for (const SandboxRef &ref : m_sandbox.refs()) {
    const QString label =
        ref.isHead ? tr("%1 (checked out)").arg(ref.name) : ref.name;
    m_leftRefCombo->addItem(label, ref.name);
    m_rightRefCombo->addItem(label, ref.name);
  }
  if (m_leftRefCombo->findText(previousLeft) >= 0) {
    m_leftRefCombo->setCurrentText(previousLeft);
  }
  if (m_rightRefCombo->findText(previousRight) >= 0) {
    m_rightRefCombo->setCurrentText(previousRight);
  } else if (m_rightRefCombo->count() > 1) {

    m_rightRefCombo->setCurrentIndex(1);
  }

  const QStringList unreachable = m_sandbox.unreachableCommitIds();
  const QSet<QString> orphaned(unreachable.constBegin(),
                               unreachable.constEnd());

  m_graphList->clear();
  for (const SandboxCommit &commit : m_sandbox.commits()) {
    QStringList refNames;
    for (const SandboxRef &ref : m_sandbox.refs()) {
      if (ref.commitId == commit.id) {
        refNames << ref.name;
      }
    }

    const QString marker = commit.synthetic ? QStringLiteral("+")
                           : orphaned.contains(commit.id) ? QStringLiteral("✕")
                                                          : QStringLiteral("│");
    QListWidgetItem *item = new QListWidgetItem(
        QStringLiteral("%1 %2  %3%4")
            .arg(marker, commit.shortHash, commit.subject,
                 refNames.isEmpty() ? QString()
                                    : QStringLiteral("   [%1]").arg(
                                          refNames.join(QStringLiteral(", ")))),
        m_graphList);
    item->setData(COMMIT_ID_ROLE, commit.id);
  }

  m_journalList->clear();
  for (const QString &entry : m_sandbox.journal()) {
    QListWidgetItem *item = new QListWidgetItem(entry, m_journalList);
    item->setFlags(Qt::ItemIsEnabled);
  }

  m_unreachableLabel->setText(
      unreachable.isEmpty()
          ? tr("Every commit in the sandbox is still reachable from some ref.")
          : tr("⚠ %1 commits would have no ref pointing at them.")
                .arg(unreachable.size()));

  m_applyButton->setEnabled(m_sandbox.isModified());
  m_resetSandboxButton->setEnabled(m_sandbox.isModified());
  applyTheme(m_theme);
}

QString WhatIfSandboxDialog::selectedRef(QComboBox *combo) const {
  return combo ? combo->currentData().toString() : QString();
}

QString WhatIfSandboxDialog::selectedCommitId() const {
  QListWidgetItem *item = m_graphList ? m_graphList->currentItem() : nullptr;
  return item ? item->data(COMMIT_ID_ROLE).toString() : QString();
}

void WhatIfSandboxDialog::note(const QString &message) {
  refreshViews();
  m_unreachableLabel->setText(m_unreachableLabel->text() +
                              QStringLiteral("\n") + message);
}

void WhatIfSandboxDialog::onMerge() {
  note(m_sandbox.simulateMerge(selectedRef(m_leftRefCombo),
                               selectedRef(m_rightRefCombo)));
}

void WhatIfSandboxDialog::onRebase() {
  note(m_sandbox.simulateRebase(selectedRef(m_leftRefCombo),
                                selectedRef(m_rightRefCombo)));
}

void WhatIfSandboxDialog::onReset() {
  const QString commit = selectedCommitId();
  if (commit.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Pick a commit"),
        tr("Select the commit in the graph you want the ref moved to."));
    return;
  }
  note(m_sandbox.simulateReset(selectedRef(m_leftRefCombo), commit));
}

void WhatIfSandboxDialog::onCherryPick() {
  const QString commit = selectedCommitId();
  if (commit.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Pick a commit"),
        tr("Select the commit in the graph you want copied."));
    return;
  }
  note(m_sandbox.simulateCherryPick(selectedRef(m_leftRefCombo), commit));
}

void WhatIfSandboxDialog::onDeleteRef() {
  note(m_sandbox.simulateDeleteRef(selectedRef(m_leftRefCombo)));
}

void WhatIfSandboxDialog::onResetSandbox() {
  m_sandbox.reset();
  refreshViews();
}

void WhatIfSandboxDialog::onApplyPlan() {
  const QList<GitOperationRequest> plan = m_sandbox.plan();
  if (plan.isEmpty()) {
    return;
  }

  if (ThemedMessageBox::question(
          this, tr("Leave the sandbox"),
          tr("Run these %1 operations against the real repository?<br><br>"
             "<i>Each one still gets its own preview before it runs, and the "
             "sandbox did not model conflicts.</i>")
              .arg(plan.size())) != ThemedMessageBox::Yes) {
    return;
  }

  emit planRequested(plan);
  accept();
}

void WhatIfSandboxDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_bannerLabel) {

    m_bannerLabel->setStyleSheet(
        QString("QLabel { background: %1; color: %2; border: 1px dashed %2; "
                "border-radius: %3px; padding: 6px; font-weight: bold; }")
            .arg(theme.surfaceAltColor.name(), theme.infoColor.name())
            .arg(UiMetrics::RadiusSm));
  }
  for (QComboBox *combo : {m_leftRefCombo, m_rightRefCombo}) {
    if (combo) {
      combo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
    }
  }
  const QString dashed =
      QString("QListWidget { background: %1; border: 1px dashed %2; }")
          .arg(theme.surfaceColor.name(), theme.infoColor.name());
  if (m_graphList) {
    m_graphList->setStyleSheet(dashed);
    for (int i = 0; i < m_graphList->count(); ++i) {
      QListWidgetItem *item = m_graphList->item(i);
      const QString text = item->text();
      if (text.startsWith(QLatin1Char('+'))) {
        item->setForeground(theme.successColor);
      } else if (text.startsWith(QStringLiteral("✕"))) {
        item->setForeground(theme.errorColor);
      } else {
        item->setForeground(theme.foregroundColor);
      }
    }
  }
  for (QListWidget *list : {m_journalList, m_limitationList}) {
    if (list) {
      list->setStyleSheet(
          QString("QListWidget { background: %1; color: %2; border: 1px "
                  "dashed %3; }")
              .arg(theme.surfaceColor.name(),
                   theme.singleLineCommentFormat.name(),
                   theme.infoColor.name()));
    }
  }
  if (m_unreachableLabel) {
    m_unreachableLabel->setStyleSheet(
        QString("color: %1;")
            .arg((m_sandbox.unreachableCommitIds().isEmpty()
                      ? theme.successColor
                      : theme.warningColor)
                     .name()));
  }
  for (const char *name :
       {"sandboxLeftLabel", "sandboxRightLabel", "sandboxJournalLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  for (QPushButton *button :
       {m_mergeButton, m_rebaseButton, m_cherryPickButton, m_resetButton,
        m_deleteButton, m_resetSandboxButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_applyButton) {
    stylePrimaryButton(m_applyButton);
  }
}
