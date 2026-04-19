#include "gitworkbenchdialog.h"
#include "../../ui/uistylehelper.h"

#include "themedmessagebox.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QTextStream>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

const QStringList GitWorkbenchDialog::REBASE_ACTIONS = {
    "pick", "reword", "edit", "squash", "fixup", "drop"};

const QStringList GitWorkbenchDialog::REBASE_ACTIONS_EXTENDED = {
    "pick", "reword", "edit", "squash", "fixup", "drop", "drop-keep"};

const QMap<QString, QString> GitWorkbenchDialog::ACTION_ICONS = {
    {"pick", "✓"},  {"reword", "✏"}, {"edit", "⚙"},     {"squash", "◫"},
    {"fixup", "⊕"}, {"drop", "✕"},   {"drop-keep", "◌"}};

class CommitGraphDelegate : public QStyledItemDelegate {
public:
  CommitGraphDelegate(const QList<WorkbenchRebaseEntry> *entries,
                      const QMap<QString, int> *hashIndex, const Theme *theme,
                      QObject *parent = nullptr)
      : QStyledItemDelegate(parent), m_entries(entries), m_hashIndex(hashIndex),
        m_theme(theme) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (index.column() != 0) {
      QStyledItemDelegate::paint(painter, option, index);
      return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (option.state & QStyle::State_Selected) {
      QColor sel = m_theme->accentSoftColor;
      sel.setAlpha(80);
      painter->fillRect(option.rect, sel);

      QColor stripe = m_theme->accentColor;
      stripe.setAlpha(200);
      painter->fillRect(option.rect.left(), option.rect.top(), 3,
                        option.rect.height(), stripe);
    } else if (option.state & QStyle::State_MouseOver) {
      painter->fillRect(option.rect, m_theme->hoverColor);
    }

    int row = index.row();
    if (row >= 0 && row < m_entries->size()) {
      const auto &entry = (*m_entries)[row];
      bool isDrop = (entry.action == "drop");
      bool isDropKeep = (entry.action == "drop-keep");

      int dotX = option.rect.left() + 18;
      int dotY = option.rect.center().y();
      int dotR = 6;

      static const QList<QColor> laneColors = {
          QColor(0x4E, 0xC9, 0xB0), QColor(0xCE, 0x91, 0x78),
          QColor(0x56, 0x9C, 0xD6), QColor(0xDC, 0xDC, 0xAA),
          QColor(0xC5, 0x86, 0xC0), QColor(0xD7, 0xBA, 0x7D),
          QColor(0x6A, 0x99, 0x55), QColor(0xD1, 0x6D, 0x6D),
      };
      QColor dotColor = laneColors[0];

      if (isDrop) {
        dotColor = m_theme->errorColor;
        dotColor.setAlpha(120);
      } else if (isDropKeep) {
        dotColor = m_theme->warningColor;
        dotColor.setAlpha(160);
      }

      QColor dropLaneColor = m_theme->errorColor;
      dropLaneColor.setAlpha(80);
      QPen lanePen(isDrop ? dropLaneColor : dotColor, 2.0,
                   isDrop ? Qt::DashLine : Qt::SolidLine);
      painter->setPen(lanePen);
      if (row > 0)
        painter->drawLine(dotX, option.rect.top(), dotX, dotY - dotR - 1);
      if (row < m_entries->size() - 1)
        painter->drawLine(dotX, dotY + dotR + 1, dotX, option.rect.bottom());

      if (entry.parents.size() > 1) {
        QPen mergePen(laneColors[1 % laneColors.size()], 1.5, Qt::DashLine);
        painter->setPen(mergePen);
        painter->drawLine(dotX + dotR + 2, dotY, dotX + 28, dotY);

        painter->setPen(Qt::NoPen);
        painter->setBrush(dotColor);
        QPolygonF diamond;
        diamond << QPointF(dotX, dotY - dotR - 1)
                << QPointF(dotX + dotR + 1, dotY)
                << QPointF(dotX, dotY + dotR + 1)
                << QPointF(dotX - dotR - 1, dotY);
        painter->drawPolygon(diamond);
      } else if (isDrop) {

        painter->setPen(QPen(m_theme->errorColor, 2));
        painter->drawLine(dotX - 3, dotY - 3, dotX + 3, dotY + 3);
        painter->drawLine(dotX + 3, dotY - 3, dotX - 3, dotY + 3);
      } else if (isDropKeep) {

        painter->setPen(QPen(m_theme->warningColor, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(dotX, dotY), dotR, dotR);
      } else {

        painter->setPen(Qt::NoPen);
        painter->setBrush(dotColor);
        painter->drawEllipse(QPointF(dotX, dotY), dotR, dotR);
      }

      int textX = dotX + 24;
      if (!entry.refs.isEmpty()) {
        QFont refFont = option.font;
        refFont.setPointSize(10);
        refFont.setBold(true);
        painter->setFont(refFont);
        QFontMetrics rfm(refFont);
        for (const auto &ref : entry.refs) {
          bool isTag = ref.startsWith("tag: ");
          QString label = isTag ? ref.mid(5) : ref;
          QColor pillColor =
              isTag ? m_theme->warningColor : m_theme->accentColor;
          int pw = rfm.horizontalAdvance(label) + 14;
          int ph = 20;
          int py = dotY - ph / 2;
          painter->setPen(Qt::NoPen);
          QColor pillBg = pillColor;
          pillBg.setAlpha(30);
          painter->setBrush(pillBg);
          painter->drawRoundedRect(textX, py, pw, ph, 5, 5);
          pillColor.setAlpha(220);
          painter->setPen(pillColor);
          painter->drawText(textX + 7, py, pw - 14, ph, Qt::AlignVCenter,
                            label);
          textX += pw + 5;
        }
      }

      QFont f = option.font;
      f.setPointSize(12);
      painter->setFont(f);
      QColor textColor = m_theme->foregroundColor;
      if (isDrop) {
        textColor.setAlpha(100);
        f.setStrikeOut(true);
        painter->setFont(f);
      } else if (isDropKeep) {
        textColor.setAlpha(180);
      }
      painter->setPen(textColor);
      QString subject = entry.subject;
      QFontMetrics fm(f);
      int availW = option.rect.right() - textX - 4;
      subject = fm.elidedText(subject, Qt::ElideRight, availW);
      painter->drawText(textX, option.rect.top(), availW, option.rect.height(),
                        Qt::AlignVCenter, subject);
    }

    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    Q_UNUSED(index);
    return QSize(option.rect.width(), 42);
  }

private:
  const QList<WorkbenchRebaseEntry> *m_entries;
  const QMap<QString, int> *m_hashIndex;
  const Theme *m_theme;
};

GitWorkbenchDialog::GitWorkbenchDialog(GitIntegration *git, const Theme &theme,
                                       QWidget *parent)
    : StyledDialog(parent), m_git(git), m_rewriteMode(false) {
  setWindowTitle(tr("Git Workbench"));
  setMinimumSize(1100, 700);
  resize(1300, 820);
  buildUi();
  applyTheme(theme);
}

void GitWorkbenchDialog::buildUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  auto *titleBar = new QWidget(this);
  titleBar->setObjectName("workbenchTitleBar");
  auto *titleLayout = new QHBoxLayout(titleBar);
  titleLayout->setContentsMargins(20, 12, 20, 12);

  QString repoName;
  if (m_git && m_git->isValidRepository()) {
    repoName = QDir(m_git->repositoryPath()).dirName();
  }
  auto *titleLabel = new QLabel(
      tr("Git Workbench") +
          (repoName.isEmpty() ? ""
                              : QString::fromUtf8(" \xe2\x80\x94 ") + repoName),
      this);
  titleLabel->setObjectName("workbenchTitle");
  titleLayout->addWidget(titleLabel);
  titleLayout->addStretch();

  auto *shortcutHint = new QLabel(
      tr("J/K navigate  \xc2\xb7  S squash  \xc2\xb7  D drop  \xc2\xb7  "
         "R reword  \xc2\xb7  Ctrl+K commands"),
      this);
  shortcutHint->setObjectName("workbenchShortcutHint");
  titleLayout->addWidget(shortcutHint);

  mainLayout->addWidget(titleBar);

  auto *sep = new QFrame(this);
  sep->setFrameShape(QFrame::HLine);
  sep->setObjectName("workbenchSeparator");
  mainLayout->addWidget(sep);

  m_mainSplitter = new QSplitter(Qt::Horizontal, this);
  m_mainSplitter->setHandleWidth(1);
  m_mainSplitter->setChildrenCollapsible(false);

  m_branchPanel = new QWidget(this);
  m_branchPanel->setObjectName("branchPanel");
  auto *branchLayout = new QVBoxLayout(m_branchPanel);
  branchLayout->setContentsMargins(0, 0, 0, 0);
  branchLayout->setSpacing(0);
  buildBranchExplorer(branchLayout);
  m_mainSplitter->addWidget(m_branchPanel);

  m_commitPanel = new QWidget(this);
  m_commitPanel->setObjectName("commitPanel");
  auto *commitLayout = new QVBoxLayout(m_commitPanel);
  commitLayout->setContentsMargins(0, 0, 0, 0);
  commitLayout->setSpacing(0);
  buildCommitCanvas(commitLayout);
  m_mainSplitter->addWidget(m_commitPanel);

  m_inspectorPanel = new QWidget(this);
  m_inspectorPanel->setObjectName("inspectorPanel");
  auto *inspLayout = new QVBoxLayout(m_inspectorPanel);
  inspLayout->setContentsMargins(0, 0, 0, 0);
  inspLayout->setSpacing(0);
  buildInspector(inspLayout);
  m_mainSplitter->addWidget(m_inspectorPanel);

  m_mainSplitter->setSizes({240, 580, 340});
  mainLayout->addWidget(m_mainSplitter, 1);

  buildBottomBar(mainLayout);
}

void GitWorkbenchDialog::buildBranchExplorer(QVBoxLayout *layout) {

  auto *header = new QWidget(this);
  header->setObjectName("branchHeader");
  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(14, 10, 14, 10);
  auto *headerLabel = new QLabel(tr("BRANCHES"), this);
  headerLabel->setObjectName("sectionHeader");
  headerLayout->addWidget(headerLabel);
  headerLayout->addStretch();
  m_createBranchBtn = new QPushButton(tr("+"), this);
  m_createBranchBtn->setObjectName("createBranchBtn");
  m_createBranchBtn->setToolTip(tr("Create new branch"));
  m_createBranchBtn->setFixedSize(28, 28);
  headerLayout->addWidget(m_createBranchBtn);
  layout->addWidget(header);

  m_branchSearch = new QLineEdit(this);
  m_branchSearch->setObjectName("branchSearch");
  m_branchSearch->setPlaceholderText(tr("Filter branches..."));
  m_branchSearch->setClearButtonEnabled(true);
  auto *searchContainer = new QWidget(this);
  auto *searchLayout = new QHBoxLayout(searchContainer);
  searchLayout->setContentsMargins(10, 6, 10, 6);
  searchLayout->addWidget(m_branchSearch);
  layout->addWidget(searchContainer);

  m_branchTree = new QTreeWidget(this);
  m_branchTree->setObjectName("branchTree");
  m_branchTree->setHeaderHidden(true);
  m_branchTree->setRootIsDecorated(true);
  m_branchTree->setIndentation(16);
  m_branchTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_branchTree->setContextMenuPolicy(Qt::CustomContextMenu);
  m_branchTree->setFocusPolicy(Qt::NoFocus);
  layout->addWidget(m_branchTree, 1);

  connect(m_branchTree, &QTreeWidget::itemClicked, this,
          &GitWorkbenchDialog::onBranchItemClicked);
  connect(m_branchTree, &QTreeWidget::itemDoubleClicked, this,
          &GitWorkbenchDialog::onBranchItemDoubleClicked);
  connect(m_branchTree, &QTreeWidget::customContextMenuRequested, this,
          &GitWorkbenchDialog::onBranchContextMenu);
  connect(m_branchSearch, &QLineEdit::textChanged, this,
          &GitWorkbenchDialog::onBranchSearchChanged);
  connect(m_createBranchBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onCreateBranchClicked);
}

