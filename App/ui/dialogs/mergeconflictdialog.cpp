#include "mergeconflictdialog.h"
#include "../uistylehelper.h"
#include <QMessageBox>
#include <QFileInfo>

MergeConflictDialog::MergeConflictDialog(GitIntegration* git, QWidget* parent)
    : QDialog(parent)
    , m_git(git)
    , m_fileList(nullptr)
    , m_oursPreview(nullptr)
    , m_theirsPreview(nullptr)
    , m_statusLabel(nullptr)
    , m_conflictCountLabel(nullptr)
    , m_acceptOursButton(nullptr)
    , m_acceptTheirsButton(nullptr)
    , m_openEditorButton(nullptr)
    , m_markResolvedButton(nullptr)
    , m_abortButton(nullptr)
    , m_continueButton(nullptr)
{
    setWindowTitle(tr("Resolve Merge Conflicts"));
    setMinimumSize(800, 600);
    setupUI();
    applyStyles();
}

MergeConflictDialog::~MergeConflictDialog()
{
}

void MergeConflictDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // Header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QLabel* iconLabel = new QLabel("⚠️", this);
    iconLabel->setStyleSheet("font-size: 28px;");
    headerLayout->addWidget(iconLabel);
    
    QVBoxLayout* titleLayout = new QVBoxLayout();
    QLabel* titleLabel = new QLabel(tr("Merge Conflicts Detected"), this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #f85149;");
    m_statusLabel = new QLabel(tr("Resolve conflicts before completing the merge"), this);
    m_statusLabel->setStyleSheet("color: #8b949e; font-size: 12px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(m_statusLabel);
    headerLayout->addLayout(titleLayout, 1);
    
    m_conflictCountLabel = new QLabel(this);
    m_conflictCountLabel->setStyleSheet("background: #f85149; color: white; padding: 4px 12px; border-radius: 12px; font-weight: bold;");
    headerLayout->addWidget(m_conflictCountLabel);
    
    mainLayout->addLayout(headerLayout);

    // Main content splitter
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Left panel - File list
    QWidget* leftPanel = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* filesLabel = new QLabel(tr("Conflicted Files"), this);
    filesLabel->setStyleSheet("color: #8b949e; font-size: 11px; text-transform: uppercase; font-weight: bold;");
    leftLayout->addWidget(filesLabel);
    
    m_fileList = new QListWidget(this);
    connect(m_fileList, &QListWidget::itemClicked, this, &MergeConflictDialog::onFileSelected);
    leftLayout->addWidget(m_fileList);
    
    mainSplitter->addWidget(leftPanel);

    // Right panel - Preview and actions
    QWidget* rightPanel = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // Preview splitter (ours vs theirs)
    QSplitter* previewSplitter = new QSplitter(Qt::Horizontal, this);
    
    // Ours panel
    QWidget* oursPanel = new QWidget(this);
    QVBoxLayout* oursLayout = new QVBoxLayout(oursPanel);
    oursLayout->setContentsMargins(0, 0, 4, 0);
    
    QLabel* oursLabel = new QLabel(tr("⬅ Current Branch (Ours)"), this);
    oursLabel->setStyleSheet("color: #3fb950; font-weight: bold; font-size: 12px;");
    oursLayout->addWidget(oursLabel);
    
    m_oursPreview = new QTextEdit(this);
    m_oursPreview->setReadOnly(true);
    m_oursPreview->setPlaceholderText(tr("Select a file to preview ours changes"));
    oursLayout->addWidget(m_oursPreview);
    
    previewSplitter->addWidget(oursPanel);
    
    // Theirs panel
    QWidget* theirsPanel = new QWidget(this);
    QVBoxLayout* theirsLayout = new QVBoxLayout(theirsPanel);
    theirsLayout->setContentsMargins(4, 0, 0, 0);
    
    QLabel* theirsLabel = new QLabel(tr("Incoming Changes (Theirs) ➡"), this);
    theirsLabel->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 12px;");
    theirsLayout->addWidget(theirsLabel);
    
    m_theirsPreview = new QTextEdit(this);
    m_theirsPreview->setReadOnly(true);
    m_theirsPreview->setPlaceholderText(tr("Select a file to preview theirs changes"));
    theirsLayout->addWidget(m_theirsPreview);
    
    previewSplitter->addWidget(theirsPanel);
    
    rightLayout->addWidget(previewSplitter, 1);

    // Action buttons for selected file
    QHBoxLayout* fileActionsLayout = new QHBoxLayout();
    fileActionsLayout->setSpacing(8);
    
    m_acceptOursButton = new QPushButton(tr("Accept Ours"), this);
    m_acceptOursButton->setToolTip(tr("Use your local version, discarding incoming changes"));
    connect(m_acceptOursButton, &QPushButton::clicked, this, &MergeConflictDialog::onAcceptOursClicked);
    fileActionsLayout->addWidget(m_acceptOursButton);
    
    m_acceptTheirsButton = new QPushButton(tr("Accept Theirs"), this);
    m_acceptTheirsButton->setToolTip(tr("Use the incoming version, discarding your changes"));
    connect(m_acceptTheirsButton, &QPushButton::clicked, this, &MergeConflictDialog::onAcceptTheirsClicked);
    fileActionsLayout->addWidget(m_acceptTheirsButton);
    
    m_openEditorButton = new QPushButton(tr("Open in Editor"), this);
    m_openEditorButton->setToolTip(tr("Manually edit the file to resolve conflicts"));
    connect(m_openEditorButton, &QPushButton::clicked, this, &MergeConflictDialog::onOpenInEditorClicked);
    fileActionsLayout->addWidget(m_openEditorButton);
    
    m_markResolvedButton = new QPushButton(tr("Mark Resolved"), this);
    m_markResolvedButton->setToolTip(tr("Mark this file as resolved after manual editing"));
    connect(m_markResolvedButton, &QPushButton::clicked, this, &MergeConflictDialog::onMarkResolvedClicked);
    fileActionsLayout->addWidget(m_markResolvedButton);
    
    rightLayout->addLayout(fileActionsLayout);
    
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setSizes({250, 550});
    
    mainLayout->addWidget(mainSplitter, 1);

    // Bottom action buttons
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    
    m_abortButton = new QPushButton(tr("Abort Merge"), this);
    m_abortButton->setToolTip(tr("Cancel the merge and return to the previous state"));
    connect(m_abortButton, &QPushButton::clicked, this, &MergeConflictDialog::onAbortMergeClicked);
    bottomLayout->addWidget(m_abortButton);
    
    bottomLayout->addStretch();
    
    m_continueButton = new QPushButton(tr("Complete Merge"), this);
    m_continueButton->setToolTip(tr("Commit the merge after all conflicts are resolved"));
    m_continueButton->setEnabled(false);
    connect(m_continueButton, &QPushButton::clicked, this, &MergeConflictDialog::onContinueMergeClicked);
    bottomLayout->addWidget(m_continueButton);
    
    mainLayout->addLayout(bottomLayout);
    
    updateButtons();
}

void MergeConflictDialog::applyStyles()
{
    setStyleSheet(R"(
        QDialog {
            background: #0d1117;
        }
        QListWidget {
            background: #161b22;
            color: #e6edf3;
            border: 1px solid #30363d;
            border-radius: 6px;
            font-size: 12px;
        }
        QListWidget::item {
            padding: 8px 12px;
            border-bottom: 1px solid #21262d;
        }
        QListWidget::item:selected {
            background: #1f6feb;
        }
        QListWidget::item:hover {
            background: #21262d;
        }
        QTextEdit {
            background: #161b22;
            color: #e6edf3;
            border: 1px solid #30363d;
            border-radius: 6px;
            font-family: monospace;
            font-size: 12px;
            padding: 8px;
        }
        QPushButton {
            background: #21262d;
            color: #e6edf3;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 12px;
        }
        QPushButton:hover {
            background: #30363d;
        }
        QPushButton:disabled {
            background: #161b22;
            color: #484f58;
        }
        QPushButton#acceptOursButton {
            background: #238636;
            border-color: #238636;
            color: white;
        }
        QPushButton#acceptOursButton:hover {
            background: #2ea043;
        }
        QPushButton#acceptTheirsButton {
            background: #1f6feb;
            border-color: #1f6feb;
            color: white;
        }
        QPushButton#acceptTheirsButton:hover {
            background: #388bfd;
        }
        QPushButton#abortButton {
            background: #da3633;
            border-color: #da3633;
            color: white;
        }
        QPushButton#abortButton:hover {
            background: #f85149;
        }
        QPushButton#continueButton {
            background: #238636;
            border-color: #238636;
            color: white;
            font-weight: bold;
        }
        QPushButton#continueButton:hover {
            background: #2ea043;
        }
    )");
    
    m_acceptOursButton->setObjectName("acceptOursButton");
    m_acceptTheirsButton->setObjectName("acceptTheirsButton");
    m_abortButton->setObjectName("abortButton");
    m_continueButton->setObjectName("continueButton");
}

