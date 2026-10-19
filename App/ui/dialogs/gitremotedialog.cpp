#include "gitremotedialog.h"
#include "../uistylehelper.h"
#include "gitautorefresh.h"
#include "themedmessagebox.h"
#include <QCheckBox>

GitRemoteDialog::GitRemoteDialog(GitIntegration *git, Mode mode,
                                 QWidget *parent)
    : GitDialog(parent), m_git(git), m_mode(mode), m_remoteSelector(nullptr),
      m_branchSelector(nullptr), m_setUpstreamCheckbox(nullptr),
      m_forceCheckbox(nullptr), m_pushButton(nullptr), m_pullButton(nullptr),
      m_fetchButton(nullptr), m_progressBar(nullptr), m_statusLabel(nullptr),
      m_remoteList(nullptr), m_remoteNameEdit(nullptr),
      m_remoteUrlEdit(nullptr), m_addRemoteButton(nullptr),
      m_removeRemoteButton(nullptr), m_closeButton(nullptr) {
  QString title;
  switch (mode) {
  case Mode::Push:
    title = tr("Push to Remote");
    break;
  case Mode::Pull:
    title = tr("Pull from Remote");
    break;
  case Mode::Fetch:
    title = tr("Fetch from Remote");
    break;
  case Mode::ManageRemotes:
    title = tr("Manage Remotes");
    break;
  }
  setWindowTitle(title);
  setGitDescription(
      tr("Manage remotes and transfer commits between repositories."));
  setMinimumSize(550, 450);
  setupUI();
  refresh();
  reloadOnExternalGitChanges(this, m_git, [this]() { refresh(); });
}

GitRemoteDialog::~GitRemoteDialog() {}

