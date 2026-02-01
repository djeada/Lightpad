#include "sourcecontrolpanel.h"
#include "../dialogs/gitinitdialog.h"
#include "../dialogs/mergeconflictdialog.h"
#include "../dialogs/gitremotedialog.h"
#include "../dialogs/gitstashdialog.h"
#include <QDir>
#include <QHeaderView>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QInputDialog>
#include <QListWidget>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QToolTip>

// Constants for the commit history display
namespace {
    constexpr int DEFAULT_HISTORY_COMMIT_COUNT = 20;
    constexpr int MAX_COMMIT_DISPLAY_LENGTH = 60;
    constexpr int MAX_DIFF_PREVIEW_LENGTH = 2000;
    constexpr int STAGED_STATUS_ROLE = Qt::UserRole + 1;
}

SourceControlPanel::SourceControlPanel(QWidget* parent)
    : QWidget(parent)
    , m_git(nullptr)
    , m_stackedWidget(nullptr)
    , m_noRepoWidget(nullptr)
    , m_initRepoButton(nullptr)
    , m_noRepoLabel(nullptr)
    , m_conflictWidget(nullptr)
    , m_conflictLabel(nullptr)
    , m_conflictFilesList(nullptr)
    , m_resolveConflictsButton(nullptr)
    , m_abortMergeButton(nullptr)
    , m_repoWidget(nullptr)
    , m_branchLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_branchSelector(nullptr)
    , m_newBranchButton(nullptr)
    , m_deleteBranchButton(nullptr)
    , m_commitMessage(nullptr)
    , m_commitButton(nullptr)
    , m_amendCheckbox(nullptr)
    , m_stageAllButton(nullptr)
    , m_unstageAllButton(nullptr)
    , m_refreshButton(nullptr)
    , m_pushButton(nullptr)
    , m_pullButton(nullptr)
    , m_fetchButton(nullptr)
    , m_stashButton(nullptr)
    , m_stagedLabel(nullptr)
    , m_stagedTree(nullptr)
    , m_changesLabel(nullptr)
    , m_changesTree(nullptr)
    , m_historyHeader(nullptr)
    , m_historyLabel(nullptr)
    , m_historyTree(nullptr)
    , m_historyToggleButton(nullptr)
    , m_historyExpanded(false)
    , m_updatingBranchSelector(false)
    , m_stagedCount(0)
    , m_changesCount(0)
{
    setupUI();
}

SourceControlPanel::~SourceControlPanel()
{
}

void SourceControlPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header bar with title
    QWidget* header = new QWidget(this);
    header->setStyleSheet("background: #171c24; border-bottom: 1px solid #2a3241;");
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    QLabel* titleLabel = new QLabel(tr("Source Control"), header);
    titleLabel->setStyleSheet("font-weight: bold; color: #e6edf3; font-size: 13px;");
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    m_refreshButton = new QPushButton("↻", header);
    m_refreshButton->setFixedSize(24, 24);
    m_refreshButton->setToolTip(tr("Refresh (F5)"));
    m_refreshButton->setStyleSheet(
        "QPushButton { background: transparent; color: #e6edf3; border: none; font-size: 14px; border-radius: 4px; }"
        "QPushButton:hover { background: #2a3241; }"
        "QPushButton:pressed { background: #1f6feb; }"
    );
    connect(m_refreshButton, &QPushButton::clicked, this, &SourceControlPanel::onRefreshClicked);
    headerLayout->addWidget(m_refreshButton);

    mainLayout->addWidget(header);

    // Create stacked widget for different states
    m_stackedWidget = new QStackedWidget(this);
    
    // Setup the three different UI states
    setupNoRepoUI();
    setupMergeConflictUI();
    setupRepoUI();
    
    m_stackedWidget->addWidget(m_noRepoWidget);
    m_stackedWidget->addWidget(m_conflictWidget);
    m_stackedWidget->addWidget(m_repoWidget);
    
    mainLayout->addWidget(m_stackedWidget, 1);

    // Status bar with improved styling
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "  background: #161b22;"
        "  color: #8b949e;"
        "  padding: 6px 10px;"
        "  font-size: 11px;"
        "  border-top: 1px solid #21262d;"
        "}"
        "QLabel:hover {"
        "  color: #e6edf3;"
        "}"
    );
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(m_statusLabel);
}

void SourceControlPanel::setupNoRepoUI()
{
    m_noRepoWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_noRepoWidget);
    layout->setContentsMargins(20, 40, 20, 20);
    layout->setSpacing(16);
    
    // Icon
    QLabel* iconLabel = new QLabel("🗃️", m_noRepoWidget);
    iconLabel->setStyleSheet("font-size: 48px;");
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);
    
    // Title
    m_noRepoLabel = new QLabel(tr("No Git Repository"), m_noRepoWidget);
    m_noRepoLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #e6edf3;");
    m_noRepoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_noRepoLabel);
    
    // Description
    QLabel* descLabel = new QLabel(tr("This project is not a Git repository.\nInitialize one or open a Git project to start tracking changes."), m_noRepoWidget);
    descLabel->setStyleSheet("color: #8b949e; font-size: 12px;");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);
    
    // Initialize button
    m_initRepoButton = new QPushButton(tr("Initialize Repository"), m_noRepoWidget);
    m_initRepoButton->setStyleSheet(
        "QPushButton {"
        "  background: #238636;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 12px 24px;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background: #2ea043;"
        "  transform: translateY(-2px);"
        "  box-shadow: 0 4px 12px rgba(46, 160, 67, 0.4);"
        "}"
        "QPushButton:pressed {"
        "  background: #1a6f2a;"
        "  transform: translateY(0px);"
        "}"
    );
    connect(m_initRepoButton, &QPushButton::clicked, this, &SourceControlPanel::onInitRepositoryClicked);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_initRepoButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    
    layout->addStretch();
}

