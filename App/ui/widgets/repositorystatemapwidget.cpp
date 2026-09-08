#include "repositorystatemapwidget.h"
#include "../uimetrics.h"
#include "flowlayout.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int AUTO_COLLAPSE_WIDTH = 200;
constexpr int MAX_CHIP_TEXT_CHARS = 24;

QString countOf(int count, const QString &singular, const QString &plural) {
  return count == 1 ? singular : plural.arg(count);
}

QString elideMiddle(const QString &text, int maxChars = MAX_CHIP_TEXT_CHARS) {
  if (text.size() <= maxChars) {
    return text;
  }
  const int head = (maxChars - 1) / 2;
  const int tail = maxChars - 1 - head;
  return text.left(head) + QStringLiteral("…") + text.right(tail);
}

} // namespace

RepositoryStateMapWidget::RepositoryStateMapWidget(QWidget *parent)
    : QWidget(parent), m_state(), m_theme(), m_themeInitialized(false),
      m_collapseMode(CollapseMode::Auto), m_collapsed(false),
      m_expandedWidget(nullptr), m_operationChip(nullptr),
      m_chipsWidget(nullptr), m_chipsLayout(nullptr), m_summaryLabel(nullptr),
      m_collapsedButton(nullptr) {

  QSizePolicy flexible(QSizePolicy::Ignored, QSizePolicy::Preferred);
  flexible.setHeightForWidth(true);
  setSizePolicy(flexible);
  setMinimumWidth(0);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setSizeConstraint(QLayout::SetNoConstraint);
  layout->setContentsMargins(UiMetrics::SpaceMd, UiMetrics::SpaceMd,
                             UiMetrics::SpaceMd, UiMetrics::SpaceMd);
  layout->setSpacing(UiMetrics::SpaceSm);

  m_expandedWidget = new QWidget(this);
  m_expandedWidget->setSizePolicy(flexible);
  m_expandedWidget->setMinimumWidth(0);
  QVBoxLayout *expandedLayout = new QVBoxLayout(m_expandedWidget);
  expandedLayout->setSizeConstraint(QLayout::SetNoConstraint);
  expandedLayout->setContentsMargins(0, 0, 0, 0);
  expandedLayout->setSpacing(UiMetrics::SpaceSm);

  m_operationChip = new QToolButton(m_expandedWidget);
  m_operationChip->setObjectName(QStringLiteral("stateMapOperation"));
  m_operationChip->setFocusPolicy(Qt::StrongFocus);
  m_operationChip->setCursor(Qt::PointingHandCursor);
  m_operationChip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_operationChip->hide();
  connect(m_operationChip, &QToolButton::clicked, this,
          [this]() { emit layerActivated(Layer::Operation); });
  expandedLayout->addWidget(m_operationChip);

  m_chipsWidget = new QWidget(m_expandedWidget);
  m_chipsWidget->setSizePolicy(flexible);
  m_chipsWidget->setMinimumWidth(0);
  m_chipsLayout =
      new FlowLayout(m_chipsWidget, 0, UiMetrics::SpaceSm, UiMetrics::SpaceSm);
  m_chipsLayout->setSizeConstraint(QLayout::SetNoConstraint);
  expandedLayout->addWidget(m_chipsWidget);

  m_summaryLabel = new QLabel(m_expandedWidget);
  m_summaryLabel->setWordWrap(true);
  m_summaryLabel->setSizePolicy(flexible);
  m_summaryLabel->setMinimumWidth(0);
  m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  expandedLayout->addWidget(m_summaryLabel);

  layout->addWidget(m_expandedWidget);

  m_collapsedButton = new QToolButton(this);
  m_collapsedButton->setObjectName(QStringLiteral("stateMapCollapsed"));
  m_collapsedButton->setFocusPolicy(Qt::StrongFocus);
  m_collapsedButton->setCursor(Qt::PointingHandCursor);
  m_collapsedButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_collapsedButton->hide();
  connect(m_collapsedButton, &QToolButton::clicked, this,
          [this]() { emit layerActivated(Layer::Branch); });
  layout->addWidget(m_collapsedButton);

  setState(GitRepositoryState());
}

void RepositoryStateMapWidget::setState(const GitRepositoryState &state) {
  m_state = state;
  rebuildChips();
  updateCollapsedState();
}

void RepositoryStateMapWidget::setCollapseMode(CollapseMode mode) {
  if (m_collapseMode == mode) {
    return;
  }
  m_collapseMode = mode;
  updateCollapsedState();
}

void RepositoryStateMapWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateCollapsedState();
}

