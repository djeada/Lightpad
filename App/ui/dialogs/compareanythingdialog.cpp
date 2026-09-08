#include "compareanythingdialog.h"
#include "../../git/gitdiffmodel.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int ENDPOINT_KIND_ROLE = Qt::UserRole + 1;
constexpr int ENDPOINT_REF_ROLE = Qt::UserRole + 2;
constexpr int PATH_ROLE = Qt::UserRole + 3;

constexpr int MAX_RECENT_COMPARISONS = 10;
const char *RECENT_SETTINGS_KEY = "git/recentComparisons";

QString encodeEndpoint(const GitCompareEndpoint &endpoint) {
  return QStringLiteral("%1:%2")
      .arg(static_cast<int>(endpoint.kind))
      .arg(endpoint.ref);
}

GitCompareEndpoint decodeEndpoint(const QString &encoded) {
  const int separator = encoded.indexOf(QLatin1Char(':'));
  if (separator < 0) {
    return GitCompareEndpoint::head();
  }
  const auto kind =
      static_cast<GitCompareEndpoint::Kind>(encoded.left(separator).toInt());
  const QString ref = encoded.mid(separator + 1);

  switch (kind) {
  case GitCompareEndpoint::Kind::WorkingTree:
    return GitCompareEndpoint::workingTree();
  case GitCompareEndpoint::Kind::Index:
    return GitCompareEndpoint::index();
  case GitCompareEndpoint::Kind::Branch:
    return GitCompareEndpoint::branch(ref);
  case GitCompareEndpoint::Kind::RemoteBranch:
    return GitCompareEndpoint::remoteBranch(ref);
  case GitCompareEndpoint::Kind::Tag:
    return GitCompareEndpoint::tag(ref);
  case GitCompareEndpoint::Kind::Stash:
    return GitCompareEndpoint::stash(ref);
  case GitCompareEndpoint::Kind::Commit:
    break;
  }
  return GitCompareEndpoint::commit(ref, ref);
}

} // namespace

CompareAnythingDialog::CompareAnythingDialog(GitIntegration *git,
                                             const Theme &theme,
                                             QWidget *parent)
    : StyledDialog(parent), m_git(git), m_presetCombo(nullptr),
      m_baseCombo(nullptr), m_compareCombo(nullptr), m_recentCombo(nullptr),
      m_swapButton(nullptr), m_compareButton(nullptr), m_summaryLabel(nullptr),
      m_mergeBaseLabel(nullptr), m_fileTree(nullptr), m_diffView(nullptr),
      m_onlyBaseTree(nullptr), m_onlyCompareTree(nullptr),
      m_onlyBaseLabel(nullptr), m_onlyCompareLabel(nullptr),
      m_populating(false) {
  setWindowTitle(tr("Compare Anything"));
  setMinimumSize(900, 560);
  resize(1120, 700);

  buildUi();
  setKeyboardDefault(m_compareButton);
  applyTheme(theme);

  setEndpoints(GitCompareEndpoint::head(), GitCompareEndpoint::workingTree());
}

void CompareAnythingDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  buildEndpointBar(layout);
  buildResultArea(layout);
}

