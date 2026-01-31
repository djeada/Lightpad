#include "sourcecontrolpanel.h"
#include <QHeaderView>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QInputDialog>

SourceControlPanel::SourceControlPanel(QWidget* parent)
    : QWidget(parent)
    , m_git(nullptr)
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
    , m_stagedTree(nullptr)
    , m_changesTree(nullptr)
    , m_updatingBranchSelector(false)
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
    m_refreshButton->setToolTip(tr("Refresh"));
    m_refreshButton->setStyleSheet(
        "QPushButton { background: transparent; color: #e6edf3; border: none; font-size: 14px; border-radius: 4px; }"
        "QPushButton:hover { background: #2a3241; }"
    );
    connect(m_refreshButton, &QPushButton::clicked, this, &SourceControlPanel::onRefreshClicked);
    headerLayout->addWidget(m_refreshButton);

    mainLayout->addWidget(header);

    // Branch management section
    QWidget* branchSection = new QWidget(this);
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
        "}"
        "QComboBox:hover {"
        "  border-color: #58a6ff;"
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
        "QComboBox QAbstractItemView {"
        "  background: #21262d;"
        "  color: #e6edf3;"
        "  border: 1px solid #30363d;"
        "  selection-background-color: #1f6feb;"
        "}"
    );
    connect(m_branchSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &SourceControlPanel::onBranchSelectorChanged);
    branchSelectorLayout->addWidget(m_branchSelector, 1);

    m_newBranchButton = new QPushButton("+", branchSection);
    m_newBranchButton->setFixedSize(28, 28);
    m_newBranchButton->setToolTip(tr("Create New Branch"));
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
        "}"
    );
    connect(m_newBranchButton, &QPushButton::clicked, this, &SourceControlPanel::onCreateBranchClicked);
    branchSelectorLayout->addWidget(m_newBranchButton);

    m_deleteBranchButton = new QPushButton("🗑", branchSection);
    m_deleteBranchButton->setFixedSize(28, 28);
    m_deleteBranchButton->setToolTip(tr("Delete Branch"));
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
        "}"
    );
    connect(m_deleteBranchButton, &QPushButton::clicked, this, &SourceControlPanel::onDeleteBranchClicked);
    branchSelectorLayout->addWidget(m_deleteBranchButton);

    branchLayout->addLayout(branchSelectorLayout);
    mainLayout->addWidget(branchSection);

    // Commit section with improved styling
    QWidget* commitSection = new QWidget(this);
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
        "}"
        "QTextEdit:focus {"
        "  border-color: #58a6ff;"
        "  border-width: 2px;"
        "}"
    );
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
        "QCheckBox::indicator {"
        "  width: 14px;"
        "  height: 14px;"
        "  border-radius: 3px;"
        "  border: 1px solid #30363d;"
        "  background: #21262d;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background: #238636;"
        "  border-color: #238636;"
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
    QWidget* stagedHeader = new QWidget(this);
    stagedHeader->setStyleSheet("background: #161b22; border-bottom: 1px solid #21262d;");
    QHBoxLayout* stagedHeaderLayout = new QHBoxLayout(stagedHeader);
    stagedHeaderLayout->setContentsMargins(8, 6, 8, 6);
    
    QLabel* stagedIcon = new QLabel("📦", stagedHeader);
    stagedIcon->setStyleSheet("font-size: 12px;");
    stagedHeaderLayout->addWidget(stagedIcon);
    
    QLabel* stagedLabel = new QLabel(tr("Staged Changes"), stagedHeader);
    stagedLabel->setStyleSheet("color: #3fb950; font-weight: bold; font-size: 12px;");
    stagedHeaderLayout->addWidget(stagedLabel);
    
    stagedHeaderLayout->addStretch();
    
    m_unstageAllButton = new QPushButton("−", stagedHeader);
    m_unstageAllButton->setFixedSize(22, 22);
    m_unstageAllButton->setToolTip(tr("Unstage All"));
    m_unstageAllButton->setStyleSheet(
        "QPushButton { background: transparent; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: #21262d; color: #f85149; border-color: #f85149; }"
    );
    connect(m_unstageAllButton, &QPushButton::clicked, this, &SourceControlPanel::onUnstageAllClicked);
    stagedHeaderLayout->addWidget(m_unstageAllButton);
    
    mainLayout->addWidget(stagedHeader);

    m_stagedTree = new QTreeWidget(this);
    m_stagedTree->setHeaderHidden(true);
    m_stagedTree->setRootIsDecorated(false);
    m_stagedTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_stagedTree->setStyleSheet(
        "QTreeWidget {"
        "  background: #0d1117;"
        "  color: #e6edf3;"
        "  border: none;"
        "  font-size: 12px;"
        "}"
        "QTreeWidget::item {"
        "  padding: 6px 8px;"
        "  border-bottom: 1px solid #21262d;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: #1f6feb;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: #161b22;"
        "}"
    );
    m_stagedTree->setMinimumHeight(80);
    m_stagedTree->setMaximumHeight(150);
    connect(m_stagedTree, &QTreeWidget::itemDoubleClicked, this, &SourceControlPanel::onItemDoubleClicked);
    connect(m_stagedTree, &QTreeWidget::customContextMenuRequested, this, &SourceControlPanel::onItemContextMenu);
    mainLayout->addWidget(m_stagedTree);

    // Changes section
    QWidget* changesHeader = new QWidget(this);
    changesHeader->setStyleSheet("background: #161b22; border-bottom: 1px solid #21262d;");
    QHBoxLayout* changesHeaderLayout = new QHBoxLayout(changesHeader);
    changesHeaderLayout->setContentsMargins(8, 6, 8, 6);
    
    QLabel* changesIcon = new QLabel("📝", changesHeader);
    changesIcon->setStyleSheet("font-size: 12px;");
    changesHeaderLayout->addWidget(changesIcon);
    
    QLabel* changesLabel = new QLabel(tr("Changes"), changesHeader);
    changesLabel->setStyleSheet("color: #e2c08d; font-weight: bold; font-size: 12px;");
    changesHeaderLayout->addWidget(changesLabel);
    
    changesHeaderLayout->addStretch();
    
    m_stageAllButton = new QPushButton("+", changesHeader);
    m_stageAllButton->setFixedSize(22, 22);
    m_stageAllButton->setToolTip(tr("Stage All"));
    m_stageAllButton->setStyleSheet(
        "QPushButton { background: transparent; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: #21262d; color: #3fb950; border-color: #3fb950; }"
    );
    connect(m_stageAllButton, &QPushButton::clicked, this, &SourceControlPanel::onStageAllClicked);
    changesHeaderLayout->addWidget(m_stageAllButton);
    
    mainLayout->addWidget(changesHeader);

    m_changesTree = new QTreeWidget(this);
    m_changesTree->setHeaderHidden(true);
    m_changesTree->setRootIsDecorated(false);
    m_changesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_changesTree->setStyleSheet(
        "QTreeWidget {"
        "  background: #0d1117;"
        "  color: #e6edf3;"
        "  border: none;"
        "  font-size: 12px;"
        "}"
        "QTreeWidget::item {"
        "  padding: 6px 8px;"
        "  border-bottom: 1px solid #21262d;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: #1f6feb;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: #161b22;"
        "}"
    );
    connect(m_changesTree, &QTreeWidget::itemDoubleClicked, this, &SourceControlPanel::onItemDoubleClicked);
    connect(m_changesTree, &QTreeWidget::customContextMenuRequested, this, &SourceControlPanel::onItemContextMenu);
    mainLayout->addWidget(m_changesTree, 1);

    // Status bar with improved styling
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("background: #161b22; color: #8b949e; padding: 6px 10px; font-size: 11px; border-top: 1px solid #21262d;");
    mainLayout->addWidget(m_statusLabel);
}

