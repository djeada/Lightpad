#include "gitinitdialog.h"
#include "../uistylehelper.h"
#include <QFileDialog>
#include <QMessageBox>

GitInitDialog::GitInitDialog(const QString &projectPath, QWidget *parent)
    : QDialog(parent), m_projectPath(projectPath), m_pathEdit(nullptr),
      m_browseButton(nullptr), m_initialCommitCheckbox(nullptr),
      m_gitIgnoreCheckbox(nullptr), m_remoteEdit(nullptr),
      m_initButton(nullptr), m_cancelButton(nullptr) {
  setWindowTitle(tr("Initialize Git Repository"));
  setMinimumSize(500, 350);
  setupUI();
  applyStyles();
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
  titleLabel->setStyleSheet(
      "font-size: 18px; font-weight: bold; color: #e6edf3;");
  QLabel *subtitleLabel =
      new QLabel(tr("This folder is not a Git repository. Initialize one to "
                    "start tracking changes."),
                 this);
  subtitleLabel->setStyleSheet("color: #8b949e; font-size: 12px;");
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
  remoteLabel->setStyleSheet("color: #8b949e;");
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

void GitInitDialog::applyStyles() {
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
            padding: 8px 16px;
            font-size: 12px;
        }
        QPushButton:hover {
            background: #30363d;
        }
        QPushButton#initButton {
            background: #238636;
            border-color: #238636;
            color: white;
            font-weight: bold;
        }
        QPushButton#initButton:hover {
            background: #2ea043;
        }
    )");

  m_initButton->setObjectName("initButton");
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
    QMessageBox::warning(this, tr("Error"),
                         tr("Please specify a repository path."));
    return;
  }

  QDir dir(path);
  if (!dir.exists()) {
    QMessageBox::warning(this, tr("Error"),
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
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  for (QGroupBox *groupBox : findChildren<QGroupBox *>()) {
    groupBox->setStyleSheet(UIStyleHelper::groupBoxStyle(theme));
  }

  if (m_pathEdit) {
    m_pathEdit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  }
  if (m_remoteEdit) {
    m_remoteEdit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  }

  if (m_initialCommitCheckbox) {
    m_initialCommitCheckbox->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
  }
  if (m_gitIgnoreCheckbox) {
    m_gitIgnoreCheckbox->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
  }

  if (m_cancelButton) {
    m_cancelButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
  }
  if (m_browseButton) {
    m_browseButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
  }
  if (m_initButton) {
    m_initButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
  }
}
