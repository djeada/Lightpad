#include "gitstashdialog.h"
#include "themedmessagebox.h"

GitStashDialog::GitStashDialog(GitIntegration *git, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_stashList(nullptr),
      m_messageEdit(nullptr), m_includeUntrackedCheckbox(nullptr),
      m_stashButton(nullptr), m_popButton(nullptr), m_applyButton(nullptr),
      m_dropButton(nullptr), m_clearButton(nullptr), m_closeButton(nullptr),
      m_statusLabel(nullptr), m_detailsLabel(nullptr) {
  setWindowTitle(tr("Git Stash"));
  setMinimumSize(600, 500);
  setupUI();
  refresh();
}

GitStashDialog::~GitStashDialog() {}

void GitStashDialog::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(16);
  mainLayout->setContentsMargins(20, 20, 20, 20);

  QHBoxLayout *headerLayout = new QHBoxLayout();

  QLabel *iconLabel = new QLabel("📦", this);
  iconLabel->setStyleSheet("font-size: 28px;");
  headerLayout->addWidget(iconLabel);

  QVBoxLayout *titleLayout = new QVBoxLayout();
  QLabel *titleLabel = new QLabel(tr("Stash Changes"), this);
  titleLabel->setStyleSheet(
      QString("font-size: 18px; font-weight: bold; color: %1;").arg(m_theme.foregroundColor.name()));
  QLabel *subtitleLabel =
      new QLabel(tr("Save your work in progress for later"), this);
  subtitleLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(m_theme.singleLineCommentFormat.name()));
  titleLayout->addWidget(titleLabel);
  titleLayout->addWidget(subtitleLabel);
  headerLayout->addLayout(titleLayout, 1);

  mainLayout->addLayout(headerLayout);

  QGroupBox *newStashGroup = new QGroupBox(tr("Stash Current Changes"), this);
  QVBoxLayout *newStashLayout = new QVBoxLayout(newStashGroup);

  QHBoxLayout *messageLayout = new QHBoxLayout();
  QLabel *messageLabel = new QLabel(tr("Message:"), this);
  messageLabel->setFixedWidth(70);
  m_messageEdit = new QLineEdit(this);
  m_messageEdit->setPlaceholderText(tr("Optional stash message"));
  messageLayout->addWidget(messageLabel);
  messageLayout->addWidget(m_messageEdit);
  newStashLayout->addLayout(messageLayout);

  QHBoxLayout *optionsLayout = new QHBoxLayout();
  m_includeUntrackedCheckbox =
      new QCheckBox(tr("Include untracked files"), this);
  m_includeUntrackedCheckbox->setToolTip(
      tr("Also stash files that are not yet tracked by git"));
  optionsLayout->addWidget(m_includeUntrackedCheckbox);
  optionsLayout->addStretch();

  m_stashButton = new QPushButton(tr("Stash Changes"), this);
  connect(m_stashButton, &QPushButton::clicked, this,
          &GitStashDialog::onStashClicked);
  optionsLayout->addWidget(m_stashButton);
  newStashLayout->addLayout(optionsLayout);

  mainLayout->addWidget(newStashGroup);

  QGroupBox *listGroup = new QGroupBox(tr("Stash List"), this);
  QVBoxLayout *listLayout = new QVBoxLayout(listGroup);

  m_stashList = new QListWidget(this);
  connect(m_stashList, &QListWidget::itemClicked, this,
          &GitStashDialog::onStashSelected);
  listLayout->addWidget(m_stashList);

  QHBoxLayout *actionsLayout = new QHBoxLayout();

  m_popButton = new QPushButton(tr("Pop"), this);
  m_popButton->setToolTip(tr("Apply and remove the latest stash"));
  m_popButton->setEnabled(false);
  connect(m_popButton, &QPushButton::clicked, this,
          &GitStashDialog::onPopClicked);
  actionsLayout->addWidget(m_popButton);

  m_applyButton = new QPushButton(tr("Apply"), this);
  m_applyButton->setToolTip(tr("Apply the selected stash without removing it"));
  m_applyButton->setEnabled(false);
  connect(m_applyButton, &QPushButton::clicked, this,
          &GitStashDialog::onApplyClicked);
  actionsLayout->addWidget(m_applyButton);

  m_dropButton = new QPushButton(tr("Drop"), this);
  m_dropButton->setToolTip(tr("Delete the selected stash"));
  m_dropButton->setEnabled(false);
  connect(m_dropButton, &QPushButton::clicked, this,
          &GitStashDialog::onDropClicked);
  actionsLayout->addWidget(m_dropButton);

  actionsLayout->addStretch();

  m_clearButton = new QPushButton(tr("Clear All"), this);
  m_clearButton->setToolTip(tr("Delete all stash entries"));
  connect(m_clearButton, &QPushButton::clicked, this,
          &GitStashDialog::onClearClicked);
  actionsLayout->addWidget(m_clearButton);

  listLayout->addLayout(actionsLayout);
  mainLayout->addWidget(listGroup, 1);

  QGroupBox *detailsGroup = new QGroupBox(tr("Stash Details"), this);
  QVBoxLayout *detailsLayout = new QVBoxLayout(detailsGroup);

  m_detailsLabel = new QLabel(tr("Select a stash entry to view details"), this);
  m_detailsLabel->setStyleSheet(QString("color: %1; font-family: monospace;").arg(m_theme.singleLineCommentFormat.name()));
  m_detailsLabel->setWordWrap(true);
  detailsLayout->addWidget(m_detailsLabel);

  mainLayout->addWidget(detailsGroup);

  QHBoxLayout *bottomLayout = new QHBoxLayout();

  m_statusLabel = new QLabel(this);
  m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.singleLineCommentFormat.name()));
  bottomLayout->addWidget(m_statusLabel, 1);

  m_closeButton = new QPushButton(tr("Close"), this);
  connect(m_closeButton, &QPushButton::clicked, this,
          &GitStashDialog::onCloseClicked);
  bottomLayout->addWidget(m_closeButton);

  mainLayout->addLayout(bottomLayout);
}

