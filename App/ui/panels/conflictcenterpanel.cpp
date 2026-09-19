#include "conflictcenterpanel.h"
#include "../../git/gitconflictresolution.h"
#include "../../git/gitintegration.h"
#include "../dialogs/themedmessagebox.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "../widgets/flowlayout.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

enum Column {
  ColumnFile = 0,
  ColumnState = 1,
  ColumnKind = 2,
};

QString operationName(GitOperation operation) {
  switch (operation) {
  case GitOperation::Merge:
    return QObject::tr("merge");
  case GitOperation::Rebase:
    return QObject::tr("rebase");
  case GitOperation::CherryPick:
    return QObject::tr("cherry-pick");
  case GitOperation::Revert:
    return QObject::tr("revert");
  case GitOperation::Bisect:
    return QObject::tr("bisect");
  case GitOperation::None:
    break;
  }
  return QObject::tr("operation");
}

} // namespace

ConflictCenterPanel::ConflictCenterPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("conflictCenterPanel"));
  buildUi();
  updateFromContext();
}

void ConflictCenterPanel::buildUi() {
  QVBoxLayout *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  m_scroll = new QScrollArea(this);
  m_scroll->setObjectName(QStringLiteral("conflictCenterScrollArea"));
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QWidget *content = new QWidget(m_scroll);
  content->setObjectName(QStringLiteral("conflictCenterContent"));
  QVBoxLayout *layout = new QVBoxLayout(content);
  layout->setContentsMargins(UiMetrics::PanelMargin, UiMetrics::PanelMargin,
                             UiMetrics::PanelMargin, UiMetrics::PanelMargin);
  layout->setSpacing(UiMetrics::PanelSpacing);

  buildHeader(layout);
  buildFileList(layout);
  buildGlossary(layout);
  buildActions(layout);

  m_scroll->setWidget(content);
  outer->addWidget(m_scroll);
}

void ConflictCenterPanel::buildHeader(QVBoxLayout *layout) {
  m_banner = new QWidget(this);
  m_banner->setObjectName(QStringLiteral("conflictCenterBanner"));
  QVBoxLayout *bannerLayout = new QVBoxLayout(m_banner);
  bannerLayout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceMd,
                                   UiMetrics::SpaceLg, UiMetrics::SpaceMd);
  bannerLayout->setSpacing(UiMetrics::SpaceSm);

  QHBoxLayout *titleRow = new QHBoxLayout();
  titleRow->setSpacing(UiMetrics::SpaceMd);

  m_bannerIcon = new QLabel(QStringLiteral("!"), m_banner);
  m_bannerIcon->setObjectName(QStringLiteral("conflictCenterIcon"));
  titleRow->addWidget(m_bannerIcon);

  m_bannerTitle = new QLabel(tr("Merge paused"), m_banner);
  m_bannerTitle->setObjectName(QStringLiteral("conflictCenterTitle"));

  m_bannerTitle->setWordWrap(true);
  m_bannerTitle->setMinimumWidth(0);
  titleRow->addWidget(m_bannerTitle, 1);

  bannerLayout->addLayout(titleRow);

  m_operationLabel = new QLabel(m_banner);
  m_operationLabel->setObjectName(QStringLiteral("conflictCenterOperation"));
  m_operationLabel->setWordWrap(true);
  bannerLayout->addWidget(m_operationLabel);

  m_summaryLabel = new QLabel(m_banner);
  m_summaryLabel->setObjectName(QStringLiteral("conflictCenterSummary"));
  m_summaryLabel->setWordWrap(true);
  bannerLayout->addWidget(m_summaryLabel);

  m_progress = new QProgressBar(m_banner);
  m_progress->setObjectName(QStringLiteral("conflictCenterProgress"));
  m_progress->setTextVisible(false);
  m_progress->setFixedHeight(8);
  bannerLayout->addWidget(m_progress);

  layout->addWidget(m_banner);
}