void CompareAnythingDialog::buildEndpointBar(QVBoxLayout *layout) {
  QHBoxLayout *presets = new QHBoxLayout();
  presets->setSpacing(UiMetrics::ToolbarSpacing);

  QLabel *presetLabel = new QLabel(tr("Preset:"), this);
  presetLabel->setObjectName(QStringLiteral("comparePresetLabel"));
  presets->addWidget(presetLabel);

  m_presetCombo = new QComboBox(this);
  m_presetCombo->setObjectName(QStringLiteral("comparePresetCombo"));
  m_presetCombo->setToolTip(
      tr("Common comparisons, so the usual questions do not need two picks."));
  connect(m_presetCombo, QOverload<int>::of(&QComboBox::activated), this,
          &CompareAnythingDialog::onPresetChosen);
  presets->addWidget(m_presetCombo, 1);

  QLabel *recentLabel = new QLabel(tr("Recent:"), this);
  recentLabel->setObjectName(QStringLiteral("compareRecentLabel"));
  presets->addWidget(recentLabel);

  m_recentCombo = new QComboBox(this);
  m_recentCombo->setObjectName(QStringLiteral("compareRecentCombo"));
  m_recentCombo->setToolTip(tr("Comparisons you ran before."));
  connect(m_recentCombo, QOverload<int>::of(&QComboBox::activated), this,
          &CompareAnythingDialog::onRecentChosen);
  presets->addWidget(m_recentCombo, 1);

  layout->addLayout(presets);

  QHBoxLayout *endpoints = new QHBoxLayout();
  endpoints->setSpacing(UiMetrics::ToolbarSpacing);

  QLabel *baseLabel = new QLabel(tr("Base:"), this);
  baseLabel->setObjectName(QStringLiteral("compareBaseLabel"));
  endpoints->addWidget(baseLabel);

  m_baseCombo = new QComboBox(this);
  m_baseCombo->setObjectName(QStringLiteral("compareBaseCombo"));
  m_baseCombo->setEditable(true);
  m_baseCombo->setToolTip(
      tr("The state to compare from. Type any revision Git understands."));
  endpoints->addWidget(m_baseCombo, 1);

  m_swapButton = new QPushButton(tr("↔ Swap"), this);
  m_swapButton->setObjectName(QStringLiteral("compareSwapButton"));
  m_swapButton->setToolTip(
      tr("Swap Base and Compare, which flips the sign of every change."));
  connect(m_swapButton, &QPushButton::clicked, this,
          &CompareAnythingDialog::onSwapClicked);
  endpoints->addWidget(m_swapButton);

  QLabel *compareLabel = new QLabel(tr("Compare:"), this);
  compareLabel->setObjectName(QStringLiteral("compareCompareLabel"));
  endpoints->addWidget(compareLabel);

  m_compareCombo = new QComboBox(this);
  m_compareCombo->setObjectName(QStringLiteral("compareCompareCombo"));
  m_compareCombo->setEditable(true);
  m_compareCombo->setToolTip(tr("The state to compare to."));
  endpoints->addWidget(m_compareCombo, 1);

  m_compareButton = new QPushButton(tr("Compare"), this);
  m_compareButton->setObjectName(QStringLiteral("compareRunButton"));
  connect(m_compareButton, &QPushButton::clicked, this,
          &CompareAnythingDialog::runComparison);
  endpoints->addWidget(m_compareButton);

  layout->addLayout(endpoints);

  populateEndpointCombo(m_baseCombo);
  populateEndpointCombo(m_compareCombo);
  populatePresets();
  populateRecents();
}