void SourceControlPanel::setupMergeConflictUI()
{
    m_conflictWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_conflictWidget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    
    // Warning header
    QWidget* warningHeader = new QWidget(m_conflictWidget);
    warningHeader->setStyleSheet("background: #f8514933; border: 1px solid #f85149; border-radius: 6px;");
    QHBoxLayout* warningLayout = new QHBoxLayout(warningHeader);
    warningLayout->setContentsMargins(12, 8, 12, 8);
    
    QLabel* warningIcon = new QLabel("⚠️", warningHeader);
    warningIcon->setStyleSheet("font-size: 18px;");
    warningLayout->addWidget(warningIcon);
    
    m_conflictLabel = new QLabel(tr("Merge Conflicts Detected"), warningHeader);
    m_conflictLabel->setStyleSheet("color: #f85149; font-weight: bold; font-size: 13px;");
    warningLayout->addWidget(m_conflictLabel, 1);
    
    layout->addWidget(warningHeader);
    
    // Conflict files list
    QLabel* conflictFilesLabel = new QLabel(tr("Conflicted Files:"), m_conflictWidget);
    conflictFilesLabel->setStyleSheet("color: #8b949e; font-size: 11px; text-transform: uppercase;");
    layout->addWidget(conflictFilesLabel);
    
    m_conflictFilesList = new QListWidget(m_conflictWidget);
    m_conflictFilesList->setStyleSheet(
        "QListWidget {"
        "  background: #161b22;"
        "  color: #e6edf3;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  padding: 8px;"
        "  border-bottom: 1px solid #21262d;"
        "}"
        "QListWidget::item:hover {"
        "  background: #21262d;"
        "}"
        "QListWidget::item:selected {"
        "  background: #1f6feb;"
        "  color: white;"
        "}"
        "QListWidget::item:selected:hover {"
        "  background: #388bfd;"
        "}"
        "QListWidget:focus {"
        "  border-color: #58a6ff;"
        "}"
    );
    layout->addWidget(m_conflictFilesList, 1);
    
    // Action buttons
    QHBoxLayout* actionLayout = new QHBoxLayout();
    
    m_resolveConflictsButton = new QPushButton(tr("Resolve Conflicts..."), m_conflictWidget);
    m_resolveConflictsButton->setStyleSheet(
        "QPushButton {"
        "  background: #1f6feb;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 16px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: #388bfd;"
        "  transform: translateY(-1px);"
        "  box-shadow: 0 2px 8px rgba(31, 111, 235, 0.4);"
        "}"
        "QPushButton:pressed {"
        "  background: #0d419d;"
        "  transform: translateY(0px);"
        "}"
    );
    connect(m_resolveConflictsButton, &QPushButton::clicked, this, &SourceControlPanel::onResolveConflictsClicked);
    actionLayout->addWidget(m_resolveConflictsButton);
    
    m_abortMergeButton = new QPushButton(tr("Abort Merge"), m_conflictWidget);
    m_abortMergeButton->setStyleSheet(
        "QPushButton {"
        "  background: #21262d;"
        "  color: #f85149;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 8px 16px;"
        "}"
        "QPushButton:hover {"
        "  background: #da3633;"
        "  color: white;"
        "  border-color: #da3633;"
        "  transform: translateY(-1px);"
        "  box-shadow: 0 2px 8px rgba(218, 54, 51, 0.4);"
        "}"
        "QPushButton:pressed {"
        "  background: #b62324;"
        "  transform: translateY(0px);"
        "}"
    );
    connect(m_abortMergeButton, &QPushButton::clicked, [this]() {
        if (m_git && m_git->abortMerge()) {
            refresh();
        }
    });
    actionLayout->addWidget(m_abortMergeButton);
    
    layout->addLayout(actionLayout);
}