void GitWorkbenchDialog::buildCommitCanvas(QVBoxLayout *layout) {

  auto *header = new QWidget(this);
  header->setObjectName("commitHeader");
  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(14, 10, 14, 10);

  auto *commitsLabel = new QLabel(tr("COMMITS"), this);
  commitsLabel->setObjectName("sectionHeader");
  headerLayout->addWidget(commitsLabel);
  headerLayout->addStretch();

  m_rewriteToggleBtn = new QPushButton(tr("Rewrite Mode"), this);
  m_rewriteToggleBtn->setObjectName("rewriteToggleBtn");
  m_rewriteToggleBtn->setCheckable(true);
  m_rewriteToggleBtn->setToolTip(
      tr("Toggle interactive rewrite mode (reorder, squash, drop commits)"));
  headerLayout->addWidget(m_rewriteToggleBtn);
  layout->addWidget(header);

  m_commitSearch = new QLineEdit(this);
  m_commitSearch->setObjectName("commitSearch");
  m_commitSearch->setPlaceholderText(
      tr("Filter by hash, author, or message..."));
  m_commitSearch->setClearButtonEnabled(true);
  auto *searchContainer = new QWidget(this);
  auto *searchLayout = new QHBoxLayout(searchContainer);
  searchLayout->setContentsMargins(10, 6, 10, 6);
  searchLayout->addWidget(m_commitSearch);
  layout->addWidget(searchContainer);

  auto *toolbarContainer = new QWidget(this);
  toolbarContainer->setObjectName("rewriteToolbarContainer");
  auto *tbLayout = new QHBoxLayout(toolbarContainer);
  tbLayout->setContentsMargins(10, 6, 10, 6);
  tbLayout->setSpacing(6);

  m_moveUpBtn = new QPushButton(tr("▲"), this);
  m_moveDownBtn = new QPushButton(tr("▼"), this);
  m_squashBtn = new QPushButton(tr("Squash"), this);
  m_fixupBtn = new QPushButton(tr("Fixup"), this);
  m_rewordBtn = new QPushButton(tr("Reword"), this);
  m_dropBtn = new QPushButton(tr("Drop"), this);
  m_pickAllBtn = new QPushButton(tr("Reset All"), this);

  m_moveUpBtn->setObjectName("rewriteToolBtn");
  m_moveDownBtn->setObjectName("rewriteToolBtn");
  m_squashBtn->setObjectName("rewriteToolBtn");
  m_fixupBtn->setObjectName("rewriteToolBtn");
  m_rewordBtn->setObjectName("rewriteToolBtn");
  m_dropBtn->setObjectName("rewriteDropBtn");
  m_pickAllBtn->setObjectName("rewriteToolBtn");

  m_moveUpBtn->setToolTip(tr("Move commit up (Shift+K)"));
  m_moveDownBtn->setToolTip(tr("Move commit down (Shift+J)"));
  m_squashBtn->setToolTip(tr("Squash selected commits (S)"));
  m_fixupBtn->setToolTip(tr("Fixup selected into previous (F)"));
  m_rewordBtn->setToolTip(tr("Reword commit message (R)"));
  m_dropBtn->setToolTip(tr("Drop selected commits (D)"));
  m_pickAllBtn->setToolTip(tr("Reset all actions to pick"));

  m_moveUpBtn->setFixedWidth(36);
  m_moveDownBtn->setFixedWidth(36);

  tbLayout->addWidget(m_moveUpBtn);
  tbLayout->addWidget(m_moveDownBtn);
  tbLayout->addSpacing(8);
  tbLayout->addWidget(m_squashBtn);
  tbLayout->addWidget(m_fixupBtn);
  tbLayout->addWidget(m_rewordBtn);
  tbLayout->addWidget(m_dropBtn);
  tbLayout->addStretch();
  tbLayout->addWidget(m_pickAllBtn);

  toolbarContainer->setVisible(false);
  m_rewriteToolbar = nullptr;
  layout->addWidget(toolbarContainer);

  m_selectionBar = new QWidget(this);
  m_selectionBar->setObjectName("selectionBar");
  auto *selBarLayout = new QHBoxLayout(m_selectionBar);
  selBarLayout->setContentsMargins(12, 6, 12, 6);
  selBarLayout->setSpacing(8);

  m_selectionCountLabel = new QLabel(this);
  m_selectionCountLabel->setObjectName("selectionCountLabel");
  selBarLayout->addWidget(m_selectionCountLabel);
  selBarLayout->addStretch();

  auto *selHintLabel = new QLabel(tr("Shift+J/K to extend  \xc2\xb7  "
                                     "Enter Rewrite Mode (R) to apply actions"),
                                  this);
  selHintLabel->setObjectName("selectionHintLabel");
  selBarLayout->addWidget(selHintLabel);

  m_selectionBar->setVisible(false);
  layout->addWidget(m_selectionBar);

  m_commitTree = new QTreeWidget(this);
  m_commitTree->setObjectName("commitTree");
  m_commitTree->setRootIsDecorated(false);
  m_commitTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_commitTree->setAlternatingRowColors(false);
  m_commitTree->setContextMenuPolicy(Qt::CustomContextMenu);
  m_commitTree->setUniformRowHeights(true);
  m_commitTree->setFocusPolicy(Qt::StrongFocus);

  m_commitTree->setHeaderLabels(
      {tr("Commit"), tr("Hash"), tr("Author"), tr("Date")});
  m_commitTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_commitTree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
  m_commitTree->header()->resizeSection(1, 80);
  m_commitTree->header()->setSectionResizeMode(2, QHeaderView::Fixed);
  m_commitTree->header()->resizeSection(2, 120);
  m_commitTree->header()->setSectionResizeMode(3, QHeaderView::Fixed);
  m_commitTree->header()->resizeSection(3, 100);

  m_commitTree->setItemDelegateForColumn(
      0, new CommitGraphDelegate(&m_entries, &m_hashToEntryIndex, &m_theme,
                                 m_commitTree));

  layout->addWidget(m_commitTree, 1);

  m_commitStatusLabel = new QLabel(this);
  m_commitStatusLabel->setObjectName("commitStatusLabel");
  auto *statusContainer = new QWidget(this);
  auto *statusLayout = new QHBoxLayout(statusContainer);
  statusLayout->setContentsMargins(12, 4, 12, 4);
  statusLayout->addWidget(m_commitStatusLabel);
  layout->addWidget(statusContainer);

  connect(m_rewriteToggleBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onToggleRewriteMode);
  connect(m_commitTree, &QTreeWidget::itemSelectionChanged, this,
          &GitWorkbenchDialog::onCommitSelectionChanged);
  connect(m_commitTree, &QTreeWidget::customContextMenuRequested, this,
          &GitWorkbenchDialog::onCommitContextMenu);
  connect(m_commitTree, &QTreeWidget::itemDoubleClicked, this,
          &GitWorkbenchDialog::onCommitDoubleClicked);
  connect(m_commitSearch, &QLineEdit::textChanged, this,
          &GitWorkbenchDialog::onCommitSearchChanged);
  connect(m_moveUpBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onMoveUp);
  connect(m_moveDownBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onMoveDown);
  connect(m_squashBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onSquashSelected);
  connect(m_fixupBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onFixupSelected);
  connect(m_rewordBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onRewordSelected);
  connect(m_dropBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onDropSelected);
  connect(m_pickAllBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onPickAll);
}

void GitWorkbenchDialog::buildInspector(QVBoxLayout *layout) {

  auto *header = new QWidget(this);
  header->setObjectName("inspectorHeader");
  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(14, 10, 14, 10);
  auto *headerLabel = new QLabel(tr("INSPECTOR"), this);
  headerLabel->setObjectName("sectionHeader");
  headerLayout->addWidget(headerLabel);
  layout->addWidget(header);

  m_inspectorStack = new QStackedWidget(this);
  m_inspectorStack->setObjectName("inspectorStack");

  auto *emptyPage = new QWidget(this);
  auto *emptyLayout = new QVBoxLayout(emptyPage);
  emptyLayout->setAlignment(Qt::AlignCenter);
  auto *emptyLabel = new QLabel(
      tr("Select a commit or branch\nto view details and actions"), this);
  emptyLabel->setObjectName("inspectorEmpty");
  emptyLabel->setAlignment(Qt::AlignCenter);
  emptyLayout->addWidget(emptyLabel);
  m_inspectorStack->addWidget(emptyPage);

  m_commitInspectorPage = new QWidget(this);
  auto *ciLayout = new QVBoxLayout(m_commitInspectorPage);
  ciLayout->setContentsMargins(14, 12, 14, 12);
  ciLayout->setSpacing(10);

  m_inspCommitHash = new QLabel(this);
  m_inspCommitHash->setObjectName("inspMonoLabel");
  m_inspCommitHash->setTextInteractionFlags(Qt::TextSelectableByMouse);
  ciLayout->addWidget(m_inspCommitHash);

  m_inspCommitAuthor = new QLabel(this);
  m_inspCommitAuthor->setObjectName("inspDetailLabel");
  ciLayout->addWidget(m_inspCommitAuthor);

  m_inspCommitDate = new QLabel(this);
  m_inspCommitDate->setObjectName("inspSubduedLabel");
  ciLayout->addWidget(m_inspCommitDate);

  m_inspCommitParents = new QLabel(this);
  m_inspCommitParents->setObjectName("inspMonoLabel");
  m_inspCommitParents->setWordWrap(true);
  ciLayout->addWidget(m_inspCommitParents);

  m_inspCommitRefs = new QLabel(this);
  m_inspCommitRefs->setObjectName("inspCommitRefs");
  m_inspCommitRefs->setWordWrap(true);
  ciLayout->addWidget(m_inspCommitRefs);

  auto *msgSep = new QFrame(this);
  msgSep->setFrameShape(QFrame::HLine);
  msgSep->setObjectName("inspectorSep");
  ciLayout->addWidget(msgSep);

  m_inspCommitMessage = new QTextEdit(this);
  m_inspCommitMessage->setObjectName("inspCommitMessage");
  m_inspCommitMessage->setReadOnly(true);
  m_inspCommitMessage->setMaximumHeight(100);
  ciLayout->addWidget(m_inspCommitMessage);

  auto *filesLabel = new QLabel(tr("Files Changed"), this);
  filesLabel->setObjectName("inspSectionLabel");
  ciLayout->addWidget(filesLabel);

  m_inspFileList = new QTreeWidget(this);
  m_inspFileList->setObjectName("inspFileList");
  m_inspFileList->setHeaderLabels({tr("File"), tr("+"), tr("−")});
  m_inspFileList->setRootIsDecorated(false);
  m_inspFileList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_inspFileList->header()->setSectionResizeMode(1, QHeaderView::Fixed);
  m_inspFileList->header()->resizeSection(1, 40);
  m_inspFileList->header()->setSectionResizeMode(2, QHeaderView::Fixed);
  m_inspFileList->header()->resizeSection(2, 40);
  m_inspFileList->setMaximumHeight(180);
  ciLayout->addWidget(m_inspFileList);

  m_inspPatchStats = new QLabel(this);
  m_inspPatchStats->setObjectName("inspPatchStats");
  ciLayout->addWidget(m_inspPatchStats);

  auto *actSep = new QFrame(this);
  actSep->setFrameShape(QFrame::HLine);
  actSep->setObjectName("inspectorSep");
  ciLayout->addWidget(actSep);

  auto *actionsLabel = new QLabel(tr("Actions"), this);
  actionsLabel->setObjectName("inspSectionLabel");
  ciLayout->addWidget(actionsLabel);

  m_inspViewDiffBtn = new QPushButton(tr("View Diff"), this);
  m_inspViewDiffBtn->setObjectName("inspActionBtn");
  ciLayout->addWidget(m_inspViewDiffBtn);

  m_inspEditMessageBtn = new QPushButton(tr("Edit Message..."), this);
  m_inspEditMessageBtn->setObjectName("inspActionBtn");
  ciLayout->addWidget(m_inspEditMessageBtn);

  m_inspCherryPickBtn = new QPushButton(tr("Cherry-pick"), this);
  m_inspCherryPickBtn->setObjectName("inspActionBtn");
  ciLayout->addWidget(m_inspCherryPickBtn);

  m_inspMoveToBranchBtn = new QPushButton(tr("Move to Branch..."), this);
  m_inspMoveToBranchBtn->setObjectName("inspActionBtn");
  ciLayout->addWidget(m_inspMoveToBranchBtn);

  m_inspRevertBtn = new QPushButton(tr("Revert"), this);
  m_inspRevertBtn->setObjectName("inspActionBtn");
  ciLayout->addWidget(m_inspRevertBtn);

  ciLayout->addStretch();
  m_inspectorStack->addWidget(m_commitInspectorPage);

  m_branchInspectorPage = new QWidget(this);
  auto *biLayout = new QVBoxLayout(m_branchInspectorPage);
  biLayout->setContentsMargins(14, 12, 14, 12);
  biLayout->setSpacing(10);

  m_inspBranchName = new QLabel(this);
  m_inspBranchName->setObjectName("inspBranchTitle");
  biLayout->addWidget(m_inspBranchName);

  m_inspBranchTip = new QLabel(this);
  m_inspBranchTip->setObjectName("inspMonoLabel");
  biLayout->addWidget(m_inspBranchTip);

  m_inspBranchUpstream = new QLabel(this);
  m_inspBranchUpstream->setObjectName("inspSubduedLabel");
  biLayout->addWidget(m_inspBranchUpstream);

  m_inspBranchAheadBehind = new QLabel(this);
  m_inspBranchAheadBehind->setObjectName("inspDetailLabel");
  biLayout->addWidget(m_inspBranchAheadBehind);

  m_inspBranchActivity = new QLabel(this);
  m_inspBranchActivity->setObjectName("inspBranchActivity");
  m_inspBranchActivity->setWordWrap(true);
  biLayout->addWidget(m_inspBranchActivity);

  auto *brSep = new QFrame(this);
  brSep->setFrameShape(QFrame::HLine);
  brSep->setObjectName("inspectorSep");
  biLayout->addWidget(brSep);

  m_inspBranchCheckoutBtn = new QPushButton(tr("Switch To"), this);
  m_inspBranchCheckoutBtn->setObjectName("inspActionBtn");
  biLayout->addWidget(m_inspBranchCheckoutBtn);

  m_inspBranchRenameBtn = new QPushButton(tr("Rename..."), this);
  m_inspBranchRenameBtn->setObjectName("inspActionBtn");
  biLayout->addWidget(m_inspBranchRenameBtn);

  m_inspBranchMergeBtn = new QPushButton(tr("Merge into Current"), this);
  m_inspBranchMergeBtn->setObjectName("inspActionBtn");
  biLayout->addWidget(m_inspBranchMergeBtn);

  m_inspBranchRebaseBtn = new QPushButton(tr("Rebase onto This"), this);
  m_inspBranchRebaseBtn->setObjectName("inspActionBtn");
  biLayout->addWidget(m_inspBranchRebaseBtn);

  m_inspBranchDeleteBtn = new QPushButton(tr("Delete Branch"), this);
  m_inspBranchDeleteBtn->setObjectName("inspDangerBtn");
  biLayout->addWidget(m_inspBranchDeleteBtn);

  m_inspBranchResetBtn = new QPushButton(tr("Reset to Tip"), this);
  m_inspBranchResetBtn->setObjectName("inspActionBtn");
  biLayout->addWidget(m_inspBranchResetBtn);

  biLayout->addStretch();
  m_inspectorStack->addWidget(m_branchInspectorPage);

  m_planInspectorPage = new QWidget(this);
  auto *piLayout = new QVBoxLayout(m_planInspectorPage);
  piLayout->setContentsMargins(14, 12, 14, 12);
  piLayout->setSpacing(10);

  auto *planTitle = new QLabel(tr("Rewrite Plan"), this);
  planTitle->setObjectName("inspBranchTitle");
  piLayout->addWidget(planTitle);

  m_planStepsList = new QTreeWidget(this);
  m_planStepsList->setObjectName("planStepsList");
  m_planStepsList->setHeaderLabels({tr("Step"), tr("Action"), tr("Commit")});
  m_planStepsList->setRootIsDecorated(false);
  m_planStepsList->header()->setSectionResizeMode(0, QHeaderView::Fixed);
  m_planStepsList->header()->resizeSection(0, 40);
  m_planStepsList->header()->setSectionResizeMode(1, QHeaderView::Fixed);
  m_planStepsList->header()->resizeSection(1, 70);
  m_planStepsList->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  piLayout->addWidget(m_planStepsList, 1);

  m_planRiskLabel = new QLabel(this);
  m_planRiskLabel->setObjectName("planRiskLabel");
  piLayout->addWidget(m_planRiskLabel);

  m_planAffectedRefs = new QLabel(this);
  m_planAffectedRefs->setObjectName("inspSubduedLabel");
  m_planAffectedRefs->setWordWrap(true);
  piLayout->addWidget(m_planAffectedRefs);

  m_planRecoveryInfo = new QLabel(this);
  m_planRecoveryInfo->setObjectName("inspSubduedLabel");
  m_planRecoveryInfo->setWordWrap(true);
  piLayout->addWidget(m_planRecoveryInfo);

  piLayout->addStretch();
  m_inspectorStack->addWidget(m_planInspectorPage);

  m_recoveryCenterPage = new QWidget(this);
  auto *rcLayout = new QVBoxLayout(m_recoveryCenterPage);
  rcLayout->setContentsMargins(14, 12, 14, 12);
  rcLayout->setSpacing(10);

  auto *rcTitle = new QLabel(tr("Recovery Center"), this);
  rcTitle->setObjectName("inspBranchTitle");
  rcLayout->addWidget(rcTitle);

  auto *rcDesc = new QLabel(
      tr("Browse safety backups and recent reflog entries. "
         "Select an entry and restore to recover from destructive operations."),
      this);
  rcDesc->setObjectName("inspSubduedLabel");
  rcDesc->setWordWrap(true);
  rcLayout->addWidget(rcDesc);

  m_recoveryList = new QTreeWidget(this);
  m_recoveryList->setObjectName("recoveryList");
  m_recoveryList->setHeaderLabels({tr("Type"), tr("Ref / Action"), tr("Age")});
  m_recoveryList->setRootIsDecorated(false);
  m_recoveryList->header()->setSectionResizeMode(0, QHeaderView::Fixed);
  m_recoveryList->header()->resizeSection(0, 60);
  m_recoveryList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_recoveryList->header()->setSectionResizeMode(2, QHeaderView::Fixed);
  m_recoveryList->header()->resizeSection(2, 80);
  rcLayout->addWidget(m_recoveryList, 1);

  m_recoveryStatusLabel = new QLabel(this);
  m_recoveryStatusLabel->setObjectName("inspSubduedLabel");
  rcLayout->addWidget(m_recoveryStatusLabel);

  m_recoveryRestoreBtn = new QPushButton(tr("Restore Selected"), this);
  m_recoveryRestoreBtn->setObjectName("inspActionBtn");
  m_recoveryRestoreBtn->setEnabled(false);
  rcLayout->addWidget(m_recoveryRestoreBtn);

  rcLayout->addStretch();
  m_inspectorStack->addWidget(m_recoveryCenterPage);

  m_stashInspectorPage = new QWidget(this);
  auto *siLayout = new QVBoxLayout(m_stashInspectorPage);
  siLayout->setContentsMargins(14, 12, 14, 12);
  siLayout->setSpacing(10);

  auto *stashTitle = new QLabel(tr("Stash Details"), this);
  stashTitle->setObjectName("inspBranchTitle");
  siLayout->addWidget(stashTitle);

  m_inspStashRef = new QLabel(this);
  m_inspStashRef->setObjectName("inspMonoLabel");
  m_inspStashRef->setTextInteractionFlags(Qt::TextSelectableByMouse);
  siLayout->addWidget(m_inspStashRef);

  m_inspStashMessage = new QLabel(this);
  m_inspStashMessage->setObjectName("inspDetailLabel");
  m_inspStashMessage->setWordWrap(true);
  siLayout->addWidget(m_inspStashMessage);

  m_inspStashBranch = new QLabel(this);
  m_inspStashBranch->setObjectName("inspSubduedLabel");
  siLayout->addWidget(m_inspStashBranch);

  m_inspStashHash = new QLabel(this);
  m_inspStashHash->setObjectName("inspMonoLabel");
  m_inspStashHash->setTextInteractionFlags(Qt::TextSelectableByMouse);
  siLayout->addWidget(m_inspStashHash);

  auto *stashSep = new QFrame(this);
  stashSep->setFrameShape(QFrame::HLine);
  stashSep->setObjectName("inspectorSep");
  siLayout->addWidget(stashSep);

  auto *stashActionsLabel = new QLabel(tr("Actions"), this);
  stashActionsLabel->setObjectName("inspSectionLabel");
  siLayout->addWidget(stashActionsLabel);

  m_inspStashApplyBtn = new QPushButton(tr("Apply"), this);
  m_inspStashApplyBtn->setObjectName("inspActionBtn");
  siLayout->addWidget(m_inspStashApplyBtn);

  m_inspStashPopBtn = new QPushButton(tr("Pop"), this);
  m_inspStashPopBtn->setObjectName("inspActionBtn");
  siLayout->addWidget(m_inspStashPopBtn);

  m_inspStashDropBtn = new QPushButton(tr("Drop"), this);
  m_inspStashDropBtn->setObjectName("inspDangerBtn");
  siLayout->addWidget(m_inspStashDropBtn);

  siLayout->addStretch();
  m_inspectorStack->addWidget(m_stashInspectorPage);

  layout->addWidget(m_inspectorStack, 1);

  connect(m_inspViewDiffBtn, &QPushButton::clicked, [this]() {
    if (!m_selectedCommitHash.isEmpty())
      emit viewCommitDiff(m_selectedCommitHash);
  });
  connect(m_inspEditMessageBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onEditCommitMessage);
  connect(m_inspCherryPickBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onCherryPickClicked);
  connect(m_inspMoveToBranchBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onMoveToBranchClicked);
  connect(m_inspRevertBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onRevertCommitClicked);
  connect(m_inspBranchCheckoutBtn, &QPushButton::clicked, [this]() {
    if (!m_selectedBranchName.isEmpty() && m_git) {
      m_git->checkoutBranch(m_selectedBranchName);
      loadRepository();
    }
  });
  connect(m_inspBranchDeleteBtn, &QPushButton::clicked, [this]() {
    if (!m_selectedBranchName.isEmpty() && m_git) {
      if (confirmOperation(
              tr("Delete Branch"),
              tr("Delete branch '%1'? This cannot be undone for unmerged work.")
                  .arg(m_selectedBranchName),
              OperationRisk::High, false)) {
        m_git->deleteBranch(m_selectedBranchName);
        loadRepository();
      }
    }
  });
  connect(m_inspBranchRenameBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onRenameBranchClicked);
  connect(m_inspBranchMergeBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onMergeBranchClicked);
  connect(m_inspBranchRebaseBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onRebaseBranchClicked);
  connect(m_inspBranchResetBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onResetToBranchClicked);
  connect(m_recoveryRestoreBtn, &QPushButton::clicked, [this]() {
    auto *item = m_recoveryList->currentItem();
    if (!item)
      return;
    QString ref = item->data(1, Qt::UserRole).toString();
    if (ref.isEmpty())
      return;
    if (confirmOperation(
            tr("Restore"),
            tr("Reset current branch to '%1'? This rewrites history.").arg(ref),
            OperationRisk::High)) {
      m_git->resetToCommit(ref, "mixed");
      loadRepository();
    }
  });
  connect(m_recoveryList, &QTreeWidget::currentItemChanged,
          [this](QTreeWidgetItem *item) {
            m_recoveryRestoreBtn->setEnabled(
                item != nullptr &&
                !item->data(1, Qt::UserRole).toString().isEmpty());
          });
  connect(m_inspStashApplyBtn, &QPushButton::clicked, [this]() {
    QString ref = m_inspStashRef->text();
    QRegularExpression re("stash@\\{(\\d+)\\}");
    auto match = re.match(ref);
    if (!match.hasMatch() || !m_git)
      return;
    int idx = match.captured(1).toInt();
    m_git->stashApply(idx);
    loadRepository();
  });
  connect(m_inspStashPopBtn, &QPushButton::clicked, [this]() {
    QString ref = m_inspStashRef->text();
    QRegularExpression re("stash@\\{(\\d+)\\}");
    auto match = re.match(ref);
    if (!match.hasMatch() || !m_git)
      return;
    int idx = match.captured(1).toInt();
    if (confirmOperation(
            tr("Pop Stash"),
            tr("Pop %1? This removes the stash entry after applying.").arg(ref),
            OperationRisk::Medium, false)) {
      m_git->stashPop(idx);
      loadRepository();
    }
  });
  connect(m_inspStashDropBtn, &QPushButton::clicked, [this]() {
    QString ref = m_inspStashRef->text();
    QRegularExpression re("stash@\\{(\\d+)\\}");
    auto match = re.match(ref);
    if (!match.hasMatch() || !m_git)
      return;
    int idx = match.captured(1).toInt();
    if (confirmOperation(
            tr("Drop Stash"),
            tr("Drop %1? This permanently deletes the stash.").arg(ref),
            OperationRisk::High, false)) {
      m_git->stashDrop(idx);
      loadRepository();
    }
  });
}

