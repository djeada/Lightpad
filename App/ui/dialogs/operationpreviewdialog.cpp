#include "operationpreviewdialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include <QCheckBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {

const char *SKIP_SAFE_KEY = "git/skipSafeOperationPreview";

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

bool OperationPreviewDialog::safeOperationsSkipConfirmation() {
  QSettings settings("Lightpad", "Lightpad");
  return settings.value(QString::fromLatin1(SKIP_SAFE_KEY), false).toBool();
}

void OperationPreviewDialog::setSafeOperationsSkipConfirmation(bool skip) {
  QSettings settings("Lightpad", "Lightpad");
  settings.setValue(QString::fromLatin1(SKIP_SAFE_KEY), skip);
}

bool OperationPreviewDialog::confirm(GitIntegration *git,
                                     const GitOperationRequest &request,
                                     const Theme &theme, QWidget *parent) {
  const GitOperationPreview preview = previewGitOperation(git, request);
  if (!preview.effect.valid) {
    return false;
  }

  if (preview.effect.risk == GitOperationRisk::Safe &&
      safeOperationsSkipConfirmation()) {
    return true;
  }

  OperationPreviewDialog dialog(git, request, theme, parent);
  return dialog.exec() == QDialog::Accepted;
}

OperationPreviewDialog::OperationPreviewDialog(
    GitIntegration *git, const GitOperationRequest &request, const Theme &theme,
    QWidget *parent)
    : StyledDialog(parent), m_git(git), m_headlineLabel(nullptr),
      m_riskLabel(nullptr), m_rationaleLabel(nullptr), m_layersLabel(nullptr),
      m_beforeLabel(nullptr), m_afterLabel(nullptr), m_beforeList(nullptr),
      m_afterList(nullptr), m_riskList(nullptr), m_commandLabel(nullptr),
      m_skipSafeCheck(nullptr), m_proceedButton(nullptr),
      m_cancelButton(nullptr) {

  m_preview = previewGitOperation(git, request);

  setWindowTitle(tr("%1 — preview").arg(gitOperationKindName(request.kind)));
  setMinimumSize(820, 560);
  resize(960, 660);

  buildUi();
  setKeyboardDefault(m_proceedButton);
  applyTheme(theme);
}

void OperationPreviewDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  m_headlineLabel = new QLabel(gitOperationHeadline(m_preview), this);
  m_headlineLabel->setObjectName(QStringLiteral("previewHeadlineLabel"));
  m_headlineLabel->setWordWrap(true);
  layout->addWidget(m_headlineLabel);

  m_riskLabel = new QLabel(this);
  m_riskLabel->setObjectName(QStringLiteral("previewRiskLabel"));
  m_riskLabel->setText(gitOperationRiskName(m_preview.effect.risk));
  layout->addWidget(m_riskLabel);

  m_rationaleLabel = new QLabel(m_preview.effect.rationale, this);
  m_rationaleLabel->setObjectName(QStringLiteral("previewRationaleLabel"));
  m_rationaleLabel->setWordWrap(true);
  layout->addWidget(m_rationaleLabel);

  buildGraphs(layout);
  buildDetails(layout);
}

void OperationPreviewDialog::buildGraphs(QVBoxLayout *layout) {
  QHBoxLayout *graphs = new QHBoxLayout();
  graphs->setSpacing(UiMetrics::SpaceLg);

  const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

  const auto makeColumn = [&](QLabel **label, QListWidget **list,
                              const QString &title, const QString &name) {
    QWidget *column = new QWidget(this);
    QVBoxLayout *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(UiMetrics::SpaceXs);

    *label = new QLabel(title, column);
    (*label)->setObjectName(name + QStringLiteral("Label"));
    columnLayout->addWidget(*label);

    *list = new QListWidget(column);
    (*list)->setObjectName(name);
    (*list)->setFont(mono);
    (*list)->setSelectionMode(QAbstractItemView::NoSelection);

    (*list)->setFocusPolicy(Qt::NoFocus);
    columnLayout->addWidget(*list, 1);

    graphs->addWidget(column, 1);
  };

  makeColumn(&m_beforeLabel, &m_beforeList, tr("Now"),
             QStringLiteral("previewBeforeList"));
  makeColumn(&m_afterLabel, &m_afterList, tr("After this operation"),
             QStringLiteral("previewAfterList"));

  fillGraph(m_beforeList, m_preview.before);
  fillGraph(m_afterList, m_preview.after);

  layout->addLayout(graphs, 1);

  QLabel *legend = new QLabel(
      tr("│ unchanged   + new commit   ↻ recreated with a new hash   "
         "✕ no longer on this branch"),
      this);
  legend->setObjectName(QStringLiteral("previewLegendLabel"));
  layout->addWidget(legend);
}