void SourceControlPanel::setupRepoUI()
{
    m_repoWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(m_repoWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Branch management section
    QWidget* branchSection = new QWidget(m_repoWidget);
    branchSection->setStyleSheet("background: #161b22; border-bottom: 1px solid #2a3241;");
    QVBoxLayout* branchLayout = new QVBoxLayout(branchSection);
    branchLayout->setContentsMargins(8, 8, 8, 8);
    branchLayout->setSpacing(6);

    // Branch header with icon
    QHBoxLayout* branchHeaderLayout = new QHBoxLayout();
    branchHeaderLayout->setSpacing(6);
    
    QLabel* branchIcon = new QLabel("⎇", branchSection);
    branchIcon->setStyleSheet("color: #58a6ff; font-size: 14px;");
    branchHeaderLayout->addWidget(branchIcon);
    
    m_branchLabel = new QLabel(tr("Branch"), branchSection);
    m_branchLabel->setStyleSheet("color: #8b949e; font-size: 11px; text-transform: uppercase;");
    branchHeaderLayout->addWidget(m_branchLabel);
    branchHeaderLayout->addStretch();
    branchLayout->addLayout(branchHeaderLayout);

    // Branch selector row
    QHBoxLayout* branchSelectorLayout = new QHBoxLayout();
    branchSelectorLayout->setSpacing(4);

    m_branchSelector = new QComboBox(branchSection);
    m_branchSelector->setMinimumWidth(120);
    m_branchSelector->setStyleSheet(
        "QComboBox {"
        "  background: #21262d;"
        "  color: #e6edf3;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  font-size: 12px;"
        "  selection-background-color: #1f6feb;"
        "}"
        "QComboBox:hover {"
        "  border-color: #58a6ff;"
        "  background: #161b22;"
        "}"
        "QComboBox:focus {"
        "  border-color: #58a6ff;"
        "  border-width: 2px;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 20px;"
        "}"
        "QComboBox::down-arrow {"
        "  image: none;"
        "  border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent;"
        "  border-top: 5px solid #8b949e;"
        "  margin-right: 8px;"
        "}"
        "QComboBox::down-arrow:hover {"
        "  border-top-color: #e6edf3;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: #21262d;"
        "  color: #e6edf3;"
        "  border: 1px solid #30363d;"
        "  selection-background-color: #1f6feb;"
        "  outline: none;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  padding: 6px 10px;"
        "}"
        "QComboBox QAbstractItemView::item:hover {"
        "  background: #161b22;"
        "}"
    );
    connect(m_branchSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &SourceControlPanel::onBranchSelectorChanged);
    branchSelectorLayout->addWidget(m_branchSelector, 1);

    m_newBranchButton = new QPushButton("+", branchSection);
    m_newBranchButton->setFixedSize(28, 28);
    m_newBranchButton->setToolTip(tr("Create New Branch (Ctrl+Shift+B)"));
    m_newBranchButton->setStyleSheet(
        "QPushButton {"
        "  background: #238636;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: #2ea043;"
        "  transform: scale(1.05);"
        "}"
        "QPushButton:pressed {"
        "  background: #1a6f2a;"
        "}"
    );
    connect(m_newBranchButton, &QPushButton::clicked, this, &SourceControlPanel::onCreateBranchClicked);
    branchSelectorLayout->addWidget(m_newBranchButton);

    m_deleteBranchButton = new QPushButton("🗑", branchSection);
    m_deleteBranchButton->setFixedSize(28, 28);
    m_deleteBranchButton->setToolTip(tr("Delete Branch (Ctrl+Shift+D)"));
    m_deleteBranchButton->setStyleSheet(
        "QPushButton {"
        "  background: #21262d;"
        "  color: #f85149;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background: #da3633;"
        "  color: white;"
        "  border-color: #da3633;"
        "  transform: scale(1.05);"
        "}"
        "QPushButton:pressed {"
        "  background: #b62324;"
        "}"
        "QPushButton:disabled {"
        "  background: #161b22;"
        "  color: #484f58;"
        "  border-color: #30363d;"
        "}"
    );
    connect(m_deleteBranchButton, &QPushButton::clicked, this, &SourceControlPanel::onDeleteBranchClicked);
    branchSelectorLayout->addWidget(m_deleteBranchButton);

    branchLayout->addLayout(branchSelectorLayout);
    
    // Remote operations row
    QHBoxLayout* remoteOpsLayout = new QHBoxLayout();
    remoteOpsLayout->setSpacing(4);
    
    m_pullButton = new QPushButton("⬇ Pull", branchSection);
    m_pullButton->setToolTip(tr("Pull from Remote (Ctrl+Shift+Pull)"));
    m_pullButton->setStyleSheet(
        "QPushButton {"
        "  background: #21262d;"
        "  color: #58a6ff;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "  background: #1f6feb;"
        "  color: white;"
        "  border-color: #1f6feb;"
        "}"
        "QPushButton:pressed {"
        "  background: #0d419d;"
        "}"
        "QPushButton:disabled {"
        "  background: #161b22;"
        "  color: #484f58;"
        "  border-color: #30363d;"
        "}"
    );
    connect(m_pullButton, &QPushButton::clicked, this, &SourceControlPanel::onPullClicked);
    remoteOpsLayout->addWidget(m_pullButton);
    
    m_pushButton = new QPushButton("⬆ Push", branchSection);
    m_pushButton->setToolTip(tr("Push to Remote (Ctrl+Shift+Push)"));
    m_pushButton->setStyleSheet(
        "QPushButton {"
        "  background: #21262d;"
        "  color: #3fb950;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "  background: #238636;"
        "  color: white;"
        "  border-color: #238636;"
        "}"
        "QPushButton:pressed {"
        "  background: #1a6f2a;"
        "}"
        "QPushButton:disabled {"
        "  background: #161b22;"
        "  color: #484f58;"
        "  border-color: #30363d;"
        "}"
    );
    connect(m_pushButton, &QPushButton::clicked, this, &SourceControlPanel::onPushClicked);
    remoteOpsLayout->addWidget(m_pushButton);
    
    m_fetchButton = new QPushButton("🔄 Fetch", branchSection);
    m_fetchButton->setToolTip(tr("Fetch from Remote"));
    m_fetchButton->setStyleSheet(
        "QPushButton {"
        "  background: #21262d;"
        "  color: #8b949e;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "  background: #30363d;"
        "  color: #e6edf3;"
        "}"
        "QPushButton:pressed {"
        "  background: #21262d;"
        "}"
        "QPushButton:disabled {"
        "  background: #161b22;"
        "  color: #484f58;"
        "  border-color: #30363d;"
        "}"
    );
    connect(m_fetchButton, &QPushButton::clicked, this, &SourceControlPanel::onFetchClicked);
    remoteOpsLayout->addWidget(m_fetchButton);
    
    m_stashButton = new QPushButton("📦 Stash", branchSection);
    m_stashButton->setToolTip(tr("Stash Changes"));
    m_stashButton->setStyleSheet(
        "QPushButton {"
        "  background: #21262d;"
        "  color: #a371f7;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "  background: #8957e5;"
        "  color: white;"
        "  border-color: #8957e5;"
        "}"
        "QPushButton:pressed {"
        "  background: #6e40c9;"
        "}"
        "QPushButton:disabled {"
        "  background: #161b22;"
        "  color: #484f58;"
        "  border-color: #30363d;"
        "}"
    );
    connect(m_stashButton, &QPushButton::clicked, this, &SourceControlPanel::onStashClicked);
    remoteOpsLayout->addWidget(m_stashButton);
    
    branchLayout->addLayout(remoteOpsLayout);
    mainLayout->addWidget(branchSection);

    // Commit section with improved styling
    QWidget* commitSection = new QWidget(m_repoWidget);
    commitSection->setStyleSheet("background: #0d1117;");
    QVBoxLayout* commitLayout = new QVBoxLayout(commitSection);
    commitLayout->setContentsMargins(8, 10, 8, 10);
    commitLayout->setSpacing(8);

    // Commit header
    QLabel* commitHeader = new QLabel(tr("💬 Commit Message"), commitSection);
    commitHeader->setStyleSheet("color: #8b949e; font-size: 11px; text-transform: uppercase; letter-spacing: 0.5px;");
    commitLayout->addWidget(commitHeader);

    m_commitMessage = new QTextEdit(commitSection);
    m_commitMessage->setPlaceholderText(tr("Enter commit message (required)\n\nWrite a short summary in the first line,\nthen add a blank line followed by details."));
    m_commitMessage->setMinimumHeight(90);
    m_commitMessage->setMaximumHeight(120);
    m_commitMessage->setStyleSheet(
        "QTextEdit {"
        "  background: #21262d;"
        "  color: #e6edf3;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  font-size: 12px;"
        "  line-height: 1.4;"
        "  selection-background-color: #1f6feb;"
        "}"
        "QTextEdit:focus {"
        "  border-color: #58a6ff;"
        "  border-width: 2px;"
        "  background: #161b22;"
        "}"
        "QTextEdit:hover {"
        "  border-color: #8b949e;"
        "}"
    );
    connect(m_commitMessage, &QTextEdit::textChanged, this, &SourceControlPanel::onCommitMessageChanged);
    commitLayout->addWidget(m_commitMessage);

    // Commit options row
    QHBoxLayout* commitOptionsLayout = new QHBoxLayout();
    commitOptionsLayout->setSpacing(8);
    
    m_amendCheckbox = new QCheckBox(tr("Amend last commit"), commitSection);
    m_amendCheckbox->setStyleSheet(
        "QCheckBox {"
        "  color: #8b949e;"
        "  font-size: 11px;"
        "}"
        "QCheckBox:hover {"
        "  color: #e6edf3;"
        "}"
        "QCheckBox::indicator {"
        "  width: 14px;"
        "  height: 14px;"
        "  border-radius: 3px;"
        "  border: 1px solid #30363d;"
        "  background: #21262d;"
        "}"
        "QCheckBox::indicator:hover {"
        "  border-color: #58a6ff;"
        "  background: #161b22;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background: #238636;"
        "  border-color: #238636;"
        "}"
        "QCheckBox::indicator:checked:hover {"
        "  background: #2ea043;"
        "  border-color: #2ea043;"
        "}"
    );
    commitOptionsLayout->addWidget(m_amendCheckbox);
    commitOptionsLayout->addStretch();
    commitLayout->addLayout(commitOptionsLayout);

    // Commit button row
    QHBoxLayout* commitButtonLayout = new QHBoxLayout();
    commitButtonLayout->setSpacing(8);
    
    m_commitButton = new QPushButton(tr("✓ Commit"), commitSection);
    m_commitButton->setMinimumHeight(32);
    m_commitButton->setToolTip(tr("Commit Staged Changes (Ctrl+Enter)"));
    m_commitButton->setStyleSheet(
        "QPushButton {"
        "  background: #238636;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 20px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background: #2ea043;"
        "  transform: translateY(-1px);"
        "  box-shadow: 0 2px 8px rgba(46, 160, 67, 0.4);"
        "}"
        "QPushButton:pressed {"
        "  background: #1a6f2a;"
        "  transform: translateY(0px);"
        "}"
        "QPushButton:disabled {"
        "  background: #21262d;"
        "  color: #484f58;"
        "}"
    );
    connect(m_commitButton, &QPushButton::clicked, this, &SourceControlPanel::onCommitClicked);
    commitButtonLayout->addWidget(m_commitButton, 1);
    
    commitLayout->addLayout(commitButtonLayout);
    mainLayout->addWidget(commitSection);

    // Staged changes section
    QWidget* stagedHeader = new QWidget(m_repoWidget);
    stagedHeader->setStyleSheet("background: #161b22; border-bottom: 1px solid #21262d;");
    QHBoxLayout* stagedHeaderLayout = new QHBoxLayout(stagedHeader);
    stagedHeaderLayout->setContentsMargins(8, 6, 8, 6);
    
    QLabel* stagedIcon = new QLabel("📦", stagedHeader);
    stagedIcon->setStyleSheet("font-size: 12px;");
    stagedHeaderLayout->addWidget(stagedIcon);
    
    m_stagedLabel = new QLabel(tr("Staged Changes"), stagedHeader);
    m_stagedLabel->setStyleSheet("color: #3fb950; font-weight: bold; font-size: 12px;");
    stagedHeaderLayout->addWidget(m_stagedLabel);
    
    stagedHeaderLayout->addStretch();
    
    m_unstageAllButton = new QPushButton("−", stagedHeader);
    m_unstageAllButton->setFixedSize(22, 22);
    m_unstageAllButton->setToolTip(tr("Unstage All (Ctrl+U)"));
    m_unstageAllButton->setStyleSheet(
        "QPushButton { background: transparent; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: #21262d; color: #f85149; border-color: #f85149; }"
        "QPushButton:pressed { background: #da3633; color: white; }"
        "QPushButton:disabled { color: #484f58; border-color: #30363d; }"
    );
    connect(m_unstageAllButton, &QPushButton::clicked, this, &SourceControlPanel::onUnstageAllClicked);
    stagedHeaderLayout->addWidget(m_unstageAllButton);
    
    mainLayout->addWidget(stagedHeader);

    m_stagedTree = new QTreeWidget(m_repoWidget);
    m_stagedTree->setHeaderHidden(true);
    m_stagedTree->setRootIsDecorated(false);
    m_stagedTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_stagedTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_stagedTree->setStyleSheet(
        "QTreeWidget {"
        "  background: #0d1117;"
        "  color: #e6edf3;"
        "  border: none;"
        "  font-size: 12px;"
        "  outline: none;"
        "}"
        "QTreeWidget::item {"
        "  padding: 6px 8px;"
        "  border-bottom: 1px solid #21262d;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: #1f6feb;"
        "  color: white;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: #161b22;"
        "}"
        "QTreeWidget::item:selected:hover {"
        "  background: #388bfd;"
        "}"
        "QTreeWidget:focus {"
        "  border: 1px solid #58a6ff;"
        "}"
    );
    m_stagedTree->setMinimumHeight(80);
    m_stagedTree->setMaximumHeight(150);
    m_stagedTree->setUniformRowHeights(true);
    m_stagedTree->setMouseTracking(true);
    connect(m_stagedTree, &QTreeWidget::itemDoubleClicked, this, &SourceControlPanel::onItemDoubleClicked);
    connect(m_stagedTree, &QTreeWidget::customContextMenuRequested, this, &SourceControlPanel::onItemContextMenu);
    connect(m_stagedTree, &QTreeWidget::itemEntered, [this](QTreeWidgetItem* item, int) {
        if (item && m_statusLabel) {
            QString path = item->data(0, Qt::UserRole).toString();
            if (!path.isEmpty()) {
                m_statusLabel->setText(QString("📁 %1").arg(path));
            }
        }
    });
    mainLayout->addWidget(m_stagedTree);

    // Changes section
    QWidget* changesHeader = new QWidget(m_repoWidget);
    changesHeader->setStyleSheet("background: #161b22; border-bottom: 1px solid #21262d;");
    QHBoxLayout* changesHeaderLayout = new QHBoxLayout(changesHeader);
    changesHeaderLayout->setContentsMargins(8, 6, 8, 6);
    
    QLabel* changesIcon = new QLabel("📝", changesHeader);
    changesIcon->setStyleSheet("font-size: 12px;");
    changesHeaderLayout->addWidget(changesIcon);
    
    m_changesLabel = new QLabel(tr("Changes"), changesHeader);
    m_changesLabel->setStyleSheet("color: #e2c08d; font-weight: bold; font-size: 12px;");
    changesHeaderLayout->addWidget(m_changesLabel);
    
    changesHeaderLayout->addStretch();
    
    m_stageAllButton = new QPushButton("+", changesHeader);
    m_stageAllButton->setFixedSize(22, 22);
    m_stageAllButton->setToolTip(tr("Stage All (Ctrl+A)"));
    m_stageAllButton->setStyleSheet(
        "QPushButton { background: transparent; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: #21262d; color: #3fb950; border-color: #3fb950; }"
        "QPushButton:pressed { background: #238636; color: white; }"
        "QPushButton:disabled { color: #484f58; border-color: #30363d; }"
    );
    connect(m_stageAllButton, &QPushButton::clicked, this, &SourceControlPanel::onStageAllClicked);
    changesHeaderLayout->addWidget(m_stageAllButton);
    
    mainLayout->addWidget(changesHeader);

    m_changesTree = new QTreeWidget(m_repoWidget);
    m_changesTree->setHeaderHidden(true);
    m_changesTree->setRootIsDecorated(false);
    m_changesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_changesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_changesTree->setStyleSheet(
        "QTreeWidget {"
        "  background: #0d1117;"
        "  color: #e6edf3;"
        "  border: none;"
        "  font-size: 12px;"
        "  outline: none;"
        "}"
        "QTreeWidget::item {"
        "  padding: 6px 8px;"
        "  border-bottom: 1px solid #21262d;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: #1f6feb;"
        "  color: white;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: #161b22;"
        "}"
        "QTreeWidget::item:selected:hover {"
        "  background: #388bfd;"
        "}"
        "QTreeWidget:focus {"
        "  border: 1px solid #58a6ff;"
        "}"
    );
    m_changesTree->setUniformRowHeights(true);
    m_changesTree->setMouseTracking(true);
    connect(m_changesTree, &QTreeWidget::itemDoubleClicked, this, &SourceControlPanel::onItemDoubleClicked);
    connect(m_changesTree, &QTreeWidget::customContextMenuRequested, this, &SourceControlPanel::onItemContextMenu);
    connect(m_changesTree, &QTreeWidget::itemEntered, [this](QTreeWidgetItem* item, int) {
        if (item && m_statusLabel) {
            QString path = item->data(0, Qt::UserRole).toString();
            if (!path.isEmpty()) {
                m_statusLabel->setText(QString("📁 %1").arg(path));
            }
        }
    });
    mainLayout->addWidget(m_changesTree, 1);

    // Commit history section (collapsible)
    m_historyHeader = new QWidget(m_repoWidget);
    m_historyHeader->setStyleSheet("background: #161b22; border-bottom: 1px solid #21262d;");
    m_historyHeader->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* historyHeaderLayout = new QHBoxLayout(m_historyHeader);
    historyHeaderLayout->setContentsMargins(8, 6, 8, 6);
    
    m_historyToggleButton = new QPushButton("▶", m_historyHeader);
    m_historyToggleButton->setFixedSize(16, 16);
    m_historyToggleButton->setStyleSheet(
        "QPushButton { background: transparent; color: #8b949e; border: none; font-size: 10px; padding: 0; }"
        "QPushButton:hover { color: #e6edf3; }"
    );
    historyHeaderLayout->addWidget(m_historyToggleButton);
    
    QLabel* historyIcon = new QLabel("📜", m_historyHeader);
    historyIcon->setStyleSheet("font-size: 12px;");
    historyHeaderLayout->addWidget(historyIcon);
    
    m_historyLabel = new QLabel(tr("Recent Commits"), m_historyHeader);
    m_historyLabel->setStyleSheet("color: #a371f7; font-weight: bold; font-size: 12px;");
    historyHeaderLayout->addWidget(m_historyLabel);
    
    historyHeaderLayout->addStretch();
    
    mainLayout->addWidget(m_historyHeader);
    
    // Connect click on header to toggle
    connect(m_historyToggleButton, &QPushButton::clicked, [this]() {
        m_historyExpanded = !m_historyExpanded;
        m_historyToggleButton->setText(m_historyExpanded ? "▼" : "▶");
        m_historyTree->setVisible(m_historyExpanded);
        if (m_historyExpanded) {
            updateHistory();
        }
    });

    m_historyTree = new QTreeWidget(m_repoWidget);
    m_historyTree->setHeaderHidden(true);
    m_historyTree->setRootIsDecorated(false);
    m_historyTree->setVisible(false);  // Initially collapsed
    m_historyTree->setStyleSheet(
        "QTreeWidget {"
        "  background: #0d1117;"
        "  color: #e6edf3;"
        "  border: none;"
        "  font-size: 11px;"
        "  outline: none;"
        "}"
        "QTreeWidget::item {"
        "  padding: 4px 8px;"
        "  border-bottom: 1px solid #21262d;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: #1f6feb;"
        "  color: white;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: #161b22;"
        "}"
        "QTreeWidget::item:selected:hover {"
        "  background: #388bfd;"
        "}"
        "QTreeWidget:focus {"
        "  border: 1px solid #58a6ff;"
        "}"
    );
    m_historyTree->setMinimumHeight(100);
    m_historyTree->setMaximumHeight(200);
    m_historyTree->setUniformRowHeights(true);
    
    // Connect double-click to view commit diff
    connect(m_historyTree, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem* item, int) {
        if (!item || !m_git) return;
        QString commitHash = item->data(0, Qt::UserRole).toString();
        if (!commitHash.isEmpty()) {
            // Show commit diff in a message box for now (could be improved later)
            QString diff = m_git->getCommitDiff(commitHash);
            GitCommitInfo info = m_git->getCommitDetails(commitHash);
            
            QString message = QString("%1\n\nAuthor: %2 <%3>\nDate: %4\n\n%5")
                .arg(info.subject)
                .arg(info.author)
                .arg(info.authorEmail)
                .arg(info.relativeDate)
                .arg(info.body.isEmpty() ? "(No additional message)" : info.body);
            
            if (!diff.isEmpty()) {
                message += QString("\n\n--- Diff ---\n%1").arg(diff.left(MAX_DIFF_PREVIEW_LENGTH));
                if (diff.length() > MAX_DIFF_PREVIEW_LENGTH) {
                    message += "\n... (truncated)";
                }
            }
            
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Commit: %1").arg(info.shortHash));
            msgBox.setText(info.subject);
            msgBox.setDetailedText(message);
            msgBox.setStyleSheet(
                "QMessageBox { background: #0d1117; }"
                "QMessageBox QLabel { color: #e6edf3; }"
                "QPushButton { background: #21262d; color: #e6edf3; border: 1px solid #30363d; border-radius: 6px; padding: 6px 16px; }"
                "QPushButton:hover { background: #30363d; }"
                "QTextEdit { background: #161b22; color: #e6edf3; font-family: monospace; }"
            );
            msgBox.exec();
        }
    });
    
    mainLayout->addWidget(m_historyTree);
}

