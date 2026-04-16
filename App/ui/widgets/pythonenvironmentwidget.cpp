#include "pythonenvironmentwidget.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include "../dialogs/themedmessagebox.h"
#include "../../settings/theme.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>

PythonEnvironmentWidget::PythonEnvironmentWidget(QWidget *parent)
    : QGroupBox(tr("Python Environment"), parent) {
  auto *layout = new QVBoxLayout(this);

  m_statusLabel = new QLabel(this);
  m_statusLabel->setWordWrap(true);
  m_statusLabel->setTextFormat(Qt::RichText);
  m_statusLabel->setMinimumHeight(48);
  m_statusLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  layout->addWidget(m_statusLabel);

  m_hintLabel = new QLabel(
      tr("Templates can use ${python}, ${venv}, ${requirementsFile}."));
  m_hintLabel->setWordWrap(true);
  m_hintLabel->setStyleSheet("font-size: 11px;");
  layout->addWidget(m_hintLabel);

  auto *formLayout = new QFormLayout();

  m_modeCombo = new QComboBox(this);
  m_modeCombo->addItem(tr("Auto (project venv first)"),
                       PythonProjectEnvironment::autoMode());
  m_modeCombo->addItem(tr("Workspace virtualenv"),
                       PythonProjectEnvironment::workspaceVenvMode());
  m_modeCombo->addItem(tr("Custom interpreter"),
                       PythonProjectEnvironment::customInterpreterMode());
  connect(m_modeCombo, &QComboBox::currentIndexChanged, this,
          &PythonEnvironmentWidget::onModeChanged);
  formLayout->addRow(tr("Mode:"), m_modeCombo);

  auto *venvRow = new QWidget(this);
  auto *venvLayout = new QHBoxLayout(venvRow);
  venvLayout->setContentsMargins(0, 0, 0, 0);
  m_venvPathEdit = new QLineEdit(this);
  m_browseVenvButton = new QPushButton(tr("Browse..."), this);
  connect(m_venvPathEdit, &QLineEdit::textChanged, this,
          &PythonEnvironmentWidget::preferenceChanged);
  connect(m_venvPathEdit, &QLineEdit::textChanged, this,
          [this]() { refreshStatus(); });
  connect(m_browseVenvButton, &QPushButton::clicked, this,
          &PythonEnvironmentWidget::onBrowseVenv);
  venvLayout->addWidget(m_venvPathEdit, 1);
  venvLayout->addWidget(m_browseVenvButton);
  formLayout->addRow(tr("Virtualenv:"), venvRow);

  auto *interpreterRow = new QWidget(this);
  auto *interpreterLayout = new QHBoxLayout(interpreterRow);
  interpreterLayout->setContentsMargins(0, 0, 0, 0);
  m_interpreterEdit = new QLineEdit(this);
  m_browseInterpreterButton = new QPushButton(tr("Browse..."), this);
  connect(m_interpreterEdit, &QLineEdit::textChanged, this,
          &PythonEnvironmentWidget::preferenceChanged);
  connect(m_interpreterEdit, &QLineEdit::textChanged, this,
          [this]() { refreshStatus(); });
  connect(m_browseInterpreterButton, &QPushButton::clicked, this,
          &PythonEnvironmentWidget::onBrowseInterpreter);
  interpreterLayout->addWidget(m_interpreterEdit, 1);
  interpreterLayout->addWidget(m_browseInterpreterButton);
  formLayout->addRow(tr("Interpreter:"), interpreterRow);

  auto *requirementsRow = new QWidget(this);
  auto *requirementsLayout = new QHBoxLayout(requirementsRow);
  requirementsLayout->setContentsMargins(0, 0, 0, 0);
  m_requirementsEdit = new QLineEdit(this);
  m_browseRequirementsButton = new QPushButton(tr("Browse..."), this);
  connect(m_requirementsEdit, &QLineEdit::textChanged, this,
          &PythonEnvironmentWidget::preferenceChanged);
  connect(m_browseRequirementsButton, &QPushButton::clicked, this,
          &PythonEnvironmentWidget::onBrowseRequirements);
  requirementsLayout->addWidget(m_requirementsEdit, 1);
  requirementsLayout->addWidget(m_browseRequirementsButton);
  formLayout->addRow(tr("Requirements:"), requirementsRow);

  layout->addLayout(formLayout);

  auto *buttonLayout = new QHBoxLayout();
  m_createVenvButton = new QPushButton(tr("Create Venv"), this);
  m_installRequirementsButton =
      new QPushButton(tr("Install Requirements"), this);
  m_installDebugpyButton = new QPushButton(tr("Install debugpy"), this);
  connect(m_createVenvButton, &QPushButton::clicked, this,
          &PythonEnvironmentWidget::onCreateVenv);
  connect(m_installRequirementsButton, &QPushButton::clicked, this,
          &PythonEnvironmentWidget::onInstallRequirements);
  connect(m_installDebugpyButton, &QPushButton::clicked, this,
          &PythonEnvironmentWidget::onInstallDebugpy);
  buttonLayout->addWidget(m_createVenvButton);
  buttonLayout->addWidget(m_installRequirementsButton);
  buttonLayout->addWidget(m_installDebugpyButton);
  buttonLayout->addStretch();
  layout->addLayout(buttonLayout);

  setContext({}, {});
  setPreference({});
}

