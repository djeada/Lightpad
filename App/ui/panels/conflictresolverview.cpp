#include "conflictresolverview.h"
#include "../../git/gitintegration.h"
#include "../../theme/colorcontrast.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "../widgets/flowlayout.h"

#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

namespace {

constexpr int kContextLines = 3;
constexpr int kMaxCodeBlockHeight = 320;

QString joinLines(const QStringList &lines) {
  return lines.join(QLatin1Char('\n'));
}

QColor tintOf(const QColor &base, const QColor &background) {
  return ColorContrast::mix(background, base, 0.12);
}

QString describeLines(const QStringList &lines) {
  if (lines.isEmpty()) {
    return QObject::tr("nothing at all");
  }
  if (lines.size() == 1) {
    return QObject::tr("1 line");
  }
  return QObject::tr("%1 lines").arg(lines.size());
}

class CodeBlock : public QPlainTextEdit {
public:
  CodeBlock(const QString &text, int maximumHeight, QWidget *parent)
      : QPlainTextEdit(text, parent), m_maximumHeight(maximumHeight) {
    setReadOnly(true);
    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    adjustHeight();
  }

protected:
  void resizeEvent(QResizeEvent *event) override {
    QPlainTextEdit::resizeEvent(event);
    adjustHeight();
  }

private:
  void adjustHeight() {
    const int lineCount = qMax(1, blockCount());
    const int margins = 2 * static_cast<int>(document()->documentMargin()) +
                        2 * frameWidth() + UiMetrics::SpaceMd;

    int wanted = lineCount * fontMetrics().lineSpacing() + margins;

    if (horizontalScrollBar() && horizontalScrollBar()->isVisible()) {
      wanted += horizontalScrollBar()->sizeHint().height();
    }

    const int capped = qMin(wanted, m_maximumHeight);
    setVerticalScrollBarPolicy(wanted > m_maximumHeight
                                   ? Qt::ScrollBarAsNeeded
                                   : Qt::ScrollBarAlwaysOff);
    if (height() != capped) {
      setFixedHeight(capped);
    }
  }

  int m_maximumHeight;
};

} // namespace

ConflictResolverView::ConflictResolverView(GitIntegration *git,
                                           const QString &filePath,
                                           QWidget *parent)
    : QWidget(parent), m_git(git), m_relativePath(filePath) {
  setObjectName(QStringLiteral("conflictResolverView"));

  if (m_git && !m_relativePath.isEmpty()) {
    const QString root = m_git->repositoryPath();
    if (!root.isEmpty() && m_relativePath.startsWith(root)) {
      m_relativePath = m_relativePath.mid(root.length() + 1);
    }
  }

  buildUi();
  reload();
}

QString ConflictResolverView::absolutePath() const {
  if (!m_git || m_git->repositoryPath().isEmpty()) {
    return m_relativePath;
  }
  return m_git->repositoryPath() + QLatin1Char('/') + m_relativePath;
}

void ConflictResolverView::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  buildHeader(layout);
  buildToolbar(layout);

  m_scroll = new QScrollArea(this);
  m_scroll->setObjectName(QStringLiteral("conflictScrollArea"));
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);

  m_body = new QWidget(m_scroll);
  m_body->setObjectName(QStringLiteral("conflictBody"));
  m_bodyLayout = new QVBoxLayout(m_body);
  m_bodyLayout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                                   UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  m_bodyLayout->setSpacing(UiMetrics::SpaceMd);
  m_scroll->setWidget(m_body);

  layout->addWidget(m_scroll, 1);
}