void SourceControlPanel::setGitIntegration(GitIntegration* git)
{
    m_git = git;
    
    if (m_git) {
        connect(m_git, &GitIntegration::statusChanged, this, &SourceControlPanel::onStatusChanged);
        connect(m_git, &GitIntegration::branchChanged, this, &SourceControlPanel::onBranchChanged);
        connect(m_git, &GitIntegration::operationCompleted, this, &SourceControlPanel::onOperationCompleted);
        connect(m_git, &GitIntegration::errorOccurred, this, &SourceControlPanel::onErrorOccurred);
        connect(m_git, &GitIntegration::mergeConflictsDetected, this, &SourceControlPanel::onMergeConflictsDetected);
        connect(m_git, &GitIntegration::statusChanged, this, &SourceControlPanel::onCommitMessageChanged);
        refresh();
    }
}

void SourceControlPanel::setWorkingPath(const QString& path)
{
    m_workingPath = path;
    if (m_git) {
        m_git->setWorkingPath(path);
    }
    updateUIState();
}

void SourceControlPanel::refresh()
{
    updateUIState();
    
    if (!m_git || !m_git->isValidRepository()) {
        resetChangeCounts();
        if (m_stagedLabel) m_stagedLabel->setText(tr("Staged Changes"));
        if (m_changesLabel) m_changesLabel->setText(tr("Changes"));
        if (m_historyLabel) m_historyLabel->setText(tr("Recent Commits"));
        if (m_branchLabel) m_branchLabel->setText(tr("Branch"));
        if (m_branchSelector) m_branchSelector->clear();
        if (m_statusLabel) m_statusLabel->setText("");
        if (m_stagedTree) m_stagedTree->clear();
        if (m_changesTree) m_changesTree->clear();
        if (m_commitButton) m_commitButton->setEnabled(false);
        return;
    }
    
    updateBranchSelector();
    updateTree();
    if (m_historyExpanded) {
        updateHistory();
    }
    onCommitMessageChanged();
}