void OperationPreviewDialog::buildDetails(QVBoxLayout *layout) {
  const GitOperationEffect &effect = m_preview.effect;

  QStringList layers;
  layers << (effect.changesWorkingTree ? tr("Working Tree: changes")
                                       : tr("Working Tree: untouched"));
  layers << (effect.changesIndex ? tr("Index: changes")
                                 : tr("Index: untouched"));
  layers << (effect.movesBranch ? tr("Branch: moves") : tr("Branch: stays"));
  m_layersLabel = new QLabel(layers.join(QStringLiteral("   ·   ")), this);
  m_layersLabel->setObjectName(QStringLiteral("previewLayersLabel"));
  layout->addWidget(m_layersLabel);

  m_riskList = new QListWidget(this);
  m_riskList->setObjectName(QStringLiteral("previewRiskList"));
  m_riskList->setMaximumHeight(110);
  m_riskList->setSelectionMode(QAbstractItemView::NoSelection);

  for (const QString &path : effect.atRiskPaths) {
    QListWidgetItem *item = new QListWidgetItem(
        tr("⚠ %1 — uncommitted, would be lost").arg(path), m_riskList);
    item->setFlags(Qt::ItemIsEnabled);
  }
  for (const QString &path : effect.conflictPaths) {
    QListWidgetItem *item = new QListWidgetItem(
        tr("⚡ %1 — Git predicts a conflict here").arg(path), m_riskList);
    item->setFlags(Qt::ItemIsEnabled);
  }
  if (!effect.conflictsKnown) {
    QListWidgetItem *item = new QListWidgetItem(
        tr("Conflicts cannot be predicted for this operation without running "
           "it — Git replays each commit in turn."),
        m_riskList);
    item->setFlags(Qt::ItemIsEnabled);
  }
  if (m_riskList->count() == 0) {
    QListWidgetItem *item = new QListWidgetItem(
        tr("Nothing at risk: no uncommitted work is touched and Git predicts "
           "no conflicts."),
        m_riskList);
    item->setFlags(Qt::ItemIsEnabled);
  }
  layout->addWidget(m_riskList);

  m_commandLabel =
      new QLabel(tr("Lightpad will run: <code>%1</code>")
                     .arg(effect.commands.join(QStringLiteral(" &amp;&amp; "))
                              .toHtmlEscaped()),
                 this);
  m_commandLabel->setObjectName(QStringLiteral("previewCommandLabel"));
  m_commandLabel->setWordWrap(true);
  m_commandLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(m_commandLabel);

  QHBoxLayout *actions = new QHBoxLayout();
  m_skipSafeCheck =
      new QCheckBox(tr("Don't ask again for operations classified safe"), this);
  m_skipSafeCheck->setObjectName(QStringLiteral("previewSkipSafeCheck"));
  m_skipSafeCheck->setChecked(safeOperationsSkipConfirmation());
  m_skipSafeCheck->setToolTip(
      tr("Only affects operations this preview calls safe. Anything that "
         "rewrites history or can lose work always asks."));
  connect(m_skipSafeCheck, &QCheckBox::toggled, this,
          [](bool checked) { setSafeOperationsSkipConfirmation(checked); });
  actions->addWidget(m_skipSafeCheck);
  actions->addStretch();

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setObjectName(QStringLiteral("previewCancelButton"));
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  actions->addWidget(m_cancelButton);

  m_proceedButton = new QPushButton(
      tr("Run %1").arg(gitOperationKindName(m_preview.request.kind).toLower()),
      this);
  m_proceedButton->setObjectName(QStringLiteral("previewProceedButton"));
  connect(m_proceedButton, &QPushButton::clicked, this, &QDialog::accept);
  actions->addWidget(m_proceedButton);

  layout->addLayout(actions);
}