void ConflictResolverView::buildHeader(QVBoxLayout *layout) {
  QWidget *header = new QWidget(this);
  header->setObjectName(QStringLiteral("conflictResolverHeader"));
  QVBoxLayout *headerLayout = new QVBoxLayout(header);
  headerLayout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                                   UiMetrics::SpaceLg, UiMetrics::SpaceMd);
  headerLayout->setSpacing(UiMetrics::SpaceSm);

  m_fileLabel = new QLabel(header);
  m_fileLabel->setObjectName(QStringLiteral("conflictFileLabel"));

  m_fileLabel->setWordWrap(true);
  m_fileLabel->setMinimumWidth(0);
  headerLayout->addWidget(m_fileLabel);

  m_branchesLabel = new QLabel(header);
  m_branchesLabel->setObjectName(QStringLiteral("conflictBranchesLabel"));
  m_branchesLabel->setWordWrap(true);
  headerLayout->addWidget(m_branchesLabel);

  QHBoxLayout *progressLayout = new QHBoxLayout();
  progressLayout->setSpacing(UiMetrics::SpaceMd);

  m_progress = new QProgressBar(header);
  m_progress->setObjectName(QStringLiteral("conflictProgressBar"));
  m_progress->setTextVisible(false);
  m_progress->setFixedHeight(8);
  progressLayout->addWidget(m_progress, 1);

  m_countLabel = new QLabel(header);
  m_countLabel->setObjectName(QStringLiteral("conflictCountLabel"));
  m_countLabel->setWordWrap(true);
  m_countLabel->setMinimumWidth(0);
  progressLayout->addWidget(m_countLabel);

  headerLayout->addLayout(progressLayout);
  layout->addWidget(header);
}

void ConflictResolverView::buildToolbar(QVBoxLayout *layout) {
  QWidget *toolbar = new QWidget(this);
  toolbar->setObjectName(QStringLiteral("conflictToolbar"));
  QSizePolicy toolbarPolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  toolbarPolicy.setHeightForWidth(true);
  toolbar->setSizePolicy(toolbarPolicy);
  toolbar->setMinimumWidth(0);

  FlowLayout *toolbarLayout =
      new FlowLayout(toolbar, 0, UiMetrics::SpaceSm, UiMetrics::SpaceSm);
  toolbarLayout->setContentsMargins(UiMetrics::SpaceLg, 0, UiMetrics::SpaceLg,
                                    UiMetrics::SpaceMd);
  toolbarLayout->setSizeConstraint(QLayout::SetNoConstraint);

  const auto addButton = [&](QPushButton **target, const QString &text,
                             const QString &name, const QString &tip) {
    *target = new QPushButton(text, toolbar);
    (*target)->setObjectName(name);
    (*target)->setToolTip(tip);
    toolbarLayout->addWidget(*target);
  };

  addButton(&m_previousButton, tr("Previous"),
            QStringLiteral("conflictPreviousButton"),
            tr("Jump to the disagreement above this one (Alt+Up)"));
  addButton(
      &m_nextButton, tr("Next"), QStringLiteral("conflictNextButton"),
      tr("Jump to the next disagreement that still needs you (Alt+Down)"));

  addButton(&m_undoButton, tr("Undo"), QStringLiteral("conflictUndoButton"),
            tr("Take back the last decision"));
  addButton(&m_redoButton, tr("Redo"), QStringLiteral("conflictRedoButton"),
            tr("Put back the decision you just took back"));

  addButton(&m_keepAllOursButton, tr("Keep all mine"),
            QStringLiteral("conflictKeepAllOursButton"),
            tr("Answer every disagreement in this file with your version"));
  addButton(&m_keepAllTheirsButton, tr("Keep all theirs"),
            QStringLiteral("conflictKeepAllTheirsButton"),
            tr("Answer every disagreement in this file with their version"));

  addButton(&m_rawButton, tr("Open raw file"),
            QStringLiteral("conflictRawButton"),
            tr("Open the file in the normal editor, markers and all"));
  addButton(&m_doneButton, tr("Save and mark fixed"),
            QStringLiteral("conflictDoneButton"),
            tr("Write your choices to the file and tell Git it is settled"));

  connect(m_previousButton, &QPushButton::clicked, this,
          &ConflictResolverView::goToPreviousConflict);
  connect(m_nextButton, &QPushButton::clicked, this,
          &ConflictResolverView::goToNextConflict);
  connect(m_undoButton, &QPushButton::clicked, this,
          &ConflictResolverView::undoDecision);
  connect(m_redoButton, &QPushButton::clicked, this,
          &ConflictResolverView::redoDecision);
  connect(m_keepAllOursButton, &QPushButton::clicked, this,
          &ConflictResolverView::keepAllOurs);
  connect(m_keepAllTheirsButton, &QPushButton::clicked, this,
          &ConflictResolverView::keepAllTheirs);
  connect(m_rawButton, &QPushButton::clicked, this,
          [this]() { emit rawFileRequested(absolutePath()); });
  connect(m_doneButton, &QPushButton::clicked, this,
          [this]() { markFileDone(); });

  const auto addShortcut = [this](const QKeySequence &keys,
                                  void (ConflictResolverView::*slot)()) {
    QShortcut *shortcut = new QShortcut(keys, this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, slot);
  };

  addShortcut(QKeySequence(Qt::ALT | Qt::Key_Down),
              &ConflictResolverView::goToNextConflict);
  addShortcut(QKeySequence(Qt::ALT | Qt::Key_Up),
              &ConflictResolverView::goToPreviousConflict);
  addShortcut(QKeySequence::Undo, &ConflictResolverView::undoDecision);
  addShortcut(QKeySequence::Redo, &ConflictResolverView::redoDecision);

  layout->addWidget(toolbar);
}