void PythonEnvironmentWidget::setContext(const QString &workspaceFolder,
                                         const QString &filePath,
                                         const QString &workingDirectory) {
  m_workspaceFolder = workspaceFolder;
  m_filePath = filePath;
  m_workingDirectory = workingDirectory;

  if (m_venvPathEdit->text().trimmed().isEmpty()) {
    m_venvPathEdit->setText(PythonProjectEnvironment::defaultVenvPath(
        m_workspaceFolder, m_filePath, m_workingDirectory));
  }
  if (m_requirementsEdit->text().trimmed().isEmpty()) {
    m_requirementsEdit->setText(
        PythonProjectEnvironment::defaultRequirementsPath(
            m_workspaceFolder, m_filePath, m_workingDirectory));
  }

  refreshStatus();
}

void PythonEnvironmentWidget::setPreference(
    const PythonEnvironmentPreference &preference) {
  const QString mode = preference.mode.trimmed().isEmpty()
                           ? PythonProjectEnvironment::autoMode()
                           : preference.mode;
  const int modeIndex = m_modeCombo->findData(mode);
  m_modeCombo->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
  m_venvPathEdit->setText(
      preference.venvPath.isEmpty()
          ? PythonProjectEnvironment::defaultVenvPath(
                m_workspaceFolder, m_filePath, m_workingDirectory)
          : preference.venvPath);
  m_interpreterEdit->setText(preference.customInterpreter);
  m_requirementsEdit->setText(
      preference.requirementsFile.isEmpty()
          ? PythonProjectEnvironment::defaultRequirementsPath(
                m_workspaceFolder, m_filePath, m_workingDirectory)
          : preference.requirementsFile);
  updateUiVisibility();
  refreshStatus();
}

PythonEnvironmentPreference PythonEnvironmentWidget::preference() const {
  PythonEnvironmentPreference preference;
  preference.mode = m_modeCombo->currentData().toString();
  preference.customInterpreter = m_interpreterEdit->text().trimmed();
  preference.venvPath = m_venvPathEdit->text().trimmed();
  preference.requirementsFile = m_requirementsEdit->text().trimmed();
  return preference;
}

void PythonEnvironmentWidget::setDebugToolsVisible(bool visible) {
  m_installDebugpyButton->setVisible(visible);
}

