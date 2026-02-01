#include "gitremotedialog.h"
#include "../uistylehelper.h"
#include <QMessageBox>
#include <QCheckBox>

GitRemoteDialog::GitRemoteDialog(GitIntegration* git, Mode mode, QWidget* parent)
    : QDialog(parent)
    , m_git(git)
    , m_mode(mode)
    , m_remoteSelector(nullptr)
    , m_branchSelector(nullptr)
    , m_setUpstreamCheckbox(nullptr)
    , m_forceCheckbox(nullptr)
    , m_pushButton(nullptr)
    , m_pullButton(nullptr)
    , m_fetchButton(nullptr)
    , m_progressBar(nullptr)
    , m_statusLabel(nullptr)
    , m_remoteList(nullptr)
    , m_remoteNameEdit(nullptr)
    , m_remoteUrlEdit(nullptr)
    , m_addRemoteButton(nullptr)
    , m_removeRemoteButton(nullptr)
    , m_closeButton(nullptr)
{
    QString title;
    switch (mode) {
        case Mode::Push: title = tr("Push to Remote"); break;
        case Mode::Pull: title = tr("Pull from Remote"); break;
        case Mode::Fetch: title = tr("Fetch from Remote"); break;
        case Mode::ManageRemotes: title = tr("Manage Remotes"); break;
    }
    setWindowTitle(title);
    setMinimumSize(550, 450);
    setupUI();
    applyStyles();
    refresh();
}

GitRemoteDialog::~GitRemoteDialog()
{
}

void GitRemoteDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QString iconText;
    QString titleText;
    QString subtitleText;
    
    switch (m_mode) {
        case Mode::Push:
            iconText = "⬆️";
            titleText = tr("Push Changes");
            subtitleText = tr("Upload your commits to a remote repository");
            break;
        case Mode::Pull:
            iconText = "⬇️";
            titleText = tr("Pull Changes");
            subtitleText = tr("Download and integrate changes from a remote repository");
            break;
        case Mode::Fetch:
            iconText = "🔄";
            titleText = tr("Fetch Changes");
            subtitleText = tr("Download changes without merging");
            break;
        case Mode::ManageRemotes:
            iconText = "🌐";
            titleText = tr("Manage Remotes");
            subtitleText = tr("Add, remove, or modify remote repositories");
            break;
    }
    
    QLabel* iconLabel = new QLabel(iconText, this);
    iconLabel->setStyleSheet("font-size: 28px;");
    headerLayout->addWidget(iconLabel);
    
    QVBoxLayout* titleLayout = new QVBoxLayout();
    QLabel* titleLabel = new QLabel(titleText, this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #e6edf3;");
    QLabel* subtitleLabel = new QLabel(subtitleText, this);
    subtitleLabel->setStyleSheet("color: #8b949e; font-size: 12px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(titleLayout, 1);
    
    mainLayout->addLayout(headerLayout);

    if (m_mode != Mode::ManageRemotes) {
        // Remote and branch selection
        QGroupBox* selectionGroup = new QGroupBox(tr("Remote & Branch"), this);
        QVBoxLayout* selectionLayout = new QVBoxLayout(selectionGroup);
        
        QHBoxLayout* remoteLayout = new QHBoxLayout();
        QLabel* remoteLabel = new QLabel(tr("Remote:"), this);
        remoteLabel->setFixedWidth(80);
        m_remoteSelector = new QComboBox(this);
        m_remoteSelector->setMinimumWidth(200);
        connect(m_remoteSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
                this, &GitRemoteDialog::onRemoteSelected);
        remoteLayout->addWidget(remoteLabel);
        remoteLayout->addWidget(m_remoteSelector, 1);
        selectionLayout->addLayout(remoteLayout);
        
        QHBoxLayout* branchLayout = new QHBoxLayout();
        QLabel* branchLabel = new QLabel(tr("Branch:"), this);
        branchLabel->setFixedWidth(80);
        m_branchSelector = new QComboBox(this);
        m_branchSelector->setMinimumWidth(200);
        branchLayout->addWidget(branchLabel);
        branchLayout->addWidget(m_branchSelector, 1);
        selectionLayout->addLayout(branchLayout);
        
        mainLayout->addWidget(selectionGroup);

        // Options
        QGroupBox* optionsGroup = new QGroupBox(tr("Options"), this);
        QVBoxLayout* optionsLayout = new QVBoxLayout(optionsGroup);
        
        if (m_mode == Mode::Push) {
            m_setUpstreamCheckbox = new QCheckBox(tr("Set upstream (tracking branch)"), this);
            m_setUpstreamCheckbox->setToolTip(tr("Set the remote branch as the tracking branch for the local branch"));
            optionsLayout->addWidget(m_setUpstreamCheckbox);
            
            m_forceCheckbox = new QCheckBox(tr("Force push (dangerous!)"), this);
            m_forceCheckbox->setToolTip(tr("Force push even if it would overwrite remote changes. Use with caution!"));
            m_forceCheckbox->setStyleSheet("QCheckBox { color: #f85149; }");
            optionsLayout->addWidget(m_forceCheckbox);
        }
        
        mainLayout->addWidget(optionsGroup);
    } else {
        // Manage remotes UI
        QGroupBox* remotesGroup = new QGroupBox(tr("Configured Remotes"), this);
        QVBoxLayout* remotesLayout = new QVBoxLayout(remotesGroup);
        
        m_remoteList = new QListWidget(this);
        remotesLayout->addWidget(m_remoteList);
        
        QHBoxLayout* addRemoteLayout = new QHBoxLayout();
        
        QVBoxLayout* inputsLayout = new QVBoxLayout();
        
        QHBoxLayout* nameLayout = new QHBoxLayout();
        QLabel* nameLabel = new QLabel(tr("Name:"), this);
        nameLabel->setFixedWidth(50);
        m_remoteNameEdit = new QLineEdit(this);
        m_remoteNameEdit->setPlaceholderText(tr("e.g., origin"));
        nameLayout->addWidget(nameLabel);
        nameLayout->addWidget(m_remoteNameEdit);
        inputsLayout->addLayout(nameLayout);
        
        QHBoxLayout* urlLayout = new QHBoxLayout();
        QLabel* urlLabel = new QLabel(tr("URL:"), this);
        urlLabel->setFixedWidth(50);
        m_remoteUrlEdit = new QLineEdit(this);
        m_remoteUrlEdit->setPlaceholderText(tr("https://github.com/user/repo.git"));
        urlLayout->addWidget(urlLabel);
        urlLayout->addWidget(m_remoteUrlEdit);
        inputsLayout->addLayout(urlLayout);
        
        addRemoteLayout->addLayout(inputsLayout, 1);
        
        QVBoxLayout* buttonsLayout = new QVBoxLayout();
        m_addRemoteButton = new QPushButton(tr("Add"), this);
        connect(m_addRemoteButton, &QPushButton::clicked, this, &GitRemoteDialog::onAddRemoteClicked);
        buttonsLayout->addWidget(m_addRemoteButton);
        
        m_removeRemoteButton = new QPushButton(tr("Remove"), this);
        connect(m_removeRemoteButton, &QPushButton::clicked, this, &GitRemoteDialog::onRemoveRemoteClicked);
        buttonsLayout->addWidget(m_removeRemoteButton);
        
        addRemoteLayout->addLayout(buttonsLayout);
        remotesLayout->addLayout(addRemoteLayout);
        
        mainLayout->addWidget(remotesGroup);
    }

    // Progress and status
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);  // Indeterminate
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);
    
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #8b949e; font-size: 11px;");
    mainLayout->addWidget(m_statusLabel);
    
    mainLayout->addStretch();

    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    if (m_mode != Mode::ManageRemotes) {
        if (m_mode == Mode::Push || m_mode == Mode::ManageRemotes) {
            m_pushButton = new QPushButton(tr("Push"), this);
            connect(m_pushButton, &QPushButton::clicked, this, &GitRemoteDialog::onPushClicked);
            buttonLayout->addWidget(m_pushButton);
        }
        
        if (m_mode == Mode::Pull || m_mode == Mode::ManageRemotes) {
            m_pullButton = new QPushButton(tr("Pull"), this);
            connect(m_pullButton, &QPushButton::clicked, this, &GitRemoteDialog::onPullClicked);
            buttonLayout->addWidget(m_pullButton);
        }
        
        if (m_mode == Mode::Fetch) {
            m_fetchButton = new QPushButton(tr("Fetch"), this);
            connect(m_fetchButton, &QPushButton::clicked, this, &GitRemoteDialog::onFetchClicked);
            buttonLayout->addWidget(m_fetchButton);
        }
    }
    
    buttonLayout->addStretch();
    
    m_closeButton = new QPushButton(tr("Close"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &GitRemoteDialog::onCloseClicked);
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
}