void SourceControlPanel::updateUIState()
{
    if (!m_stackedWidget) return;
    
    if (!m_git || !m_git->isValidRepository()) {
        // Show "no repository" state
        m_stackedWidget->setCurrentWidget(m_noRepoWidget);
        if (m_statusLabel) m_statusLabel->setText(tr("No Git repository"));
        if (m_pullButton) m_pullButton->setEnabled(false);
        if (m_pushButton) m_pushButton->setEnabled(false);
        if (m_fetchButton) m_fetchButton->setEnabled(false);
        if (m_stashButton) m_stashButton->setEnabled(false);
        if (m_stageAllButton) m_stageAllButton->setEnabled(false);
        if (m_unstageAllButton) m_unstageAllButton->setEnabled(false);
        if (m_commitButton) m_commitButton->setEnabled(false);
    } else if (m_git->hasMergeConflicts()) {
        // Show merge conflict state
        m_stackedWidget->setCurrentWidget(m_conflictWidget);
        
        // Update conflict files list
        if (m_conflictFilesList) {
            m_conflictFilesList->clear();
            QStringList conflicts = m_git->getConflictedFiles();
            for (const QString& file : conflicts) {
                QFileInfo fileInfo(file);
                QListWidgetItem* item = new QListWidgetItem(m_conflictFilesList);
                item->setText(QString("❗ %1").arg(fileInfo.fileName()));
                item->setToolTip(file);
                item->setData(Qt::UserRole, file);
            }
            if (m_conflictLabel) {
                m_conflictLabel->setText(QString(tr("%1 Conflict(s) Detected")).arg(conflicts.size()));
            }
        }
        if (m_statusLabel) m_statusLabel->setText(tr("⚠️ Merge conflicts - resolve before continuing"));
        if (m_pullButton) m_pullButton->setEnabled(false);
        if (m_pushButton) m_pushButton->setEnabled(false);
        if (m_fetchButton) m_fetchButton->setEnabled(false);
        if (m_stashButton) m_stashButton->setEnabled(false);
        if (m_stageAllButton) m_stageAllButton->setEnabled(false);
        if (m_unstageAllButton) m_unstageAllButton->setEnabled(false);
        if (m_commitButton) m_commitButton->setEnabled(false);
    } else {
        // Show normal repository state
        m_stackedWidget->setCurrentWidget(m_repoWidget);
        if (m_pullButton) m_pullButton->setEnabled(true);
        if (m_pushButton) m_pushButton->setEnabled(true);
        if (m_fetchButton) m_fetchButton->setEnabled(true);
        if (m_stashButton) m_stashButton->setEnabled(true);
        if (m_stageAllButton) m_stageAllButton->setEnabled(true);
        if (m_unstageAllButton) m_unstageAllButton->setEnabled(true);
    }
}