void SourceControlPanel::setGitIntegration(GitIntegration* git)
{
    m_git = git;
    
    if (m_git) {
        connect(m_git, &GitIntegration::statusChanged, this, &SourceControlPanel::onStatusChanged);
        connect(m_git, &GitIntegration::branchChanged, this, &SourceControlPanel::onBranchChanged);
        connect(m_git, &GitIntegration::operationCompleted, this, &SourceControlPanel::onOperationCompleted);
        connect(m_git, &GitIntegration::errorOccurred, this, &SourceControlPanel::onErrorOccurred);
        refresh();
    }
}

void SourceControlPanel::refresh()
{
    if (!m_git) {
        m_branchLabel->setText(tr("Branch"));
        m_branchSelector->clear();
        m_statusLabel->setText("");
        m_stagedTree->clear();
        m_changesTree->clear();
        return;
    }
    
    updateBranchSelector();
    updateTree();
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
    
    if (!m_git || !m_git->isValidRepository()) {
        m_statusLabel->setText(tr("Not a git repository"));
        return;
    }
    
    QList<GitFileInfo> status = m_git->getStatus();
    
    int stagedCount = 0;
    int changesCount = 0;
    
    for (const GitFileInfo& file : status) {
        // Check if file is staged (has index status)
        if (file.indexStatus != GitFileStatus::Clean && 
            file.indexStatus != GitFileStatus::Untracked) {
            QTreeWidgetItem* item = new QTreeWidgetItem(m_stagedTree);
            
            QString icon = statusIcon(file.indexStatus);
            QFileInfo fileInfo(file.filePath);
            item->setText(0, QString("%1 %2").arg(icon).arg(fileInfo.fileName()));
            item->setToolTip(0, file.filePath);
            item->setData(0, Qt::UserRole, file.filePath);
            item->setData(0, Qt::UserRole + 1, true);  // Is staged
            item->setForeground(0, statusColor(file.indexStatus));
            
            stagedCount++;
        }
        
        // Check if file has working tree changes
        if (file.workTreeStatus != GitFileStatus::Clean) {
            QTreeWidgetItem* item = new QTreeWidgetItem(m_changesTree);
            
            QString icon = statusIcon(file.workTreeStatus);
            QFileInfo fileInfo(file.filePath);
            item->setText(0, QString("%1 %2").arg(icon).arg(fileInfo.fileName()));
            item->setToolTip(0, file.filePath);
            item->setData(0, Qt::UserRole, file.filePath);
            item->setData(0, Qt::UserRole + 1, false);  // Not staged
            item->setForeground(0, statusColor(file.workTreeStatus));
            
            changesCount++;
        }
    }
    
    QString statusText = QString(tr("%1 staged, %2 changed")).arg(stagedCount).arg(changesCount);
    m_statusLabel->setText(statusText);
    
    m_commitButton->setEnabled(stagedCount > 0);
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
    bool isStaged = item->data(0, Qt::UserRole + 1).toBool();
    
    QMenu menu(this);
    
    if (isStaged) {
        QAction* unstageAction = menu.addAction(tr("Unstage"));
        connect(unstageAction, &QAction::triggered, [this, filePath]() {
            if (m_git) m_git->unstageFile(filePath);
        });
    } else {
        QAction* stageAction = menu.addAction(tr("Stage"));
        connect(stageAction, &QAction::triggered, [this, filePath]() {
            if (m_git) m_git->stageFile(filePath);
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
    
    QAction* diffAction = menu.addAction(tr("View Diff"));
    connect(diffAction, &QAction::triggered, [this, filePath]() {
        emit diffRequested(filePath);
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