bool ConflictResolverView::reload() {
  QString content;
  if (m_git) {
    content = m_git->workingFileContent(m_relativePath);
  }

  m_resolver.load(content);
  m_dirty = false;
  m_expandedContext.clear();
  m_showBase.clear();
  m_editing.clear();
  m_currentRegionId = firstUndecidedRegion();

  rebuildBody();
  return m_resolver.hasConflicts();
}

QString ConflictResolverView::sideName(const QString &markerLabel,
                                       bool ours) const {

  if (markerLabel.isEmpty() || markerLabel == QLatin1String("HEAD")) {
    const QString branch = m_git ? m_git->currentBranch() : QString();
    if (ours && !branch.isEmpty()) {
      return branch;
    }
    return ours ? tr("this branch") : tr("the incoming branch");
  }
  return markerLabel;
}

int ConflictResolverView::firstUndecidedRegion() const {
  for (const ConflictRegion &region : m_resolver.regions()) {
    if (!region.resolved()) {
      return region.id;
    }
  }
  return m_resolver.regions().isEmpty() ? 0 : m_resolver.regions().first().id;
}

void ConflictResolverView::rebuildBody() {
  const int scrollValue = m_scroll->verticalScrollBar()
                              ? m_scroll->verticalScrollBar()->value()
                              : 0;

  m_regionCards.clear();
  QLayoutItem *item = nullptr;
  while ((item = m_bodyLayout->takeAt(0)) != nullptr) {
    if (QWidget *widget = item->widget()) {

      widget->setParent(nullptr);
      widget->deleteLater();
    }
    delete item;
  }

  if (!m_resolver.hasConflicts()) {
    QLabel *empty = new QLabel(
        tr("There are no conflict markers left in this file. Press \"Save and "
           "mark fixed\" to tell Git you are done with it."),
        m_body);
    empty->setObjectName(QStringLiteral("conflictEmptyLabel"));
    empty->setWordWrap(true);
    m_bodyLayout->addWidget(empty);
    m_bodyLayout->addStretch();
    updateHeader();
    updateToolbar();
    return;
  }

  const QList<ConflictDocumentItem> items =
      m_resolver.documentItems(kContextLines);
  for (int index = 0; index < items.size(); ++index) {
    const ConflictDocumentItem &documentItem = items.at(index);
    if (documentItem.kind == ConflictDocumentItem::Kind::Context) {
      if (QWidget *card = buildContextCard(documentItem, index)) {
        m_bodyLayout->addWidget(card);
      }
      continue;
    }

    const ConflictRegion *region = m_resolver.region(documentItem.regionId);
    if (!region) {
      continue;
    }
    QWidget *card = buildConflictCard(*region, documentItem.startLine);
    m_regionCards.insert(region->id, card);
    m_bodyLayout->addWidget(card);
  }

  m_bodyLayout->addStretch();
  updateHeader();
  updateToolbar();

  if (m_scroll->verticalScrollBar()) {
    m_scroll->verticalScrollBar()->setValue(scrollValue);
  }
}