void OperationPreviewDialog::fillGraph(QListWidget *list,
                                       const QList<PreviewNode> &nodes) {
  list->clear();
  for (const PreviewNode &node : nodes) {
    const QString indent = node.lane > 0 ? QStringLiteral("    ") : QString();
    QListWidgetItem *item = new QListWidgetItem(
        QStringLiteral("%1%2 %3  %4%5")
            .arg(indent, glyphFor(node.state), node.shortHash, node.subject,
                 node.isHead ? tr("   ← HEAD") : QString()),
        list);
    item->setFlags(Qt::ItemIsEnabled);
    item->setData(Qt::UserRole, static_cast<int>(node.state));
  }
  if (nodes.isEmpty()) {
    QListWidgetItem *item = new QListWidgetItem(tr("(no commits)"), list);
    item->setFlags(Qt::ItemIsEnabled);
  }
}

void OperationPreviewDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  QColor riskColor = theme.successColor;
  switch (m_preview.effect.risk) {
  case GitOperationRisk::Safe:
    riskColor = theme.successColor;
    break;
  case GitOperationRisk::RewritesLocalHistory:
    riskColor = theme.warningColor;
    break;
  case GitOperationRisk::MayDiscardUncommitted:
  case GitOperationRisk::AffectsSharedHistory:
    riskColor = theme.errorColor;
    break;
  }

  if (m_headlineLabel) {
    styleTitleLabel(m_headlineLabel);
  }
  if (m_riskLabel) {
    m_riskLabel->setStyleSheet(
        QString("QLabel { color: %1; border: 1px solid %1; border-radius: "
                "%2px; padding: 2px 8px; font-weight: bold; }")
            .arg(riskColor.name())
            .arg(UiMetrics::RadiusSm));
  }
  for (const char *name : {"previewRationaleLabel", "previewLayersLabel",
                           "previewCommandLabel", "previewLegendLabel",
                           "previewBeforeListLabel", "previewAfterListLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }

  const auto styleGraph = [&](QListWidget *list) {
    if (!list) {
      return;
    }
    list->setStyleSheet(
        QString("QListWidget { background: %1; border: 1px solid %2; }")
            .arg(theme.surfaceColor.name(), theme.borderColor.name()));
    for (int i = 0; i < list->count(); ++i) {
      QListWidgetItem *item = list->item(i);
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
  };
  styleGraph(m_beforeList);
  styleGraph(m_afterList);

  if (m_riskList) {

    const bool hasWarnings = !m_preview.effect.atRiskPaths.isEmpty() ||
                             !m_preview.effect.conflictPaths.isEmpty();
    const QColor listColor = !m_preview.effect.atRiskPaths.isEmpty()
                                 ? theme.errorColor
                             : hasWarnings ? theme.warningColor
                                           : theme.singleLineCommentFormat;
    m_riskList->setStyleSheet(
        QString("QListWidget { background: %1; color: %2; border: 1px solid "
                "%3; }")
            .arg(theme.surfaceColor.name(), listColor.name(),
                 theme.borderColor.name()));
  }
  if (m_skipSafeCheck) {
    m_skipSafeCheck->setStyleSheet(
        UIStyleHelper::checkBoxStyle(theme) +
        QString("QCheckBox { color: %1; }").arg(theme.foregroundColor.name()));
  }
  if (m_cancelButton) {
    styleSecondaryButton(m_cancelButton);
  }
  if (m_proceedButton) {
    if (m_preview.effect.risk == GitOperationRisk::Safe) {
      stylePrimaryButton(m_proceedButton);
    } else {
      styleDangerButton(m_proceedButton);
    }
  }
}