void ConflictCenterPanel::buildFileList(QVBoxLayout *layout) {
  m_fileTree = new QTreeWidget(this);
  m_fileTree->setObjectName(QStringLiteral("conflictCenterFileTree"));
  m_fileTree->setColumnCount(3);
  m_fileTree->setHeaderLabels({tr("File"), tr("To decide"), tr("Reason")});
  m_fileTree->setRootIsDecorated(false);
  m_fileTree->setAlternatingRowColors(false);
  m_fileTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_fileTree->setTextElideMode(Qt::ElideRight);
  m_fileTree->setMinimumWidth(0);
  m_fileTree->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
  m_fileTree->setMinimumHeight(120);
  m_fileTree->header()->setMinimumSectionSize(48);
  m_fileTree->header()->setStretchLastSection(false);
  m_fileTree->header()->setSectionResizeMode(ColumnFile, QHeaderView::Stretch);
  m_fileTree->header()->setSectionResizeMode(ColumnState,
                                             QHeaderView::ResizeToContents);
  m_fileTree->header()->setSectionResizeMode(ColumnKind, QHeaderView::Stretch);
  connect(m_fileTree, &QTreeWidget::itemActivated, this,
          &ConflictCenterPanel::onFileActivated);
  connect(m_fileTree, &QTreeWidget::itemDoubleClicked, this,
          &ConflictCenterPanel::onFileActivated);
  layout->addWidget(m_fileTree, 1);

  m_emptyLabel = new QLabel(tr("No merge is waiting on you. This panel wakes "
                               "up the moment one does."),
                            this);
  m_emptyLabel->setObjectName(QStringLiteral("conflictCenterEmptyLabel"));
  m_emptyLabel->setWordWrap(true);
  m_emptyLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_emptyLabel, 1);
}

void ConflictCenterPanel::buildGlossary(QVBoxLayout *layout) {
  m_glossaryOurs = new QLabel(this);
  m_glossaryOurs->setObjectName(QStringLiteral("conflictCenterOursHint"));
  m_glossaryOurs->setWordWrap(true);
  layout->addWidget(m_glossaryOurs);

  m_glossaryTheirs = new QLabel(this);
  m_glossaryTheirs->setObjectName(QStringLiteral("conflictCenterTheirsHint"));
  m_glossaryTheirs->setWordWrap(true);
  layout->addWidget(m_glossaryTheirs);

  m_glossaryBase = new QLabel(this);
  m_glossaryBase->setObjectName(QStringLiteral("conflictCenterBaseHint"));
  m_glossaryBase->setWordWrap(true);
  layout->addWidget(m_glossaryBase);
}

QWidget *ConflictCenterPanel::makeButtonRow(QVBoxLayout *layout,
                                            FlowLayout **flow) {
  QWidget *row = new QWidget(this);
  QSizePolicy flexible(QSizePolicy::Ignored, QSizePolicy::Preferred);
  flexible.setHeightForWidth(true);
  row->setSizePolicy(flexible);
  row->setMinimumWidth(0);

  *flow = new FlowLayout(row, 0, UiMetrics::SpaceSm, UiMetrics::SpaceSm);
  (*flow)->setSizeConstraint(QLayout::SetNoConstraint);
  layout->addWidget(row);
  return row;
}

void ConflictCenterPanel::buildActions(QVBoxLayout *layout) {
  FlowLayout *fileActions = nullptr;
  makeButtonRow(layout, &fileActions);

  m_openButton = new QPushButton(tr("Fix this file"), this);
  m_openButton->setObjectName(QStringLiteral("conflictCenterOpenButton"));
  m_openButton->setToolTip(
      tr("Open the selected file in the side-by-side resolver"));
  connect(m_openButton, &QPushButton::clicked, this,
          [this]() { onFileActivated(m_fileTree->currentItem(), ColumnFile); });
  fileActions->addWidget(m_openButton);

  m_keepAllOursButton = new QPushButton(tr("Keep mine everywhere"), this);
  m_keepAllOursButton->setObjectName(
      QStringLiteral("conflictCenterKeepOursButton"));
  m_keepAllOursButton->setToolTip(
      tr("Answer every disagreement in every file with your version"));
  connect(m_keepAllOursButton, &QPushButton::clicked, this,
          &ConflictCenterPanel::onKeepAllOurs);
  fileActions->addWidget(m_keepAllOursButton);

  m_keepAllTheirsButton = new QPushButton(tr("Keep theirs everywhere"), this);
  m_keepAllTheirsButton->setObjectName(
      QStringLiteral("conflictCenterKeepTheirsButton"));
  m_keepAllTheirsButton->setToolTip(
      tr("Answer every disagreement in every file with the incoming version"));
  connect(m_keepAllTheirsButton, &QPushButton::clicked, this,
          &ConflictCenterPanel::onKeepAllTheirs);
  fileActions->addWidget(m_keepAllTheirsButton);

  FlowLayout *mergeActions = nullptr;
  makeButtonRow(layout, &mergeActions);

  m_abortButton = new QPushButton(tr("Undo the whole merge"), this);
  m_abortButton->setObjectName(QStringLiteral("conflictCenterAbortButton"));
  m_abortButton->setToolTip(
      tr("Put the branch back exactly as it was before the merge started"));
  connect(m_abortButton, &QPushButton::clicked, this,
          &ConflictCenterPanel::onAbort);
  mergeActions->addWidget(m_abortButton);

  m_finishButton = new QPushButton(tr("Finish the merge"), this);
  m_finishButton->setObjectName(QStringLiteral("conflictCenterFinishButton"));
  m_finishButton->setToolTip(
      tr("Make the merge commit, once every file has been decided"));
  connect(m_finishButton, &QPushButton::clicked, this,
          &ConflictCenterPanel::onFinish);
  mergeActions->addWidget(m_finishButton);
}