void GitWorkbenchDialog::buildBottomBar(QVBoxLayout *mainLayout) {
  auto *bottomSep = new QFrame(this);
  bottomSep->setFrameShape(QFrame::HLine);
  bottomSep->setObjectName("workbenchSeparator");
  mainLayout->addWidget(bottomSep);

  m_bottomBar = new QWidget(this);
  m_bottomBar->setObjectName("workbenchBottomBar");
  auto *barLayout = new QHBoxLayout(m_bottomBar);
  barLayout->setContentsMargins(20, 10, 20, 10);
  barLayout->setSpacing(14);

  m_planSummaryLabel = new QLabel(this);
  m_planSummaryLabel->setObjectName("planSummaryLabel");
  m_planSummaryLabel->setTextFormat(Qt::RichText);
  barLayout->addWidget(m_planSummaryLabel);

  m_riskIndicator = new QLabel(this);
  m_riskIndicator->setObjectName("riskIndicator");
  barLayout->addWidget(m_riskIndicator);

  barLayout->addStretch();

  m_backupCheckbox = new QCheckBox(tr("Create safety backup ref"), this);
  m_backupCheckbox->setObjectName("backupCheckbox");
  m_backupCheckbox->setChecked(true);
  m_backupCheckbox->setVisible(false);
  barLayout->addWidget(m_backupCheckbox);

  m_cancelBtn = new QPushButton(tr("Close"), this);
  m_cancelBtn->setObjectName("workbenchCancelBtn");
  barLayout->addWidget(m_cancelBtn);

  m_applyBtn = new QPushButton(tr("Apply Rewrite"), this);
  m_applyBtn->setObjectName("workbenchApplyBtn");
  m_applyBtn->setVisible(false);
  barLayout->addWidget(m_applyBtn);

  mainLayout->addWidget(m_bottomBar);

  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_applyBtn, &QPushButton::clicked, this,
          &GitWorkbenchDialog::onApplyRebase);
}

void GitWorkbenchDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);

  QString bg = theme.backgroundColor.name();
  QString fg = theme.foregroundColor.name();
  QString surface = theme.surfaceColor.name();
  QString surfaceAlt = theme.surfaceAltColor.name();
  QString border = theme.borderColor.name();
  QString accent = theme.accentColor.name();
  QString accentSoft = theme.accentSoftColor.name();
  QString hover = theme.hoverColor.name();
  QString subdued = theme.singleLineCommentFormat.name();
  QString success = theme.successColor.name();
  QString warning = theme.warningColor.name();
  QString error = theme.errorColor.name();

  setStyleSheet(
      QString(

          "QDialog { background: %1; color: %2; font-size: 13px; }"

          "#workbenchTitleBar { background: %3; }"
          "#workbenchTitle { font-size: 17px; font-weight: 600; color: %2; "
          "letter-spacing: -0.3px; }"
          "#workbenchShortcutHint { font-size: 11px; color: %11; "
          "letter-spacing: 0.5px; }"

          "#workbenchSeparator, #inspectorSep { color: %5; background: %5; "
          "max-height: 1px; }"

          "#sectionHeader { font-size: 11px; font-weight: 600; color: %11; "
          "letter-spacing: 1.5px; text-transform: uppercase; }"

          "#branchPanel, #commitPanel, #inspectorPanel { background: %1; }"
          "#branchHeader, #commitHeader, #inspectorHeader { background: %3; "
          "border-bottom: 1px solid %5; }"

          "#branchSearch, #commitSearch { background: %4; color: %2; border: "
          "1px "
          "solid %5; border-radius: 6px; padding: 7px 12px; font-size: 12px; }"
          "#branchSearch:focus, #commitSearch:focus { border-color: %6; }"

          "#branchTree { background: %1; color: %2; border: none; outline: "
          "none; "
          "font-size: 13px; }"
          "#branchTree::item { padding: 5px 10px; border-radius: 6px; margin: "
          "1px "
          "6px; }"
          "#branchTree::item:selected { background: %7; "
          "border-left: 3px solid %6; }"
          "#branchTree::item:hover:!selected { background: %8; }"
          "#branchTree::branch { background: transparent; }"

          "#commitTree { background: %1; color: %2; border: none; outline: "
          "none; "
          "font-size: 13px; }"
          "#commitTree::item { padding: 3px 6px; border-bottom: 1px solid %4; }"
          "#commitTree::item:selected { background: %7; "
          "border-left: 3px solid %6; }"
          "#commitTree::item:hover:!selected { background: %8; }"
          "#commitTree QHeaderView::section { background: %3; color: %11; "
          "border: "
          "none; border-bottom: 1px solid %5; padding: 6px 10px; font-size: "
          "11px; "
          "font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; "
          "}"

          "#createBranchBtn { background: %4; color: %2; border: 1px solid %5; "
          "border-radius: 6px; font-size: 15px; font-weight: bold; }"
          "#createBranchBtn:hover { background: %8; border-color: %6; }"

          "#rewriteToggleBtn { background: %4; color: %2; border: 1px solid "
          "%5; "
          "border-radius: 12px; padding: 5px 16px; font-size: 12px; "
          "font-weight: 500; }"
          "#rewriteToggleBtn:hover { border-color: %6; background: %8; }"
          "#rewriteToggleBtn:checked { background: %6; color: white; "
          "border-color: "
          "%6; }"

          "#rewriteToolBtn { background: %4; color: %2; border: 1px solid %5; "
          "border-radius: 6px; padding: 5px 12px; font-size: 12px; }"
          "#rewriteToolBtn:hover { background: %8; border-color: %6; }"
          "#rewriteDropBtn { background: %4; color: %13; border: 1px solid %5; "
          "border-radius: 6px; padding: 5px 12px; font-size: 12px; }"
          "#rewriteDropBtn:hover { background: %13; color: white; "
          "border-color: "
          "%13; }"

          "#rewriteToolbarContainer { background: %3; border-bottom: 1px solid "
          "%5; "
          "}"

          "#selectionBar { background: %7; border-bottom: 1px solid %5; }"
          "#selectionCountLabel { font-size: 13px; color: %6; padding: 2px "
          "6px; }"
          "#selectionHintLabel { font-size: 11px; color: %11; }"

          "#commitStatusLabel { font-size: 12px; color: %11; }"

          "#inspectorEmpty { font-size: 14px; color: %11; padding: 24px; }"

          "#inspMonoLabel { font-family: monospace; font-size: 12px; color: "
          "%6; }"
          "#inspDetailLabel { font-size: 13px; color: %2; }"
          "#inspSubduedLabel { font-size: 12px; color: %11; }"
          "#inspCommitRefs { font-size: 12px; color: %6; }"
          "#inspBranchActivity { font-size: 12px; color: %11; }"
          "#inspSectionLabel { font-size: 11px; font-weight: 600; color: %11; "
          "text-transform: uppercase; letter-spacing: 1px; margin-top: 8px; }"
          "#inspBranchTitle { font-size: 15px; font-weight: 600; color: %2; }"
          "#inspPatchStats { font-size: 12px; color: %11; }"

          "#inspCommitMessage { background: %4; color: %2; border: 1px solid "
          "%5; "
          "border-radius: 6px; padding: 8px; font-size: 12px; }"

          "#inspFileList { background: %1; color: %2; border: 1px solid %5; "
          "border-radius: 6px; font-size: 12px; }"
          "#inspFileList::item { padding: 3px 4px; }"
          "#inspFileList QHeaderView::section { background: %3; color: %11; "
          "border: none; font-size: 11px; padding: 4px; }"

          "#inspActionBtn { background: %4; color: %2; border: 1px solid %5; "
          "border-radius: 6px; padding: 8px 14px; font-size: 12px; text-align: "
          "left; }"
          "#inspActionBtn:hover { background: %8; border-color: %6; }"
          "#inspDangerBtn { background: %4; color: %13; border: 1px solid %5; "
          "border-radius: 6px; padding: 8px 14px; font-size: 12px; text-align: "
          "left; }"
          "#inspDangerBtn:hover { background: %13; color: white; }"

          "#planStepsList, #recoveryList { background: %1; color: %2; "
          "border: 1px solid %5; border-radius: 6px; font-size: 12px; }"
          "#planStepsList QHeaderView::section, "
          "#recoveryList QHeaderView::section { background: %3; color: %11; "
          "border: none; font-size: 11px; padding: 4px; }"
          "#planRiskLabel { font-size: 13px; font-weight: 600; padding: 6px 0; "
          "}"

          "#workbenchBottomBar { background: %3; }"
          "#planSummaryLabel { font-size: 12px; color: %11; }"
          "#riskIndicator { font-size: 12px; }"

          "#backupCheckbox { color: %2; font-size: 12px; }"
          "#backupCheckbox::indicator { width: 16px; height: 16px; "
          "border-radius: "
          "4px; border: 1px solid %5; background: %4; }"
          "#backupCheckbox::indicator:checked { background: %12; border-color: "
          "%12; }"

          "QScrollBar:vertical { background: transparent; width: 8px; margin: "
          "2px; }"
          "QScrollBar::handle:vertical { background: %5; min-height: 24px; "
          "border-radius: 4px; }"
          "QScrollBar::handle:vertical:hover { background: %11; }"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
          "height: "
          "0; }"
          "QScrollBar:horizontal { background: transparent; height: 8px; "
          "margin: "
          "2px; }"
          "QScrollBar::handle:horizontal { background: %5; min-width: 24px; "
          "border-radius: 4px; }"
          "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
          "width: 0; }"

          "QSplitter::handle { background: %5; }")
          .arg(bg)
          .arg(fg)
          .arg(surface)
          .arg(surfaceAlt)
          .arg(border)
          .arg(accent)
          .arg(accentSoft)
          .arg(hover)
          .arg("")
          .arg("")
          .arg(subdued)
          .arg(success)
          .arg(error));

  for (auto *btn : findChildren<QPushButton *>())
    btn->setStyleSheet(QString());
  for (auto *le : findChildren<QLineEdit *>())
    le->setStyleSheet(QString());
  for (auto *te : findChildren<QTextEdit *>())
    te->setStyleSheet(QString());
  for (auto *cb : findChildren<QCheckBox *>())
    cb->setStyleSheet(QString());

  stylePrimaryButton(m_applyBtn);
  m_applyBtn->setStyleSheet(
      m_applyBtn->styleSheet() +
      QString(" QPushButton:disabled { background: %1; color: %2; }")
          .arg(surfaceAlt, border));
  styleSecondaryButton(m_cancelBtn);
}

