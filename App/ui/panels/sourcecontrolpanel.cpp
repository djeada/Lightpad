#include "sourcecontrolpanel.h"
#include <QHeaderView>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>

SourceControlPanel::SourceControlPanel(QWidget* parent)
    : QWidget(parent)
    , m_git(nullptr)
    , m_branchLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_commitMessage(nullptr)
    , m_commitButton(nullptr)
    , m_stageAllButton(nullptr)
    , m_unstageAllButton(nullptr)
    , m_refreshButton(nullptr)
    , m_stagedTree(nullptr)
    , m_changesTree(nullptr)
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

    // Header bar
    QWidget* header = new QWidget(this);
    header->setStyleSheet("background: #171c24; border-bottom: 1px solid #2a3241;");
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    QLabel* titleLabel = new QLabel(tr("Source Control"), header);
    titleLabel->setStyleSheet("font-weight: bold; color: #e6edf3;");
    headerLayout->addWidget(titleLabel);

    m_branchLabel = new QLabel(header);
    m_branchLabel->setStyleSheet("color: #58a6ff; padding-left: 8px;");
    headerLayout->addWidget(m_branchLabel);

    headerLayout->addStretch();

    m_refreshButton = new QPushButton("↻", header);
    m_refreshButton->setFixedSize(24, 24);
    m_refreshButton->setToolTip(tr("Refresh"));
    m_refreshButton->setStyleSheet(
        "QPushButton { background: transparent; color: #e6edf3; border: none; font-size: 14px; }"
        "QPushButton:hover { background: #2a3241; }"
    );
    connect(m_refreshButton, &QPushButton::clicked, this, &SourceControlPanel::onRefreshClicked);
    headerLayout->addWidget(m_refreshButton);

    mainLayout->addWidget(header);

    // Commit message area
    QWidget* commitArea = new QWidget(this);
    commitArea->setStyleSheet("background: #0e1116;");
    QVBoxLayout* commitLayout = new QVBoxLayout(commitArea);
    commitLayout->setContentsMargins(8, 8, 8, 8);
    commitLayout->setSpacing(4);

    m_commitMessage = new QTextEdit(commitArea);
    m_commitMessage->setPlaceholderText(tr("Commit message..."));
    m_commitMessage->setMaximumHeight(80);
    m_commitMessage->setStyleSheet(
        "QTextEdit {"
        "  background: #1f2632;"
        "  color: #e6edf3;"
        "  border: 1px solid #2a3241;"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "}"
        "QTextEdit:focus {"
        "  border-color: #58a6ff;"
        "}"
    );
    commitLayout->addWidget(m_commitMessage);

    QHBoxLayout* commitButtonLayout = new QHBoxLayout();
    commitButtonLayout->setSpacing(4);
    
    m_commitButton = new QPushButton(tr("Commit"), commitArea);
    m_commitButton->setStyleSheet(
        "QPushButton {"
        "  background: #238636;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 6px 16px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: #2ea043;"
        "}"
        "QPushButton:disabled {"
        "  background: #484f58;"
        "  color: #8b949e;"
        "}"
    );
    connect(m_commitButton, &QPushButton::clicked, this, &SourceControlPanel::onCommitClicked);
    commitButtonLayout->addWidget(m_commitButton);
    commitButtonLayout->addStretch();
    
    commitLayout->addLayout(commitButtonLayout);
    mainLayout->addWidget(commitArea);

    // Staged changes section
    QWidget* stagedHeader = new QWidget(this);
    stagedHeader->setStyleSheet("background: #171c24; border-bottom: 1px solid #2a3241;");
    QHBoxLayout* stagedHeaderLayout = new QHBoxLayout(stagedHeader);
    stagedHeaderLayout->setContentsMargins(8, 4, 8, 4);
    
    QLabel* stagedLabel = new QLabel(tr("Staged Changes"), stagedHeader);
    stagedLabel->setStyleSheet("color: #e6edf3; font-weight: bold;");
    stagedHeaderLayout->addWidget(stagedLabel);
    
    stagedHeaderLayout->addStretch();
    
    m_unstageAllButton = new QPushButton("−", stagedHeader);
    m_unstageAllButton->setFixedSize(24, 24);
    m_unstageAllButton->setToolTip(tr("Unstage All"));
    m_unstageAllButton->setStyleSheet(
        "QPushButton { background: transparent; color: #e6edf3; border: none; font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { background: #2a3241; }"
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
        "  background: #0e1116;"
        "  color: #e6edf3;"
        "  border: none;"
        "}"
        "QTreeWidget::item {"
        "  padding: 4px;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: #1b2a43;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: #222a36;"
        "}"
    );
    m_stagedTree->setMaximumHeight(150);
    connect(m_stagedTree, &QTreeWidget::itemDoubleClicked, this, &SourceControlPanel::onItemDoubleClicked);
    connect(m_stagedTree, &QTreeWidget::customContextMenuRequested, this, &SourceControlPanel::onItemContextMenu);
    mainLayout->addWidget(m_stagedTree);

    // Changes section
    QWidget* changesHeader = new QWidget(this);
    changesHeader->setStyleSheet("background: #171c24; border-bottom: 1px solid #2a3241;");
    QHBoxLayout* changesHeaderLayout = new QHBoxLayout(changesHeader);
    changesHeaderLayout->setContentsMargins(8, 4, 8, 4);
    
    QLabel* changesLabel = new QLabel(tr("Changes"), changesHeader);
    changesLabel->setStyleSheet("color: #e6edf3; font-weight: bold;");
    changesHeaderLayout->addWidget(changesLabel);
    
    changesHeaderLayout->addStretch();
    
    m_stageAllButton = new QPushButton("+", changesHeader);
    m_stageAllButton->setFixedSize(24, 24);
    m_stageAllButton->setToolTip(tr("Stage All"));
    m_stageAllButton->setStyleSheet(
        "QPushButton { background: transparent; color: #e6edf3; border: none; font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { background: #2a3241; }"
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
        "  background: #0e1116;"
        "  color: #e6edf3;"
        "  border: none;"
        "}"
        "QTreeWidget::item {"
        "  padding: 4px;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: #1b2a43;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: #222a36;"
        "}"
    );
    connect(m_changesTree, &QTreeWidget::itemDoubleClicked, this, &SourceControlPanel::onItemDoubleClicked);
    connect(m_changesTree, &QTreeWidget::customContextMenuRequested, this, &SourceControlPanel::onItemContextMenu);
    mainLayout->addWidget(m_changesTree);

    // Status bar
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("background: #171c24; color: #9aa4b2; padding: 4px 8px;");
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
        m_branchLabel->setText(tr("No repository"));
        m_statusLabel->setText("");
        m_stagedTree->clear();
        m_changesTree->clear();
        return;
    }
    
    updateBranchLabel();
    updateTree();
}

void SourceControlPanel::updateBranchLabel()
{
    if (!m_git || !m_git->isValidRepository()) {
        m_branchLabel->setText(tr("No repository"));
        return;
    }
    
    QString branch = m_git->currentBranch();
    m_branchLabel->setText("⎇ " + branch);
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

void SourceControlPanel::onOperationCompleted(const QString& message)
{
    m_statusLabel->setText(message);
}

void SourceControlPanel::onErrorOccurred(const QString& error)
{
    m_statusLabel->setText("Error: " + error);
    m_statusLabel->setStyleSheet("background: #171c24; color: #f14c4c; padding: 4px 8px;");
    
    // Reset color after 3 seconds
    QTimer::singleShot(3000, [this]() {
        m_statusLabel->setStyleSheet("background: #171c24; color: #9aa4b2; padding: 4px 8px;");
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