void ConflictCenterPanel::setGitIntegration(GitIntegration *git) {
  if (m_git == git) {
    return;
  }

  if (m_git) {
    disconnect(m_git, nullptr, this, nullptr);
  }
  m_git = git;

  if (m_git) {
    connect(m_git, &GitIntegration::statusChanged, this,
            &ConflictCenterPanel::refresh);
    connect(m_git, &GitIntegration::mergeConflictsDetected, this,
            [this](const QStringList &) { refresh(); });
  }
  refresh();
}

void ConflictCenterPanel::refresh() {
  m_context = m_git ? buildGitConflictContext(m_git) : GitConflictContext();
  updateTracking();
  updateFromContext();
}

int ConflictCenterPanel::unresolvedFileCount() const {
  int count = 0;
  for (const TrackedFile &file : m_tracked) {
    if (!file.done) {
      ++count;
    }
  }
  return count;
}

void ConflictCenterPanel::updateTracking() {
  const GitRepositoryState state =
      m_git ? m_git->repositoryState() : GitRepositoryState();
  m_operationActive = state.operation != GitOperation::None;

  if (!m_operationActive) {
    m_tracked.clear();
    m_announced = false;
    return;
  }

  QHash<QString, const GitConflictFile *> unmerged;
  for (const GitConflictFile &file : m_context.files) {
    unmerged.insert(file.path, &file);
  }

  for (TrackedFile &tracked : m_tracked) {
    const GitConflictFile *current = unmerged.value(tracked.path, nullptr);
    tracked.done = current == nullptr;
    if (current) {
      tracked.spots = countConflictMarkers(current->merged);
      tracked.conflictClass = current->conflictClass;
    }
  }

  QSet<QString> known;
  for (const TrackedFile &tracked : m_tracked) {
    known.insert(tracked.path);
  }

  for (const GitConflictFile &file : m_context.files) {
    if (known.contains(file.path)) {
      continue;
    }
    TrackedFile tracked;
    tracked.path = file.path;
    tracked.conflictClass = file.conflictClass;
    tracked.spots = countConflictMarkers(file.merged);
    tracked.done = false;
    m_tracked.append(tracked);
  }
}