void GitWorkbenchDialog::loadRepository() {
  if (!m_git || !m_git->isValidRepository())
    return;

  m_currentBranch = m_git->currentBranch();
  loadBranches();
  loadCommits();
  updatePlanSummary();
  clearInspector();
}

void GitWorkbenchDialog::loadBranches() {
  m_branchTree->clear();
  if (!m_git)
    return;

  m_branches = m_git->getBranches();

  auto *localGroup = new QTreeWidgetItem(m_branchTree, {tr("Local Branches")});
  localGroup->setFlags(localGroup->flags() & ~Qt::ItemIsSelectable);
  localGroup->setExpanded(true);
  QFont groupFont = localGroup->font(0);
  groupFont.setBold(true);
  groupFont.setPointSize(11);
  localGroup->setFont(0, groupFont);

  auto *remoteGroup =
      new QTreeWidgetItem(m_branchTree, {tr("Remote Branches")});
  remoteGroup->setFlags(remoteGroup->flags() & ~Qt::ItemIsSelectable);
  remoteGroup->setExpanded(false);
  remoteGroup->setFont(0, groupFont);

  for (const auto &branch : m_branches) {
    auto *parent = branch.isRemote ? remoteGroup : localGroup;
    auto *item = new QTreeWidgetItem(parent);

    QString display = branch.name;
    if (branch.isCurrent)
      display = "● " + display;

    QString badge;
    if (branch.aheadCount > 0)
      badge += "↑" + QString::number(branch.aheadCount);
    if (branch.behindCount > 0) {
      if (!badge.isEmpty())
        badge += " ";
      badge += "↓" + QString::number(branch.behindCount);
    }
    if (!badge.isEmpty())
      display += "  " + badge;

    item->setText(0, display);
    item->setToolTip(
        0, branch.name + (badge.isEmpty() ? "" : " (" + badge.trimmed() + ")"));

    item->setData(0, Qt::UserRole, branch.name);
    item->setData(0, Qt::UserRole + 1, branch.isRemote);
    item->setData(0, Qt::UserRole + 2, branch.isCurrent);

    if (branch.isCurrent) {
      QFont f = item->font(0);
      f.setBold(true);
      item->setFont(0, f);
    }
  }

  loadStashes();

  loadTags();
}

void GitWorkbenchDialog::loadStashes() {
  if (!m_git)
    return;

  m_stashes = m_git->getStashList();
  if (m_stashes.isEmpty())
    return;

  QFont groupFont;
  groupFont.setBold(true);
  groupFont.setPointSize(10);

  auto *stashGroup = new QTreeWidgetItem(m_branchTree, {tr("Stashes")});
  stashGroup->setFlags(stashGroup->flags() & ~Qt::ItemIsSelectable);
  stashGroup->setExpanded(false);
  stashGroup->setFont(0, groupFont);

  for (const auto &stash : m_stashes) {
    auto *item = new QTreeWidgetItem(stashGroup);
    item->setText(
        0, QString("stash@{%1}: %2").arg(stash.index).arg(stash.message));
    item->setData(0, Qt::UserRole, QString("stash@{%1}").arg(stash.index));
    item->setData(0, Qt::UserRole + 1, true);
    item->setFlags(item->flags() | Qt::ItemIsSelectable);
  }
}

void GitWorkbenchDialog::loadTags() {
  if (!m_git)
    return;

  m_tags = m_git->getTags();
  if (m_tags.isEmpty())
    return;

  QFont groupFont;
  groupFont.setBold(true);
  groupFont.setPointSize(10);

  auto *tagGroup = new QTreeWidgetItem(m_branchTree, {tr("Tags")});
  tagGroup->setFlags(tagGroup->flags() & ~Qt::ItemIsSelectable);
  tagGroup->setExpanded(false);
  tagGroup->setFont(0, groupFont);

  for (const auto &tag : m_tags) {
    auto *item = new QTreeWidgetItem(tagGroup);
    item->setText(0, (tag.isAnnotated ? "🏷 " : "") + tag.name);
    item->setData(0, Qt::UserRole, tag.name);
    item->setData(0, Qt::UserRole + 1, true);
    item->setToolTip(0, tag.hash.left(8));
  }
}

void GitWorkbenchDialog::loadCommits(const QString &branch) {
  m_commitTree->clear();
  m_entries.clear();
  m_hashToEntryIndex.clear();
  m_selectedCommitHash.clear();
  m_commitRefsCache.clear();

  if (!m_git || !m_git->isValidRepository())
    return;

  QList<GitCommitInfo> commits = m_git->getCommitLog(100, branch);

  for (const auto &c : commits) {
    QStringList refs = m_git->getCommitRefs(c.hash);
    if (!refs.isEmpty())
      m_commitRefsCache[c.hash] = refs;
  }

  for (int i = 0; i < commits.size(); ++i) {
    const auto &c = commits[i];

    WorkbenchRebaseEntry entry;
    entry.action = "pick";
    entry.hash = c.hash;
    entry.shortHash = c.shortHash;
    entry.subject = c.subject;
    entry.author = c.author;
    entry.authorEmail = c.authorEmail;
    entry.date = c.date;
    entry.relativeDate = c.relativeDate;
    entry.parents = c.parents;
    entry.refs = m_commitRefsCache.value(c.hash);
    m_entries.append(entry);
    m_hashToEntryIndex[c.hash] = i;

    auto *item = new QTreeWidgetItem(m_commitTree);
    populateCommitRow(item, i);
  }

  m_commitStatusLabel->setText(
      tr("%1 commits on %2").arg(commits.size()).arg(m_currentBranch));
}

void GitWorkbenchDialog::populateCommitRow(QTreeWidgetItem *item, int index) {
  if (index < 0 || index >= m_entries.size())
    return;

  const auto &entry = m_entries[index];

  item->setData(0, Qt::UserRole, index);
  item->setData(0, Qt::UserRole + 1, entry.hash);

  item->setToolTip(0, entry.subject);

  item->setText(1, entry.shortHash);
  QFont monoFont = item->font(1);
  monoFont.setFamily("monospace");
  monoFont.setPointSize(10);
  item->setFont(1, monoFont);

  item->setText(2, entry.author);

  item->setText(3, entry.relativeDate);

  if (m_rewriteMode) {
    rebuildActionCombo(item, index);
  }
}

void GitWorkbenchDialog::rebuildActionCombo(QTreeWidgetItem *item, int index) {
  if (index < 0 || index >= m_entries.size())
    return;

  auto *combo = new QComboBox(m_commitTree);
  combo->addItems(REBASE_ACTIONS_EXTENDED);
  combo->setCurrentText(m_entries[index].action);

  QString color = actionColor(m_entries[index].action);
  combo->setStyleSheet(
      QString("QComboBox { background: %1; color: %2; border: 1px solid %3; "
              "border-radius: 3px; padding: 1px 4px; font-weight: bold; "
              "font-size: 10px; min-width: 60px; }"
              "QComboBox:hover { border-color: %4; }"
              "QComboBox::drop-down { border: none; width: 14px; }"
              "QComboBox::down-arrow { image: none; border-left: 3px solid "
              "transparent; border-right: 3px solid transparent; border-top: "
              "4px solid %2; }"
              "QComboBox QAbstractItemView { background: %1; color: %5; "
              "selection-background-color: %4; border: 1px solid %3; }")
          .arg(m_theme.surfaceAltColor.name(), color,
               m_theme.borderColor.name(), m_theme.accentColor.name(),
               m_theme.foregroundColor.name()));

  m_commitTree->setItemWidget(item, 4, combo);

  connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this, item](int idx) {
            int row = item->data(0, Qt::UserRole).toInt();
            if (row >= 0 && row < m_entries.size() && idx >= 0 &&
                idx < REBASE_ACTIONS_EXTENDED.size()) {
              m_entries[row].action = REBASE_ACTIONS_EXTENDED[idx];
              rebuildActionCombo(item, row);
              updatePlanSummary();
              if (m_rewriteMode)
                showPlanInspector();
            }
          });
}

void GitWorkbenchDialog::onToggleRewriteMode() {
  m_rewriteMode = !m_rewriteMode;
  m_rewriteToggleBtn->setChecked(m_rewriteMode);

  auto *toolbarContainer = findChild<QWidget *>("rewriteToolbarContainer");
  if (toolbarContainer)
    toolbarContainer->setVisible(m_rewriteMode);

  m_applyBtn->setVisible(m_rewriteMode);
  m_backupCheckbox->setVisible(m_rewriteMode);

  if (m_rewriteMode) {

    m_commitTree->setHeaderLabels(
        {tr("Commit"), tr("Hash"), tr("Author"), tr("Date"), tr("Action")});
    m_commitTree->header()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_commitTree->header()->resizeSection(4, 80);

    m_commitTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_commitTree->setDragEnabled(true);
    m_commitTree->setAcceptDrops(true);
    m_commitTree->setDropIndicatorShown(true);
    m_commitTree->setDefaultDropAction(Qt::MoveAction);

    int maxRewrite = qMin(m_entries.size(), 30);
    for (int i = 0; i < maxRewrite; ++i) {
      auto *item = m_commitTree->topLevelItem(i);
      if (item)
        rebuildActionCombo(item, i);
    }

    for (int i = 30; i < m_commitTree->topLevelItemCount(); ++i) {
      auto *item = m_commitTree->topLevelItem(i);
      if (item)
        item->setHidden(true);
    }

    connect(
        m_commitTree->model(), &QAbstractItemModel::rowsMoved, this, [this]() {
          syncEntriesFromTree();
          int count = qMin(m_entries.size(), m_commitTree->topLevelItemCount());
          for (int i = 0; i < count; ++i) {
            rebuildActionCombo(m_commitTree->topLevelItem(i), i);
          }
          updatePlanSummary();
          showPlanInspector();
        });

    showPlanInspector();
    m_commitStatusLabel->setText(
        tr("Rewrite mode — drag to reorder, use S/D/R keys or toolbar"));

    auto *selHint = m_selectionBar->findChild<QLabel *>("selectionHintLabel");
    if (selHint)
      selHint->setText(
          tr("S squash  \xc2\xb7  D drop  \xc2\xb7  F fixup  \xc2\xb7  "
             "R reword  \xc2\xb7  Right-click for more"));
    updateSelectionUI();
  } else {

    m_commitTree->setHeaderLabels(
        {tr("Commit"), tr("Hash"), tr("Author"), tr("Date")});

    m_commitTree->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_commitTree->setDragEnabled(false);

    for (auto &entry : m_entries)
      entry.action = "pick";

    for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
      m_commitTree->topLevelItem(i)->setHidden(false);
    }

    for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
      m_commitTree->removeItemWidget(m_commitTree->topLevelItem(i), 4);
    }

    m_commitStatusLabel->setText(
        tr("%1 commits on %2").arg(m_entries.size()).arg(m_currentBranch));
    clearInspector();

    auto *selHint = m_selectionBar->findChild<QLabel *>("selectionHintLabel");
    if (selHint)
      selHint->setText(tr("Shift+J/K to extend  \xc2\xb7  "
                          "Enter Rewrite Mode (R) to apply actions"));

    m_squashBtn->setText(tr("Squash"));
    m_fixupBtn->setText(tr("Fixup"));
    m_dropBtn->setText(tr("Drop"));
    m_rewordBtn->setText(tr("Reword"));
    updateSelectionUI();
  }

  updatePlanSummary();
}