void MergeConflictDialog::setConflictedFiles(const QStringList& files)
{
    m_fileList->clear();
    
    for (const QString& file : files) {
        QListWidgetItem* item = new QListWidgetItem(m_fileList);
        QFileInfo fileInfo(file);
        item->setText(QString("❗ %1").arg(fileInfo.fileName()));
        item->setToolTip(file);
        item->setData(Qt::UserRole, file);
    }
    
    m_conflictCountLabel->setText(tr("%n conflict(s)", "", files.size()));
    updateButtons();
}

void MergeConflictDialog::refresh()
{
    if (!m_git) return;
    
    QStringList conflicts = m_git->getConflictedFiles();
    setConflictedFiles(conflicts);
    
    if (conflicts.isEmpty()) {
        m_statusLabel->setText(tr("All conflicts resolved! You can complete the merge."));
        m_statusLabel->setStyleSheet("color: #3fb950; font-size: 12px;");
        emit allConflictsResolved();
    }
}

void MergeConflictDialog::onFileSelected(QListWidgetItem* item)
{
    if (!item) return;
    
    m_currentFile = item->data(Qt::UserRole).toString();
    updateConflictPreview(m_currentFile);
    updateButtons();
}

void MergeConflictDialog::updateConflictPreview(const QString& filePath)
{
    if (!m_git || filePath.isEmpty()) {
        m_oursPreview->clear();
        m_theirsPreview->clear();
        return;
    }
    
    QList<GitConflictMarker> markers = m_git->getConflictMarkers(filePath);
    
    QString oursContent;
    QString theirsContent;
    
    for (const GitConflictMarker& marker : markers) {
        oursContent += QString("--- Lines %1-%2 ---\n").arg(marker.startLine).arg(marker.separatorLine - 1);
        oursContent += marker.oursContent;
        oursContent += "\n";
        
        theirsContent += QString("--- Lines %1-%2 ---\n").arg(marker.separatorLine + 1).arg(marker.endLine - 1);
        theirsContent += marker.theirsContent;
        theirsContent += "\n";
    }
    
    if (markers.isEmpty()) {
        m_oursPreview->setPlainText(tr("No conflict markers found in this file.\nThe file may have been manually edited."));
        m_theirsPreview->setPlainText(tr("No conflict markers found in this file.\nThe file may have been manually edited."));
    } else {
        m_oursPreview->setPlainText(oursContent);
        m_theirsPreview->setPlainText(theirsContent);
    }
}