void GitRemoteDialog::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(16);
  mainLayout->setContentsMargins(20, 20, 20, 20);

  QHBoxLayout *headerLayout = new QHBoxLayout();

  QString titleText;
  QString subtitleText;

  switch (m_mode) {
  case Mode::Push:
    titleText = tr("Push Changes");
    subtitleText = tr("Upload your commits to a remote repository");
    break;
  case Mode::Pull:
    titleText = tr("Pull Changes");
    subtitleText =
        tr("Download and integrate changes from a remote repository");
    break;
  case Mode::Fetch:
    titleText = tr("Fetch Changes");
    subtitleText = tr("Download changes without merging");
    break;
  case Mode::ManageRemotes:
    titleText = tr("Manage Remotes");
    subtitleText = tr("Add, remove, or modify remote repositories");
    break;
  }

  QVBoxLayout *titleLayout = new QVBoxLayout();
  m_titleLabel = new QLabel(titleText, this);
  m_subtitleLabel = new QLabel(subtitleText, this);
  titleLayout->addWidget(m_titleLabel);
  titleLayout->addWidget(m_subtitleLabel);
  headerLayout->addLayout(titleLayout, 1);

  mainLayout->addLayout(headerLayout);
  useGitHeading(m_titleLabel, m_subtitleLabel);

  if (m_mode != Mode::ManageRemotes) {

    QGroupBox *selectionGroup = new QGroupBox(
        m_mode == Mode::Fetch ? tr("Remote") : tr("Remote and branch"), this);
    QVBoxLayout *selectionLayout = new QVBoxLayout(selectionGroup);

    QHBoxLayout *remoteLayout = new QHBoxLayout();
    QLabel *remoteLabel = new QLabel(tr("Remote:"), this);
    remoteLabel->setFixedWidth(80);
    m_remoteSelector = new QComboBox(this);
    m_remoteSelector->setMinimumWidth(200);
    connect(m_remoteSelector,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &GitRemoteDialog::onRemoteSelected);
    remoteLayout->addWidget(remoteLabel);
    remoteLayout->addWidget(m_remoteSelector, 1);
    selectionLayout->addLayout(remoteLayout);

    if (m_mode != Mode::Fetch) {
      QHBoxLayout *branchLayout = new QHBoxLayout();
      QLabel *branchLabel = new QLabel(tr("Branch:"), this);
      branchLabel->setFixedWidth(80);
      m_branchSelector = new QComboBox(this);
      m_branchSelector->setMinimumWidth(200);
      branchLayout->addWidget(branchLabel);
      branchLayout->addWidget(m_branchSelector, 1);
      selectionLayout->addLayout(branchLayout);
    }

    mainLayout->addWidget(selectionGroup);

    if (m_mode == Mode::Push) {
      QGroupBox *optionsGroup = new QGroupBox(tr("Options"), this);
      QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);

      if (m_mode == Mode::Push) {
        m_setUpstreamCheckbox =
            new QCheckBox(tr("Set upstream (tracking branch)"), this);
        m_setUpstreamCheckbox->setToolTip(
            tr("Set the remote branch as the tracking branch for the local "
               "branch"));
        optionsLayout->addWidget(m_setUpstreamCheckbox);

        m_forceCheckbox = new QCheckBox(tr("Force with lease"), this);
        m_forceCheckbox->setToolTip(
            tr("Replace remote history only if the remote still matches "
               "the last fetched state."));
        optionsLayout->addWidget(m_forceCheckbox);
      }

      mainLayout->addWidget(optionsGroup);
    }
  } else {

    QGroupBox *remotesGroup = new QGroupBox(tr("Configured Remotes"), this);
    QVBoxLayout *remotesLayout = new QVBoxLayout(remotesGroup);

    m_remoteList = new QListWidget(this);
    m_remoteList->setProperty(
        "gitEmptyText", tr("No remotes configured. Add a name and URL below."));
    remotesLayout->addWidget(m_remoteList);

    QHBoxLayout *addRemoteLayout = new QHBoxLayout();

    QVBoxLayout *inputsLayout = new QVBoxLayout();

    QHBoxLayout *nameLayout = new QHBoxLayout();
    QLabel *nameLabel = new QLabel(tr("Name:"), this);
    nameLabel->setFixedWidth(50);
    m_remoteNameEdit = new QLineEdit(this);
    m_remoteNameEdit->setPlaceholderText(tr("e.g., origin"));
    nameLayout->addWidget(nameLabel);
    nameLayout->addWidget(m_remoteNameEdit);
    inputsLayout->addLayout(nameLayout);

    QHBoxLayout *urlLayout = new QHBoxLayout();
    QLabel *urlLabel = new QLabel(tr("URL:"), this);
    urlLabel->setFixedWidth(50);
    m_remoteUrlEdit = new QLineEdit(this);
    m_remoteUrlEdit->setPlaceholderText(tr("https://github.com/user/repo.git"));
    urlLayout->addWidget(urlLabel);
    urlLayout->addWidget(m_remoteUrlEdit);
    inputsLayout->addLayout(urlLayout);

    addRemoteLayout->addLayout(inputsLayout, 1);

    QVBoxLayout *buttonsLayout = new QVBoxLayout();
    m_addRemoteButton = new QPushButton(tr("Add"), this);
    connect(m_addRemoteButton, &QPushButton::clicked, this,
            &GitRemoteDialog::onAddRemoteClicked);
    buttonsLayout->addWidget(m_addRemoteButton);

    m_addRemoteButton->setEnabled(false);
    const auto updateAdd = [this]() {
      m_addRemoteButton->setEnabled(
          !m_remoteNameEdit->text().trimmed().isEmpty() &&
          !m_remoteUrlEdit->text().trimmed().isEmpty());
    };
    connect(m_remoteNameEdit, &QLineEdit::textChanged, this, updateAdd);
    connect(m_remoteUrlEdit, &QLineEdit::textChanged, this, updateAdd);
    m_removeRemoteButton = new QPushButton(tr("Remove"), this);
    m_removeRemoteButton->setEnabled(false);
    connect(m_remoteList, &QListWidget::itemSelectionChanged, this, [this]() {
      m_removeRemoteButton->setEnabled(
          !m_remoteList->selectedItems().isEmpty());
    });
    connect(m_removeRemoteButton, &QPushButton::clicked, this,
            &GitRemoteDialog::onRemoveRemoteClicked);
    buttonsLayout->addWidget(m_removeRemoteButton);

    addRemoteLayout->addLayout(buttonsLayout);
    remotesLayout->addLayout(addRemoteLayout);

    mainLayout->addWidget(remotesGroup);
  }

  m_progressBar = new QProgressBar(this);
  m_progressBar->setRange(0, 0);
  m_progressBar->hide();
  mainLayout->addWidget(m_progressBar);

  m_statusLabel = new QLabel(this);
  mainLayout->addWidget(m_statusLabel);

  mainLayout->addStretch();

  QHBoxLayout *buttonLayout = new QHBoxLayout();

  if (m_mode != Mode::ManageRemotes) {
    if (m_mode == Mode::Push || m_mode == Mode::ManageRemotes) {
      m_pushButton = new QPushButton(tr("Push"), this);
      connect(m_pushButton, &QPushButton::clicked, this,
              &GitRemoteDialog::onPushClicked);
      buttonLayout->addWidget(m_pushButton);
    }

    if (m_mode == Mode::Pull || m_mode == Mode::ManageRemotes) {
      m_pullButton = new QPushButton(tr("Pull"), this);
      connect(m_pullButton, &QPushButton::clicked, this,
              &GitRemoteDialog::onPullClicked);
      buttonLayout->addWidget(m_pullButton);
    }

    if (m_mode == Mode::Fetch) {
      m_fetchButton = new QPushButton(tr("Fetch"), this);
      connect(m_fetchButton, &QPushButton::clicked, this,
              &GitRemoteDialog::onFetchClicked);
      buttonLayout->addWidget(m_fetchButton);
    }
  }

  buttonLayout->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setProperty("gitDismiss", true);
  connect(m_closeButton, &QPushButton::clicked, this,
          &GitRemoteDialog::onCloseClicked);
  buttonLayout->addWidget(m_closeButton);

  mainLayout->addLayout(buttonLayout);
}