QWidget *
ConflictResolverView::buildContextCard(const ConflictDocumentItem &item,
                                       int itemIndex) {
  if (item.lineCount() == 0) {
    return nullptr;
  }

  QWidget *card = new QWidget(m_body);
  card->setObjectName(QStringLiteral("conflictContextCard"));
  QVBoxLayout *layout = new QVBoxLayout(card);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(UiMetrics::SpaceXs);

  const int key = itemIndex;
  const bool expanded = m_expandedContext.value(key, false);

  if (!item.leadingLines.isEmpty()) {
    layout->addWidget(
        buildCodeBlock(card, item.leadingLines, m_theme.surfaceColor));
  }

  if (item.hiddenCount() > 0) {
    if (expanded) {
      layout->addWidget(
          buildCodeBlock(card, item.hiddenLines, m_theme.surfaceColor));
    }

    QPushButton *toggle = new QPushButton(
        expanded
            ? tr("Hide those %1 unchanged lines again").arg(item.hiddenCount())
            : tr("Show %1 unchanged lines").arg(item.hiddenCount()),
        card);
    toggle->setObjectName(QStringLiteral("conflictContextToggle"));
    toggle->setFlat(true);
    toggle->setCursor(Qt::PointingHandCursor);
    connect(toggle, &QPushButton::clicked, this, [this, key, expanded]() {
      m_expandedContext.insert(key, !expanded);
      rebuildBody();
    });
    layout->addWidget(toggle, 0, Qt::AlignLeft);
  }

  if (!item.trailingLines.isEmpty()) {
    layout->addWidget(
        buildCodeBlock(card, item.trailingLines, m_theme.surfaceColor));
  }

  return card;
}

QWidget *ConflictResolverView::buildCodeBlock(QWidget *parent,
                                              const QStringList &lines,
                                              const QColor &tint) {
  CodeBlock *block =
      new CodeBlock(joinLines(lines), kMaxCodeBlockHeight, parent);
  block->setObjectName(QStringLiteral("conflictCodeBlock"));

  if (tint.isValid()) {
    block->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background: %1; color: %2; border: "
                       "none; padding: 4px; }")
            .arg(tint.name(), UIStyleHelper::readableText(
                                  m_theme, tint, m_theme.foregroundColor)
                                  .name()));
  }
  return block;
}

QWidget *ConflictResolverView::buildSidePane(
    QWidget *parent, const QString &title, const QString &subtitle,
    const QStringList &lines, const QColor &tint, const QString &buttonText,
    ConflictChoice choice, int regionId) {
  QWidget *pane = new QWidget(parent);
  QVBoxLayout *layout = new QVBoxLayout(pane);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(UiMetrics::SpaceXs);

  QWidget *headerRow = new QWidget(pane);
  headerRow->setObjectName(QStringLiteral("conflictSideHeader"));
  QHBoxLayout *headerLayout = new QHBoxLayout(headerRow);
  headerLayout->setContentsMargins(UiMetrics::SpaceMd, UiMetrics::SpaceSm,
                                   UiMetrics::SpaceMd, UiMetrics::SpaceSm);
  headerLayout->setSpacing(UiMetrics::SpaceMd);

  const QColor headerFill = tintOf(tint, m_theme.surfaceColor);

  QLabel *titleLabel = new QLabel(title, headerRow);
  titleLabel->setWordWrap(true);
  titleLabel->setMinimumWidth(0);
  titleLabel->setStyleSheet(
      QStringLiteral("font-weight: bold; color: %1;")
          .arg(ColorContrast::ensure(tint, headerFill).name()));
  headerLayout->addWidget(titleLabel);

  QLabel *subtitleLabel = new QLabel(subtitle, headerRow);
  subtitleLabel->setWordWrap(true);
  subtitleLabel->setMinimumWidth(0);
  subtitleLabel->setStyleSheet(
      QStringLiteral("color: %1;")
          .arg(ColorContrast::ensure(UIStyleHelper::mutedTextColor(m_theme),
                                     headerFill,
                                     ColorContrast::SecondaryTextRatio)
                   .name()));
  headerLayout->addWidget(subtitleLabel, 1);

  QPushButton *keepButton = new QPushButton(buttonText, headerRow);
  keepButton->setObjectName(choice == ConflictChoice::Ours
                                ? QStringLiteral("conflictKeepOursButton")
                                : QStringLiteral("conflictKeepTheirsButton"));
  keepButton->setProperty("regionId", regionId);
  keepButton->setCursor(Qt::PointingHandCursor);
  keepButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(m_theme));
  connect(keepButton, &QPushButton::clicked, this,
          [this, regionId, choice]() { applyChoice(regionId, choice); });
  headerLayout->addWidget(keepButton);

  headerRow->setStyleSheet(
      QStringLiteral("QWidget#conflictSideHeader { background: %1; "
                     "border-top-left-radius: %2px; "
                     "border-top-right-radius: %2px; }")
          .arg(headerFill.name())
          .arg(UiMetrics::RadiusMd));
  layout->addWidget(headerRow);

  if (lines.isEmpty()) {
    QLabel *emptyLabel = new QLabel(tr("(this side has nothing here)"), pane);
    emptyLabel->setStyleSheet(UIStyleHelper::emptyStateStyle(m_theme) +
                              QStringLiteral("padding: 6px;"));
    layout->addWidget(emptyLabel);
  } else {
    layout->addWidget(
        buildCodeBlock(pane, lines, tintOf(tint, m_theme.backgroundColor)));
  }

  return pane;
}