void GitStashDialog::refresh() { updateStashList(); }

void GitStashDialog::updateStashList() {
  m_stashList->clear();

  if (!m_git)
    return;

  QList<GitStashEntry> stashes = m_git->getStashList();

  for (const GitStashEntry &stash : stashes) {
    QListWidgetItem *item = new QListWidgetItem(m_stashList);
    QString text =
        QString("stash@{%1}: %2")
            .arg(stash.index)
            .arg(stash.message.isEmpty() ? tr("(no message)") : stash.message);

    if (!stash.branch.isEmpty()) {
      text += QString("\n    On branch: %1").arg(stash.branch);
    }

    item->setText(text);
    item->setData(Qt::UserRole, stash.index);
  }

  m_clearButton->setEnabled(!stashes.isEmpty());

  if (stashes.isEmpty()) {
    m_statusLabel->setText(tr("No stashed changes"));
    m_detailsLabel->setText(tr("No stash entries available"));
  } else {
    m_statusLabel->setText(tr("%n stash entry(s)", "", stashes.size()));
  }

  m_popButton->setEnabled(false);
  m_applyButton->setEnabled(false);
  m_dropButton->setEnabled(false);
}

void GitStashDialog::onStashSelected(QListWidgetItem *item) {
  if (!item) {
    m_popButton->setEnabled(false);
    m_applyButton->setEnabled(false);
    m_dropButton->setEnabled(false);
    m_detailsLabel->setText(tr("Select a stash entry to view details"));
    return;
  }

  int index = item->data(Qt::UserRole).toInt();

  m_popButton->setEnabled(index == 0);
  m_applyButton->setEnabled(true);
  m_dropButton->setEnabled(true);

  if (m_git) {
    QList<GitStashEntry> stashes = m_git->getStashList();
    for (const GitStashEntry &stash : stashes) {
      if (stash.index == index) {
        QString details =
            QString(tr("Index: stash@{%1}\nBranch: %2\nMessage: %3"))
                .arg(stash.index)
                .arg(stash.branch.isEmpty() ? tr("(unknown)") : stash.branch)
                .arg(stash.message.isEmpty() ? tr("(no message)")
                                             : stash.message);
        m_detailsLabel->setText(details);
        break;
      }
    }
  }
}

