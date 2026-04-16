#include "mergeconflictdialog.h"
#include <QFileInfo>
#include "themedmessagebox.h"

MergeConflictDialog::MergeConflictDialog(GitIntegration *git, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_fileList(nullptr),
      m_oursPreview(nullptr), m_theirsPreview(nullptr),
      m_statusLabel(nullptr), m_conflictCountLabel(nullptr),
      m_acceptOursButton(nullptr), m_acceptTheirsButton(nullptr),
      m_openEditorButton(nullptr), m_markResolvedButton(nullptr),
      m_abortButton(nullptr), m_continueButton(nullptr) {
  setWindowTitle(tr("Resolve Merge Conflicts"));
  setMinimumSize(800, 600);
  setupUI();
}

MergeConflictDialog::~MergeConflictDialog() {}

void MergeConflictDialog::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(12);
  mainLayout->setContentsMargins(16, 16, 16, 16);

  QHBoxLayout *headerLayout = new QHBoxLayout();

  QLabel *iconLabel = new QLabel("⚠️", this);
  iconLabel->setStyleSheet("font-size: 28px;");
  headerLayout->addWidget(iconLabel);

  QVBoxLayout *titleLayout = new QVBoxLayout();
  QLabel *titleLabel = new QLabel(tr("Merge Conflicts Detected"), this);
  titleLabel->setStyleSheet(
      QString("font-size: 18px; font-weight: bold; color: %1;").arg(m_theme.errorColor.name()));
  m_statusLabel =
      new QLabel(tr("Resolve conflicts before completing the merge"), this);
  m_statusLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(m_theme.singleLineCommentFormat.name()));
  titleLayout->addWidget(titleLabel);
  titleLayout->addWidget(m_statusLabel);
  headerLayout->addLayout(titleLayout, 1);

  m_conflictCountLabel = new QLabel(this);
  m_conflictCountLabel->setStyleSheet(
      QString("background: %1; color: white; padding: 4px 12px; border-radius: "
      "12px; font-weight: bold;").arg(m_theme.errorColor.name()));
  headerLayout->addWidget(m_conflictCountLabel);

  mainLayout->addLayout(headerLayout);

  QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);

  QWidget *leftPanel = new QWidget(this);
  QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);

  QLabel *filesLabel = new QLabel(tr("Conflicted Files"), this);
  filesLabel->setStyleSheet(QString("color: %1; font-size: 11px; text-transform: "
                            "uppercase; font-weight: bold;").arg(m_theme.singleLineCommentFormat.name()));
  leftLayout->addWidget(filesLabel);

  m_fileList = new QListWidget(this);
  connect(m_fileList, &QListWidget::itemClicked, this,
          &MergeConflictDialog::onFileSelected);
  leftLayout->addWidget(m_fileList);

  mainSplitter->addWidget(leftPanel);

  QWidget *rightPanel = new QWidget(this);
  QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);

  QSplitter *previewSplitter = new QSplitter(Qt::Horizontal, this);

  QWidget *oursPanel = new QWidget(this);
  QVBoxLayout *oursLayout = new QVBoxLayout(oursPanel);
  oursLayout->setContentsMargins(0, 0, 4, 0);

  QLabel *oursLabel = new QLabel(tr("⬅ Current Branch (Ours)"), this);
  oursLabel->setStyleSheet(
      QString("color: %1; font-weight: bold; font-size: 12px;").arg(m_theme.successColor.name()));
  oursLayout->addWidget(oursLabel);

  m_oursPreview = new QTextEdit(this);
  m_oursPreview->setReadOnly(true);
  m_oursPreview->setPlaceholderText(
      tr("Select a file to preview ours changes"));
  oursLayout->addWidget(m_oursPreview);

  previewSplitter->addWidget(oursPanel);

  QWidget *theirsPanel = new QWidget(this);
  QVBoxLayout *theirsLayout = new QVBoxLayout(theirsPanel);
  theirsLayout->setContentsMargins(4, 0, 0, 0);

  QLabel *theirsLabel = new QLabel(tr("Incoming Changes (Theirs) ➡"), this);
  theirsLabel->setStyleSheet(
      QString("color: %1; font-weight: bold; font-size: 12px;").arg(m_theme.accentColor.name()));
  theirsLayout->addWidget(theirsLabel);

  m_theirsPreview = new QTextEdit(this);
  m_theirsPreview->setReadOnly(true);
  m_theirsPreview->setPlaceholderText(
      tr("Select a file to preview theirs changes"));
  theirsLayout->addWidget(m_theirsPreview);

  previewSplitter->addWidget(theirsPanel);

  rightLayout->addWidget(previewSplitter, 1);

  QHBoxLayout *fileActionsLayout = new QHBoxLayout();
  fileActionsLayout->setSpacing(8);

  m_acceptOursButton = new QPushButton(tr("Accept Ours"), this);
  m_acceptOursButton->setToolTip(
      tr("Use your local version, discarding incoming changes"));
  connect(m_acceptOursButton, &QPushButton::clicked, this,
          &MergeConflictDialog::onAcceptOursClicked);
  fileActionsLayout->addWidget(m_acceptOursButton);

  m_acceptTheirsButton = new QPushButton(tr("Accept Theirs"), this);
  m_acceptTheirsButton->setToolTip(
      tr("Use the incoming version, discarding your changes"));
  connect(m_acceptTheirsButton, &QPushButton::clicked, this,
          &MergeConflictDialog::onAcceptTheirsClicked);
  fileActionsLayout->addWidget(m_acceptTheirsButton);

  m_openEditorButton = new QPushButton(tr("Open in Editor"), this);
  m_openEditorButton->setToolTip(
      tr("Manually edit the file to resolve conflicts"));
  connect(m_openEditorButton, &QPushButton::clicked, this,
          &MergeConflictDialog::onOpenInEditorClicked);
  fileActionsLayout->addWidget(m_openEditorButton);

  m_markResolvedButton = new QPushButton(tr("Mark Resolved"), this);
  m_markResolvedButton->setToolTip(
      tr("Mark this file as resolved after manual editing"));
  connect(m_markResolvedButton, &QPushButton::clicked, this,
          &MergeConflictDialog::onMarkResolvedClicked);
  fileActionsLayout->addWidget(m_markResolvedButton);

  rightLayout->addLayout(fileActionsLayout);

  mainSplitter->addWidget(rightPanel);
  mainSplitter->setSizes({250, 550});

  mainLayout->addWidget(mainSplitter, 1);

  QHBoxLayout *bottomLayout = new QHBoxLayout();

  m_abortButton = new QPushButton(tr("Abort Merge"), this);
  m_abortButton->setToolTip(
      tr("Cancel the merge and return to the previous state"));
  connect(m_abortButton, &QPushButton::clicked, this,
          &MergeConflictDialog::onAbortMergeClicked);
  bottomLayout->addWidget(m_abortButton);

  bottomLayout->addStretch();

  m_continueButton = new QPushButton(tr("Complete Merge"), this);
  m_continueButton->setToolTip(
      tr("Commit the merge after all conflicts are resolved"));
  m_continueButton->setEnabled(false);
  connect(m_continueButton, &QPushButton::clicked, this,
          &MergeConflictDialog::onContinueMergeClicked);
  bottomLayout->addWidget(m_continueButton);

  mainLayout->addLayout(bottomLayout);

  updateButtons();
}