void RepositoryStateMapWidget::updateCollapsedState() {
  bool collapsed = false;
  switch (m_collapseMode) {
  case CollapseMode::Expanded:
    collapsed = false;
    break;
  case CollapseMode::Collapsed:
    collapsed = true;
    break;
  case CollapseMode::Auto:
    collapsed = width() > 0 && width() < AUTO_COLLAPSE_WIDTH;
    break;
  }

  const bool changed = m_collapsed != collapsed;
  m_collapsed = collapsed;
  m_expandedWidget->setVisible(!collapsed);
  m_collapsedButton->setVisible(collapsed);
  if (changed) {

    updateGeometry();
  }

  const QString oneLine = gitRepositoryStateOneLine(m_state);
  const int available =
      m_collapsedButton->width() - 2 * UiMetrics::SpaceMd - UiMetrics::SpaceSm;
  m_collapsedButton->setText(available > 0
                                 ? m_collapsedButton->fontMetrics().elidedText(
                                       oneLine, Qt::ElideRight, available)
                                 : oneLine);
  m_collapsedButton->setToolTip(gitRepositoryStateSummary(m_state));
  m_collapsedButton->setAccessibleName(tr("Repository state"));
  m_collapsedButton->setAccessibleDescription(
      gitRepositoryStateSummary(m_state));
}

QToolButton *RepositoryStateMapWidget::addChip(Layer layer,
                                               const QString &objectName,
                                               bool leadingArrow) {
  QWidget *host = m_chipsWidget;
  QWidget *cell = nullptr;
  QHBoxLayout *cellLayout = nullptr;

  if (leadingArrow) {

    cell = new QWidget(m_chipsWidget);
    cellLayout = new QHBoxLayout(cell);
    cellLayout->setContentsMargins(0, 0, 0, 0);
    cellLayout->setSpacing(UiMetrics::SpaceSm);

    QLabel *arrow = new QLabel(QStringLiteral("→"), cell);
    arrow->setAccessibleName(tr("then"));
    if (m_themeInitialized) {
      arrow->setStyleSheet(
          QString("color: %1;").arg(m_theme.singleLineCommentFormat.name()));
    }
    cellLayout->addWidget(arrow);
    host = cell;
  }

  QToolButton *chip = new QToolButton(host);
  chip->setObjectName(objectName);
  chip->setFocusPolicy(Qt::StrongFocus);
  chip->setCursor(Qt::PointingHandCursor);
  connect(chip, &QToolButton::clicked, this,
          [this, layer]() { emit layerActivated(layer); });

  if (cellLayout) {
    cellLayout->addWidget(chip);
    m_chipsLayout->addWidget(cell);
  } else {
    m_chipsLayout->addWidget(chip);
  }
  return chip;
}

