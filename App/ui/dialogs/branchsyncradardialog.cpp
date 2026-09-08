#include "branchsyncradardialog.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "operationpreviewdialog.h"
#include "themedmessagebox.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>
#include <QVBoxLayout>

BranchSyncRadarDialog::BranchSyncRadarDialog(GitIntegration *git,
                                             const Theme &theme,
                                             QWidget *parent)
    : StyledDialog(parent), m_git(git), m_headerLabel(nullptr),
      m_summaryLabel(nullptr), m_mergeBaseLabel(nullptr),
      m_incomingLabel(nullptr), m_outgoingLabel(nullptr),
      m_divergenceLabel(nullptr), m_strategyPreview(nullptr),
      m_pushPreview(nullptr), m_configuredLabel(nullptr),
      m_incomingTree(nullptr), m_outgoingTree(nullptr), m_mergeRadio(nullptr),
      m_rebaseRadio(nullptr), m_ffRadio(nullptr), m_pushModeCombo(nullptr),
      m_fetchButton(nullptr), m_pullButton(nullptr), m_pushButton(nullptr),
      m_upstreamButton(nullptr), m_closeButton(nullptr) {
  setWindowTitle(tr("Branch Sync Radar"));
  setMinimumSize(820, 560);
  resize(980, 660);

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  reload();
}

void BranchSyncRadarDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  QHBoxLayout *header = new QHBoxLayout();
  m_headerLabel = new QLabel(this);
  m_headerLabel->setObjectName(QStringLiteral("radarHeaderLabel"));
  header->addWidget(m_headerLabel);
  header->addStretch();

  m_fetchButton = new QPushButton(tr("Fetch"), this);
  m_fetchButton->setObjectName(QStringLiteral("radarFetchButton"));
  m_fetchButton->setToolTip(
      tr("Update the remote-tracking refs so this view is current.\n"
         "Fetch never changes your branch or your files.\ngit fetch"));
  connect(m_fetchButton, &QPushButton::clicked, this,
          &BranchSyncRadarDialog::onFetchClicked);
  header->addWidget(m_fetchButton);

  layout->addLayout(header);

  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setObjectName(QStringLiteral("radarSummaryLabel"));
  m_summaryLabel->setWordWrap(true);
  layout->addWidget(m_summaryLabel);

  m_mergeBaseLabel = new QLabel(this);
  m_mergeBaseLabel->setObjectName(QStringLiteral("radarMergeBaseLabel"));
  m_mergeBaseLabel->setWordWrap(true);
  layout->addWidget(m_mergeBaseLabel);

  buildLanes(layout);
  buildStrategyBar(layout);
}

void BranchSyncRadarDialog::buildLanes(QVBoxLayout *layout) {
  m_divergenceLabel = new QLabel(this);
  m_divergenceLabel->setObjectName(QStringLiteral("radarDivergenceLabel"));
  m_divergenceLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_divergenceLabel);

  QHBoxLayout *lanes = new QHBoxLayout();
  lanes->setSpacing(UiMetrics::SpaceLg);

  const auto makeLane = [&](QLabel **label, QTreeWidget **tree,
                            const QString &name, const QString &tip) {
    QWidget *column = new QWidget(this);
    QVBoxLayout *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(UiMetrics::SpaceXs);

    *label = new QLabel(column);
    (*label)->setObjectName(
        QString(name).replace(QStringLiteral("Tree"), QStringLiteral("Label")));
    columnLayout->addWidget(*label);

    *tree = new QTreeWidget(column);
    (*tree)->setObjectName(name);
    (*tree)->setColumnCount(3);
    (*tree)->setHeaderLabels({tr("Commit"), tr("Subject"), tr("Author")});
    (*tree)->setRootIsDecorated(false);
    (*tree)->setToolTip(tip);
    (*tree)->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    (*tree)->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    (*tree)->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    connect(*tree, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *item, int) {
              if (item) {
                emit commitRequested(item->data(0, Qt::UserRole).toString());
              }
            });
    columnLayout->addWidget(*tree, 1);
    lanes->addWidget(column, 1);
  };

  makeLane(&m_incomingLabel, &m_incomingTree,
           QStringLiteral("radarIncomingTree"),
           tr("Commits the upstream has and you do not. A pull brings these "
              "in.\ngit log HEAD..@{u}"));
  makeLane(&m_outgoingLabel, &m_outgoingTree,
           QStringLiteral("radarOutgoingTree"),
           tr("Commits you have and the upstream does not. A push sends "
              "these.\ngit log @{u}..HEAD"));

  layout->addLayout(lanes, 1);
}