QWidget *ConflictResolverView::buildConflictCard(const ConflictRegion &region,
                                                 int startLine) {
  QWidget *card = new QWidget(m_body);
  card->setObjectName(QStringLiteral("conflictCard"));
  card->setProperty("regionId", region.id);

  QVBoxLayout *layout = new QVBoxLayout(card);
  layout->setContentsMargins(UiMetrics::SpaceMd, UiMetrics::SpaceMd,
                             UiMetrics::SpaceMd, UiMetrics::SpaceMd);
  layout->setSpacing(UiMetrics::SpaceMd);

  const QColor accent =
      region.resolved() ? m_theme.successColor : m_theme.errorColor;
  const UIStyleHelper::Tone tone = region.resolved()
                                       ? UIStyleHelper::Tone::Success
                                       : UIStyleHelper::Tone::Error;
  card->setStyleSheet(
      UIStyleHelper::cardStyle(m_theme, card->objectName()) +
      QStringLiteral("QWidget#conflictCard { border: 2px solid %1; "
                     "border-radius: %2px; }")
          .arg(accent.name())
          .arg(UiMetrics::RadiusMd));

  QWidget *titleRow = new QWidget(card);
  QHBoxLayout *titleLayout = new QHBoxLayout(titleRow);
  titleLayout->setContentsMargins(0, 0, 0, 0);
  titleLayout->setSpacing(UiMetrics::SpaceMd);

  QLabel *heading = new QLabel(tr("Disagreement %1 of %2")
                                   .arg(region.id)
                                   .arg(m_resolver.totalConflicts()),
                               titleRow);
  heading->setObjectName(QStringLiteral("conflictCardHeading"));
  heading->setStyleSheet(
      UIStyleHelper::headingStyle(m_theme, 14) +
      QStringLiteral("color: %1;")
          .arg(UIStyleHelper::toneColor(m_theme, tone).name()));
  titleLayout->addWidget(heading);

  QLabel *lineLabel =
      new QLabel(tr("starts at line %1").arg(startLine), titleRow);
  lineLabel->setObjectName(QStringLiteral("conflictCardLineLabel"));
  lineLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(m_theme));
  titleLayout->addWidget(lineLabel);
  titleLayout->addStretch();

  if (region.resolved()) {
    QLabel *outcome = new QLabel(
        tr("Decided: you %1").arg(conflictChoiceOutcome(region.choice)),
        titleRow);
    outcome->setObjectName(QStringLiteral("conflictCardOutcomeLabel"));
    outcome->setStyleSheet(
        QStringLiteral("font-weight: bold; color: %1;")
            .arg(UIStyleHelper::toneColor(m_theme, tone).name()));
    titleLayout->addWidget(outcome);

    QPushButton *change = new QPushButton(tr("Change my mind"), titleRow);
    change->setObjectName(QStringLiteral("conflictReopenButton"));
    change->setProperty("regionId", region.id);
    const int id = region.id;
    connect(change, &QPushButton::clicked, this, [this, id]() {
      m_resolver.reopen(id);
      m_dirty = true;
      m_currentRegionId = id;
      rebuildBody();
      emit progressChanged();
    });
    titleLayout->addWidget(change);
  }

  layout->addWidget(titleRow);

  if (region.resolved()) {
    QLabel *resultLabel =
        new QLabel(tr("This is what ends up in the file:"), card);
    resultLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(m_theme));
    layout->addWidget(resultLabel);

    const QStringList result = region.resultLines();
    if (result.isEmpty()) {
      QLabel *nothing =
          new QLabel(tr("(nothing - both versions dropped)"), card);
      nothing->setStyleSheet(UIStyleHelper::subduedLabelStyle(m_theme) +
                             QStringLiteral("font-style: italic;"));
      layout->addWidget(nothing);
    } else {
      layout->addWidget(buildCodeBlock(
          card, result, tintOf(m_theme.successColor, m_theme.backgroundColor)));
    }
    return card;
  }

  QLabel *question = new QLabel(
      tr("Both branches changed these lines. Which version should survive?"),
      card);
  question->setObjectName(QStringLiteral("conflictCardQuestion"));
  question->setWordWrap(true);
  layout->addWidget(question);

  const QString oursBranch = sideName(region.oursLabel, true);
  const QString theirsBranch = sideName(region.theirsLabel, false);

  layout->addWidget(
      buildSidePane(card, tr("YOUR VERSION"),
                    tr("from %1, already here - %2")
                        .arg(oursBranch, describeLines(region.oursLines)),
                    region.oursLines, m_theme.infoColor, tr("Keep this one"),
                    ConflictChoice::Ours, region.id));

  layout->addWidget(
      buildSidePane(card, tr("THEIR VERSION"),
                    tr("from %1, coming in - %2")
                        .arg(theirsBranch, describeLines(region.theirsLines)),
                    region.theirsLines, m_theme.warningColor,
                    tr("Keep this one"), ConflictChoice::Theirs, region.id));

  if (region.hasBase && m_showBase.value(region.id, false)) {
    QLabel *baseLabel = new QLabel(tr("What both branches started from, before "
                                      "either of them touched it:"),
                                   card);
    baseLabel->setWordWrap(true);
    baseLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(m_theme));
    layout->addWidget(baseLabel);
    layout->addWidget(
        buildCodeBlock(card, region.baseLines, m_theme.surfaceAltColor));
  }

  const int id = region.id;

  if (m_editing.value(id, false)) {
    QLabel *editLabel = new QLabel(
        tr("Write exactly what this part of the file should say:"), card);
    editLabel->setWordWrap(true);
    layout->addWidget(editLabel);

    QPlainTextEdit *editor = new QPlainTextEdit(card);
    editor->setObjectName(QStringLiteral("conflictCustomEditor"));
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setPlainText(joinLines(region.oursLines + region.theirsLines));
    editor->setFixedHeight(160);
    layout->addWidget(editor);

    QHBoxLayout *editActions = new QHBoxLayout();
    editActions->addStretch();

    QPushButton *cancel = new QPushButton(tr("Never mind"), card);
    cancel->setObjectName(QStringLiteral("conflictCustomCancelButton"));
    connect(cancel, &QPushButton::clicked, this, [this, id]() {
      m_editing.insert(id, false);
      rebuildBody();
    });
    editActions->addWidget(cancel);

    QPushButton *useIt = new QPushButton(tr("Use this text"), card);
    useIt->setObjectName(QStringLiteral("conflictCustomAcceptButton"));
    connect(useIt, &QPushButton::clicked, this, [this, id, editor]() {
      m_editing.insert(id, false);
      applyChoice(id, ConflictChoice::Custom,
                  editor->toPlainText().split(QLatin1Char('\n')));
    });
    editActions->addWidget(useIt);

    layout->addLayout(editActions);
    return card;
  }

  QWidget *actionsRow = new QWidget(card);
  QHBoxLayout *actions = new QHBoxLayout(actionsRow);
  actions->setContentsMargins(0, 0, 0, 0);
  actions->setSpacing(UiMetrics::SpaceSm);

  const auto addAction = [&](const QString &text, const QString &name,
                             const QString &tip,
                             const std::function<void()> &handler) {
    QPushButton *button = new QPushButton(text, actionsRow);
    button->setObjectName(name);
    button->setProperty("regionId", id);
    button->setToolTip(tip);
    connect(button, &QPushButton::clicked, this, handler);
    actions->addWidget(button);
  };

  addAction(tr("Keep both - mine first"),
            QStringLiteral("conflictKeepBothOursFirstButton"),
            tr("Put your lines in, then theirs, one after the other"),
            [this, id]() { applyChoice(id, ConflictChoice::BothOursFirst); });
  addAction(tr("Keep both - theirs first"),
            QStringLiteral("conflictKeepBothTheirsFirstButton"),
            tr("Put their lines in, then yours, one after the other"),
            [this, id]() { applyChoice(id, ConflictChoice::BothTheirsFirst); });
  addAction(tr("Keep neither"), QStringLiteral("conflictKeepNeitherButton"),
            tr("Remove both versions and leave nothing here"),
            [this, id]() { applyChoice(id, ConflictChoice::Neither); });

  if (region.hasBase) {
    const bool showing = m_showBase.value(id, false);
    addAction(showing ? tr("Hide the original") : tr("Show the original"),
              QStringLiteral("conflictShowBaseButton"),
              tr("See what this looked like before either branch changed it"),
              [this, id, showing]() {
                m_showBase.insert(id, !showing);
                rebuildBody();
              });
  }

  addAction(tr("Write it myself"), QStringLiteral("conflictWriteOwnButton"),
            tr("Type the exact text that should replace both versions"),
            [this, id]() {
              m_editing.insert(id, true);
              rebuildBody();
            });

  actions->addStretch();
  layout->addWidget(actionsRow);

  return card;
}