void PythonEnvironmentWidget::refreshStatus() {
  const PythonEnvironmentPreference pref = preference();
  const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
      pref, m_workspaceFolder, m_filePath, m_workingDirectory);

  Theme t;
  const QString fgColor = t.foregroundColor.name();
  const QString successBg =
      QColor(t.successColor.red() / 8, t.successColor.green() / 8,
             t.successColor.blue() / 8)
          .name();
  const QString errorBg =
      QColor(t.errorColor.red() / 8, t.errorColor.green() / 8,
             t.errorColor.blue() / 8)
          .name();
  const QString warningBg =
      QColor(t.warningColor.red() / 8, t.warningColor.green() / 8,
             t.warningColor.blue() / 8)
          .name();

  QString bannerBg;
  QString bannerBorder;
  QString bannerIcon;
  QString bannerTextColor;
  QString bannerTitle;
  QString bannerDetail;

  m_interpreterEdit->setStyleSheet("");
  m_venvPathEdit->setStyleSheet("");

  if (info.found && info.isVirtualEnvironment()) {

    bannerBg = successBg;
    bannerBorder = t.successColor.name();
    bannerTextColor = t.successColor.name();
    bannerIcon = QString::fromUtf8("\xe2\x9c\x93");
    bannerTitle = tr("Virtual environment active");
    const QString venvName = QFileInfo(info.venvPath).fileName();
    bannerDetail = QString("<br><span style='color:%1;font-size:12px;'>"
                           "%2 &nbsp;&bull;&nbsp; %3</span>")
                       .arg(fgColor, info.interpreter.toHtmlEscaped(),
                            info.venvPath.toHtmlEscaped());
  } else if (info.found) {

    bannerBg = t.surfaceColor.name();
    bannerBorder = t.borderColor.name();
    bannerTextColor = t.borderColor.name();
    bannerIcon = QString::fromUtf8("\xe2\x9c\x93");
    bannerTitle = tr("System Python (no virtual environment)");
    bannerDetail = QString("<br><span style='color:%1;font-size:12px;'>"
                           "%2</span>")
                       .arg(fgColor, info.interpreter.toHtmlEscaped());
  } else if (pref.mode == PythonProjectEnvironment::customInterpreterMode() &&
             pref.customInterpreter.isEmpty()) {

    bannerBg = errorBg;
    bannerBorder = t.errorColor.name();
    bannerTextColor = t.errorColor.name();
    bannerIcon = QString::fromUtf8("\xe2\x9c\x97");
    bannerTitle = tr("No interpreter selected");
    bannerDetail =
        QString("<br><span style='color:%1;font-size:12px;'>"
                "Set the interpreter path below to continue.</span>")
            .arg(t.warningColor.name());
    m_interpreterEdit->setStyleSheet(
        QString("QLineEdit { border: 2px solid %1; }").arg(t.errorColor.name()));
  } else if (!pref.venvPath.isEmpty() && !info.found) {

    bannerBg = warningBg;
    bannerBorder = t.warningColor.name();
    bannerTextColor = t.warningColor.name();
    bannerIcon = QString::fromUtf8("\xe2\x9a\xa0");
    bannerTitle = tr("Virtual environment not found");
    bannerDetail = QString("<br><span style='color:%1;font-size:12px;'>"
                           "Expected at: %2<br>"
                           "Click <b>Create Venv</b> to create one.</span>")
                       .arg(t.warningColor.name(), pref.venvPath.toHtmlEscaped());
    m_venvPathEdit->setStyleSheet(
        QString("QLineEdit { border: 2px solid %1; }").arg(t.warningColor.name()));
  } else {

    bannerBg = errorBg;
    bannerBorder = t.errorColor.name();
    bannerTextColor = t.errorColor.name();
    bannerIcon = QString::fromUtf8("\xe2\x9c\x97");
    bannerTitle = tr("Python interpreter not found");
    bannerDetail =
        QString("<br><span style='color:%1;font-size:12px;'>"
                "%2</span>")
            .arg(t.warningColor.name(),
                 info.statusMessage.isEmpty()
                     ? tr("Install Python or configure a virtual environment.")
                     : info.statusMessage.toHtmlEscaped());
  }

  const QString bannerHtml =
      QString("<div style='padding:8px;'>"
              "<span style='color:%1;font-size:16px;font-weight:bold;'>"
              "%2 &nbsp;%3</span>"
              "%4</div>")
          .arg(bannerTextColor, bannerIcon, bannerTitle, bannerDetail);

  m_statusLabel->setStyleSheet(
      QString("QLabel { background-color: %1; border: 1px solid %2;"
              " border-radius: 6px; padding: 4px; }")
          .arg(bannerBg, bannerBorder));
  m_statusLabel->setText(bannerHtml);
}