void GitRemoteDialog::applyStyles()
{
    setStyleSheet(R"(
        QDialog {
            background: #0d1117;
        }
        QGroupBox {
            background: #161b22;
            border: 1px solid #30363d;
            border-radius: 6px;
            margin-top: 12px;
            padding: 12px;
            padding-top: 24px;
            font-weight: bold;
            color: #e6edf3;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 6px;
            color: #8b949e;
            font-size: 11px;
            text-transform: uppercase;
        }
        QLabel {
            color: #e6edf3;
        }
        QComboBox {
            background: #21262d;
            color: #e6edf3;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 12px;
        }
        QComboBox:hover {
            border-color: #58a6ff;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #8b949e;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background: #21262d;
            color: #e6edf3;
            border: 1px solid #30363d;
            selection-background-color: #1f6feb;
        }
        QLineEdit {
            background: #21262d;
            color: #e6edf3;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 12px;
        }
        QLineEdit:focus {
            border-color: #58a6ff;
        }
        QListWidget {
            background: #161b22;
            color: #e6edf3;
            border: 1px solid #30363d;
            border-radius: 6px;
        }
        QListWidget::item {
            padding: 8px 12px;
            border-bottom: 1px solid #21262d;
        }
        QListWidget::item:selected {
            background: #1f6feb;
        }
        QCheckBox {
            color: #e6edf3;
            font-size: 12px;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 4px;
            border: 1px solid #30363d;
            background: #21262d;
        }
        QCheckBox::indicator:checked {
            background: #238636;
            border-color: #238636;
        }
        QPushButton {
            background: #21262d;
            color: #e6edf3;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 8px 20px;
            font-size: 12px;
        }
        QPushButton:hover {
            background: #30363d;
        }
        QPushButton#pushButton {
            background: #238636;
            border-color: #238636;
            color: white;
            font-weight: bold;
        }
        QPushButton#pushButton:hover {
            background: #2ea043;
        }
        QPushButton#pullButton {
            background: #1f6feb;
            border-color: #1f6feb;
            color: white;
            font-weight: bold;
        }
        QPushButton#pullButton:hover {
            background: #388bfd;
        }
        QPushButton#fetchButton {
            background: #8b949e;
            border-color: #8b949e;
            color: white;
        }
        QPushButton#fetchButton:hover {
            background: #a5b0bc;
        }
        QProgressBar {
            background: #21262d;
            border: 1px solid #30363d;
            border-radius: 4px;
            height: 8px;
        }
        QProgressBar::chunk {
            background: #58a6ff;
            border-radius: 3px;
        }
    )");
    
    if (m_pushButton) m_pushButton->setObjectName("pushButton");
    if (m_pullButton) m_pullButton->setObjectName("pullButton");
    if (m_fetchButton) m_fetchButton->setObjectName("fetchButton");
}

void GitRemoteDialog::refresh()
{
    updateRemoteList();
    updateBranchList();
}

void GitRemoteDialog::updateRemoteList()
{
    if (!m_git) return;
    
    QList<GitRemoteInfo> remotes = m_git->getRemotes();
    
    if (m_remoteSelector) {
        m_remoteSelector->clear();
        for (const GitRemoteInfo& remote : remotes) {
            m_remoteSelector->addItem(remote.name, remote.fetchUrl);
        }
        
        // Default to "origin" if available
        int originIndex = m_remoteSelector->findText("origin");
        if (originIndex >= 0) {
            m_remoteSelector->setCurrentIndex(originIndex);
        }
    }
    
    if (m_remoteList) {
        m_remoteList->clear();
        for (const GitRemoteInfo& remote : remotes) {
            QListWidgetItem* item = new QListWidgetItem(m_remoteList);
            item->setText(tr("%1\nFetch: %2\nPush: %3")
                .arg(remote.name)
                .arg(remote.fetchUrl)
                .arg(remote.pushUrl));
            item->setData(Qt::UserRole, remote.name);
        }
    }
}