void CompareAnythingDialog::buildResultArea(QVBoxLayout *layout) {
  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setObjectName(QStringLiteral("compareSummaryLabel"));
  m_summaryLabel->setWordWrap(true);
  layout->addWidget(m_summaryLabel);

  m_mergeBaseLabel = new QLabel(this);
  m_mergeBaseLabel->setObjectName(QStringLiteral("compareMergeBaseLabel"));
  m_mergeBaseLabel->setWordWrap(true);
  layout->addWidget(m_mergeBaseLabel);

  QSplitter *vertical = new QSplitter(Qt::Vertical, this);

  QSplitter *filesAndDiff = new QSplitter(Qt::Horizontal, vertical);

  m_fileTree = new QTreeWidget(filesAndDiff);
  m_fileTree->setObjectName(QStringLiteral("compareFileTree"));
  m_fileTree->setColumnCount(3);
  m_fileTree->setHeaderLabels({tr("File"), tr("Change"), tr("± lines")});
  m_fileTree->setRootIsDecorated(false);
  m_fileTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_fileTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_fileTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  connect(m_fileTree, &QTreeWidget::currentItemChanged, this,
          &CompareAnythingDialog::onFileSelectionChanged);
  connect(m_fileTree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem *item, int) {
            const QString path =
                item ? item->data(0, PATH_ROLE).toString() : QString();
            if (!path.isEmpty()) {
              emit fileOpenRequested(path);
            }
          });
  filesAndDiff->addWidget(m_fileTree);

  m_diffView = new QListWidget(filesAndDiff);
  m_diffView->setObjectName(QStringLiteral("compareDiffView"));
  m_diffView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_diffView->setUniformItemSizes(true);
  m_diffView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  filesAndDiff->addWidget(m_diffView);
  filesAndDiff->setSizes({340, 700});

  vertical->addWidget(filesAndDiff);

  QWidget *commitPanel = new QWidget(vertical);
  QHBoxLayout *commitLayout = new QHBoxLayout(commitPanel);
  commitLayout->setContentsMargins(0, 0, 0, 0);
  commitLayout->setSpacing(UiMetrics::SpaceMd);

  const auto makeCommitList = [&](QLabel **label, QTreeWidget **tree,
                                  const QString &name, const QString &tip) {
    QWidget *column = new QWidget(commitPanel);
    QVBoxLayout *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(UiMetrics::SpaceXs);

    *label = new QLabel(column);
    (*label)->setObjectName(name + QStringLiteral("Label"));
    columnLayout->addWidget(*label);

    *tree = new QTreeWidget(column);
    (*tree)->setObjectName(name);
    (*tree)->setColumnCount(2);
    (*tree)->setHeaderLabels({tr("Commit"), tr("Subject")});
    (*tree)->setRootIsDecorated(false);
    (*tree)->setToolTip(tip);
    (*tree)->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    (*tree)->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    columnLayout->addWidget(*tree, 1);

    commitLayout->addWidget(column, 1);
  };

  makeCommitList(&m_onlyBaseLabel, &m_onlyBaseTree,
                 QStringLiteral("compareOnlyBaseTree"),
                 tr("Commits reachable from Base but not from Compare."));
  makeCommitList(&m_onlyCompareLabel, &m_onlyCompareTree,
                 QStringLiteral("compareOnlyCompareTree"),
                 tr("Commits reachable from Compare but not from Base."));

  vertical->addWidget(commitPanel);
  vertical->setStretchFactor(0, 3);
  vertical->setStretchFactor(1, 1);

  layout->addWidget(vertical, 1);
}

void CompareAnythingDialog::populateEndpointCombo(QComboBox *combo) {
  combo->clear();
  for (const GitCompareEndpoint &endpoint : availableCompareEndpoints(m_git)) {
    combo->addItem(endpoint.label);
    const int index = combo->count() - 1;
    combo->setItemData(index, static_cast<int>(endpoint.kind),
                       ENDPOINT_KIND_ROLE);
    combo->setItemData(index, endpoint.ref, ENDPOINT_REF_ROLE);
  }
}

void CompareAnythingDialog::populatePresets() {
  m_presetCombo->clear();
  m_presetCombo->addItem(tr("Choose a preset…"));

  const auto addPreset = [&](const QString &label,
                             const GitCompareEndpoint &base,
                             const GitCompareEndpoint &compare) {
    m_presetCombo->addItem(label);
    const int index = m_presetCombo->count() - 1;
    m_presetCombo->setItemData(index, encodeEndpoint(base), Qt::UserRole);
    m_presetCombo->setItemData(index, encodeEndpoint(compare),
                               Qt::UserRole + 1);
  };

  addPreset(tr("My uncommitted changes (HEAD → Working Tree)"),
            GitCompareEndpoint::head(), GitCompareEndpoint::workingTree());
  addPreset(tr("Staged vs HEAD"), GitCompareEndpoint::head(),
            GitCompareEndpoint::index());
  addPreset(tr("Unstaged vs staged (Index → Working Tree)"),
            GitCompareEndpoint::index(), GitCompareEndpoint::workingTree());
  addPreset(tr("Before vs after the last commit (HEAD~1 → HEAD)"),
            GitCompareEndpoint::commit(QStringLiteral("HEAD~1"),
                                       QStringLiteral("HEAD~1")),
            GitCompareEndpoint::head());

  if (!m_git || !m_git->isValidRepository()) {
    return;
  }

  QString current;
  QString upstream;
  QStringList branchNames;
  for (const GitBranchInfo &branch : m_git->getBranches()) {
    if (!branch.isRemote) {
      branchNames << branch.name;
    }
    if (branch.isCurrent) {
      current = branch.name;
      upstream = branch.trackingBranch;
    }
  }

  if (!current.isEmpty() && !upstream.isEmpty()) {
    addPreset(tr("Current branch vs upstream (%1 → %2)").arg(upstream, current),
              GitCompareEndpoint::remoteBranch(upstream),
              GitCompareEndpoint::branch(current));
  }

  for (const QString &trunk : {QStringLiteral("main"), QStringLiteral("master"),
                               QStringLiteral("develop")}) {
    if (branchNames.contains(trunk) && current != trunk && !current.isEmpty()) {
      addPreset(tr("Current branch vs %1 (%1 → %2)").arg(trunk, current),
                GitCompareEndpoint::branch(trunk),
                GitCompareEndpoint::branch(current));
      break;
    }
  }
}