void MergeConflictDialog::setConflictedFiles(const QStringList &files) {
  m_fileList->clear();

  for (const QString &file : files) {
    QListWidgetItem *item = new QListWidgetItem(m_fileList);
    QFileInfo fileInfo(file);
    item->setText(QString("❗ %1").arg(fileInfo.fileName()));
    item->setToolTip(file);
    item->setData(Qt::UserRole, file);
  }

  m_conflictCountLabel->setText(tr("%n conflict(s)", "", files.size()));
  updateButtons();
}

void MergeConflictDialog::refresh() {
  if (!m_git)
    return;

  QStringList conflicts = m_git->getConflictedFiles();
  setConflictedFiles(conflicts);

  if (conflicts.isEmpty()) {
    m_statusLabel->setText(
        tr("All conflicts resolved! You can complete the merge."));
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(m_theme.successColor.name()));
    emit allConflictsResolved();
  }
}

void MergeConflictDialog::onFileSelected(QListWidgetItem *item) {
  if (!item)
    return;

  m_currentFile = item->data(Qt::UserRole).toString();
  updateConflictPreview(m_currentFile);
  updateButtons();
}

void MergeConflictDialog::updateConflictPreview(const QString &filePath) {
  if (!m_git || filePath.isEmpty()) {
    m_oursPreview->clear();
    m_theirsPreview->clear();
    return;
  }

  QList<GitConflictMarker> markers = m_git->getConflictMarkers(filePath);

  QString oursContent;
  QString theirsContent;

  for (const GitConflictMarker &marker : markers) {
    oursContent += QString("--- Lines %1-%2 ---\n")
                       .arg(marker.startLine)
                       .arg(marker.separatorLine - 1);
    oursContent += marker.oursContent;
    oursContent += "\n";

    theirsContent += QString("--- Lines %1-%2 ---\n")
                         .arg(marker.separatorLine + 1)
                         .arg(marker.endLine - 1);
    theirsContent += marker.theirsContent;
    theirsContent += "\n";
  }

  if (markers.isEmpty()) {
    m_oursPreview->setPlainText(
        tr("No conflict markers found in this file.\nThe file may have been "
           "manually edited."));
    m_theirsPreview->setPlainText(
        tr("No conflict markers found in this file.\nThe file may have been "
           "manually edited."));
  } else {
    m_oursPreview->setPlainText(oursContent);
    m_theirsPreview->setPlainText(theirsContent);
  }
}