void ConflictCenterPanel::updateFromContext() {
  const bool active = m_operationActive && !m_tracked.isEmpty();
  const int total = m_tracked.size();
  const int remaining = unresolvedFileCount();
  const int done = total - remaining;

  m_banner->setVisible(active);
  m_fileTree->setVisible(active);
  m_emptyLabel->setVisible(!active);
  m_glossaryOurs->setVisible(active);
  m_glossaryTheirs->setVisible(active);
  m_glossaryBase->setVisible(active);
  m_openButton->setEnabled(active && remaining > 0);
  m_keepAllOursButton->setEnabled(active && remaining > 0);
  m_keepAllTheirsButton->setEnabled(active && remaining > 0);
  m_abortButton->setEnabled(active);

  m_finishButton->setEnabled(active && remaining == 0);

  if (!active) {
    m_fileTree->clear();
    return;
  }

  const QString operation = operationName(m_context.operation);
  const QString oursLabel =
      m_context.oursLabel.isEmpty() ? tr("this branch") : m_context.oursLabel;
  const QString theirsLabel = m_context.theirsLabel.isEmpty()
                                  ? tr("the incoming branch")
                                  : m_context.theirsLabel;

  m_operationLabel->setText(
      tr("A %1 is half finished: %2 is being brought into %3.")
          .arg(operation, theirsLabel, oursLabel));

  if (remaining == 0) {
    m_summaryLabel->setText(
        tr("Every file is settled. Finish the %1 to make it stick.")
            .arg(operation));
  } else if (remaining == 1) {
    m_summaryLabel->setText(
        tr("1 file still needs a decision from you before the %1 can finish.")
            .arg(operation));
  } else {
    m_summaryLabel->setText(
        tr("%1 files still need a decision from you before the %2 can finish.")
            .arg(remaining)
            .arg(operation));
  }

  const QString operationTitle = operation.at(0).toUpper() + operation.mid(1);
  if (remaining == 0) {
    m_bannerTitle->setText(tr("Ready to finish"));
  } else if (remaining == 1) {
    m_bannerTitle->setText(
        tr("%1 paused - 1 file needs you").arg(operationTitle));
  } else {
    m_bannerTitle->setText(
        tr("%1 paused - %2 files need you").arg(operationTitle).arg(remaining));
  }

  m_progress->setRange(0, qMax(1, total));
  m_progress->setValue(done);

  m_glossaryOurs->setText(
      tr("\"Yours\" (%1): %2").arg(oursLabel, m_context.oursHint));
  m_glossaryTheirs->setText(
      tr("\"Theirs\" (%1): %2").arg(theirsLabel, m_context.theirsHint));
  m_glossaryBase->setText(
      m_context.mergeBaseSubject.isEmpty()
          ? tr("Both branches grew out of the same starting point.")
          : tr("Both branches grew out of \"%1\", the last commit they agreed "
               "on.")
                .arg(m_context.mergeBaseSubject));

  const QString previousSelection = selectedPath();

  m_fileTree->clear();
  for (const TrackedFile &file : m_tracked) {
    QTreeWidgetItem *item = new QTreeWidgetItem(m_fileTree);
    item->setText(ColumnFile, file.path);
    item->setData(ColumnFile, Qt::UserRole, file.path);
    item->setToolTip(ColumnFile,
                     gitConflictClassExplanation(file.conflictClass));

    if (file.done) {
      item->setText(ColumnState, tr("done"));
    } else {
      if (file.spots > 1) {
        item->setText(ColumnState, tr("%1 spots").arg(file.spots));
      } else if (file.spots == 1) {
        item->setText(ColumnState, tr("1 spot"));
      } else {
        item->setText(ColumnState, tr("needs a choice"));
      }
    }

    item->setText(ColumnKind, gitConflictClassName(file.conflictClass));

    const QColor color = file.done ? m_theme.successColor : m_theme.errorColor;
    if (color.isValid()) {
      item->setForeground(ColumnState, color);
    }

    if (file.path == previousSelection) {
      m_fileTree->setCurrentItem(item);
    }
  }

  if (!m_fileTree->currentItem() && m_fileTree->topLevelItemCount() > 0) {
    m_fileTree->setCurrentItem(m_fileTree->topLevelItem(0));
  }

  if (!m_announced && remaining > 0) {
    m_announced = true;
    emit attentionNeeded();
  }
}

QString ConflictCenterPanel::selectedPath() const {
  QTreeWidgetItem *item = m_fileTree->currentItem();
  return item ? item->data(ColumnFile, Qt::UserRole).toString() : QString();
}

void ConflictCenterPanel::onFileActivated(QTreeWidgetItem *item, int) {
  if (!item || !m_git) {
    return;
  }
  const QString path = item->data(ColumnFile, Qt::UserRole).toString();
  if (path.isEmpty()) {
    return;
  }
  emit fileOpenRequested(m_git->repositoryPath() + QLatin1Char('/') + path);
}

bool ConflictCenterPanel::confirm(const QString &title,
                                  const QString &question) {
  return ThemedMessageBox::question(
             this, title, question,
             ThemedMessageBox::Yes | ThemedMessageBox::No,
             ThemedMessageBox::No) == ThemedMessageBox::Yes;
}

void ConflictCenterPanel::onKeepAllOurs() {
  if (!m_git ||
      !confirm(tr("Keep your version everywhere"),
               tr("Every disagreement in every file will be answered "
                  "with your version, and the incoming version of "
                  "those lines will be dropped.\n\nYou can still undo "
                  "the whole merge afterwards. Go ahead?"))) {
    return;
  }

  for (const TrackedFile &file : m_tracked) {
    if (!file.done) {
      m_git->resolveConflictOurs(file.path);
    }
  }
  refresh();
  emit repositoryChanged();
}