void SourceControlPanel::updateBranchSelector()
{
    if (!m_git || !m_git->isValidRepository()) {
        m_branchSelector->clear();
        m_branchLabel->setText(tr("No repository"));
        return;
    }
    
    m_updatingBranchSelector = true;
    
    QString currentBranch = m_git->currentBranch();
    QList<GitBranchInfo> branches = m_git->getBranches();
    
    m_branchSelector->clear();
    int currentIndex = 0;
    
    for (int i = 0; i < branches.size(); ++i) {
        const GitBranchInfo& branch = branches[i];
        if (!branch.isRemote) {
            QString displayName = branch.name;
            if (branch.isCurrent) {
                currentIndex = m_branchSelector->count();
            }
            m_branchSelector->addItem(displayName, branch.name);
        }
    }
    
    m_branchSelector->setCurrentIndex(currentIndex);
    m_branchLabel->setText(tr("Branch"));
    
    // Disable delete button for current branch
    m_deleteBranchButton->setEnabled(m_branchSelector->count() > 1);
    
    m_updatingBranchSelector = false;
}

void SourceControlPanel::updateBranchLabel()
{
    if (!m_git || !m_git->isValidRepository()) {
        m_branchLabel->setText(tr("No repository"));
        return;
    }
    
    m_branchLabel->setText(tr("Branch"));
    updateBranchSelector();
}

void SourceControlPanel::updateTree()
{
    m_stagedTree->clear();
    m_changesTree->clear();
    resetChangeCounts();
    
    if (!m_git || !m_git->isValidRepository()) {
        m_statusLabel->setText(tr("Not a git repository"));
        return;
    }
    
    QList<GitFileInfo> status = m_git->getStatus();
    
    for (const GitFileInfo& file : status) {
        QString fullPath = file.filePath;
        if (!m_git->repositoryPath().isEmpty() && !fullPath.startsWith('/')) {
            fullPath = m_git->repositoryPath() + "/" + file.filePath;
        }

        // Check if file is staged (has index status)
        if (file.indexStatus != GitFileStatus::Clean && 
            file.indexStatus != GitFileStatus::Untracked) {
            QTreeWidgetItem* item = new QTreeWidgetItem(m_stagedTree);
            
            QString icon = statusIcon(file.indexStatus);
            QFileInfo fileInfo(fullPath);
            item->setText(0, QString("%1 %2").arg(icon).arg(fileInfo.fileName()));
            item->setToolTip(0, fullPath);
            item->setData(0, Qt::UserRole, fullPath);
            item->setData(0, STAGED_STATUS_ROLE, true);  // Is staged
            item->setForeground(0, statusColor(file.indexStatus));
            
            m_stagedCount++;
        }
        
        // Check if file has working tree changes
        if (file.workTreeStatus != GitFileStatus::Clean) {
            QTreeWidgetItem* item = new QTreeWidgetItem(m_changesTree);
            
            QString icon = statusIcon(file.workTreeStatus);
            QFileInfo fileInfo(fullPath);
            item->setText(0, QString("%1 %2").arg(icon).arg(fileInfo.fileName()));
            item->setToolTip(0, fullPath);
            item->setData(0, Qt::UserRole, fullPath);
            item->setData(0, STAGED_STATUS_ROLE, false);  // Not staged
            item->setForeground(0, statusColor(file.workTreeStatus));
            
            m_changesCount++;
        }
    }

    if (m_stagedLabel) {
        m_stagedLabel->setText(tr("Staged Changes (%1)").arg(m_stagedCount));
    }
    if (m_changesLabel) {
        m_changesLabel->setText(tr("Changes (%1)").arg(m_changesCount));
    }
    if (m_stagedCount == 0 && m_changesCount == 0) {
        m_statusLabel->setText(tr("Working tree clean"));
        m_statusLabel->setToolTip(QString());
    } else {
        QString statusText = QString(tr("%1 staged, %2 changed")).arg(m_stagedCount).arg(m_changesCount);
        m_statusLabel->setText(statusText);
    }

    m_commitButton->setEnabled(m_stagedCount > 0 && !m_commitMessage->toPlainText().trimmed().isEmpty());
    m_stageAllButton->setEnabled(m_changesCount > 0);
    m_unstageAllButton->setEnabled(m_stagedCount > 0);
    if (m_stagedTree) {
        m_stagedTree->setToolTip(m_stagedCount == 0 ? tr("No staged changes.") : QString());
    }
    if (m_changesTree) {
        m_changesTree->setToolTip(m_changesCount == 0 ? tr("Working tree clean") : QString());
    }
}

void SourceControlPanel::resetChangeCounts()
{
    m_stagedCount = 0;
    m_changesCount = 0;
}

void SourceControlPanel::stageOrUnstageSelectedFiles(QTreeWidget* tree, bool stage)
{
    if (!m_git || !tree) return;

    QList<QTreeWidgetItem*> selectedItems = tree->selectedItems();
    bool didChange = false;
    for (QTreeWidgetItem* selected : selectedItems) {
        if (!selected) continue;
        QString selectedPath = selected->data(0, Qt::UserRole).toString();
        if (selectedPath.isEmpty()) {
            continue;
        }
        if (stage) {
            if (m_git->stageFile(selectedPath)) {
                didChange = true;
            }
        } else {
            if (m_git->unstageFile(selectedPath)) {
                didChange = true;
            }
        }
    }

    if (didChange) {
        refresh();
    }
}