void BranchSyncRadarDialog::buildStrategyBar(QVBoxLayout *layout) {
  QHBoxLayout *strategy = new QHBoxLayout();
  strategy->setSpacing(UiMetrics::ToolbarSpacing);

  QLabel *strategyLabel = new QLabel(tr("Pull strategy:"), this);
  strategyLabel->setObjectName(QStringLiteral("radarStrategyLabel"));
  strategy->addWidget(strategyLabel);

  m_mergeRadio = new QRadioButton(tr("Merge"), this);
  m_mergeRadio->setObjectName(QStringLiteral("radarMergeRadio"));
  m_rebaseRadio = new QRadioButton(tr("Rebase"), this);
  m_rebaseRadio->setObjectName(QStringLiteral("radarRebaseRadio"));
  m_ffRadio = new QRadioButton(tr("Fast-forward only"), this);
  m_ffRadio->setObjectName(QStringLiteral("radarFfRadio"));
  for (QRadioButton *radio : {m_mergeRadio, m_rebaseRadio, m_ffRadio}) {
    connect(radio, &QRadioButton::toggled, this,
            &BranchSyncRadarDialog::onStrategyChanged);
    strategy->addWidget(radio);
  }

  m_configuredLabel = new QLabel(this);
  m_configuredLabel->setObjectName(QStringLiteral("radarConfiguredLabel"));
  strategy->addWidget(m_configuredLabel);
  strategy->addStretch();

  layout->addLayout(strategy);

  m_strategyPreview = new QLabel(this);
  m_strategyPreview->setObjectName(QStringLiteral("radarStrategyPreview"));
  m_strategyPreview->setWordWrap(true);
  layout->addWidget(m_strategyPreview);

  QHBoxLayout *pushRow = new QHBoxLayout();
  pushRow->setSpacing(UiMetrics::ToolbarSpacing);

  QLabel *pushLabel = new QLabel(tr("Push mode:"), this);
  pushLabel->setObjectName(QStringLiteral("radarPushLabel"));
  pushRow->addWidget(pushLabel);

  m_pushModeCombo = new QComboBox(this);
  m_pushModeCombo->setObjectName(QStringLiteral("radarPushModeCombo"));
  m_pushModeCombo->addItem(tr("Normal — fast-forward only"),
                           static_cast<int>(GitPushForce::None));
  m_pushModeCombo->addItem(tr("Force with lease — safe overwrite"),
                           static_cast<int>(GitPushForce::WithLease));
  m_pushModeCombo->addItem(tr("Force — unconditional overwrite"),
                           static_cast<int>(GitPushForce::Force));
  connect(m_pushModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &BranchSyncRadarDialog::onPushModeChanged);
  pushRow->addWidget(m_pushModeCombo);
  pushRow->addStretch();

  layout->addLayout(pushRow);

  m_pushPreview = new QLabel(this);
  m_pushPreview->setObjectName(QStringLiteral("radarPushPreview"));
  m_pushPreview->setWordWrap(true);
  layout->addWidget(m_pushPreview);

  QHBoxLayout *actions = new QHBoxLayout();
  actions->setSpacing(UiMetrics::ToolbarGroupSpacing);

  m_pullButton = new QPushButton(tr("Pull"), this);
  m_pullButton->setObjectName(QStringLiteral("radarPullButton"));
  connect(m_pullButton, &QPushButton::clicked, this,
          &BranchSyncRadarDialog::onPullClicked);
  actions->addWidget(m_pullButton);

  m_pushButton = new QPushButton(tr("Push"), this);
  m_pushButton->setObjectName(QStringLiteral("radarPushButton"));
  connect(m_pushButton, &QPushButton::clicked, this,
          &BranchSyncRadarDialog::onPushClicked);
  actions->addWidget(m_pushButton);

  m_upstreamButton = new QPushButton(tr("Set upstream…"), this);
  m_upstreamButton->setObjectName(QStringLiteral("radarUpstreamButton"));
  connect(m_upstreamButton, &QPushButton::clicked, this,
          &BranchSyncRadarDialog::onSetUpstreamClicked);
  actions->addWidget(m_upstreamButton);

  actions->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("radarCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  actions->addWidget(m_closeButton);

  layout->addLayout(actions);
}

GitPullStrategy BranchSyncRadarDialog::selectedStrategy() const {
  if (m_rebaseRadio && m_rebaseRadio->isChecked()) {
    return GitPullStrategy::Rebase;
  }
  if (m_ffRadio && m_ffRadio->isChecked()) {
    return GitPullStrategy::FastForwardOnly;
  }
  return GitPullStrategy::Merge;
}

GitPushForce BranchSyncRadarDialog::selectedPushForce() const {
  if (!m_pushModeCombo) {
    return GitPushForce::None;
  }
  return static_cast<GitPushForce>(m_pushModeCombo->currentData().toInt());
}

void BranchSyncRadarDialog::reload() {
  if (!m_git) {
    return;
  }
  m_state = m_git->syncState();

  const GitPullStrategy configured = m_git->configuredPullStrategy();
  QSignalBlocker blockMerge(m_mergeRadio);
  QSignalBlocker blockRebase(m_rebaseRadio);
  QSignalBlocker blockFf(m_ffRadio);
  m_mergeRadio->setChecked(configured == GitPullStrategy::Merge);
  m_rebaseRadio->setChecked(configured == GitPullStrategy::Rebase);
  m_ffRadio->setChecked(configured == GitPullStrategy::FastForwardOnly);
  m_configuredLabel->setText(
      tr("(this repository is configured for %1)")
          .arg(gitPullStrategyName(configured).toLower()));

  updateFromState();
}

void BranchSyncRadarDialog::fillLane(QTreeWidget *tree,
                                     const QList<GitCommitInfo> &commits) {
  tree->clear();
  for (const GitCommitInfo &commit : commits) {
    QTreeWidgetItem *item = new QTreeWidgetItem(tree);
    item->setText(0, commit.shortHash);
    item->setText(1, commit.subject);
    item->setText(2, commit.author);
    item->setData(0, Qt::UserRole, commit.hash);
    item->setToolTip(
        1, QStringLiteral("%1\n%2 · %3")
               .arg(commit.subject, commit.author, commit.relativeDate));
  }
}

void BranchSyncRadarDialog::updateFromState() {
  m_headerLabel->setText(
      m_state.hasUpstream
          ? tr("%1  ↔  %2").arg(m_state.branch, m_state.upstream)
          : tr("%1 (no upstream)")
                .arg(m_state.branch.isEmpty() ? tr("detached HEAD")
                                              : m_state.branch));
  m_summaryLabel->setText(gitSyncSummary(m_state));

  if (m_state.mergeBase.isEmpty()) {
    m_mergeBaseLabel->setText(
        m_state.hasUpstream
            ? tr("No common ancestor — these two histories are unrelated.")
            : tr("Fetch updates the remote-tracking refs; it never changes "
                 "your branch or your files."));
  } else {
    m_mergeBaseLabel->setText(tr("Both lanes leave the merge base %1.")
                                  .arg(m_state.mergeBase.left(7)));
  }

  fillLane(m_incomingTree, m_state.incoming);
  fillLane(m_outgoingTree, m_state.outgoing);

  m_incomingLabel->setText(
      tr("⬇ Incoming — only on %1 (%2)")
          .arg(m_state.hasUpstream ? m_state.upstream : tr("the upstream"))
          .arg(m_state.incoming.size()));
  m_outgoingLabel->setText(
      tr("⬆ Outgoing — only on %1 (%2)")
          .arg(m_state.branch.isEmpty() ? tr("this branch") : m_state.branch)
          .arg(m_state.outgoing.size()));

  if (m_state.diverged()) {
    m_divergenceLabel->setText(
        tr("⚠ The lanes have separated — both sides have commits the other "
           "does not. Reconciling them needs a merge or a rebase."));
  } else if (m_state.inSync()) {
    m_divergenceLabel->setText(tr("✓ One lane — nothing to send or receive."));
  } else if (m_state.pullCanFastForward()) {
    m_divergenceLabel->setText(
        tr("One lane — you are simply behind, so a pull just moves you "
           "forward."));
  } else {
    m_divergenceLabel->setText(
        tr("One lane — you are simply ahead, so a push just moves the "
           "upstream forward."));
  }

  m_divergenceLabel->setStyleSheet(
      QString("color: %1; padding: 4px;")
          .arg((m_state.diverged() ? m_theme.warningColor
                                   : m_theme.singleLineCommentFormat)
                   .name()));

  onStrategyChanged();
  onPushModeChanged();

  m_pullButton->setEnabled(m_state.hasUpstream && !m_state.incoming.isEmpty());
  m_pushButton->setEnabled(!m_state.branch.isEmpty());
  m_upstreamButton->setEnabled(!m_state.branch.isEmpty());
  for (QRadioButton *radio : {m_mergeRadio, m_rebaseRadio, m_ffRadio}) {
    radio->setEnabled(m_state.hasUpstream);
  }
}

void BranchSyncRadarDialog::onStrategyChanged() {
  m_strategyPreview->setText(
      gitPullStrategyPreview(selectedStrategy(), m_state));
}

void BranchSyncRadarDialog::onPushModeChanged() {
  const GitPushForce force = selectedPushForce();
  m_pushPreview->setText(gitPushPreview(m_state, force));

  if (force == GitPushForce::Force) {
    m_pushButton->setText(tr("Force push…"));
  } else if (force == GitPushForce::WithLease) {
    m_pushButton->setText(tr("Push with lease…"));
  } else {
    m_pushButton->setText(tr("Push"));
  }
}

void BranchSyncRadarDialog::onFetchClicked() {
  if (!m_git) {
    return;
  }

  m_git->fetch();
  reload();
  emit repositoryChanged();
}

void BranchSyncRadarDialog::onPullClicked() {
  if (!m_git || !m_state.hasUpstream) {
    return;
  }

  const GitPullStrategy strategy = selectedStrategy();

  GitOperationRequest request;
  request.kind = GitOperationKind::Pull;
  request.target = m_state.upstream;
  request.pullStrategy = strategy;
  if (!OperationPreviewDialog::confirm(m_git, request, m_theme, this)) {
    return;
  }

  const QString remote = m_state.upstream.section(QLatin1Char('/'), 0, 0);
  const QString branch = m_state.upstream.section(QLatin1Char('/'), 1);
  if (m_git->pullWithStrategy(remote, branch, strategy)) {
    reload();
    emit repositoryChanged();
  }
}

void BranchSyncRadarDialog::onPushClicked() {
  if (!m_git) {
    return;
  }

  const GitPushForce force = selectedPushForce();
  if (force != GitPushForce::None) {
    const QString title =
        force == GitPushForce::Force ? tr("Force push") : tr("Push with lease");
    if (ThemedMessageBox::question(
            this, title,
            QStringLiteral("%1<br><br><i>%2</i>")
                .arg(tr("Overwrite %1 with %2?")
                         .arg(m_state.upstream.isEmpty() ? tr("the upstream")
                                                         : m_state.upstream,
                              m_state.branch),
                     gitPushPreview(m_state, force))) !=
        ThemedMessageBox::Yes) {
      return;
    }
  } else if (!m_state.pushIsFastForward()) {
    ThemedMessageBox::warning(
        this, tr("Push would be rejected"),
        tr("%1 has %2 commits you do not have, so a normal push cannot "
           "succeed.<br><br>Pull them first, or choose a force mode and read "
           "what it does.")
            .arg(m_state.upstream)
            .arg(m_state.incoming.size()));
    return;
  }

  const QString remote = m_state.hasUpstream
                             ? m_state.upstream.section(QLatin1Char('/'), 0, 0)
                             : QStringLiteral("origin");
  const QString branch = m_state.branch;
  if (m_git->pushWithForce(remote, branch, !m_state.hasUpstream, force)) {
    reload();
    emit repositoryChanged();
  }
}

void BranchSyncRadarDialog::onSetUpstreamClicked() {
  if (!m_git || m_state.branch.isEmpty()) {
    return;
  }

  QStringList remotes;
  for (const GitRemoteInfo &remote : m_git->getRemotes()) {
    remotes << remote.name;
  }
  if (remotes.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("No remotes"),
        tr("This repository has no remotes, so there is nothing to track."));
    return;
  }

  bool ok = false;
  const QString remote = QInputDialog::getItem(
      this, tr("Set upstream"), tr("Track a branch on which remote?"), remotes,
      0, false, &ok);
  if (!ok) {
    return;
  }

  const QString branch = QInputDialog::getText(
      this, tr("Set upstream"), tr("Branch on %1 to track:").arg(remote),
      QLineEdit::Normal, m_state.branch, &ok);
  if (!ok || branch.trimmed().isEmpty()) {
    return;
  }

  if (m_git->setUpstreamBranch(remote, branch.trimmed())) {
    reload();
    emit repositoryChanged();
  }
}

void BranchSyncRadarDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_headerLabel) {
    styleTitleLabel(m_headerLabel);
  }
  for (const char *name :
       {"radarMergeBaseLabel", "radarStrategyLabel", "radarConfiguredLabel",
        "radarPushLabel", "radarStrategyPreview", "radarPushPreview",
        "radarSummaryLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }

  const auto styleLane = [&](QLabel *label, QTreeWidget *tree,
                             const QColor &accent) {
    if (label) {
      label->setStyleSheet(
          QString("color: %1; font-weight: bold;").arg(accent.name()));
    }
    if (tree) {
      tree->setStyleSheet(
          UIStyleHelper::treeWidgetStyle(theme) +
          QString("QTreeWidget { border: 1px solid %1; }").arg(accent.name()));
    }
  };
  styleLane(m_incomingLabel, m_incomingTree, theme.infoColor);
  styleLane(m_outgoingLabel, m_outgoingTree, theme.successColor);

  if (m_divergenceLabel) {
    m_divergenceLabel->setStyleSheet(
        QString("color: %1; padding: 4px;")
            .arg((m_state.diverged() ? theme.warningColor
                                     : theme.singleLineCommentFormat)
                     .name()));
  }
  if (m_pushModeCombo) {
    m_pushModeCombo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }

  for (QRadioButton *radio : {m_mergeRadio, m_rebaseRadio, m_ffRadio}) {
    if (radio) {
      radio->setStyleSheet(UIStyleHelper::checkBoxStyle(theme) +
                           QString("QRadioButton { color: %1; }"
                                   "QRadioButton:disabled { color: %2; }")
                               .arg(theme.foregroundColor.name(),
                                    theme.singleLineCommentFormat.name()));
    }
  }
  for (QPushButton *button :
       {m_fetchButton, m_pullButton, m_upstreamButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_pushButton) {
    if (selectedPushForce() == GitPushForce::None) {
      stylePrimaryButton(m_pushButton);
    } else {
      styleDangerButton(m_pushButton);
    }
  }
}