void GitRemoteDialog::refresh() {
  updateRemoteList();
  updateBranchList();
  const bool hasRemote = m_remoteSelector && m_remoteSelector->count() > 0;
  const bool hasBranch = m_branchSelector && m_branchSelector->count() > 0;
  if (m_pushButton)
    m_pushButton->setEnabled(hasRemote && hasBranch);
  if (m_pullButton)
    m_pullButton->setEnabled(hasRemote && hasBranch);
  if (m_fetchButton)
    m_fetchButton->setEnabled(hasRemote);
  if (m_remoteSelector && !hasRemote)
    setStatus(
        tr("No remotes configured. Add one in Repository Tools → Remotes."),
        UIStyleHelper::Tone::Neutral);
}

void GitRemoteDialog::updateRemoteList() {
  if (!m_git)
    return;

  QList<GitRemoteInfo> remotes = m_git->getRemotes();

  if (m_remoteSelector) {
    m_remoteSelector->clear();
    for (const GitRemoteInfo &remote : remotes) {
      m_remoteSelector->addItem(remote.name, remote.fetchUrl);
    }

    int originIndex = m_remoteSelector->findText("origin");
    if (originIndex >= 0) {
      m_remoteSelector->setCurrentIndex(originIndex);
    }
  }

  if (m_remoteList) {
    m_remoteList->clear();
    for (const GitRemoteInfo &remote : remotes) {
      QListWidgetItem *item = new QListWidgetItem(m_remoteList);
      item->setText(tr("%1\nFetch: %2\nPush: %3")
                        .arg(remote.name)
                        .arg(remote.fetchUrl)
                        .arg(remote.pushUrl));
      item->setData(Qt::UserRole, remote.name);
    }
  }
}