void CompareAnythingDialog::populateRecents() {
  m_populating = true;
  m_recentCombo->clear();
  m_recentCombo->addItem(tr("Recent comparisons…"));

  QSettings settings("Lightpad", "Lightpad");
  const QStringList recents =
      settings.value(QString::fromLatin1(RECENT_SETTINGS_KEY)).toStringList();

  for (const QString &entry : recents) {
    const QStringList parts = entry.split(QLatin1Char('|'));
    if (parts.size() != 2) {
      continue;
    }
    const GitCompareEndpoint base = decodeEndpoint(parts[0]);
    const GitCompareEndpoint compare = decodeEndpoint(parts[1]);
    m_recentCombo->addItem(
        QStringLiteral("%1 → %2").arg(base.label, compare.label));
    const int index = m_recentCombo->count() - 1;
    m_recentCombo->setItemData(index, parts[0], Qt::UserRole);
    m_recentCombo->setItemData(index, parts[1], Qt::UserRole + 1);
  }

  m_recentCombo->setEnabled(m_recentCombo->count() > 1);
  m_populating = false;
}

void CompareAnythingDialog::rememberComparison(
    const GitCompareEndpoint &base, const GitCompareEndpoint &compare) {
  QSettings settings("Lightpad", "Lightpad");
  const QString key = QString::fromLatin1(RECENT_SETTINGS_KEY);
  QStringList recents = settings.value(key).toStringList();

  const QString entry = QStringLiteral("%1|%2").arg(encodeEndpoint(base),
                                                    encodeEndpoint(compare));
  recents.removeAll(entry);
  recents.prepend(entry);
  while (recents.size() > MAX_RECENT_COMPARISONS) {
    recents.removeLast();
  }

  settings.setValue(key, recents);
  populateRecents();
}

GitCompareEndpoint CompareAnythingDialog::endpointFrom(QComboBox *combo) const {
  const int index = combo->currentIndex();
  const QString text = combo->currentText().trimmed();

  if (index >= 0 && combo->itemText(index) == text) {
    GitCompareEndpoint endpoint;
    endpoint.kind = static_cast<GitCompareEndpoint::Kind>(
        combo->itemData(index, ENDPOINT_KIND_ROLE).toInt());
    endpoint.ref = combo->itemData(index, ENDPOINT_REF_ROLE).toString();
    endpoint.label = text;
    return endpoint;
  }

  return GitCompareEndpoint::commit(text, text);
}

void CompareAnythingDialog::selectEndpoint(QComboBox *combo,
                                           const GitCompareEndpoint &endpoint) {
  for (int i = 0; i < combo->count(); ++i) {
    const auto kind = static_cast<GitCompareEndpoint::Kind>(
        combo->itemData(i, ENDPOINT_KIND_ROLE).toInt());
    if (kind == endpoint.kind &&
        combo->itemData(i, ENDPOINT_REF_ROLE).toString() == endpoint.ref) {
      combo->setCurrentIndex(i);
      return;
    }
  }
  combo->setEditText(endpoint.ref.isEmpty() ? endpoint.label : endpoint.ref);
}

void CompareAnythingDialog::setEndpoints(const GitCompareEndpoint &base,
                                         const GitCompareEndpoint &compare) {
  selectEndpoint(m_baseCombo, base);
  selectEndpoint(m_compareCombo, compare);
  runComparison();
}

void CompareAnythingDialog::compareWithWorkingTree(const QString &ref) {
  setEndpoints(GitCompareEndpoint::commit(ref, ref),
               GitCompareEndpoint::workingTree());
}