void GitStashDialog::onStashClicked() {
  if (!m_git)
    return;

  QString message = m_messageEdit->text().trimmed();
  bool includeUntracked = m_includeUntrackedCheckbox->isChecked();

  if (m_git->stash(message, includeUntracked)) {
    m_messageEdit->clear();
    m_statusLabel->setText(tr("✓ Changes stashed successfully"));
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.successColor.name()));
    refresh();
    emit stashOperationCompleted(tr("Changes stashed"));
  } else {
    m_statusLabel->setText(tr("✗ Failed to stash changes"));
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.errorColor.name()));
  }
}

void GitStashDialog::onPopClicked() {
  if (!m_git)
    return;

  if (m_git->stashPop()) {
    m_statusLabel->setText(tr("✓ Stash popped successfully"));
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.successColor.name()));
    refresh();
    emit stashOperationCompleted(tr("Stash popped"));
  } else {
    if (m_git->hasMergeConflicts()) {
      m_statusLabel->setText(tr("⚠ Stash popped with conflicts"));
      m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.errorColor.name()));
    } else {
      m_statusLabel->setText(tr("✗ Failed to pop stash"));
      m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.errorColor.name()));
    }
  }
}

void GitStashDialog::onApplyClicked() {
  if (!m_git)
    return;

  QListWidgetItem *item = m_stashList->currentItem();
  if (!item)
    return;

  int index = item->data(Qt::UserRole).toInt();

  if (m_git->stashApply(index)) {
    m_statusLabel->setText(QString(tr("✓ Stash %1 applied")).arg(index));
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.successColor.name()));
    emit stashOperationCompleted(QString(tr("Stash %1 applied")).arg(index));
  } else {
    if (m_git->hasMergeConflicts()) {
      m_statusLabel->setText(tr("⚠ Stash applied with conflicts"));
      m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.errorColor.name()));
    } else {
      m_statusLabel->setText(tr("✗ Failed to apply stash"));
      m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.errorColor.name()));
    }
  }
}

void GitStashDialog::onDropClicked() {
  if (!m_git)
    return;

  QListWidgetItem *item = m_stashList->currentItem();
  if (!item)
    return;

  int index = item->data(Qt::UserRole).toInt();

  int result = ThemedMessageBox::question(
      this, tr("Drop Stash"),
      QString(tr("Are you sure you want to drop stash@{%1}?\nThis cannot be "
                 "undone."))
          .arg(index),
      ThemedMessageBox::Yes | ThemedMessageBox::No, ThemedMessageBox::No);

  if (result == ThemedMessageBox::Yes) {
    if (m_git->stashDrop(index)) {
      m_statusLabel->setText(QString(tr("✓ Stash %1 dropped")).arg(index));
      m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.successColor.name()));
      refresh();
      emit stashOperationCompleted(QString(tr("Stash %1 dropped")).arg(index));
    } else {
      m_statusLabel->setText(tr("✗ Failed to drop stash"));
      m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.errorColor.name()));
    }
  }
}

void GitStashDialog::onClearClicked() {
  if (!m_git)
    return;

  int result = ThemedMessageBox::warning(
      this, tr("Clear All Stashes"),
      tr("Are you sure you want to clear all stash "
         "entries?\nThis action cannot be undone!"),
      ThemedMessageBox::Yes | ThemedMessageBox::No);

  if (result == ThemedMessageBox::Yes) {
    if (m_git->stashClear()) {
      m_statusLabel->setText(tr("✓ All stashes cleared"));
      m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.successColor.name()));
      refresh();
      emit stashOperationCompleted(tr("All stashes cleared"));
    } else {
      m_statusLabel->setText(tr("✗ Failed to clear stashes"));
      m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_theme.errorColor.name()));
    }
  }
}

void GitStashDialog::onCloseClicked() { accept(); }

void GitStashDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);

  stylePrimaryButton(m_stashButton);
  styleDangerButton(m_dropButton);
  styleDangerButton(m_clearButton);
  styleSubduedLabel(m_statusLabel);
  styleSubduedLabel(m_detailsLabel);
}