void RepositoryStateMapWidget::rebuildChips() {
  while (QLayoutItem *item = m_chipsLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }

  const QColor muted = m_themeInitialized ? m_theme.singleLineCommentFormat
                                          : palette().color(QPalette::Mid);
  const QColor accent = m_themeInitialized
                            ? m_theme.accentColor
                            : palette().color(QPalette::Highlight);

  if (!m_state.valid) {
    m_summaryLabel->setText(gitRepositoryStateSummary(m_state));
    if (m_operationChip) {
      m_operationChip->hide();
    }
    return;
  }

  {
    QToolButton *chip = addChip(Layer::WorkingTree,
                                QStringLiteral("stateMapWorkingTree"), false);
    const int count = m_state.workingTreeCount();
    chip->setText(tr("✎ Working Tree %1").arg(count));
    chip->setAccessibleName(
        tr("Working tree, %1")
            .arg(countOf(count, tr("1 change"), tr("%1 changes"))));
    chip->setToolTip(
        tr("<b>Working Tree</b> — your files as they are on disk right now."
           "<br>%1 unstaged, %2 untracked."
           "<br><br>Click to focus unstaged changes."
           "<br><code>git status</code> · <code>git diff</code>")
            .arg(m_state.modifiedCount)
            .arg(m_state.untrackedCount));
    styleChip(chip, count > 0 ? (m_themeInitialized ? m_theme.gitModifiedColor
                                                    : accent)
                              : muted);
  }

  {
    QToolButton *chip =
        addChip(Layer::Index, QStringLiteral("stateMapIndex"), true);
    chip->setText(tr("▣ Index %1").arg(m_state.stagedCount));
    chip->setAccessibleName(
        tr("Index, %1")
            .arg(countOf(m_state.stagedCount, tr("1 staged change"),
                         tr("%1 staged changes"))));
    chip->setToolTip(
        tr("<b>Index (staging area)</b> — exactly what the next commit will "
           "contain. Git commits from here, not from the editor buffer."
           "<br>%1."
           "<br><br>Click to focus staged changes."
           "<br><code>git add</code> · <code>git restore --staged</code>")
            .arg(countOf(m_state.stagedCount, tr("1 staged change"),
                         tr("%1 staged changes"))));
    styleChip(chip, m_state.stagedCount > 0
                        ? (m_themeInitialized ? m_theme.gitAddedColor : accent)
                        : muted);
  }

  {
    QToolButton *chip =
        addChip(Layer::Head, QStringLiteral("stateMapHead"), true);
    const QString hash =
        m_state.headShortHash.isEmpty() ? tr("none") : m_state.headShortHash;
    chip->setText(tr("◆ HEAD %1").arg(hash));
    chip->setAccessibleName(tr("HEAD at %1").arg(hash));
    chip->setToolTip(
        m_state.unbornBranch
            ? tr("<b>HEAD</b> — no commits yet, so there is nothing to build "
                 "on.<br><code>git commit</code> creates the first one.")
            : tr("<b>HEAD</b> — the commit your next commit will build on."
                 "<br>%1 %2"
                 "<br><br>Click to inspect this commit."
                 "<br><code>git show HEAD</code>")
                  .arg(hash,
                       elideMiddle(m_state.headSubject, 40).toHtmlEscaped()));
    styleChip(chip, accent);
  }

  {
    QToolButton *chip =
        addChip(Layer::Branch, QStringLiteral("stateMapBranch"), true);
    if (m_state.detachedHead) {
      chip->setText(tr("⚠ DETACHED HEAD"));
      chip->setAccessibleName(
          tr("Detached HEAD at %1").arg(m_state.headShortHash));
      chip->setToolTip(
          tr("<b>Detached HEAD</b> — you are on a commit, not a branch. New "
             "commits belong to no branch and are easy to lose."
             "<br><br>Click to pick a branch to return to."
             "<br><code>git switch -c &lt;name&gt;</code> to keep this work."));
      styleChip(chip, m_themeInitialized ? m_theme.warningColor : accent);
    } else {
      const QString branch =
          m_state.branch.isEmpty() ? tr("(no branch)") : m_state.branch;
      chip->setText(QStringLiteral("⎇ ") + elideMiddle(branch));
      chip->setAccessibleName(tr("Branch %1").arg(branch));
      chip->setToolTip(
          tr("<b>Branch</b> — the ref HEAD follows, so committing moves it "
             "forward."
             "<br>Currently <b>%1</b>."
             "<br><br>Click to switch branch."
             "<br><code>git switch</code> · <code>git branch</code>")
              .arg(branch.toHtmlEscaped()));
      styleChip(chip, accent);
    }
  }

  {
    QToolButton *chip =
        addChip(Layer::Upstream, QStringLiteral("stateMapUpstream"), true);
    if (!m_state.hasUpstream) {
      chip->setText(tr("☁ No upstream"));
      chip->setAccessibleName(tr("No upstream branch"));
      chip->setToolTip(
          tr("<b>Upstream</b> — no remote branch is tracked, so there is "
             "nothing to be ahead of or behind."
             "<br><br><code>git push -u origin %1</code>")
              .arg(m_state.branch.isEmpty() ? tr("&lt;branch&gt;")
                                            : m_state.branch.toHtmlEscaped()));
      styleChip(chip, muted);
    } else {
      QString divergence;
      if (m_state.ahead > 0) {
        divergence += tr(" ↑%1").arg(m_state.ahead);
      }
      if (m_state.behind > 0) {
        divergence += tr(" ↓%1").arg(m_state.behind);
      }
      if (divergence.isEmpty()) {
        divergence = tr(" in sync");
      }
      chip->setText(QStringLiteral("☁ ") + elideMiddle(m_state.upstream, 18) +
                    divergence);
      chip->setAccessibleName(tr("Upstream %1, %2 ahead, %3 behind")
                                  .arg(m_state.upstream)
                                  .arg(m_state.ahead)
                                  .arg(m_state.behind));
      chip->setToolTip(
          tr("<b>Upstream</b> — the remote branch this one tracks."
             "<br>%1 ahead, %2 behind <b>%3</b>."
             "<br><br>Click to inspect incoming and outgoing commits."
             "<br><code>git log @{u}..HEAD</code> · "
             "<code>git log HEAD..@{u}</code>")
              .arg(m_state.ahead)
              .arg(m_state.behind)
              .arg(m_state.upstream.toHtmlEscaped()));
      const bool diverged = m_state.ahead > 0 || m_state.behind > 0;
      styleChip(chip,
                diverged ? (m_themeInitialized ? m_theme.warningColor : accent)
                         : (m_themeInitialized ? m_theme.successColor : muted));
    }
  }

  if (m_state.conflictedCount > 0) {
    QToolButton *chip =
        addChip(Layer::WorkingTree, QStringLiteral("stateMapConflicts"), true);
    chip->setText(tr("⚠ Conflicts %1").arg(m_state.conflictedCount));
    chip->setAccessibleName(countOf(m_state.conflictedCount,
                                    tr("1 conflicted file"),
                                    tr("%1 conflicted files")));
    chip->setToolTip(
        tr("<b>Conflicts</b> — Git could not merge these files on its own and "
           "is waiting for you to decide."
           "<br><br>Click to focus the conflicted files."
           "<br><code>git status</code> · <code>git add &lt;file&gt;</code> "
           "once resolved."));
    styleChip(chip, m_themeInitialized ? m_theme.errorColor : accent);
  }

  if (m_state.stashCount > 0) {
    QToolButton *chip =
        addChip(Layer::Stash, QStringLiteral("stateMapStash"), false);
    chip->setText(tr("▤ Stashes %1").arg(m_state.stashCount));
    chip->setAccessibleName(
        countOf(m_state.stashCount, tr("1 stash"), tr("%1 stashes")));
    chip->setToolTip(
        tr("<b>Stashes</b> — changes set aside off any branch."
           "<br>%1 saved."
           "<br><br>Click to open the stash list."
           "<br><code>git stash list</code> · <code>git stash pop</code>")
            .arg(m_state.stashCount));
    styleChip(chip, m_themeInitialized ? m_theme.infoColor : accent);
  }

  if (m_state.operation != GitOperation::None) {
    const QString name = gitOperationName(m_state.operation);
    const QString label =
        m_state.operationDetail.isEmpty()
            ? tr("⚠ %1 in progress").arg(name)
            : tr("⚠ %1 in progress — %2").arg(name, m_state.operationDetail);
    m_operationChip->setText(label);
    m_operationChip->setAccessibleName(label);
    m_operationChip->setToolTip(
        tr("<b>%1 in progress</b> — Git is part-way through a multi-step "
           "operation and will refuse most others until it finishes."
           "<br><br>Click to see the files it is waiting on."
           "<br><code>%2</code>")
            .arg(name,
                 gitOperationExitHint(m_state.operation).toHtmlEscaped()));
    styleChip(m_operationChip,
              m_themeInitialized ? m_theme.warningColor : accent);
    m_operationChip->show();
  } else {
    m_operationChip->hide();
  }

  m_summaryLabel->setText(gitRepositoryStateSummary(m_state));
  m_summaryLabel->setAccessibleName(tr("Repository state summary"));
}