void ConflictCenterPanel::onKeepAllTheirs() {
  if (!m_git ||
      !confirm(tr("Keep their version everywhere"),
               tr("Every disagreement in every file will be answered "
                  "with the incoming version, and your version of "
                  "those lines will be dropped.\n\nYou can still undo "
                  "the whole merge afterwards. Go ahead?"))) {
    return;
  }

  for (const TrackedFile &file : m_tracked) {
    if (!file.done) {
      m_git->resolveConflictTheirs(file.path);
    }
  }
  refresh();
  emit repositoryChanged();
}

void ConflictCenterPanel::onAbort() {
  if (!m_git ||
      !confirm(tr("Undo the whole merge"),
               tr("Your branch goes back to exactly where it was before the "
                  "merge started, and every decision you have made since then "
                  "is thrown away.\n\nUndo it?"))) {
    return;
  }

  if (m_git->abortMerge()) {
    refresh();
    emit repositoryChanged();
  }
}

void ConflictCenterPanel::onFinish() {
  if (!m_git) {
    return;
  }
  if (unresolvedFileCount() > 0) {
    ThemedMessageBox::warning(
        this, tr("Not finished yet"),
        tr("Some files still have disagreements waiting for an answer. Open "
           "each one marked in red and decide, then come back here."));
    return;
  }

  if (m_git->continueMerge()) {
    refresh();
    emit repositoryChanged();
  }
}

void ConflictCenterPanel::applyTheme(const Theme &theme) {
  m_theme = theme;

  setStyleSheet(
      UIStyleHelper::panelStyle(theme, objectName()) +
      QStringLiteral(
          "QWidget#conflictCenterPanel { background: %1; color: %2; }"
          "QLabel { color: %2; background: transparent; }"
          "QScrollArea#conflictCenterScrollArea { background: transparent; "
          "border: none; }"
          "QScrollArea#conflictCenterScrollArea > QWidget { background: "
          "transparent; }"
          "QWidget#conflictCenterContent { background: %1; }")
          .arg(theme.backgroundColor.name(), theme.foregroundColor.name()));

  m_banner->setStyleSheet(
      UIStyleHelper::cardStyle(theme, m_banner->objectName()) +
      QStringLiteral("QWidget#conflictCenterBanner { border: 2px solid %1; "
                     "border-radius: %2px; }")
          .arg(theme.errorColor.name())
          .arg(UiMetrics::RadiusMd));

  m_bannerIcon->setStyleSheet(
      UIStyleHelper::badgeStyle(theme, UIStyleHelper::Tone::Error) +
      QStringLiteral("font-size: 14px; border-radius: 10px; padding: 0; "
                     "min-width: 20px; max-width: 20px; min-height: 20px; "
                     "max-height: 20px; qproperty-alignment: AlignCenter;"));
  m_bannerTitle->setStyleSheet(
      UIStyleHelper::headingStyle(theme, 15) +
      QStringLiteral("color: %1;")
          .arg(UIStyleHelper::toneColor(theme, UIStyleHelper::Tone::Error)
                   .name()));
  m_operationLabel->setStyleSheet(
      QStringLiteral("color: %1;").arg(theme.foregroundColor.name()));
  m_summaryLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
  m_progress->setStyleSheet(
      QStringLiteral("QProgressBar { background: %1; border: none; "
                     "border-radius: 4px; } QProgressBar::chunk { background: "
                     "%2; border-radius: 4px; }")
          .arg(theme.surfaceAltColor.name(), theme.successColor.name()));

  m_fileTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));

  const QString hintStyle = UIStyleHelper::subduedLabelStyle(theme);
  m_glossaryOurs->setStyleSheet(hintStyle);
  m_glossaryTheirs->setStyleSheet(hintStyle);
  m_glossaryBase->setStyleSheet(hintStyle);
  m_emptyLabel->setStyleSheet(UIStyleHelper::emptyStateStyle(theme));

  m_openButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
  m_finishButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
  m_keepAllOursButton->setStyleSheet(
      UIStyleHelper::secondaryButtonStyle(theme));
  m_keepAllTheirsButton->setStyleSheet(
      UIStyleHelper::secondaryButtonStyle(theme));
  m_abortButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));

  updateFromContext();
}