void GitWorkbenchDialog::syncEntriesFromTree() {
  QList<WorkbenchRebaseEntry> reordered;
  for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
    auto *item = m_commitTree->topLevelItem(i);
    int origIdx = item->data(0, Qt::UserRole).toInt();
    if (origIdx >= 0 && origIdx < m_entries.size()) {
      reordered.append(m_entries[origIdx]);
    }
  }
  m_entries = reordered;
  m_hashToEntryIndex.clear();
  for (int i = 0; i < m_entries.size(); ++i) {
    m_hashToEntryIndex[m_entries[i].hash] = i;
    auto *item = m_commitTree->topLevelItem(i);
    if (item)
      item->setData(0, Qt::UserRole, i);
  }
}

void GitWorkbenchDialog::updatePlanSummary() {
  if (!m_rewriteMode) {
    m_planSummaryLabel->setText(
        tr("Browse mode — select commits or branches to inspect"));
    m_riskIndicator->clear();
    return;
  }

  int pick = 0, reword = 0, edit = 0, squash = 0, fixup = 0, drop = 0,
      dropKeep = 0;
  int limit = qMin(m_entries.size(), 30);
  for (int i = 0; i < limit; ++i) {
    const auto &action = m_entries[i].action;
    if (action == "pick")
      ++pick;
    else if (action == "reword")
      ++reword;
    else if (action == "edit")
      ++edit;
    else if (action == "squash")
      ++squash;
    else if (action == "fixup")
      ++fixup;
    else if (action == "drop")
      ++drop;
    else if (action == "drop-keep")
      ++dropKeep;
  }

  QStringList parts;
  auto colorSpan = [](const QString &color, int count, const QString &label) {
    return QString("<span style='color:%1'>%2 %3</span>")
        .arg(color)
        .arg(count)
        .arg(label);
  };

  if (pick > 0)
    parts << colorSpan(actionColor("pick"), pick, "pick");
  if (reword > 0)
    parts << colorSpan(actionColor("reword"), reword, "reword");
  if (edit > 0)
    parts << colorSpan(actionColor("edit"), edit, "edit");
  if (squash > 0)
    parts << colorSpan(actionColor("squash"), squash, "squash");
  if (fixup > 0)
    parts << colorSpan(actionColor("fixup"), fixup, "fixup");
  if (drop > 0)
    parts << colorSpan(actionColor("drop"), drop, "drop");
  if (dropKeep > 0)
    parts << colorSpan(actionColor("drop-keep"), dropKeep, "drop-keep");

  m_planSummaryLabel->setText(
      tr("Plan: %1 commits — %2")
          .arg(limit)
          .arg(parts.isEmpty() ? tr("none") : parts.join(" · ")));

  OperationRisk risk = assessRebaseRisk();
  m_riskIndicator->setText(QString("<span style='color:%1'>● %2</span>")
                               .arg(riskColor(risk), riskLabel(risk)));
  m_riskIndicator->setTextFormat(Qt::RichText);

  bool hasWork = reword > 0 || edit > 0 || squash > 0 || fixup > 0 ||
                 drop > 0 || dropKeep > 0 || pick < limit;
  m_applyBtn->setEnabled(hasWork && limit > 0);
}

void GitWorkbenchDialog::showCommitInspector(const QString &hash) {
  if (!m_git || hash.isEmpty())
    return;

  m_selectedCommitHash = hash;

  GitCommitInfo details = m_git->getCommitDetails(hash);
  if (details.hash.isEmpty()) {
    clearInspector();
    return;
  }

  m_inspCommitHash->setText(details.hash);
  m_inspCommitAuthor->setText(
      tr("%1 <%2>").arg(details.author, details.authorEmail));
  m_inspCommitDate->setText(
      tr("%1 (%2)").arg(details.relativeDate, details.date));
  m_inspCommitParents->setText(
      details.parents.isEmpty()
          ? tr("Root commit")
          : tr("Parents: %1").arg(details.parents.join(", ")));

  QStringList refs = m_commitRefsCache.value(hash);
  if (refs.isEmpty())
    refs = m_git->getCommitRefs(hash);
  if (refs.isEmpty()) {
    m_inspCommitRefs->clear();
    m_inspCommitRefs->setVisible(false);
  } else {
    m_inspCommitRefs->setText(tr("Refs: %1").arg(refs.join(", ")));
    m_inspCommitRefs->setVisible(true);
  }

  QString fullMessage = details.subject;
  if (!details.body.isEmpty())
    fullMessage += "\n\n" + details.body;
  m_inspCommitMessage->setPlainText(fullMessage);

  m_inspFileList->clear();
  QList<GitCommitFileStat> stats = m_git->getCommitFileStats(hash);
  int totalAdd = 0, totalDel = 0;
  for (const auto &fs : stats) {
    auto *fileItem = new QTreeWidgetItem(m_inspFileList);
    fileItem->setText(0, fs.filePath);
    fileItem->setText(1, QString("+%1").arg(fs.additions));
    fileItem->setText(2, QString("-%1").arg(fs.deletions));
    fileItem->setForeground(1, m_theme.successColor);
    fileItem->setForeground(2, m_theme.errorColor);
    totalAdd += fs.additions;
    totalDel += fs.deletions;
  }

  m_inspPatchStats->setText(
      tr("%1 file(s) · <span style='color:%4'>+%2</span> "
         "<span style='color:%5'>-%3</span>")
          .arg(stats.size())
          .arg(totalAdd)
          .arg(totalDel)
          .arg(m_theme.successColor.name(), m_theme.errorColor.name()));
  m_inspPatchStats->setTextFormat(Qt::RichText);

  m_inspectorStack->setCurrentIndex(1);
}

void GitWorkbenchDialog::showBranchInspector(const QString &branchName) {
  m_selectedBranchName = branchName;

  GitBranchInfo info;
  bool found = false;
  for (const auto &b : m_branches) {
    if (b.name == branchName) {
      info = b;
      found = true;
      break;
    }
  }

  if (!found) {
    clearInspector();
    return;
  }

  m_inspBranchName->setText((info.isCurrent ? "● " : "") + info.name);
  m_inspBranchTip->setText(info.trackingBranch.isEmpty()
                               ? tr("No upstream tracking")
                               : tr("Tracks: %1").arg(info.trackingBranch));
  m_inspBranchUpstream->setText(info.isRemote ? tr("Remote branch")
                                              : tr("Local branch"));

  QString aheadBehind;
  if (info.aheadCount > 0 || info.behindCount > 0) {
    if (info.aheadCount > 0)
      aheadBehind += tr("↑%1 ahead").arg(info.aheadCount);
    if (info.behindCount > 0) {
      if (!aheadBehind.isEmpty())
        aheadBehind += "  ";
      aheadBehind += tr("↓%1 behind").arg(info.behindCount);
    }
  } else {
    aheadBehind = tr("Up to date");
  }
  m_inspBranchAheadBehind->setText(aheadBehind);

  auto recentCommits = m_git->getCommitLog(5, branchName);
  if (!recentCommits.isEmpty()) {
    QString activityText;
    for (const auto &c : recentCommits) {
      if (!activityText.isEmpty())
        activityText += "\n";
      activityText += c.shortHash + " " + c.subject;
    }
    m_inspBranchActivity->setText(activityText);
  } else {
    m_inspBranchActivity->setText(tr("No recent activity"));
  }

  m_inspBranchCheckoutBtn->setEnabled(!info.isCurrent);

  m_inspBranchDeleteBtn->setEnabled(!info.isCurrent);

  m_inspBranchRenameBtn->setEnabled(!info.isRemote);
  m_inspBranchMergeBtn->setEnabled(!info.isCurrent);
  m_inspBranchRebaseBtn->setEnabled(!info.isCurrent);

  m_inspectorStack->setCurrentIndex(2);
}

void GitWorkbenchDialog::showPlanInspector() {
  m_planStepsList->clear();

  int limit = qMin(m_entries.size(), 30);
  int step = 1;
  for (int i = limit - 1; i >= 0; --i) {
    const auto &entry = m_entries[i];
    if (entry.action == "pick")
      continue;

    auto *item = new QTreeWidgetItem(m_planStepsList);
    item->setText(0, QString::number(step++));
    item->setText(1, entry.action);
    item->setText(2, entry.shortHash + " " + entry.subject);

    QString color = actionColor(entry.action);
    item->setForeground(1, QColor(color));
  }

  if (step == 1) {
    auto *item = new QTreeWidgetItem(m_planStepsList);
    item->setText(0, "—");
    item->setText(2, tr("No modifications planned"));
  }

  OperationRisk risk = assessRebaseRisk();
  m_planRiskLabel->setText(QString("<span style='color:%1'>● %2 Risk</span>")
                               .arg(riskColor(risk), riskLabel(risk)));
  m_planRiskLabel->setTextFormat(Qt::RichText);

  m_planAffectedRefs->setText(
      tr("Branch: %1 — all descendant commits will be rewritten")
          .arg(m_currentBranch));

  m_planRecoveryInfo->setText(
      tr("Recovery: Use 'git reflog' to find pre-rewrite state. "
         "Safety backup ref recommended."));

  m_inspectorStack->setCurrentIndex(3);
}

void GitWorkbenchDialog::clearInspector() {
  m_selectedCommitHash.clear();
  m_selectedBranchName.clear();
  m_inspectorStack->setCurrentIndex(0);
}

void GitWorkbenchDialog::showRecoveryCenter() {
  m_recoveryList->clear();

  if (!m_git) {
    m_inspectorStack->setCurrentIndex(4);
    return;
  }

  QStringList backupRefs = m_git->getBackupRefs();
  for (const auto &ref : backupRefs) {
    auto *item = new QTreeWidgetItem(m_recoveryList);
    item->setText(0, tr("Backup"));
    QString display = ref;
    if (display.startsWith("refs/backup/"))
      display = display.mid(QString("refs/backup/").length());
    item->setText(1, display);
    item->setText(2, QString());
    item->setData(1, Qt::UserRole, ref);
  }

  QList<GitReflogEntry> reflogEntries = m_git->getReflog(20);
  for (const auto &entry : reflogEntries) {
    auto *item = new QTreeWidgetItem(m_recoveryList);
    item->setText(0, tr("Reflog"));
    item->setText(1, entry.action + QString::fromUtf8(" — ") + entry.subject);
    item->setText(2, entry.relativeDate);
    item->setData(1, Qt::UserRole, entry.hash);
  }

  if (backupRefs.isEmpty() && reflogEntries.isEmpty()) {
    m_recoveryStatusLabel->setText(tr("No recovery data found"));
  } else {
    m_recoveryStatusLabel->setText(
        tr("Found %1 backup ref(s) and %2 reflog entries")
            .arg(backupRefs.size())
            .arg(reflogEntries.size()));
  }

  m_inspectorStack->setCurrentIndex(4);
}

void GitWorkbenchDialog::showStashInspector(int stashIndex) {

  GitStashEntry entry;
  bool found = false;
  for (const auto &s : m_stashes) {
    if (s.index == stashIndex) {
      entry = s;
      found = true;
      break;
    }
  }

  if (!found) {
    clearInspector();
    return;
  }

  m_inspStashRef->setText(QString("stash@{%1}").arg(entry.index));
  m_inspStashMessage->setText(entry.message);
  m_inspStashBranch->setText(entry.branch.isEmpty()
                                 ? tr("Unknown branch")
                                 : tr("Created on: %1").arg(entry.branch));
  m_inspStashHash->setText(entry.commitHash);

  m_inspectorStack->setCurrentIndex(5);
}

void GitWorkbenchDialog::onCreateStashClicked() {
  bool ok = false;
  QString message = QInputDialog::getText(this, tr("Create Stash"),
                                          tr("Stash message (optional):"),
                                          QLineEdit::Normal, QString(), &ok);

  if (!ok || !m_git)
    return;

  if (m_git->stash(message, true)) {
    loadRepository();
  }
}

OperationRisk GitWorkbenchDialog::assessRebaseRisk() const {
  int dropCount = 0;
  int dropKeepCount = 0;
  int totalModified = 0;
  int limit = qMin(m_entries.size(), 30);

  for (int i = 0; i < limit; ++i) {
    const auto &action = m_entries[i].action;
    if (action == "drop")
      ++dropCount;
    else if (action == "drop-keep")
      ++dropKeepCount;
    if (action != "pick")
      ++totalModified;
  }

  if (dropCount > 3 || totalModified > 10)
    return OperationRisk::Critical;
  if (dropCount > 0)
    return OperationRisk::High;
  if (dropKeepCount > 0 || totalModified > 3)
    return OperationRisk::Medium;
  if (totalModified > 0)
    return OperationRisk::Low;
  return OperationRisk::Low;
}

QString GitWorkbenchDialog::riskLabel(OperationRisk risk) const {
  switch (risk) {
  case OperationRisk::Low:
    return tr("Low");
  case OperationRisk::Medium:
    return tr("Medium");
  case OperationRisk::High:
    return tr("High");
  case OperationRisk::Critical:
    return tr("Critical");
  }
  return tr("Unknown");
}

QString GitWorkbenchDialog::riskColor(OperationRisk risk) const {
  switch (risk) {
  case OperationRisk::Low:
    return m_theme.successColor.name();
  case OperationRisk::Medium:
    return m_theme.accentColor.name();
  case OperationRisk::High:
    return m_theme.warningColor.name();
  case OperationRisk::Critical:
    return m_theme.errorColor.name();
  }
  return m_theme.foregroundColor.name();
}

