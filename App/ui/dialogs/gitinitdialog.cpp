#include "gitinitdialog.h"
#include "../uistylehelper.h"
#include "themedmessagebox.h"
#include <QFileDialog>

GitInitDialog::GitInitDialog(const QString &projectPath, QWidget *parent)
    : StyledDialog(parent), m_projectPath(projectPath), m_pathEdit(nullptr),
      m_browseButton(nullptr), m_initialCommitCheckbox(nullptr),
      m_gitIgnoreCheckbox(nullptr), m_remoteEdit(nullptr),
      m_initButton(nullptr), m_cancelButton(nullptr) {
  setWindowTitle(tr("Initialize Git Repository"));
  setMinimumSize(500, 350);
  setupUI();
}

GitInitDialog::~GitInitDialog() {}

void GitInitDialog::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(16);
  mainLayout->setContentsMargins(24, 24, 24, 24);

  QHBoxLayout *headerLayout = new QHBoxLayout();
  QLabel *iconLabel = new QLabel("🗃️", this);
  iconLabel->setStyleSheet("font-size: 32px;");
  headerLayout->addWidget(iconLabel);

  QVBoxLayout *titleLayout = new QVBoxLayout();
  QLabel *titleLabel = new QLabel(tr("Initialize Git Repository"), this);
  titleLabel->setObjectName("titleLabel");
  QLabel *subtitleLabel =
      new QLabel(tr("This folder is not a Git repository. Initialize one to "
                    "start tracking changes."),
                 this);
  subtitleLabel->setWordWrap(true);
  titleLayout->addWidget(titleLabel);
  titleLayout->addWidget(subtitleLabel);
  headerLayout->addLayout(titleLayout, 1);
  mainLayout->addLayout(headerLayout);

  QGroupBox *pathGroup = new QGroupBox(tr("Repository Location"), this);
  QVBoxLayout *pathLayout = new QVBoxLayout(pathGroup);

  QHBoxLayout *pathInputLayout = new QHBoxLayout();
  m_pathEdit = new QLineEdit(m_projectPath, this);
  m_pathEdit->setPlaceholderText(tr("Path to initialize repository"));
  m_browseButton = new QPushButton(tr("Browse..."), this);
  m_browseButton->setFixedWidth(100);
  connect(m_browseButton, &QPushButton::clicked, this,
          &GitInitDialog::onBrowseClicked);
  pathInputLayout->addWidget(m_pathEdit);
  pathInputLayout->addWidget(m_browseButton);
  pathLayout->addLayout(pathInputLayout);
  mainLayout->addWidget(pathGroup);

  QGroupBox *optionsGroup = new QGroupBox(tr("Options"), this);
  QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);

  m_initialCommitCheckbox = new QCheckBox(tr("Create initial commit"), this);
  m_initialCommitCheckbox->setChecked(true);
  m_initialCommitCheckbox->setToolTip(
      tr("Create an empty initial commit after initialization"));
  optionsLayout->addWidget(m_initialCommitCheckbox);

  m_gitIgnoreCheckbox = new QCheckBox(tr("Add .gitignore template"), this);
  m_gitIgnoreCheckbox->setChecked(true);
  m_gitIgnoreCheckbox->setToolTip(tr("Create a default .gitignore file"));
  optionsLayout->addWidget(m_gitIgnoreCheckbox);

  mainLayout->addWidget(optionsGroup);

  QGroupBox *remoteGroup = new QGroupBox(tr("Remote (Optional)"), this);
  QVBoxLayout *remoteLayout = new QVBoxLayout(remoteGroup);

  QLabel *remoteLabel = new QLabel(tr("Add a remote repository URL:"), this);
  remoteLayout->addWidget(remoteLabel);

  m_remoteEdit = new QLineEdit(this);
  m_remoteEdit->setPlaceholderText(
      tr("https://github.com/username/repository.git"));
  remoteLayout->addWidget(m_remoteEdit);

  mainLayout->addWidget(remoteGroup);

  mainLayout->addStretch();

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setFixedWidth(100);
  connect(m_cancelButton, &QPushButton::clicked, this,
          &GitInitDialog::onCancelClicked);
  buttonLayout->addWidget(m_cancelButton);

  m_initButton = new QPushButton(tr("Initialize"), this);
  m_initButton->setFixedWidth(120);
  m_initButton->setDefault(true);
  connect(m_initButton, &QPushButton::clicked, this,
          &GitInitDialog::onInitClicked);
  buttonLayout->addWidget(m_initButton);

  mainLayout->addLayout(buttonLayout);
}

QString GitInitDialog::repositoryPath() const {
  return m_pathEdit->text().trimmed();
}

bool GitInitDialog::createInitialCommit() const {
  return m_initialCommitCheckbox->isChecked();
}

bool GitInitDialog::addGitIgnore() const {
  return m_gitIgnoreCheckbox->isChecked();
}

QString GitInitDialog::remoteUrl() const {
  return m_remoteEdit->text().trimmed();
}

void GitInitDialog::onInitClicked() {
  QString path = repositoryPath();
  if (path.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Error"),
                              tr("Please specify a repository path."));
    return;
  }

  QDir dir(path);
  if (!dir.exists()) {
    ThemedMessageBox::warning(this, tr("Error"),
                              tr("The specified path does not exist."));
    return;
  }

  emit initializeRequested(path);
  accept();
}

void GitInitDialog::onCancelClicked() { reject(); }

void GitInitDialog::onBrowseClicked() {
  QString dir = QFileDialog::getExistingDirectory(
      this, tr("Select Repository Location"), m_pathEdit->text(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (!dir.isEmpty()) {
    m_pathEdit->setText(dir);
  }
}

void GitInitDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  stylePrimaryButton(m_initButton);
}