void PythonEnvironmentWidget::onModeChanged() {
  updateUiVisibility();
  refreshStatus();
  emit preferenceChanged();
}

void PythonEnvironmentWidget::onBrowseInterpreter() {
  const QString file = QFileDialog::getOpenFileName(
      this, tr("Select Python Interpreter"), m_workspaceFolder);
  if (!file.isEmpty()) {
    m_interpreterEdit->setText(file);
    refreshStatus();
  }
}

void PythonEnvironmentWidget::onBrowseVenv() {
  const QString path = QFileDialog::getExistingDirectory(
      this, tr("Select Virtual Environment"), m_workspaceFolder);
  if (!path.isEmpty()) {
    m_venvPathEdit->setText(path);
    refreshStatus();
  }
}

void PythonEnvironmentWidget::onBrowseRequirements() {
  const QString file = QFileDialog::getOpenFileName(
      this, tr("Select Requirements File"), m_workspaceFolder,
      tr("Python Project Files (requirements*.txt pyproject.toml setup.py);;"
         "All Files (*)"));
  if (!file.isEmpty()) {
    m_requirementsEdit->setText(file);
    refreshStatus();
  }
}

void PythonEnvironmentWidget::onCreateVenv() {
  const QString targetPath = PythonProjectEnvironment::normalizePath(
      preference().venvPath, m_workspaceFolder, m_filePath, m_workingDirectory);
  if (targetPath.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Python Environment"),
                              tr("Set a virtual environment path first."));
    return;
  }

  if (!PythonProjectEnvironment::pythonExecutableInEnvironment(targetPath)
           .isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Python Environment"),
        tr("The selected virtual environment already exists."));
    return;
  }

  const QString interpreter = creationInterpreter();
  if (interpreter.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Python Environment"),
                              tr("No base Python interpreter is available to create "
                                 "the virtual environment."));
    return;
  }

  startProcess(tr("Create Virtual Environment"), interpreter,
               {"-m", "venv", targetPath}, m_workspaceFolder);
}

void PythonEnvironmentWidget::onInstallRequirements() {
  const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
      preference(), m_workspaceFolder, m_filePath, m_workingDirectory);
  if (!info.found || info.interpreter.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Python Environment"),
                              tr("Resolve a Python interpreter before installing "
                                 "requirements."));
    return;
  }

  const PythonInstallPlan plan =
      PythonProjectEnvironment::requirementsInstallPlan(
          info, m_workspaceFolder, m_filePath, m_workingDirectory,
          m_requirementsEdit->text().trimmed());
  if (plan.arguments.isEmpty()) {
    ThemedMessageBox::warning(
        this, tr("Python Environment"),
        tr("No requirements file or installable Python project "
           "was found."));
    return;
  }

  startProcess(tr("Install Requirements"), info.interpreter, plan.arguments,
               plan.workingDirectory);
}

void PythonEnvironmentWidget::onInstallDebugpy() {
  const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
      preference(), m_workspaceFolder, m_filePath, m_workingDirectory);
  if (!info.found || info.interpreter.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Python Environment"),
                              tr("Resolve a Python interpreter before installing "
                                 "debugpy."));
    return;
  }

  startProcess(tr("Install debugpy"), info.interpreter,
               {"-m", "pip", "install", "debugpy"}, m_workspaceFolder);
}