void GitRemoteDialog::updateBranchList() {
  if (!m_git || !m_branchSelector)
    return;

  m_branchSelector->clear();

  QString currentBranch = m_git->currentBranch();
  QList<GitBranchInfo> branches = m_git->getBranches();

  int currentIndex = 0;
  for (int i = 0; i < branches.size(); ++i) {
    const GitBranchInfo &branch = branches[i];
    if (!branch.isRemote) {
      if (branch.name == currentBranch) {
        currentIndex = m_branchSelector->count();
      }
      m_branchSelector->addItem(branch.name, branch.name);
    }
  }

  m_branchSelector->setCurrentIndex(currentIndex);
}

void GitRemoteDialog::onRemoteSelected(int index) { Q_UNUSED(index); }

void GitRemoteDialog::onPushClicked() {
  if (!m_git)
    return;

  QString remote = m_remoteSelector->currentText();
  QString branch = m_branchSelector->currentText();
  bool setUpstream =
      m_setUpstreamCheckbox && m_setUpstreamCheckbox->isChecked();

  if (remote.isEmpty() || branch.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Push"),
                              tr("Please select a remote and branch."));
    return;
  }

  m_progressBar->show();
  setStatus(tr("Pushing to %1/%2...").arg(remote).arg(branch),
            UIStyleHelper::Tone::Neutral);

  const bool forceWithLease = m_forceCheckbox && m_forceCheckbox->isChecked();
  if (forceWithLease && ThemedMessageBox::question(
                            this, tr("Force with Lease"),
                            tr("Replace history on %1/%2? The push will be "
                               "rejected if the remote "
                               "has changed since your last fetch.")
                                .arg(remote, branch),
                            ThemedMessageBox::Yes | ThemedMessageBox::No,
                            ThemedMessageBox::No) != ThemedMessageBox::Yes) {
    m_progressBar->hide();
    setStatus(tr("Push canceled"), UIStyleHelper::Tone::Neutral);
    return;
  }
  bool success = m_git->pushWithForce(remote, branch, setUpstream,
                                      forceWithLease ? GitPushForce::WithLease
                                                     : GitPushForce::None);

  m_progressBar->hide();

  if (success) {
    setStatus(tr("Successfully pushed to %1/%2").arg(remote).arg(branch),
              UIStyleHelper::Tone::Success);
    emit operationCompleted(QString("Pushed to %1/%2").arg(remote).arg(branch));
  } else {
    setStatus(tr("Push failed"), UIStyleHelper::Tone::Error);
  }
}

void GitRemoteDialog::onPullClicked() {
  if (!m_git)
    return;

  QString remote = m_remoteSelector->currentText();
  QString branch = m_branchSelector->currentText();

  if (remote.isEmpty() || branch.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Pull"),
                              tr("Please select a remote and branch."));
    return;
  }

  m_progressBar->show();
  setStatus(tr("Pulling from %1/%2...").arg(remote).arg(branch),
            UIStyleHelper::Tone::Neutral);

  bool success = m_git->pull(remote, branch);

  m_progressBar->hide();

  if (success) {
    setStatus(tr("Successfully pulled from %1/%2").arg(remote).arg(branch),
              UIStyleHelper::Tone::Success);
    emit operationCompleted(
        QString("Pulled from %1/%2").arg(remote).arg(branch));
  } else {
    if (m_git->hasMergeConflicts()) {
      setStatus(tr("Pull completed with merge conflicts"),
                UIStyleHelper::Tone::Warning);
    } else {
      setStatus(tr("Pull failed"), UIStyleHelper::Tone::Error);
    }
  }
}