void ConflictResolverView::applyChoice(int regionId, ConflictChoice choice,
                                       const QStringList &customLines) {
  if (!m_resolver.resolve(regionId, choice, customLines)) {
    return;
  }

  m_dirty = true;
  m_currentRegionId = regionId;
  rebuildBody();
  emit progressChanged();

  const int next = firstUndecidedRegion();
  if (next != 0 && next != regionId) {
    scrollToRegion(next);
  }
}

void ConflictResolverView::scrollToRegion(int regionId) {
  QWidget *card = m_regionCards.value(regionId, nullptr);
  if (!card || !m_scroll->verticalScrollBar()) {
    return;
  }
  m_currentRegionId = regionId;
  QPointer<QWidget> target(card);
  QTimer::singleShot(0, this, [this, target]() {
    if (target) {
      m_scroll->ensureWidgetVisible(target, 0, UiMetrics::SpaceXl);
    }
  });
}

void ConflictResolverView::goToNextConflict() {
  const QList<ConflictRegion> &regions = m_resolver.regions();
  for (const ConflictRegion &region : regions) {
    if (region.id > m_currentRegionId && !region.resolved()) {
      scrollToRegion(region.id);
      return;
    }
  }
  for (const ConflictRegion &region : regions) {
    if (!region.resolved()) {
      scrollToRegion(region.id);
      return;
    }
  }
  for (const ConflictRegion &region : regions) {
    if (region.id > m_currentRegionId) {
      scrollToRegion(region.id);
      return;
    }
  }
}