bool GitWorkbenchDialog::confirmOperation(const QString &title,
                                          const QString &description,
                                          OperationRisk risk,
                                          bool offerBackup) {

  if (risk == OperationRisk::Critical) {
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.setFixedWidth(460);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto *riskBadge = new QLabel(
        QString("<span style='color:%1; font-weight:bold;'>[%2 Risk]</span> %3")
            .arg(riskColor(risk), riskLabel(risk), description),
        &dlg);
    riskBadge->setTextFormat(Qt::RichText);
    riskBadge->setWordWrap(true);
    layout->addWidget(riskBadge);

    auto *warningLabel = new QLabel(
        tr("This operation may cause data loss and cannot be easily undone."),
        &dlg);
    warningLabel->setWordWrap(true);
    layout->addWidget(warningLabel);

    auto *promptLabel = new QLabel(tr("Type <b>CONFIRM</b> to proceed:"), &dlg);
    promptLabel->setTextFormat(Qt::RichText);
    layout->addWidget(promptLabel);

    auto *confirmEdit = new QLineEdit(&dlg);
    confirmEdit->setPlaceholderText(tr("CONFIRM"));
    layout->addWidget(confirmEdit);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *cancelBtn = new QPushButton(tr("Cancel"), &dlg);
    auto *applyBtn = new QPushButton(tr("Apply"), &dlg);
    applyBtn->setEnabled(false);
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(applyBtn);
    layout->addLayout(btnLayout);

    connect(confirmEdit, &QLineEdit::textChanged,
            [applyBtn](const QString &text) {
              applyBtn->setEnabled(
                  text.compare("CONFIRM", Qt::CaseInsensitive) == 0);
            });
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(applyBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.setStyleSheet(
        QString(
            "QDialog { background: %1; }"
            "QLabel { color: %2; font-size: 13px; }"
            "QLineEdit { background: %3; color: %2; border: 1px solid %4; "
            "border-radius: 6px; padding: 8px 12px; font-size: 13px; }"
            "QPushButton { background: %3; color: %2; border: 1px solid %4; "
            "border-radius: 8px; padding: 8px 18px; font-size: 13px; }"
            "QPushButton:hover { background: %5; }"
            "QPushButton:disabled { color: %4; }")
            .arg(m_theme.backgroundColor.name(), m_theme.foregroundColor.name(),
                 m_theme.surfaceAltColor.name(), m_theme.borderColor.name(),
                 m_theme.hoverColor.name()));

    return dlg.exec() == QDialog::Accepted;
  }

  ThemedMessageBox box(this);
  box.setWindowTitle(title);

  QString riskBadge = QString("[%1 Risk] ").arg(riskLabel(risk));
  box.setText(riskBadge + description);

  if (risk == OperationRisk::Critical) {
    box.setIcon(ThemedMessageBox::Critical);
    box.setInformativeText(
        tr("This operation may cause data loss and cannot be easily undone."));
  } else if (risk == OperationRisk::High) {
    box.setIcon(ThemedMessageBox::Warning);
    box.setInformativeText(
        tr("This operation rewrites history. A reflog entry will be created."));
  } else {
    box.setIcon(ThemedMessageBox::Question);
  }

  if (offerBackup && risk >= OperationRisk::High) {
    box.setStandardButtons(ThemedMessageBox::Yes | ThemedMessageBox::No |
                           ThemedMessageBox::Cancel);
    box.button(ThemedMessageBox::Yes)->setText(tr("Create Backup && Apply"));
    box.button(ThemedMessageBox::No)->setText(tr("Apply Without Backup"));
    box.setDefaultButton(ThemedMessageBox::Yes);
  } else {
    box.setStandardButtons(ThemedMessageBox::Ok | ThemedMessageBox::Cancel);
    box.button(ThemedMessageBox::Ok)->setText(tr("Apply"));
    box.setDefaultButton(ThemedMessageBox::Cancel);
  }

  int result = box.exec();
  return result == ThemedMessageBox::Yes || result == ThemedMessageBox::Ok;
}

void GitWorkbenchDialog::onCommitSelectionChanged() {
  auto selected = m_commitTree->selectedItems();

  updateSelectionUI();

  if (selected.isEmpty()) {
    if (!m_rewriteMode)
      clearInspector();
    return;
  }

  auto *item = selected.last();
  QString hash = item->data(0, Qt::UserRole + 1).toString();
  if (!hash.isEmpty()) {
    showCommitInspector(hash);
  }
}

void GitWorkbenchDialog::onCommitDoubleClicked(QTreeWidgetItem *item,
                                               int column) {
  QString hash = item->data(0, Qt::UserRole + 1).toString();
  if (hash.isEmpty())
    return;

  if (column == 0) {
    m_selectedCommitHash = hash;
    onEditCommitMessage();
    return;
  }

  emit viewCommitDiff(hash);
}

void GitWorkbenchDialog::onCommitContextMenu(const QPoint &pos) {
  auto *item = m_commitTree->itemAt(pos);
  if (!item)
    return;

  QString hash = item->data(0, Qt::UserRole + 1).toString();
  int idx = item->data(0, Qt::UserRole).toInt();
  if (hash.isEmpty())
    return;

  auto selected = m_commitTree->selectedItems();
  int selCount = selected.size();
  bool isMulti = selCount > 1;

  QMenu menu(this);
  menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));

  if (isMulti) {

    auto *headerAction =
        menu.addAction(tr("%1 commits selected").arg(selCount));
    headerAction->setEnabled(false);
    menu.addSeparator();

    menu.addAction(tr("Copy Hashes (%1)").arg(selCount), [this, &selected]() {
      QStringList hashes;
      for (auto *sel : selected)
        hashes << sel->data(0, Qt::UserRole + 1).toString().left(8);
      QApplication::clipboard()->setText(hashes.join("\n"));
    });
    menu.addSeparator();

    if (m_rewriteMode) {
      menu.addAction(tr("Squash %1 commits").arg(selCount), this,
                     &GitWorkbenchDialog::onSquashSelected);
      menu.addAction(tr("Fixup %1 commits").arg(selCount), this,
                     &GitWorkbenchDialog::onFixupSelected);
      menu.addAction(tr("Drop %1 commits").arg(selCount), this,
                     &GitWorkbenchDialog::onDropSelected);
      menu.addAction(tr("Reword %1 commits").arg(selCount), this,
                     &GitWorkbenchDialog::onRewordSelected);
      menu.addSeparator();
      menu.addAction(tr("Set all to pick"), [this, &selected]() {
        for (auto *sel : selected) {
          int r = sel->data(0, Qt::UserRole).toInt();
          if (r >= 0 && r < m_entries.size()) {
            m_entries[r].action = "pick";
            rebuildActionCombo(sel, r);
          }
        }
        updatePlanSummary();
        showPlanInspector();
      });
    }
  } else {

    menu.addAction(tr("View Diff"),
                   [this, hash]() { emit viewCommitDiff(hash); });
    menu.addAction(tr("Edit Message..."), [this, hash]() {
      m_selectedCommitHash = hash;
      onEditCommitMessage();
    });
    menu.addAction(tr("Copy Hash"),
                   [hash]() { QApplication::clipboard()->setText(hash); });
    menu.addSeparator();
    menu.addAction(tr("Cherry-pick"), this,
                   &GitWorkbenchDialog::onCherryPickClicked);
    menu.addAction(tr("Revert"), this,
                   &GitWorkbenchDialog::onRevertCommitClicked);
    menu.addAction(tr("Move to Branch..."), this,
                   &GitWorkbenchDialog::onMoveToBranchClicked);
    menu.addSeparator();

    if (m_rewriteMode && idx >= 0 && idx < m_entries.size()) {
      menu.addAction(tr("Set: pick"), [this, idx]() {
        m_entries[idx].action = "pick";
        rebuildActionCombo(m_commitTree->topLevelItem(idx), idx);
        updatePlanSummary();
        showPlanInspector();
      });
      menu.addAction(tr("Set: squash"), [this, idx]() {
        m_entries[idx].action = "squash";
        rebuildActionCombo(m_commitTree->topLevelItem(idx), idx);
        updatePlanSummary();
        showPlanInspector();
      });
      menu.addAction(tr("Set: drop"), [this, idx]() {
        m_entries[idx].action = "drop";
        rebuildActionCombo(m_commitTree->topLevelItem(idx), idx);
        updatePlanSummary();
        showPlanInspector();
      });
    }
  }

  menu.exec(m_commitTree->viewport()->mapToGlobal(pos));
}

void GitWorkbenchDialog::onCommitSearchChanged(const QString &text) {
  for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
    auto *item = m_commitTree->topLevelItem(i);
    int idx = item->data(0, Qt::UserRole).toInt();
    if (m_rewriteMode && idx >= 30) {
      item->setHidden(true);
      continue;
    }
    if (text.isEmpty()) {
      item->setHidden(false);
    } else {
      bool match =
          (idx >= 0 && idx < m_entries.size() &&
           (m_entries[idx].subject.contains(text, Qt::CaseInsensitive) ||
            m_entries[idx].shortHash.contains(text, Qt::CaseInsensitive) ||
            m_entries[idx].author.contains(text, Qt::CaseInsensitive)));
      item->setHidden(!match);
    }
  }
}

void GitWorkbenchDialog::onBranchItemClicked(QTreeWidgetItem *item,
                                             int column) {
  Q_UNUSED(column);
  QString ref = item->data(0, Qt::UserRole).toString();
  bool isSpecial = item->data(0, Qt::UserRole + 1).toBool();
  if (ref.isEmpty())
    return;

  if (isSpecial && ref.startsWith("stash@{")) {
    QRegularExpression re("stash@\\{(\\d+)\\}");
    auto match = re.match(ref);
    if (match.hasMatch()) {
      showStashInspector(match.captured(1).toInt());
      return;
    }
  }

  if (!ref.isEmpty()) {
    showBranchInspector(ref);
  }
}

void GitWorkbenchDialog::onBranchItemDoubleClicked(QTreeWidgetItem *item,
                                                   int column) {
  Q_UNUSED(column);
  QString branchName = item->data(0, Qt::UserRole).toString();
  bool isCurrent = item->data(0, Qt::UserRole + 2).toBool();
  if (!branchName.isEmpty() && !isCurrent && m_git) {
    m_git->checkoutBranch(branchName);
    loadRepository();
  }
}

void GitWorkbenchDialog::onBranchContextMenu(const QPoint &pos) {
  auto *item = m_branchTree->itemAt(pos);
  if (!item)
    return;

  QString ref = item->data(0, Qt::UserRole).toString();
  bool isSpecial = item->data(0, Qt::UserRole + 1).toBool();
  if (ref.isEmpty())
    return;

  if (isSpecial && ref.startsWith("stash@{")) {
    QRegularExpression re("stash@\\{(\\d+)\\}");
    auto match = re.match(ref);
    if (!match.hasMatch())
      return;
    int stashIndex = match.captured(1).toInt();

    QMenu menu(this);
    menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));

    menu.addAction(tr("Apply"), [this, stashIndex]() {
      if (m_git) {
        m_git->stashApply(stashIndex);
        loadRepository();
      }
    });

    menu.addAction(tr("Pop"), [this, stashIndex]() {
      if (confirmOperation(
              tr("Pop Stash"),
              tr("Pop stash@{%1}? This removes the stash entry after applying.")
                  .arg(stashIndex),
              OperationRisk::Medium, false)) {
        if (m_git) {
          m_git->stashPop(stashIndex);
          loadRepository();
        }
      }
    });

    menu.addAction(tr("Drop"), [this, stashIndex]() {
      if (confirmOperation(
              tr("Drop Stash"),
              tr("Drop stash@{%1}? This permanently deletes the stash.")
                  .arg(stashIndex),
              OperationRisk::High, false)) {
        if (m_git) {
          m_git->stashDrop(stashIndex);
          loadRepository();
        }
      }
    });

    menu.exec(m_branchTree->viewport()->mapToGlobal(pos));
    return;
  }

  QString branchName = ref;
  bool isCurrent = item->data(0, Qt::UserRole + 2).toBool();
  bool isRemote = isSpecial;
  if (branchName.isEmpty())
    return;

  QMenu menu(this);
  menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));

  if (!isCurrent) {
    menu.addAction(tr("Switch To"), [this, branchName]() {
      if (m_git) {
        m_git->checkoutBranch(branchName);
        loadRepository();
      }
    });
  }

  menu.addAction(tr("Copy Name"), [branchName]() {
    QApplication::clipboard()->setText(branchName);
  });

  if (!isCurrent && !isRemote) {
    menu.addSeparator();
    menu.addAction(tr("Delete"), [this, branchName]() {
      if (confirmOperation(tr("Delete Branch"),
                           tr("Delete branch '%1'?").arg(branchName),
                           OperationRisk::High, false)) {
        if (m_git)
          m_git->deleteBranch(branchName);
        loadRepository();
      }
    });
  }

  if (!isCurrent) {
    menu.addAction(tr("View Commits"),
                   [this, branchName]() { loadCommits(branchName); });
  }

  if (!isRemote) {
    menu.addSeparator();
    menu.addAction(tr("Rename..."), [this, branchName]() {
      m_selectedBranchName = branchName;
      onRenameBranchClicked();
    });
  }
  if (!isCurrent) {
    menu.addAction(tr("Merge into Current"), [this, branchName]() {
      m_selectedBranchName = branchName;
      onMergeBranchClicked();
    });
    menu.addAction(tr("Rebase onto This"), [this, branchName]() {
      m_selectedBranchName = branchName;
      onRebaseBranchClicked();
    });
  }

  menu.exec(m_branchTree->viewport()->mapToGlobal(pos));
}

void GitWorkbenchDialog::onBranchSearchChanged(const QString &text) {
  for (int i = 0; i < m_branchTree->topLevelItemCount(); ++i) {
    auto *group = m_branchTree->topLevelItem(i);
    bool anyVisible = false;
    for (int j = 0; j < group->childCount(); ++j) {
      auto *child = group->child(j);
      if (text.isEmpty()) {
        child->setHidden(false);
        anyVisible = true;
      } else {
        bool match = child->text(0).contains(text, Qt::CaseInsensitive);
        child->setHidden(!match);
        if (match)
          anyVisible = true;
      }
    }
    group->setHidden(!anyVisible && !text.isEmpty());
  }
}

void GitWorkbenchDialog::onCreateBranchClicked() {
  if (!m_git)
    return;

  bool ok;
  QString name =
      QInputDialog::getText(this, tr("Create Branch"), tr("Branch name:"),
                            QLineEdit::Normal, QString(), &ok);
  if (ok && !name.isEmpty()) {
    if (m_git->createBranch(name, true)) {
      loadRepository();
    } else {
      ThemedMessageBox::warning(this, tr("Error"),
                                tr("Failed to create branch '%1'.").arg(name));
    }
  }
}