void PythonEnvironmentWidget::onProcessFinished(
    int exitCode, QProcess::ExitStatus exitStatus) {
  Q_UNUSED(exitStatus);
  const bool success = exitCode == 0;
  finishProcess(m_pendingTitle, success);
}

void PythonEnvironmentWidget::onProcessError(QProcess::ProcessError error) {
  Q_UNUSED(error);
  finishProcess(m_pendingTitle, false);
}

void PythonEnvironmentWidget::updateUiVisibility() {
  const bool customMode = m_modeCombo->currentData().toString() ==
                          PythonProjectEnvironment::customInterpreterMode();
  m_interpreterEdit->setEnabled(customMode);
  m_browseInterpreterButton->setEnabled(customMode);
}

void PythonEnvironmentWidget::startProcess(const QString &title,
                                           const QString &program,
                                           const QStringList &arguments,
                                           const QString &workingDirectory) {
  if (m_process) {
    ThemedMessageBox::information(
        this, tr("Python Environment"),
        tr("Wait for the current Python setup task to finish."));
    return;
  }

  m_pendingTitle = title;
  m_pendingOperation = title;
  m_process = new QProcess(this);
  if (!workingDirectory.trimmed().isEmpty()) {
    m_process->setWorkingDirectory(workingDirectory);
  }

  connect(m_process, &QProcess::finished, this,
          &PythonEnvironmentWidget::onProcessFinished);
  connect(m_process, &QProcess::errorOccurred, this,
          &PythonEnvironmentWidget::onProcessError);

  const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
      preference(), m_workspaceFolder, m_filePath, m_workingDirectory);
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  const QMap<QString, QString> activationEnv =
      PythonProjectEnvironment::activationEnvironment(info);
  for (auto it = activationEnv.begin(); it != activationEnv.end(); ++it) {
    env.insert(it.key(), it.value());
  }
  m_process->setProcessEnvironment(env);

  m_createVenvButton->setEnabled(false);
  m_installRequirementsButton->setEnabled(false);
  m_installDebugpyButton->setEnabled(false);
  m_statusLabel->setText(
      tr("%1...\n%2 %3").arg(title, program, arguments.join(" ")));
  m_process->start(program, arguments);
}

QString PythonEnvironmentWidget::creationInterpreter() const {
  const PythonEnvironmentPreference current = preference();
  if (current.mode == PythonProjectEnvironment::customInterpreterMode()) {
    return PythonProjectEnvironment::normalizePath(
        current.customInterpreter, m_workspaceFolder, m_filePath,
        m_workingDirectory);
  }

  const QString active = PythonProjectEnvironment::globalPythonInterpreter();
  if (!active.isEmpty()) {
    return active;
  }

  const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
      current, m_workspaceFolder, m_filePath, m_workingDirectory);
  return info.interpreter;
}

void PythonEnvironmentWidget::finishProcess(const QString &message,
                                            bool success) {
  QString details;
  if (m_process) {
    details = QString::fromUtf8(m_process->readAllStandardOutput());
    const QString stderrText =
        QString::fromUtf8(m_process->readAllStandardError());
    if (!stderrText.trimmed().isEmpty()) {
      if (!details.trimmed().isEmpty()) {
        details += "\n";
      }
      details += stderrText;
    }
    m_process->deleteLater();
    m_process = nullptr;
  }

  m_createVenvButton->setEnabled(true);
  m_installRequirementsButton->setEnabled(true);
  m_installDebugpyButton->setEnabled(true);

  refreshStatus();
  emit environmentChanged();

  if (details.trimmed().isEmpty()) {
    details = success ? tr("Completed successfully.") : tr("Command failed.");
  }

  if (success) {
    ThemedMessageBox::information(this, message, details);
  } else {
    ThemedMessageBox::warning(this, message, details);
  }
}