void RepositoryStateMapWidget::styleChip(QToolButton *chip,
                                         const QColor &accent) const {
  if (!chip) {
    return;
  }

  const QColor background = m_themeInitialized
                                ? m_theme.surfaceColor
                                : palette().color(QPalette::Base);
  const QColor foreground = m_themeInitialized
                                ? m_theme.foregroundColor
                                : palette().color(QPalette::Text);
  const QColor hover = m_themeInitialized ? m_theme.hoverColor
                                          : palette().color(QPalette::Midlight);

  chip->setStyleSheet(
      QString("QToolButton {"
              "  background: %1;"
              "  color: %2;"
              "  border: 1px solid %3;"
              "  border-radius: %4px;"
              "  padding: 2px %5px;"
              "  font-size: 11px;"
              "}"
              "QToolButton:hover { background: %6; }"
              "QToolButton:focus { border: 1px solid %2; }")
          .arg(background.name(), foreground.name(), accent.name())
          .arg(UiMetrics::RadiusSm)
          .arg(UiMetrics::SpaceMd)
          .arg(hover.name()));
}

void RepositoryStateMapWidget::applyTheme(const Theme &theme) {
  m_theme = theme;
  m_themeInitialized = true;

  setStyleSheet(QString("background: %1;").arg(theme.backgroundColor.name()));
  if (m_summaryLabel) {
    m_summaryLabel->setStyleSheet(
        QString("color: %1; font-size: 11px;")
            .arg(theme.singleLineCommentFormat.name()));
  }
  if (m_collapsedButton) {
    styleChip(m_collapsedButton, theme.accentColor);
  }

  rebuildChips();
}