void ConflictResolverView::goToPreviousConflict() {
  const QList<ConflictRegion> &regions = m_resolver.regions();
  for (int i = regions.size() - 1; i >= 0; --i) {
    if (regions.at(i).id < m_currentRegionId) {
      scrollToRegion(regions.at(i).id);
      return;
    }
  }
}

void ConflictResolverView::undoDecision() {
  if (!m_resolver.undo()) {
    return;
  }
  m_dirty = true;
  rebuildBody();
  emit progressChanged();
}

void ConflictResolverView::redoDecision() {
  if (!m_resolver.redo()) {
    return;
  }
  m_dirty = true;
  rebuildBody();
  emit progressChanged();
}

void ConflictResolverView::keepAllOurs() {
  if (!m_resolver.resolveAll(ConflictChoice::Ours)) {
    return;
  }
  m_dirty = true;
  rebuildBody();
  emit progressChanged();
}

void ConflictResolverView::keepAllTheirs() {
  if (!m_resolver.resolveAll(ConflictChoice::Theirs)) {
    return;
  }
  m_dirty = true;
  rebuildBody();
  emit progressChanged();
}

bool ConflictResolverView::saveProgress() {
  if (!m_git) {
    return false;
  }
  if (!m_git->writeWorkingFile(m_relativePath, m_resolver.text())) {
    return false;
  }
  m_dirty = false;
  return true;
}