void CompareAnythingDialog::selectFile(const QString &filePath) {
  for (int i = 0; i < m_fileTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_fileTree->topLevelItem(i);
    if (item->data(0, PATH_ROLE).toString() == filePath) {
      m_fileTree->setCurrentItem(item);
      m_fileTree->scrollToItem(item);
      return;
    }
  }
}

void CompareAnythingDialog::onSwapClicked() {
  const GitCompareEndpoint base = endpointFrom(m_baseCombo);
  const GitCompareEndpoint compare = endpointFrom(m_compareCombo);
  setEndpoints(compare, base);
}

void CompareAnythingDialog::onPresetChosen(int index) {
  if (index <= 0) {
    return;
  }
  setEndpoints(
      decodeEndpoint(m_presetCombo->itemData(index, Qt::UserRole).toString()),
      decodeEndpoint(
          m_presetCombo->itemData(index, Qt::UserRole + 1).toString()));
}

void CompareAnythingDialog::onRecentChosen(int index) {
  if (index <= 0 || m_populating) {
    return;
  }
  setEndpoints(
      decodeEndpoint(m_recentCombo->itemData(index, Qt::UserRole).toString()),
      decodeEndpoint(
          m_recentCombo->itemData(index, Qt::UserRole + 1).toString()));
}

void CompareAnythingDialog::runComparison() {
  const GitCompareEndpoint base = endpointFrom(m_baseCombo);
  const GitCompareEndpoint compare = endpointFrom(m_compareCombo);

  m_result = runGitComparison(m_git, base, compare);

  m_fileTree->clear();
  for (const GitComparisonFile &file : m_result.files) {
    QTreeWidgetItem *item = new QTreeWidgetItem(m_fileTree);
    item->setText(0, file.oldPath.isEmpty() ? file.path
                                            : QStringLiteral("%1 → %2").arg(
                                                  file.oldPath, file.path));
    item->setText(1, file.statusText());
    item->setText(2, file.isBinary ? tr("binary")
                                   : QStringLiteral("+%1 −%2")
                                         .arg(file.additions)
                                         .arg(file.deletions));
    item->setData(0, PATH_ROLE, file.path);
    item->setToolTip(0, file.path);
  }

  const auto fillCommits = [](QTreeWidget *tree,
                              const QList<GitCommitInfo> &commits) {
    tree->clear();
    for (const GitCommitInfo &commit : commits) {
      QTreeWidgetItem *item = new QTreeWidgetItem(tree);
      item->setText(0, commit.shortHash);
      item->setText(1, commit.subject);
      item->setToolTip(
          1, QStringLiteral("%1\n%2 · %3")
                 .arg(commit.subject, commit.author, commit.relativeDate));
    }
  };
  fillCommits(m_onlyBaseTree, m_result.onlyInBase);
  fillCommits(m_onlyCompareTree, m_result.onlyInCompare);

  renderDiffLines(m_result.diffText);
  updateSummary();

  if (m_result.valid && !m_result.isEmpty()) {
    rememberComparison(base, compare);
  }
}

void CompareAnythingDialog::updateSummary() {
  const GitCompareEndpoint &base = m_result.base;
  const GitCompareEndpoint &compare = m_result.compare;

  if (!m_result.valid) {
    m_summaryLabel->setText(m_result.error);
    m_mergeBaseLabel->clear();
    return;
  }

  if (m_result.isEmpty()) {
    m_summaryLabel->setText(tr("%1 — no differences.")
                                .arg(gitComparisonDescription(base, compare)));
  } else {
    m_summaryLabel->setText(
        tr("%1 — %2 %3, +%4 −%5.")
            .arg(gitComparisonDescription(base, compare))
            .arg(m_result.files.size())
            .arg(m_result.files.size() == 1 ? tr("file") : tr("files"))
            .arg(m_result.additions())
            .arg(m_result.deletions()));
  }

  if (base.isCommitish() && compare.isCommitish()) {
    if (!m_result.mergeBaseMeaningful) {
      m_mergeBaseLabel->setText(
          tr("These two share no common ancestor, so the file diff is the "
             "whole story — there is no merge base to reason from."));
    } else if (m_result.diverged) {
      m_mergeBaseLabel->setText(
          tr("Diverged since %1: %2 commits only on %3, %4 only on %5.")
              .arg(m_result.mergeBase.left(7))
              .arg(m_result.onlyInBase.size())
              .arg(base.label)
              .arg(m_result.onlyInCompare.size())
              .arg(compare.label));
    } else if (!m_result.onlyInCompare.isEmpty()) {
      m_mergeBaseLabel->setText(
          tr("%1 is %2 commits ahead of %3; %3 is an ancestor, so this would "
             "fast-forward.")
              .arg(compare.label)
              .arg(m_result.onlyInCompare.size())
              .arg(base.label));
    } else if (!m_result.onlyInBase.isEmpty()) {
      m_mergeBaseLabel->setText(tr("%1 is %2 commits behind %3.")
                                    .arg(compare.label)
                                    .arg(m_result.onlyInBase.size())
                                    .arg(base.label));
    } else {
      m_mergeBaseLabel->setText(tr("Both point at the same commit."));
    }
  } else {
    m_mergeBaseLabel->setText(
        tr("The working tree and the index are not commits, so there is no "
           "merge base or commit delta — only a file difference."));
  }

  m_onlyBaseLabel->setText(
      tr("Only in %1 (%2)")
          .arg(base.label, QString::number(m_result.onlyInBase.size())));
  m_onlyCompareLabel->setText(
      tr("Only in %1 (%2)")
          .arg(compare.label, QString::number(m_result.onlyInCompare.size())));
}