void GitWorkbenchDialog::onRenameBranchClicked() {
  if (m_selectedBranchName.isEmpty() || !m_git)
    return;

  bool ok;
  QString newName =
      QInputDialog::getText(this, tr("Rename Branch"), tr("New branch name:"),
                            QLineEdit::Normal, m_selectedBranchName, &ok);
  if (ok && !newName.isEmpty() && newName != m_selectedBranchName) {
    if (m_git->renameBranch(m_selectedBranchName, newName)) {
      loadRepository();
    } else {
      ThemedMessageBox::warning(
          this, tr("Rename Failed"),
          tr("Failed to rename branch '%1'.").arg(m_selectedBranchName));
    }
  }
}

void GitWorkbenchDialog::onMergeBranchClicked() {
  if (m_selectedBranchName.isEmpty() || !m_git)
    return;

  if (confirmOperation(tr("Merge Branch"),
                       tr("Merge branch '%1' into current branch '%2'?")
                           .arg(m_selectedBranchName, m_currentBranch),
                       OperationRisk::Medium, false)) {
    if (m_git->mergeBranch(m_selectedBranchName)) {
      loadRepository();
    } else {
      ThemedMessageBox::warning(this, tr("Merge Failed"),
                                tr("Merge failed. Check for conflicts."));
    }
  }
}

void GitWorkbenchDialog::onRebaseBranchClicked() {
  if (m_selectedBranchName.isEmpty() || !m_git)
    return;

  if (confirmOperation(tr("Rebase onto Branch"),
                       tr("Rebase current branch '%1' onto '%2'?")
                           .arg(m_currentBranch, m_selectedBranchName),
                       OperationRisk::High)) {
    if (m_git->rebaseBranch(m_selectedBranchName)) {
      loadRepository();
    } else {
      ThemedMessageBox::warning(this, tr("Rebase Failed"),
                                tr("Rebase failed. Check for conflicts."));
    }
  }
}

void GitWorkbenchDialog::onMoveUp() {
  if (!m_rewriteMode)
    return;
  int row = m_commitTree->indexOfTopLevelItem(m_commitTree->currentItem());
  if (row <= 0)
    return;

  auto *item = m_commitTree->takeTopLevelItem(row);
  m_commitTree->insertTopLevelItem(row - 1, item);
  m_commitTree->setCurrentItem(item);

  m_entries.swapItemsAt(row, row - 1);
  m_hashToEntryIndex[m_entries[row].hash] = row;
  m_hashToEntryIndex[m_entries[row - 1].hash] = row - 1;

  item->setData(0, Qt::UserRole, row - 1);
  m_commitTree->topLevelItem(row)->setData(0, Qt::UserRole, row);

  rebuildActionCombo(item, row - 1);
  rebuildActionCombo(m_commitTree->topLevelItem(row), row);
  updatePlanSummary();
  showPlanInspector();
}

void GitWorkbenchDialog::onMoveDown() {
  if (!m_rewriteMode)
    return;
  int row = m_commitTree->indexOfTopLevelItem(m_commitTree->currentItem());
  int maxRow = qMin(m_commitTree->topLevelItemCount() - 1, 29);
  if (row < 0 || row >= maxRow)
    return;

  auto *item = m_commitTree->takeTopLevelItem(row);
  m_commitTree->insertTopLevelItem(row + 1, item);
  m_commitTree->setCurrentItem(item);

  m_entries.swapItemsAt(row, row + 1);
  m_hashToEntryIndex[m_entries[row].hash] = row;
  m_hashToEntryIndex[m_entries[row + 1].hash] = row + 1;

  item->setData(0, Qt::UserRole, row + 1);
  m_commitTree->topLevelItem(row)->setData(0, Qt::UserRole, row);

  rebuildActionCombo(item, row + 1);
  rebuildActionCombo(m_commitTree->topLevelItem(row), row);
  updatePlanSummary();
  showPlanInspector();
}

void GitWorkbenchDialog::onSquashSelected() {
  if (!m_rewriteMode)
    return;
  auto selected = m_commitTree->selectedItems();
  if (selected.size() < 2) {
    ThemedMessageBox::information(this, tr("Squash"),
                                  tr("Select at least 2 commits to squash."));
    return;
  }

  for (auto *item : selected) {
    int r = item->data(0, Qt::UserRole).toInt();
    if (r >= 0 && r < m_entries.size()) {
      if (item == selected.first())
        m_entries[r].action = "pick";
      else
        m_entries[r].action = "squash";
      rebuildActionCombo(item, r);
    }
  }
  updatePlanSummary();
  showPlanInspector();
}

void GitWorkbenchDialog::onFixupSelected() {
  if (!m_rewriteMode)
    return;
  auto selected = m_commitTree->selectedItems();
  if (selected.size() < 2) {
    ThemedMessageBox::information(this, tr("Fixup"),
                                  tr("Select at least 2 commits to fixup."));
    return;
  }

  for (auto *item : selected) {
    int r = item->data(0, Qt::UserRole).toInt();
    if (r >= 0 && r < m_entries.size()) {
      if (item == selected.first())
        m_entries[r].action = "pick";
      else
        m_entries[r].action = "fixup";
      rebuildActionCombo(item, r);
    }
  }
  updatePlanSummary();
  showPlanInspector();
}

void GitWorkbenchDialog::onRewordSelected() {
  if (!m_rewriteMode)
    return;
  auto selected = m_commitTree->selectedItems();
  for (auto *item : selected) {
    int r = item->data(0, Qt::UserRole).toInt();
    if (r >= 0 && r < m_entries.size()) {
      m_entries[r].action = "reword";
      rebuildActionCombo(item, r);
    }
  }
  updatePlanSummary();
  showPlanInspector();
}

void GitWorkbenchDialog::onDropSelected() {
  if (!m_rewriteMode)
    return;
  auto selected = m_commitTree->selectedItems();
  if (selected.isEmpty())
    return;

  for (auto *item : selected) {
    int r = item->data(0, Qt::UserRole).toInt();
    if (r >= 0 && r < m_entries.size()) {
      m_entries[r].action = "drop";
      rebuildActionCombo(item, r);
    }
  }
  updatePlanSummary();
  showPlanInspector();
}

void GitWorkbenchDialog::onPickAll() {
  if (!m_rewriteMode)
    return;
  int limit = qMin(m_entries.size(), 30);
  for (int i = 0; i < limit; ++i) {
    m_entries[i].action = "pick";
    auto *item = m_commitTree->topLevelItem(i);
    if (item)
      rebuildActionCombo(item, i);
  }
  updatePlanSummary();
  showPlanInspector();
}

void GitWorkbenchDialog::onApplyRebase() {
  if (m_entries.isEmpty() || !m_git)
    return;

  OperationRisk risk = assessRebaseRisk();

  int dropCount = 0, squashCount = 0, rewordCount = 0, editCount = 0;
  int dropKeepCount = 0;
  int limit = qMin(m_entries.size(), 30);
  for (int i = 0; i < limit; ++i) {
    const auto &a = m_entries[i].action;
    if (a == "drop")
      ++dropCount;
    if (a == "squash" || a == "fixup")
      ++squashCount;
    if (a == "reword")
      ++rewordCount;
    if (a == "edit")
      ++editCount;
    if (a == "drop-keep")
      ++dropKeepCount;
  }

  QStringList consequences;
  if (dropCount > 0)
    consequences << tr("%1 commit(s) will be dropped").arg(dropCount);
  if (squashCount > 0)
    consequences << tr("%1 commit(s) will be squashed").arg(squashCount);
  if (rewordCount > 0)
    consequences << tr("%1 commit(s) will be reworded").arg(rewordCount);
  if (editCount > 0)
    consequences << tr("%1 commit(s) will be edited").arg(editCount);
  if (dropKeepCount > 0)
    consequences << tr("%1 commit(s) will be removed (content preserved)")
                        .arg(dropKeepCount);

  QString description =
      tr("This rewrite will affect %1 on branch '%2':\n• %3\n\n"
         "All descendant commits will receive new hashes.")
          .arg(tr("%1 commit(s)").arg(limit), m_currentBranch,
               consequences.join("\n• "));

  if (!confirmOperation(tr("Apply Rewrite"), description, risk,
                        m_backupCheckbox->isChecked()))
    return;

  if (m_backupCheckbox->isChecked()) {
    QString backupRef =
        QString("refs/backup/%1-%2")
            .arg(m_currentBranch,
                 QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
    QProcess backupProc;
    backupProc.setWorkingDirectory(m_git->repositoryPath());
    backupProc.start("git", {"update-ref", backupRef, "HEAD"});
    backupProc.waitForFinished(5000);
  }

  QString todoScript;
  for (int i = limit - 1; i >= 0; --i) {
    QString scriptAction = m_entries[i].action;
    if (scriptAction == "drop-keep")
      scriptAction = "fixup";
    todoScript += scriptAction + " " + m_entries[i].shortHash + " " +
                  m_entries[i].subject + "\n";
  }

  QTemporaryFile todoFile;
  todoFile.setAutoRemove(false);
  if (!todoFile.open()) {
    m_commitStatusLabel->setText(tr("Failed to create rebase script"));
    return;
  }
  QTextStream out(&todoFile);
  out << todoScript;
  todoFile.close();

  QString scriptPath = todoFile.fileName();
  QString upstream = QString("HEAD~%1").arg(limit);

  QProcess proc;
  proc.setWorkingDirectory(m_git->repositoryPath());
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GIT_SEQUENCE_EDITOR", QString("cp %1").arg(scriptPath));
  proc.setProcessEnvironment(env);
  proc.start("git", {"rebase", "-i", upstream});
  proc.waitForFinished(15000);

  QFile::remove(scriptPath);

  QString output = proc.readAllStandardOutput() + proc.readAllStandardError();
  if (proc.exitCode() == 0) {
    m_commitStatusLabel->setText(tr("Rewrite completed successfully"));

    QString backupMsg;
    if (m_backupCheckbox->isChecked()) {
      backupMsg = tr("\n\nSafety backup ref created. "
                     "Use 'git reflog' to recover if needed.");
    }

    ThemedMessageBox::information(
        this, tr("Rewrite Complete"),
        tr("Interactive rebase completed successfully.%1").arg(backupMsg));
    accept();
  } else {
    m_commitStatusLabel->setText(
        tr("Rewrite failed or needs conflict resolution"));
    ThemedMessageBox::warning(this, tr("Rewrite Issue"),
                              tr("Rebase encountered issues:\n%1").arg(output));
  }
}

void GitWorkbenchDialog::onCherryPickClicked() {
  if (m_selectedCommitHash.isEmpty() || !m_git)
    return;

  if (confirmOperation(tr("Cherry-pick"),
                       tr("Cherry-pick commit %1 onto current branch?")
                           .arg(m_selectedCommitHash.left(8)),
                       OperationRisk::Medium, false)) {
    if (m_git->cherryPick(m_selectedCommitHash)) {
      loadRepository();
    } else {
      ThemedMessageBox::warning(this, tr("Cherry-pick Failed"),
                                tr("Cherry-pick failed. Check for conflicts."));
    }
  }
}

void GitWorkbenchDialog::onMoveToBranchClicked() {
  if (m_selectedCommitHash.isEmpty() || !m_git)
    return;

  QStringList branchNames;
  for (const auto &b : m_branches) {
    if (!b.isCurrent && !b.isRemote)
      branchNames << b.name;
  }

  if (branchNames.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Move to Branch"),
        tr("No other local branches available. Create one first."));
    return;
  }

  bool ok;
  QString target = QInputDialog::getItem(this, tr("Move to Branch"),
                                         tr("Select target branch:"),
                                         branchNames, 0, false, &ok);
  if (ok && !target.isEmpty()) {
    if (confirmOperation(tr("Move Commit"),
                         tr("Move commit %1 to branch '%2'? "
                            "This rewrites history on the current branch.")
                             .arg(m_selectedCommitHash.left(8), target),
                         OperationRisk::High)) {
      if (m_git->moveCommitToBranch(m_selectedCommitHash, target)) {
        loadRepository();
      } else {
        ThemedMessageBox::warning(this, tr("Move Failed"),
                                  tr("Failed to move commit to branch."));
      }
    }
  }
}

void GitWorkbenchDialog::onRevertCommitClicked() {
  if (m_selectedCommitHash.isEmpty() || !m_git)
    return;

  if (confirmOperation(
          tr("Revert Commit"),
          tr("Create a new commit that undoes the changes from %1?")
              .arg(m_selectedCommitHash.left(8)),
          OperationRisk::Low, false)) {
    if (m_git->revertCommit(m_selectedCommitHash)) {
      loadRepository();
    } else {
      ThemedMessageBox::warning(this, tr("Revert Failed"),
                                tr("Revert failed. Check for conflicts."));
    }
  }
}

void GitWorkbenchDialog::onEditCommitMessage() {
  if (m_selectedCommitHash.isEmpty() || !m_git)
    return;

  QString currentMsg = m_git->getCommitMessage(m_selectedCommitHash);
  if (currentMsg.isEmpty())
    return;

  QDialog editDlg(this);
  editDlg.setWindowTitle(tr("Edit Commit Message"));
  editDlg.setMinimumSize(520, 360);

  auto *layout = new QVBoxLayout(&editDlg);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  auto *hashLabel = new QLabel(
      tr("Editing message for commit %1").arg(m_selectedCommitHash.left(8)),
      &editDlg);
  QFont monoFont = hashLabel->font();
  monoFont.setFamily("monospace");
  hashLabel->setFont(monoFont);
  layout->addWidget(hashLabel);

  auto *msgEdit = new QTextEdit(&editDlg);
  msgEdit->setObjectName("commitMessageEdit");
  msgEdit->setPlainText(currentMsg);
  msgEdit->setTabChangesFocus(true);
  layout->addWidget(msgEdit, 1);

  auto *warningLabel = new QLabel(&editDlg);
  warningLabel->setWordWrap(true);

  auto headCommits = m_git->getCommitLog(1);
  bool isHead = !headCommits.isEmpty() && headCommits.first().hash.startsWith(
                                              m_selectedCommitHash.left(8));

  if (isHead) {
    warningLabel->setText(
        tr("This commit is HEAD — the message will be amended directly."));
  } else {
    warningLabel->setText(
        tr("This rewrites history. All descendant commits will receive "
           "new hashes."));
  }
  layout->addWidget(warningLabel);

  auto *btnRow = new QHBoxLayout();
  btnRow->addStretch();
  auto *cancelBtn = new QPushButton(tr("Cancel"), &editDlg);
  auto *saveBtn = new QPushButton(tr("Save Message"), &editDlg);
  saveBtn->setDefault(true);
  btnRow->addWidget(cancelBtn);
  btnRow->addWidget(saveBtn);
  layout->addLayout(btnRow);

  editDlg.setStyleSheet(
      QString("QDialog { background: %1; color: %2; }"
              "QTextEdit { background: %3; color: %2; border: 1px solid %4; "
              "border-radius: 6px; padding: 8px; font-size: 13px; }"
              "QLabel { color: %2; font-size: 13px; }")
          .arg(m_theme.surfaceColor.name(), m_theme.foregroundColor.name(),
               m_theme.surfaceAltColor.name(), m_theme.borderColor.name()));
  cancelBtn->setStyleSheet(toolButtonStyle());
  saveBtn->setStyleSheet(accentButtonStyle());

  connect(cancelBtn, &QPushButton::clicked, &editDlg, &QDialog::reject);
  connect(saveBtn, &QPushButton::clicked, &editDlg, &QDialog::accept);

  if (editDlg.exec() != QDialog::Accepted)
    return;

  QString newMsg = msgEdit->toPlainText().trimmed();
  if (newMsg.isEmpty() || newMsg == currentMsg)
    return;

  OperationRisk risk = isHead ? OperationRisk::Low : OperationRisk::Medium;
  if (!confirmOperation(tr("Edit Commit Message"),
                        tr("Change the message of commit %1?")
                            .arg(m_selectedCommitHash.left(8)),
                        risk, !isHead))
    return;

  bool ok = false;
  if (isHead) {
    ok = m_git->commitAmend(newMsg);
  } else {
    ok = m_git->rewordCommit(m_selectedCommitHash, newMsg);
  }

  if (ok) {
    m_commitStatusLabel->setText(
        tr("Commit message updated for %1").arg(m_selectedCommitHash.left(8)));
    loadRepository();
  } else {
    ThemedMessageBox::warning(
        this, tr("Edit Message Failed"),
        tr("Failed to update commit message. Check for conflicts."));
  }
}