void SourceControlPanel::updateHistory()
{
    if (!m_historyTree) return;
    
    m_historyTree->clear();
    
    if (!m_git || !m_git->isValidRepository()) {
        return;
    }
    
    QList<GitCommitInfo> commits = m_git->getCommitLog(DEFAULT_HISTORY_COMMIT_COUNT);
    if (m_historyLabel) {
        m_historyLabel->setText(tr("Recent Commits (%1)").arg(commits.size()));
    }
    
    for (const GitCommitInfo& commit : commits) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_historyTree);
        
        // Format: short_hash subject (relative_date)
        QString displayText = QString("%1  %2").arg(commit.shortHash).arg(commit.subject);
        if (displayText.length() > MAX_COMMIT_DISPLAY_LENGTH) {
            displayText = displayText.left(MAX_COMMIT_DISPLAY_LENGTH - 3) + "...";
        }
        
        item->setText(0, displayText);
        item->setToolTip(0, QString("%1\n\nAuthor: %2\nDate: %3\n\n%4")
            .arg(commit.hash)
            .arg(commit.author)
            .arg(commit.relativeDate)
            .arg(commit.subject));
        item->setData(0, Qt::UserRole, commit.hash);
        
        // Color the hash part differently
        QFont font = item->font(0);
        font.setFamily("monospace");
        item->setFont(0, font);
        
        // Set foreground color based on commit type
        if (commit.parents.size() > 1) {
            // Merge commit - show in purple
            item->setForeground(0, QColor("#a371f7"));
        } else {
            item->setForeground(0, QColor("#8b949e"));
        }
    }
}

void SourceControlPanel::onCommitMessageChanged()
{
    if (!m_git || !m_git->isValidRepository()) {
        if (m_commitButton) m_commitButton->setEnabled(false);
        return;
    }

    bool hasMessage = m_commitMessage && !m_commitMessage->toPlainText().trimmed().isEmpty();
    if (m_commitButton) {
        m_commitButton->setEnabled(m_stagedCount > 0 && hasMessage);
    }
}

void SourceControlPanel::onStageAllClicked()
{
    if (m_git) {
        m_git->stageAll();
    }
}

void SourceControlPanel::onUnstageAllClicked()
{
    if (!m_git) return;
    
    QList<GitFileInfo> status = m_git->getStatus();
    for (const GitFileInfo& file : status) {
        if (file.indexStatus != GitFileStatus::Clean && 
            file.indexStatus != GitFileStatus::Untracked) {
            m_git->unstageFile(file.filePath);
        }
    }
}

void SourceControlPanel::onCommitClicked()
{
    if (!m_git) return;
    
    QString message = m_commitMessage->toPlainText().trimmed();
    if (message.isEmpty()) {
        QMessageBox::warning(this, tr("Commit"), tr("Please enter a commit message."));
        return;
    }

    if (m_amendCheckbox && m_amendCheckbox->isChecked()) {
        QMessageBox::warning(this, tr("Commit"), tr("Amend is not supported yet."));
        return;
    }
    
    if (m_git->commit(message)) {
        m_commitMessage->clear();
    }
}

void SourceControlPanel::onRefreshClicked()
{
    if (m_git) {
        m_git->refresh();
    }
    refresh();
}

void SourceControlPanel::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    if (!item) return;
    
    QString filePath = item->data(0, Qt::UserRole).toString();
    if (!filePath.isEmpty()) {
        emit fileOpenRequested(filePath);
    }
}