void MergeConflictDialog::updateButtons() {
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

void MergeConflictDialog::onAcceptOursClicked() {
  if (!m_git || m_currentFile.isEmpty())
    return;

  if (m_git->resolveConflictOurs(m_currentFile)) {
    refresh();
  }
}

void MergeConflictDialog::onAcceptTheirsClicked() {
  if (!m_git || m_currentFile.isEmpty())
    return;

  if (m_git->resolveConflictTheirs(m_currentFile)) {
    refresh();
  }
}

void MergeConflictDialog::onOpenInEditorClicked() {
  if (m_currentFile.isEmpty())
    return;

  QString fullPath = m_currentFile;
  if (m_git && !fullPath.startsWith('/')) {
    fullPath = m_git->repositoryPath() + "/" + m_currentFile;
  }

  emit openFileRequested(fullPath);
}

void MergeConflictDialog::onMarkResolvedClicked() {
  if (!m_git || m_currentFile.isEmpty())
    return;

  if (m_git->markConflictResolved(m_currentFile)) {
    refresh();
  }
}

void MergeConflictDialog::onAbortMergeClicked() {
  if (!m_git)
    return;

  int result = ThemedMessageBox::question(
      this, tr("Abort Merge"),
      tr("Are you sure you want to abort the merge?\nAll merge progress will "
         "be lost."),
      ThemedMessageBox::Yes | ThemedMessageBox::No, ThemedMessageBox::No);

  if (result == ThemedMessageBox::Yes) {
    if (m_git->abortMerge()) {
      accept();
    }
  }
}

void MergeConflictDialog::onContinueMergeClicked() {
  if (!m_git)
    return;

  if (m_git->hasMergeConflicts()) {
    ThemedMessageBox::warning(
        this, tr("Unresolved Conflicts"),
        tr("There are still unresolved conflicts. Please resolve all conflicts "
           "before completing the merge."));
    return;
  }

  if (m_git->continueMerge()) {
    accept();
  }
}

void MergeConflictDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);

  stylePrimaryButton(m_acceptOursButton);
  stylePrimaryButton(m_acceptTheirsButton);
  styleDangerButton(m_abortButton);
  stylePrimaryButton(m_continueButton);
  styleSubduedLabel(m_statusLabel);
  styleTitleLabel(m_conflictCountLabel);
}