void CompareAnythingDialog::onFileSelectionChanged() {
  QTreeWidgetItem *item = m_fileTree->currentItem();
  showDiffFor(item ? item->data(0, PATH_ROLE).toString() : QString());
}

void CompareAnythingDialog::showDiffFor(const QString &filePath) {
  if (filePath.isEmpty()) {
    renderDiffLines(m_result.diffText);
    return;
  }

  for (const GitDiffFile &file : parseUnifiedDiffFiles(m_result.diffText)) {
    if (file.path != filePath) {
      continue;
    }
    QStringList lines = file.headerLines;
    for (const GitHunk &hunk : file.hunks) {
      lines << hunk.header;
      for (const GitDiffLine &line : hunk.lines) {
        const QString marker =
            line.type == GitDiffLineType::Added       ? QStringLiteral("+")
            : line.type == GitDiffLineType::Removed   ? QStringLiteral("-")
            : line.type == GitDiffLineType::NoNewline ? QStringLiteral("\\")
                                                      : QStringLiteral(" ");
        lines << marker + line.text;
      }
    }
    renderDiffLines(lines.join(QLatin1Char('\n')));
    return;
  }

  renderDiffLines(QString());
}

void CompareAnythingDialog::renderDiffLines(const QString &diffText) {
  m_diffView->clear();

  if (diffText.trimmed().isEmpty()) {
    QListWidgetItem *item = new QListWidgetItem(
        tr("Nothing to show for this selection."), m_diffView);
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
    } else if (line.startsWith(QLatin1String("diff --git"))) {
      item->setForeground(m_theme.foregroundColor);
    } else {
      item->setForeground(m_theme.singleLineCommentFormat);
    }
  }
}

void CompareAnythingDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  for (QComboBox *combo :
       {m_presetCombo, m_recentCombo, m_baseCombo, m_compareCombo}) {
    if (combo) {
      combo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
    }
  }
  for (QTreeWidget *tree : {m_fileTree, m_onlyBaseTree, m_onlyCompareTree}) {
    if (tree) {
      tree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
    }
  }
  if (m_diffView) {
    m_diffView->setStyleSheet(
        QString("QListWidget { background: %1; border: 1px solid %2; }"
                "QListWidget::item:selected { background: %3; }")
            .arg(theme.surfaceColor.name(), theme.borderColor.name(),
                 theme.hoverColor.name()));
  }
  if (m_compareButton) {
    stylePrimaryButton(m_compareButton);
  }
  if (m_swapButton) {
    styleSecondaryButton(m_swapButton);
  }
  for (const char *name :
       {"comparePresetLabel", "compareRecentLabel", "compareBaseLabel",
        "compareCompareLabel", "compareOnlyBaseTreeLabel",
        "compareOnlyCompareTreeLabel", "compareMergeBaseLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  if (m_summaryLabel) {
    styleTitleLabel(m_summaryLabel);
  }

  renderDiffLines(m_result.diffText);
}