void SourceControlPanel::onItemContextMenu(const QPoint& pos)
{
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(sender());
    if (!tree) return;
    
    QTreeWidgetItem* item = tree->itemAt(pos);
    if (!item) return;
    
    QString filePath = item->data(0, Qt::UserRole).toString();
    bool isStaged = item->data(0, STAGED_STATUS_ROLE).toBool();
    QString repoPath = m_git ? m_git->repositoryPath() : QString();
    QString relativePath = filePath;
    if (!repoPath.isEmpty() && filePath.startsWith(repoPath)) {
        relativePath = filePath.mid(repoPath.length() + 1);
    }
    
    QMenu menu(this);
    
    if (isStaged) {
        QAction* unstageAction = menu.addAction(tr("Unstage"));
        connect(unstageAction, &QAction::triggered, [this, filePath]() {
            if (m_git) {
                m_git->unstageFile(filePath);
                refresh();
            }
        });

        QAction* unstageSelectedAction = menu.addAction(tr("Unstage Selected"));
        connect(unstageSelectedAction, &QAction::triggered, [this, tree]() {
            stageOrUnstageSelectedFiles(tree, false);
        });
    } else {
        QAction* stageAction = menu.addAction(tr("Stage"));
        connect(stageAction, &QAction::triggered, [this, filePath]() {
            if (m_git) {
                m_git->stageFile(filePath);
                refresh();
            }
        });
        
        QAction* stageSelectedAction = menu.addAction(tr("Stage Selected"));
        connect(stageSelectedAction, &QAction::triggered, [this, tree]() {
            stageOrUnstageSelectedFiles(tree, true);
        });

        menu.addSeparator();
        
        QAction* discardAction = menu.addAction(tr("Discard Changes"));
        connect(discardAction, &QAction::triggered, [this, filePath]() {
            if (m_git) {
                if (QMessageBox::question(this, tr("Discard Changes"),
                    tr("Are you sure you want to discard changes to this file?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
                    m_git->discardChanges(filePath);
                }
            }
        });
    }
    
    menu.addSeparator();
    
    QAction* openAction = menu.addAction(tr("Open File"));
    connect(openAction, &QAction::triggered, [this, filePath]() {
        emit fileOpenRequested(filePath);
    });
    
    QAction* diffAction = menu.addAction(isStaged ? tr("View Staged Diff") : tr("View Unstaged Diff"));
    connect(diffAction, &QAction::triggered, [this, filePath, isStaged]() {
        emit diffRequested(filePath, isStaged);
    });

    QAction* copyPathAction = menu.addAction(tr("Copy Path"));
    connect(copyPathAction, &QAction::triggered, [this, filePath, relativePath]() {
        QString value = relativePath;
        if (!value.isEmpty()) {
            QApplication::clipboard()->setText(value);
            QToolTip::showText(QCursor::pos(), tr("Copied path: %1").arg(value), this);
        }
    });
    
    menu.exec(tree->mapToGlobal(pos));
}

void SourceControlPanel::onStatusChanged()
{
    refresh();
}

void SourceControlPanel::onBranchChanged(const QString& branchName)
{
    Q_UNUSED(branchName);
    updateBranchLabel();
}

void SourceControlPanel::onBranchSelectorChanged(int index)
{
    if (m_updatingBranchSelector || !m_git || index < 0) return;
    
    QString branchName = m_branchSelector->itemData(index).toString();
    QString currentBranch = m_git->currentBranch();
    
    if (branchName != currentBranch && !branchName.isEmpty()) {
        m_git->checkoutBranch(branchName);
    }
}

void SourceControlPanel::onCreateBranchClicked()
{
    if (!m_git) return;
    
    bool ok;
    QString branchName = QInputDialog::getText(this, 
        tr("Create Branch"),
        tr("Enter new branch name:"),
        QLineEdit::Normal,
        "",
        &ok);
    
    if (ok && !branchName.isEmpty()) {
        // Remove spaces and invalid characters
        branchName = branchName.trimmed().replace(" ", "-");
        if (m_git->createBranch(branchName, true)) {
            updateBranchSelector();
        }
    }
}

void SourceControlPanel::onDeleteBranchClicked()
{
    if (!m_git || m_branchSelector->count() <= 1) return;
    
    QString currentBranch = m_git->currentBranch();
    QString selectedBranch = m_branchSelector->currentData().toString();
    
    if (selectedBranch == currentBranch) {
        QMessageBox::warning(this, tr("Delete Branch"), 
            tr("Cannot delete the current branch. Please switch to another branch first."));
        return;
    }
    
    if (QMessageBox::question(this, tr("Delete Branch"),
        tr("Are you sure you want to delete branch '%1'?").arg(selectedBranch),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
        if (m_git->deleteBranch(selectedBranch, false)) {
            updateBranchSelector();
        }
    }
}

void SourceControlPanel::onOperationCompleted(const QString& message)
{
    m_statusLabel->setText(message);
}

void SourceControlPanel::onErrorOccurred(const QString& error)
{
    m_statusLabel->setText("⚠️ " + error);
    m_statusLabel->setStyleSheet("background: #161b22; color: #f85149; padding: 6px 10px; font-size: 11px; border-top: 1px solid #21262d;");
    
    // Reset color after 3 seconds
    QTimer::singleShot(3000, [this]() {
        m_statusLabel->setStyleSheet("background: #161b22; color: #8b949e; padding: 6px 10px; font-size: 11px; border-top: 1px solid #21262d;");
    });
}

QString SourceControlPanel::statusIcon(GitFileStatus status) const
{
    switch (status) {
        case GitFileStatus::Modified:
            return "M";
        case GitFileStatus::Added:
            return "A";
        case GitFileStatus::Deleted:
            return "D";
        case GitFileStatus::Renamed:
            return "R";
        case GitFileStatus::Copied:
            return "C";
        case GitFileStatus::Untracked:
            return "U";
        case GitFileStatus::Unmerged:
            return "!";
        case GitFileStatus::Ignored:
            return "I";
        default:
            return " ";
    }
}

QString SourceControlPanel::statusText(GitFileStatus status) const
{
    switch (status) {
        case GitFileStatus::Modified:
            return tr("Modified");
        case GitFileStatus::Added:
            return tr("Added");
        case GitFileStatus::Deleted:
            return tr("Deleted");
        case GitFileStatus::Renamed:
            return tr("Renamed");
        case GitFileStatus::Copied:
            return tr("Copied");
        case GitFileStatus::Untracked:
            return tr("Untracked");
        case GitFileStatus::Unmerged:
            return tr("Unmerged");
        case GitFileStatus::Ignored:
            return tr("Ignored");
        default:
            return tr("Clean");
    }
}

QColor SourceControlPanel::statusColor(GitFileStatus status) const
{
    switch (status) {
        case GitFileStatus::Modified:
            return QColor("#e2c08d");  // Yellow/orange for modified
        case GitFileStatus::Added:
            return QColor("#3fb950");  // Green for added
        case GitFileStatus::Deleted:
            return QColor("#f14c4c");  // Red for deleted
        case GitFileStatus::Renamed:
            return QColor("#a371f7");  // Purple for renamed
        case GitFileStatus::Copied:
            return QColor("#a371f7");  // Purple for copied
        case GitFileStatus::Untracked:
            return QColor("#3fb950");  // Green for untracked
        case GitFileStatus::Unmerged:
            return QColor("#f14c4c");  // Red for unmerged
        case GitFileStatus::Ignored:
            return QColor("#8b949e");  // Gray for ignored
        default:
            return QColor("#e6edf3");  // White for clean
    }
}

// New slot implementations for extended functionality

void SourceControlPanel::onInitRepositoryClicked()
{
    QString path = m_workingPath;
    if (path.isEmpty() && m_git) {
        path = m_git->workingPath();
    }
    if (path.isEmpty()) {
        path = QDir::currentPath();
    }
    
    GitInitDialog dialog(path, this);
    connect(&dialog, &GitInitDialog::initializeRequested, [this](const QString& repoPath) {
        if (m_git) {
            if (m_git->initRepository(repoPath)) {
                // If user wants to add .gitignore, create a default one
                // Note: The dialog's options would be used here in a full implementation
                refresh();
                emit repositoryInitialized(repoPath);
            }
        }
    });
    
    dialog.exec();
}

void SourceControlPanel::onPushClicked()
{
    if (!m_git || !m_git->isValidRepository()) return;
    
    GitRemoteDialog dialog(m_git, GitRemoteDialog::Mode::Push, this);
    connect(&dialog, &GitRemoteDialog::operationCompleted, [this](const QString& msg) {
        m_statusLabel->setText(msg);
        refresh();
    });
    dialog.exec();
}

void SourceControlPanel::onPullClicked()
{
    if (!m_git || !m_git->isValidRepository()) return;
    
    GitRemoteDialog dialog(m_git, GitRemoteDialog::Mode::Pull, this);
    connect(&dialog, &GitRemoteDialog::operationCompleted, [this](const QString& msg) {
        m_statusLabel->setText(msg);
        refresh();
    });
    dialog.exec();
}

void SourceControlPanel::onFetchClicked()
{
    if (!m_git || !m_git->isValidRepository()) return;
    
    GitRemoteDialog dialog(m_git, GitRemoteDialog::Mode::Fetch, this);
    connect(&dialog, &GitRemoteDialog::operationCompleted, [this](const QString& msg) {
        m_statusLabel->setText(msg);
        refresh();
    });
    dialog.exec();
}

void SourceControlPanel::onStashClicked()
{
    if (!m_git || !m_git->isValidRepository()) return;
    
    GitStashDialog dialog(m_git, this);
    connect(&dialog, &GitStashDialog::stashOperationCompleted, [this](const QString& msg) {
        m_statusLabel->setText(msg);
        refresh();
    });
    dialog.exec();
}

void SourceControlPanel::onMergeConflictsDetected(const QStringList& files)
{
    Q_UNUSED(files);
    updateUIState();
}

void SourceControlPanel::onResolveConflictsClicked()
{
    if (!m_git || !m_git->isValidRepository()) return;
    
    MergeConflictDialog dialog(m_git, this);
    connect(&dialog, &MergeConflictDialog::openFileRequested, this, &SourceControlPanel::fileOpenRequested);
    connect(&dialog, &MergeConflictDialog::allConflictsResolved, [this]() {
        refresh();
    });
    
    dialog.exec();
    refresh();
}