void MergeConflictDialog::updateButtons()
{
    bool hasSelection = !m_currentFile.isEmpty();
    m_acceptOursButton->setEnabled(hasSelection);
    m_acceptTheirsButton->setEnabled(hasSelection);
    m_openEditorButton->setEnabled(hasSelection);
    m_markResolvedButton->setEnabled(hasSelection);
    
    if (m_git) {
        QStringList conflicts = m_git->getConflictedFiles();
        m_continueButton->setEnabled(conflicts.isEmpty());
    }
}

void MergeConflictDialog::onAcceptOursClicked()
{
    if (!m_git || m_currentFile.isEmpty()) return;
    
    if (m_git->resolveConflictOurs(m_currentFile)) {
        refresh();
    }
}

void MergeConflictDialog::onAcceptTheirsClicked()
{
    if (!m_git || m_currentFile.isEmpty()) return;
    
    if (m_git->resolveConflictTheirs(m_currentFile)) {
        refresh();
    }
}

void MergeConflictDialog::onOpenInEditorClicked()
{
    if (m_currentFile.isEmpty()) return;
    
    QString fullPath = m_currentFile;
    if (m_git && !fullPath.startsWith('/')) {
        fullPath = m_git->repositoryPath() + "/" + m_currentFile;
    }
    
    emit openFileRequested(fullPath);
}

void MergeConflictDialog::onMarkResolvedClicked()
{
    if (!m_git || m_currentFile.isEmpty()) return;
    
    if (m_git->markConflictResolved(m_currentFile)) {
        refresh();
    }
}

void MergeConflictDialog::onAbortMergeClicked()
{
    if (!m_git) return;
    
    int result = QMessageBox::question(this, tr("Abort Merge"),
        tr("Are you sure you want to abort the merge?\nAll merge progress will be lost."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        if (m_git->abortMerge()) {
            accept();
        }
    }
}

void MergeConflictDialog::onContinueMergeClicked()
{
    if (!m_git) return;
    
    if (m_git->hasMergeConflicts()) {
        QMessageBox::warning(this, tr("Unresolved Conflicts"),
            tr("There are still unresolved conflicts. Please resolve all conflicts before completing the merge."));
        return;
    }
    
    if (m_git->continueMerge()) {
        accept();
    }
}


void MergeConflictDialog::applyTheme(const Theme& theme)
{
    setStyleSheet(UIStyleHelper::formDialogStyle(theme));
    
    // Apply list widget style
    if (m_fileList) {
        m_fileList->setStyleSheet(UIStyleHelper::resultListStyle(theme));
    }
    
    // Apply text edit styles for preview
    QString textEditStyle = QString(
        "QTextEdit {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "}")
        .arg(theme.surfaceAltColor.name())
        .arg(theme.foregroundColor.name())
        .arg(theme.borderColor.name());
    
    if (m_oursPreview) {
        m_oursPreview->setStyleSheet(textEditStyle);
    }
    if (m_theirsPreview) {
        m_theirsPreview->setStyleSheet(textEditStyle);
    }
    
    // Apply button styles
    if (m_acceptOursButton) {
        m_acceptOursButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
    }
    if (m_acceptTheirsButton) {
        m_acceptTheirsButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
    }
    for (QPushButton* btn : {m_openEditorButton, m_markResolvedButton, m_abortButton, m_continueButton}) {
        if (btn) {
            btn->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
        }
    }
    
    // Apply label styles
    if (m_statusLabel) {
        m_statusLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
    }
    if (m_conflictCountLabel) {
        m_conflictCountLabel->setStyleSheet(UIStyleHelper::titleLabelStyle(theme));
    }
}