bool ConflictResolverView::markFileDone() {
  if (!m_git || m_resolver.remainingCount() > 0) {
    return false;
  }
  if (!m_git->resolveConflictWith(m_relativePath, m_resolver.text())) {
    return false;
  }

  m_dirty = false;
  emit fileResolved(m_relativePath);
  emit progressChanged();
  return true;
}

void ConflictResolverView::updateHeader() {
  m_fileLabel->setText(m_relativePath);

  const QString ours = sideName(m_resolver.oursLabel(), true);
  const QString theirs = sideName(m_resolver.theirsLabel(), false);
  m_branchesLabel->setText(
      tr("Your version comes from %1. Their version comes from %2.")
          .arg(ours, theirs));

  const int total = m_resolver.totalConflicts();
  const int done = m_resolver.resolvedCount();

  m_progress->setRange(0, qMax(1, total));
  m_progress->setValue(done);

  if (total == 0) {
    m_countLabel->setText(tr("Nothing left to decide"));
  } else if (done == total) {
    m_countLabel->setText(
        total == 1 ? tr("The one disagreement is decided - save it to finish")
                   : tr("All %1 disagreements decided - save it to finish")
                         .arg(total));
  } else {
    m_countLabel->setText(tr("%1 of %2 decided, %3 to go")
                              .arg(done)
                              .arg(total)
                              .arg(total - done));
  }
}

void ConflictResolverView::updateToolbar() {
  const int total = m_resolver.totalConflicts();
  const int remaining = m_resolver.remainingCount();

  m_previousButton->setEnabled(total > 1);
  m_nextButton->setEnabled(total > 1);
  m_undoButton->setEnabled(m_resolver.canUndo());
  m_redoButton->setEnabled(m_resolver.canRedo());
  m_undoButton->setToolTip(
      m_resolver.canUndo()
          ? tr("Take back: %1").arg(m_resolver.undoDescription())
          : tr("Nothing to take back yet"));
  m_keepAllOursButton->setEnabled(remaining > 0);
  m_keepAllTheirsButton->setEnabled(remaining > 0);
  m_doneButton->setEnabled(remaining == 0);
  m_doneButton->setToolTip(
      remaining == 0
          ? tr("Write your choices to the file and tell Git it is settled")
      : remaining == 1
          ? tr("1 disagreement still needs an answer")
          : tr("%1 disagreements still need an answer").arg(remaining));
}

void ConflictResolverView::applyTheme(const Theme &theme) {
  m_theme = theme;
  m_themeReady = true;

  setStyleSheet(
      UIStyleHelper::panelStyle(theme, objectName()) +
      QStringLiteral(
          "QWidget#conflictResolverView, QWidget#conflictBody { background: "
          "%1; color: %2; }"
          "QLabel { color: %2; background: transparent; }"
          "QScrollArea#conflictScrollArea { background: transparent; border: "
          "none; }"
          "QScrollArea#conflictScrollArea > QWidget { background: "
          "transparent; }")
          .arg(theme.backgroundColor.name(), theme.foregroundColor.name()));

  if (QWidget *header =
          findChild<QWidget *>(QStringLiteral("conflictResolverHeader"))) {
    header->setStyleSheet(
        UIStyleHelper::panelHeaderStyle(theme, header->objectName()));
  }

  m_fileLabel->setStyleSheet(UIStyleHelper::headingStyle(theme, 16));
  m_branchesLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
  m_countLabel->setStyleSheet(UIStyleHelper::titleLabelStyle(theme));
  m_progress->setStyleSheet(
      QStringLiteral("QProgressBar { background: %1; border: none; "
                     "border-radius: 4px; } QProgressBar::chunk { background: "
                     "%2; border-radius: 4px; }")
          .arg(theme.surfaceAltColor.name(), theme.accentColor.name()));

  rebuildBody();
}