void GitRemoteDialog::onFetchClicked() {
  if (!m_git)
    return;

  QString remote = m_remoteSelector->currentText();

  if (remote.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Fetch"), tr("Please select a remote."));
    return;
  }

  m_progressBar->show();
  setStatus(tr("Fetching from %1...").arg(remote),
            UIStyleHelper::Tone::Neutral);

  bool success = m_git->fetch(remote);

  m_progressBar->hide();

  if (success) {
    setStatus(tr("Successfully fetched from %1").arg(remote),
              UIStyleHelper::Tone::Success);
    emit operationCompleted(QString("Fetched from %1").arg(remote));
  } else {
    setStatus(tr("Fetch failed"), UIStyleHelper::Tone::Error);
  }
}

void GitRemoteDialog::onAddRemoteClicked() {
  if (!m_git)
    return;

  QString name = m_remoteNameEdit->text().trimmed();
  QString url = m_remoteUrlEdit->text().trimmed();

  if (name.isEmpty() || url.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Add Remote"),
                              tr("Please enter both remote name and URL."));
    return;
  }

  if (m_git->addRemote(name, url)) {
    m_remoteNameEdit->clear();
    m_remoteUrlEdit->clear();
    updateRemoteList();
    setStatus(tr("Remote '%1' added").arg(name), UIStyleHelper::Tone::Success);
  } else {
    setStatus(tr("Failed to add remote"), UIStyleHelper::Tone::Error);
  }
}

void GitRemoteDialog::onRemoveRemoteClicked() {
  if (!m_git || !m_remoteList)
    return;

  QListWidgetItem *item = m_remoteList->currentItem();
  if (!item) {
    ThemedMessageBox::warning(this, tr("Remove Remote"),
                              tr("Please select a remote to remove."));
    return;
  }

  QString name = item->data(Qt::UserRole).toString();

  int result = ThemedMessageBox::question(
      this, tr("Remove Remote"),
      tr("Are you sure you want to remove remote '%1'?").arg(name),
      ThemedMessageBox::Yes | ThemedMessageBox::No, ThemedMessageBox::No);

  if (result == ThemedMessageBox::Yes) {
    if (m_git->removeRemote(name)) {
      updateRemoteList();
      setStatus(tr("Remote '%1' removed").arg(name),
                UIStyleHelper::Tone::Success);
    } else {
      setStatus(tr("Failed to remove remote"), UIStyleHelper::Tone::Error);
    }
  }
}

void GitRemoteDialog::onCloseClicked() { accept(); }

void GitRemoteDialog::applyTheme(const Theme &theme) {
  GitDialog::applyTheme(theme);

  stylePrimaryButton(m_pushButton);
  stylePrimaryButton(m_pullButton);
  stylePrimaryButton(m_fetchButton);
  stylePrimaryButton(m_addRemoteButton);

  if (m_progressBar) {
    m_progressBar->setStyleSheet(UIStyleHelper::progressBarStyle(theme));
  }

  applyHeroHeaderStyle();
  applyStatusStyle();

  if (m_forceCheckbox) {
    m_forceCheckbox->setStyleSheet(
        UIStyleHelper::checkBoxStyle(theme) +
        QString("QCheckBox { color: %1; }")
            .arg(UIStyleHelper::toneColor(theme, UIStyleHelper::Tone::Error)
                     .name()));
  }
}

void GitRemoteDialog::applyHeroHeaderStyle() {
  if (m_titleLabel)
    m_titleLabel->setStyleSheet(UIStyleHelper::headingStyle(m_theme, 17));
  if (m_subtitleLabel)
    m_subtitleLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(m_theme) +
                                   " font-size: 12px;");
}

void GitRemoteDialog::setStatus(const QString &text, UIStyleHelper::Tone tone) {
  m_statusTone = tone;
  m_statusLabel->setText(text);
  applyStatusStyle();
}

void GitRemoteDialog::applyStatusStyle() {
  if (!m_statusLabel)
    return;
  m_statusLabel->setStyleSheet(
      m_statusTone == UIStyleHelper::Tone::Neutral
          ? UIStyleHelper::infoLabelStyle(m_theme)
          : UIStyleHelper::toneLabelStyle(m_theme, m_statusTone));
}