void GitWorkbenchDialog::onResetToBranchClicked() {
  if (m_selectedBranchName.isEmpty() || !m_git)
    return;

  QStringList modes = {"mixed", "soft", "hard"};
  bool ok;
  QString mode = QInputDialog::getItem(
      this, tr("Reset to Branch"),
      tr("Reset mode (mixed keeps changes staged, soft keeps changes, "
         "hard discards everything):"),
      modes, 0, false, &ok);
  if (ok) {
    OperationRisk risk =
        (mode == "hard") ? OperationRisk::Critical : OperationRisk::High;
    if (confirmOperation(tr("Reset"),
                         tr("Reset current branch to '%1' using %2 mode?")
                             .arg(m_selectedBranchName, mode),
                         risk)) {

      QList<GitCommitInfo> tipCommits =
          m_git->getCommitLog(1, m_selectedBranchName);
      if (!tipCommits.isEmpty()) {
        m_git->resetToCommit(tipCommits.first().hash, mode);
        loadRepository();
      }
    }
  }
}

void GitWorkbenchDialog::keyPressEvent(QKeyEvent *event) {

  if (event->key() == Qt::Key_K && (event->modifiers() & Qt::ControlModifier)) {
    onCommandPalette();
    return;
  }

  bool commitFocused =
      m_commitTree->hasFocus() ||
      (!m_branchSearch->hasFocus() && !m_commitSearch->hasFocus());

  if (commitFocused) {
    switch (event->key()) {
    case Qt::Key_J:
      if (event->modifiers() & Qt::ShiftModifier) {
        if (m_rewriteMode)
          onMoveDown();
        else
          navigateCommit(1, true);
      } else {
        navigateCommit(1);
      }
      return;

    case Qt::Key_K:
      if (event->modifiers() & Qt::ShiftModifier) {
        if (m_rewriteMode)
          onMoveUp();
        else
          navigateCommit(-1, true);
      } else {
        navigateCommit(-1);
      }
      return;

    case Qt::Key_S:
      if (m_rewriteMode && !(event->modifiers() & Qt::ControlModifier)) {
        onSquashSelected();
        return;
      }
      break;

    case Qt::Key_D:
      if (m_rewriteMode && !(event->modifiers() & Qt::ControlModifier)) {
        onDropSelected();
        return;
      }
      break;

    case Qt::Key_F:
      if (m_rewriteMode && !(event->modifiers() & Qt::ControlModifier)) {
        onFixupSelected();
        return;
      }
      break;

    case Qt::Key_R:
      if (event->modifiers() & Qt::ControlModifier) {
        break;
      }
      if (m_rewriteMode) {
        onRewordSelected();
      } else {
        onToggleRewriteMode();
      }
      return;

    case Qt::Key_P:
      if (m_rewriteMode && !(event->modifiers() & Qt::ControlModifier)) {
        setCommitAction("pick");
        return;
      }
      break;

    case Qt::Key_E:
      if (m_rewriteMode && !(event->modifiers() & Qt::ControlModifier)) {
        setCommitAction("edit");
        return;
      }
      break;

    case Qt::Key_Return:
    case Qt::Key_Enter: {
      auto selected = m_commitTree->selectedItems();
      if (!selected.isEmpty()) {
        QString hash = selected.last()->data(0, Qt::UserRole + 1).toString();
        if (!hash.isEmpty())
          showCommitInspector(hash);
      }
      return;
    }

    case Qt::Key_Space: {
      auto selected = m_commitTree->selectedItems();
      if (!selected.isEmpty()) {
        QString hash = selected.last()->data(0, Qt::UserRole + 1).toString();
        if (!hash.isEmpty())
          emit viewCommitDiff(hash);
      }
      return;
    }

    case Qt::Key_Slash:
      m_commitSearch->setFocus();
      return;

    case Qt::Key_Escape:
      if (m_rewriteMode) {
        onToggleRewriteMode();
        return;
      }
      break;

    case Qt::Key_Z:
      if (event->modifiers() & Qt::ControlModifier && m_rewriteMode) {
        onPickAll();
        return;
      }
      break;

    case Qt::Key_B:
      if (!(event->modifiers() & Qt::ControlModifier)) {
        m_branchTree->setFocus();
        return;
      }
      break;

    case Qt::Key_M:
      if (!(event->modifiers() & Qt::ControlModifier)) {
        onMoveToBranchClicked();
        return;
      }
      break;

    case Qt::Key_Question: {
      ThemedMessageBox::information(
          this, tr("Keyboard Shortcuts"),
          tr("J/K — Navigate commits\n"
             "Shift+J/K — Extend selection / Move in rewrite\n"
             "S — Squash (rewrite mode)\n"
             "D — Drop (rewrite mode)\n"
             "F — Fixup (rewrite mode)\n"
             "R — Reword / Toggle rewrite mode\n"
             "E — Edit (rewrite mode)\n"
             "P — Pick (rewrite mode)\n"
             "B — Focus branch explorer\n"
             "M — Move commit to branch\n"
             "Enter — Inspect commit\n"
             "Space — Quick diff\n"
             "/ — Search commits\n"
             "Escape — Exit rewrite mode\n"
             "Ctrl+Z — Reset all (rewrite mode)\n"
             "Ctrl+K — Command palette\n"
             "? — Show this help"));
    }
      return;
    }
  }

  QDialog::keyPressEvent(event);
}

void GitWorkbenchDialog::navigateCommit(int delta, bool extendSelection) {
  int count = m_commitTree->topLevelItemCount();
  if (count == 0)
    return;

  auto *current = m_commitTree->currentItem();
  int row = current ? m_commitTree->indexOfTopLevelItem(current) : -1;
  int newRow = qBound(0, row + delta, count - 1);

  auto *newItem = m_commitTree->topLevelItem(newRow);
  if (!newItem)
    return;

  if (extendSelection) {
    newItem->setSelected(true);
  } else {
    m_commitTree->clearSelection();
    newItem->setSelected(true);
  }
  m_commitTree->setCurrentItem(newItem);
  m_commitTree->scrollToItem(newItem);
}

void GitWorkbenchDialog::setCommitAction(const QString &action) {
  auto selected = m_commitTree->selectedItems();
  for (auto *item : selected) {
    int r = item->data(0, Qt::UserRole).toInt();
    if (r >= 0 && r < m_entries.size()) {
      m_entries[r].action = action;
      rebuildActionCombo(item, r);
    }
  }
  updatePlanSummary();
  if (m_rewriteMode)
    showPlanInspector();
}

void GitWorkbenchDialog::updateSelectionUI() {
  auto selected = m_commitTree->selectedItems();
  int count = selected.size();

  bool showBar = count >= 2;
  m_selectionBar->setVisible(showBar);
  if (showBar) {
    m_selectionCountLabel->setText(tr("<b>%1 commits selected</b>").arg(count));
  }

  if (m_rewriteMode) {
    if (count >= 2) {
      m_squashBtn->setText(tr("Squash (%1)").arg(count));
      m_fixupBtn->setText(tr("Fixup (%1)").arg(count));
      m_dropBtn->setText(tr("Drop (%1)").arg(count));
      m_rewordBtn->setText(tr("Reword (%1)").arg(count));
    } else {
      m_squashBtn->setText(tr("Squash"));
      m_fixupBtn->setText(tr("Fixup"));
      m_dropBtn->setText(tr("Drop"));
      m_rewordBtn->setText(tr("Reword"));
    }
  }
}

QString GitWorkbenchDialog::actionColor(const QString &action) const {
  if (action == "pick")
    return m_theme.successColor.name();
  if (action == "reword")
    return m_theme.warningColor.name();
  if (action == "edit")
    return m_theme.accentColor.name();
  if (action == "squash")
    return m_theme.accentColor.lighter(130).name();
  if (action == "fixup")
    return m_theme.singleLineCommentFormat.name();
  if (action == "drop")
    return m_theme.errorColor.name();
  if (action == "drop-keep")
    return m_theme.warningColor.name();
  return m_theme.foregroundColor.name();
}

QString GitWorkbenchDialog::toolButtonStyle() const {
  return UIStyleHelper::secondaryButtonStyle(m_theme);
}

QString GitWorkbenchDialog::dangerButtonStyle() const {
  return QString("QPushButton { background: %1; color: %2; border: 1px solid "
                 "%3; border-radius: 6px; padding: 6px 12px; }"
                 "QPushButton:hover { background: %2; color: white; }")
      .arg(m_theme.surfaceAltColor.name(), m_theme.errorColor.name(),
           m_theme.borderColor.name());
}

QString GitWorkbenchDialog::accentButtonStyle() const {
  return UIStyleHelper::primaryButtonStyle(m_theme);
}

QString GitWorkbenchDialog::ghostButtonStyle() const {
  return UIStyleHelper::breadcrumbButtonStyle(m_theme);
}

void GitWorkbenchDialog::onCommandPalette() {
  QDialog palette(this);
  palette.setWindowTitle(tr("Command Palette"));
  palette.setFixedSize(440, 420);
  palette.setStyleSheet(
      QString("QDialog { background: %1; border: 1px solid %2; "
              "border-radius: 8px; }")
          .arg(m_theme.surfaceColor.name(), m_theme.borderColor.name()));

  auto *layout = new QVBoxLayout(&palette);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);

  auto *filterEdit = new QLineEdit(&palette);
  filterEdit->setPlaceholderText(tr("Type to filter commands..."));
  filterEdit->setStyleSheet(
      QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
              "border-radius: 6px; padding: 8px 12px; font-size: 13px; }")
          .arg(m_theme.surfaceAltColor.name(), m_theme.foregroundColor.name(),
               m_theme.borderColor.name()));
  layout->addWidget(filterEdit);

  auto *listWidget = new QListWidget(&palette);
  listWidget->setStyleSheet(
      QString("QListWidget { background: %1; color: %2; border: none; "
              "font-size: 13px; }"
              "QListWidget::item { padding: 6px 10px; border-radius: 4px; }"
              "QListWidget::item:selected { background: %3; }"
              "QListWidget::item:hover:!selected { background: %4; }")
          .arg(m_theme.surfaceColor.name(), m_theme.foregroundColor.name(),
               m_theme.accentSoftColor.name(), m_theme.hoverColor.name()));
  layout->addWidget(listWidget);

  using Command = QPair<QString, std::function<void()>>;
  QList<Command> commands = {
      {tr("Toggle Rewrite Mode"), [this]() { onToggleRewriteMode(); }},
      {tr("Squash Selected"), [this]() { onSquashSelected(); }},
      {tr("Drop Selected"), [this]() { onDropSelected(); }},
      {tr("Fixup Selected"), [this]() { onFixupSelected(); }},
      {tr("Reword Selected"), [this]() { onRewordSelected(); }},
      {tr("Pick All (Reset)"), [this]() { onPickAll(); }},
      {tr("Apply Rewrite"), [this]() { onApplyRebase(); }},
      {tr("Cherry-pick to Current"), [this]() { onCherryPickClicked(); }},
      {tr("Move to Branch"), [this]() { onMoveToBranchClicked(); }},
      {tr("Revert Commit"), [this]() { onRevertCommitClicked(); }},
      {tr("Edit Commit Message"), [this]() { onEditCommitMessage(); }},
      {tr("Create Branch"), [this]() { onCreateBranchClicked(); }},
      {tr("Rename Branch"), [this]() { onRenameBranchClicked(); }},
      {tr("Merge Branch"), [this]() { onMergeBranchClicked(); }},
      {tr("Rebase Branch"), [this]() { onRebaseBranchClicked(); }},
      {tr("Create Stash"), [this]() { onCreateStashClicked(); }},
      {tr("Recovery Center"), [this]() { showRecoveryCenter(); }},
  };

  for (const auto &cmd : commands) {
    listWidget->addItem(cmd.first);
  }

  connect(
      filterEdit, &QLineEdit::textChanged, [listWidget](const QString &text) {
        for (int i = 0; i < listWidget->count(); ++i) {
          auto *item = listWidget->item(i);
          item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
      });

  int selectedIndex = -1;
  auto executeSelection = [&]() {
    auto *item = listWidget->currentItem();
    if (!item || item->isHidden())
      return;
    for (int i = 0; i < listWidget->count(); ++i) {
      if (listWidget->item(i) == item) {
        selectedIndex = i;
        break;
      }
    }
    palette.accept();
  };

  connect(listWidget, &QListWidget::itemActivated,
          [&](QListWidgetItem *) { executeSelection(); });

  connect(filterEdit, &QLineEdit::returnPressed, [&]() {
    for (int i = 0; i < listWidget->count(); ++i) {
      if (!listWidget->item(i)->isHidden()) {
        listWidget->setCurrentRow(i);
        executeSelection();
        return;
      }
    }
  });

  filterEdit->setFocus();
  if (palette.exec() == QDialog::Accepted && selectedIndex >= 0 &&
      selectedIndex < commands.size()) {
    commands[selectedIndex].second();
  }
}

void GitWorkbenchDialog::onActionComboChanged(int) {}