void GitRemoteDialog::updateBranchList()
{
    if (!m_git || !m_branchSelector) return;
    
    m_branchSelector->clear();
    
    QString currentBranch = m_git->currentBranch();
    QList<GitBranchInfo> branches = m_git->getBranches();
    
    int currentIndex = 0;
    for (int i = 0; i < branches.size(); ++i) {
        const GitBranchInfo& branch = branches[i];
        if (!branch.isRemote) {
            if (branch.name == currentBranch) {
                currentIndex = m_branchSelector->count();
            }
            m_branchSelector->addItem(branch.name, branch.name);
        }
    }
    
    m_branchSelector->setCurrentIndex(currentIndex);
}

void GitRemoteDialog::onRemoteSelected(int index)
{
    Q_UNUSED(index);
    // Could update status with remote info
}

void GitRemoteDialog::onPushClicked()
{
    if (!m_git) return;
    
    QString remote = m_remoteSelector->currentText();
    QString branch = m_branchSelector->currentText();
    bool setUpstream = m_setUpstreamCheckbox && m_setUpstreamCheckbox->isChecked();
    
    if (remote.isEmpty() || branch.isEmpty()) {
        QMessageBox::warning(this, tr("Push"), tr("Please select a remote and branch."));
        return;
    }
    
    m_progressBar->show();
    m_statusLabel->setText(tr("Pushing to %1/%2...").arg(remote).arg(branch));
    
    bool success = m_git->push(remote, branch, setUpstream);
    
    m_progressBar->hide();
    
    if (success) {
        m_statusLabel->setText(tr("✓ Successfully pushed to %1/%2").arg(remote).arg(branch));
        m_statusLabel->setStyleSheet("color: #3fb950; font-size: 11px;");
        emit operationCompleted(QString("Pushed to %1/%2").arg(remote).arg(branch));
    } else {
        m_statusLabel->setText(tr("✗ Push failed"));
        m_statusLabel->setStyleSheet("color: #f85149; font-size: 11px;");
    }
}

void GitRemoteDialog::onPullClicked()
{
    if (!m_git) return;
    
    QString remote = m_remoteSelector->currentText();
    QString branch = m_branchSelector->currentText();
    
    if (remote.isEmpty() || branch.isEmpty()) {
        QMessageBox::warning(this, tr("Pull"), tr("Please select a remote and branch."));
        return;
    }
    
    m_progressBar->show();
    m_statusLabel->setText(tr("Pulling from %1/%2...").arg(remote).arg(branch));
    
    bool success = m_git->pull(remote, branch);
    
    m_progressBar->hide();
    
    if (success) {
        m_statusLabel->setText(tr("✓ Successfully pulled from %1/%2").arg(remote).arg(branch));
        m_statusLabel->setStyleSheet("color: #3fb950; font-size: 11px;");
        emit operationCompleted(QString("Pulled from %1/%2").arg(remote).arg(branch));
    } else {
        if (m_git->hasMergeConflicts()) {
            m_statusLabel->setText(tr("⚠ Pull completed with merge conflicts"));
            m_statusLabel->setStyleSheet("color: #f85149; font-size: 11px;");
        } else {
            m_statusLabel->setText(tr("✗ Pull failed"));
            m_statusLabel->setStyleSheet("color: #f85149; font-size: 11px;");
        }
    }
}

void GitRemoteDialog::onFetchClicked()
{
    if (!m_git) return;
    
    QString remote = m_remoteSelector->currentText();
    
    if (remote.isEmpty()) {
        QMessageBox::warning(this, tr("Fetch"), tr("Please select a remote."));
        return;
    }
    
    m_progressBar->show();
    m_statusLabel->setText(tr("Fetching from %1...").arg(remote));
    
    bool success = m_git->fetch(remote);
    
    m_progressBar->hide();
    
    if (success) {
        m_statusLabel->setText(tr("✓ Successfully fetched from %1").arg(remote));
        m_statusLabel->setStyleSheet("color: #3fb950; font-size: 11px;");
        emit operationCompleted(QString("Fetched from %1").arg(remote));
    } else {
        m_statusLabel->setText(tr("✗ Fetch failed"));
        m_statusLabel->setStyleSheet("color: #f85149; font-size: 11px;");
    }
}

void GitRemoteDialog::onAddRemoteClicked()
{
    if (!m_git) return;
    
    QString name = m_remoteNameEdit->text().trimmed();
    QString url = m_remoteUrlEdit->text().trimmed();
    
    if (name.isEmpty() || url.isEmpty()) {
        QMessageBox::warning(this, tr("Add Remote"), tr("Please enter both remote name and URL."));
        return;
    }
    
    if (m_git->addRemote(name, url)) {
        m_remoteNameEdit->clear();
        m_remoteUrlEdit->clear();
        updateRemoteList();
        m_statusLabel->setText(tr("✓ Remote '%1' added").arg(name));
        m_statusLabel->setStyleSheet("color: #3fb950; font-size: 11px;");
    } else {
        m_statusLabel->setText(tr("✗ Failed to add remote"));
        m_statusLabel->setStyleSheet("color: #f85149; font-size: 11px;");
    }
}

void GitRemoteDialog::onRemoveRemoteClicked()
{
    if (!m_git || !m_remoteList) return;
    
    QListWidgetItem* item = m_remoteList->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("Remove Remote"), tr("Please select a remote to remove."));
        return;
    }
    
    QString name = item->data(Qt::UserRole).toString();
    
    int result = QMessageBox::question(this, tr("Remove Remote"),
        tr("Are you sure you want to remove remote '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        if (m_git->removeRemote(name)) {
            updateRemoteList();
            m_statusLabel->setText(tr("✓ Remote '%1' removed").arg(name));
            m_statusLabel->setStyleSheet("color: #3fb950; font-size: 11px;");
        } else {
            m_statusLabel->setText(tr("✗ Failed to remove remote"));
            m_statusLabel->setStyleSheet("color: #f85149; font-size: 11px;");
        }
    }
}

void GitRemoteDialog::onCloseClicked()
{
    accept();
}

void GitRemoteDialog::applyTheme(const Theme& theme)
{
    setStyleSheet(UIStyleHelper::formDialogStyle(theme));
    
    // Apply styles to group boxes
    for (QGroupBox* groupBox : findChildren<QGroupBox*>()) {
        groupBox->setStyleSheet(UIStyleHelper::groupBoxStyle(theme));
    }
    
    // Apply combobox style
    if (m_remoteSelector) {
        m_remoteSelector->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
    }
    if (m_branchSelector) {
        m_branchSelector->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
    }
    
    // Apply line edit style
    if (m_remoteNameEdit) {
        m_remoteNameEdit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
    }
    if (m_remoteUrlEdit) {
        m_remoteUrlEdit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
    }
    
    // Apply checkbox style
    if (m_setUpstreamCheckbox) {
        m_setUpstreamCheckbox->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
    }
    if (m_forceCheckbox) {
        m_forceCheckbox->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
    }
    
    // Apply list widget style
    if (m_remoteList) {
        m_remoteList->setStyleSheet(UIStyleHelper::resultListStyle(theme));
    }
    
    // Apply button styles
    if (m_pushButton) {
        m_pushButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
    }
    if (m_pullButton) {
        m_pullButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
    }
    if (m_fetchButton) {
        m_fetchButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
    }
    if (m_addRemoteButton) {
        m_addRemoteButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
    }
    if (m_removeRemoteButton) {
        m_removeRemoteButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
    }
    if (m_closeButton) {
        m_closeButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
    }
    
    // Apply status label style
    if (m_statusLabel) {
        m_statusLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
    }
}
