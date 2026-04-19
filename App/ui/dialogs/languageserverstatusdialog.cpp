#include "languageserverstatusdialog.h"
#include "../../language/languagecatalog.h"
#include "../../language/languagefeaturemanager.h"
#include "../../run_templates/runtemplatemanager.h"
#include "../../settings/settingsmanager.h"
#include "../uistylehelper.h"

#include "themedmessagebox.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {
QString displayValue(const QString &value,
                     const QString &fallback = QString("(not set)")) {
  return value.trimmed().isEmpty() ? fallback : value;
}

QString healthLabel(ServerHealthStatus status) {
  switch (status) {
  case ServerHealthStatus::Starting:
    return "Starting";
  case ServerHealthStatus::Running:
    return "Running";
  case ServerHealthStatus::Error:
    return "Error";
  case ServerHealthStatus::Stopped:
    return "Stopped";
  default:
    return "Unknown";
  }
}
} // namespace

LanguageServerStatusDialog::LanguageServerStatusDialog(
    const QString &languageId, const QString &workspaceFolder,
    const QString &filePath, const QString &effectiveLanguageId,
    const QString &overrideLanguageId, LanguageFeatureManager *manager,
    const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_languageId(languageId),
      m_workspaceFolder(workspaceFolder), m_filePath(filePath),
      m_effectiveLanguageId(effectiveLanguageId),
      m_overrideLanguageId(overrideLanguageId), m_manager(manager),
      m_theme(theme), m_statusBanner(nullptr), m_enabledCheck(nullptr),
      m_languageCombo(nullptr), m_commandEdit(nullptr),
      m_argumentsEdit(nullptr), m_detailsEdit(nullptr),
      m_pythonEnvironmentButton(nullptr), m_saveButton(nullptr),
      m_closeButton(nullptr) {
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(tr("Language Server Status"));
  resize(760, 560);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  auto *titleLabel =
      new QLabel(tr("%1 Language Server")
                     .arg(LanguageCatalog::displayName(languageId).isEmpty()
                              ? languageId
                              : LanguageCatalog::displayName(languageId)),
                 this);
  layout->addWidget(titleLabel);

  m_statusBanner = new QLabel(this);
  m_statusBanner->setWordWrap(true);
  m_statusBanner->setTextFormat(Qt::RichText);
  layout->addWidget(m_statusBanner);

  auto *formLayout = new QFormLayout();
  m_enabledCheck = new QCheckBox(tr("Enabled"), this);
  formLayout->addRow(tr("Status:"), m_enabledCheck);

  m_languageCombo = new QComboBox(this);
  m_languageCombo->addItem(tr("Auto Detect"), "");
  const QVector<LanguageInfo> languages = LanguageCatalog::allLanguages();
  for (const LanguageInfo &language : languages) {
    m_languageCombo->addItem(language.displayName, language.id);
  }
  const QString selectedOverride =
      LanguageCatalog::normalize(overrideLanguageId);
  const int languageIndex = selectedOverride.isEmpty()
                                ? 0
                                : m_languageCombo->findData(selectedOverride);
  m_languageCombo->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);
  formLayout->addRow(tr("File language:"), m_languageCombo);

  m_commandEdit = new QLineEdit(this);
  formLayout->addRow(tr("Command:"), m_commandEdit);

  m_argumentsEdit = new QLineEdit(this);
  formLayout->addRow(tr("Arguments:"), m_argumentsEdit);
  layout->addLayout(formLayout);

  auto *hintLabel = new QLabel(
      tr("Saved in settings.json under languageServers.%1.*").arg(languageId),
      this);
  layout->addWidget(hintLabel);

  m_detailsEdit = new QPlainTextEdit(this);
  m_detailsEdit->setReadOnly(true);
  layout->addWidget(m_detailsEdit, 1);

  auto *buttonRow = new QHBoxLayout();
  m_pythonEnvironmentButton =
      new QPushButton(tr("Open Python Environment"), this);
  m_pythonEnvironmentButton->setVisible(languageId == "py");
  buttonRow->addWidget(m_pythonEnvironmentButton);
  buttonRow->addStretch();

  m_saveButton = new QPushButton(tr("Save and Retry"), this);
  m_closeButton = new QPushButton(tr("Close"), this);
  buttonRow->addWidget(m_saveButton);
  buttonRow->addWidget(m_closeButton);
  layout->addLayout(buttonRow);

  applyTheme(theme);
  styleTitleLabel(titleLabel);
  styleSubduedLabel(hintLabel);
  stylePrimaryButton(m_saveButton);

  if (m_manager) {
    const DiagnosticsServerConfig config = m_manager->serverConfig(languageId);
    m_enabledCheck->setChecked(config.enabled);
    m_commandEdit->setText(config.command);
    m_argumentsEdit->setText(config.arguments.join(' '));
  }

  connect(m_enabledCheck, &QCheckBox::toggled, this,
          &LanguageServerStatusDialog::refreshDetails);
  connect(m_languageCombo, &QComboBox::currentIndexChanged, this,
          &LanguageServerStatusDialog::refreshDetails);
  connect(m_commandEdit, &QLineEdit::textChanged, this,
          &LanguageServerStatusDialog::refreshDetails);
  connect(m_argumentsEdit, &QLineEdit::textChanged, this,
          &LanguageServerStatusDialog::refreshDetails);
  connect(m_saveButton, &QPushButton::clicked, this,
          &LanguageServerStatusDialog::saveAndRetry);
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);
  connect(m_pythonEnvironmentButton, &QPushButton::clicked, this,
          &LanguageServerStatusDialog::pythonEnvironmentRequested);

  refreshDetails();
}

void LanguageServerStatusDialog::refreshDetails() {
  const ServerHealthStatus health = m_manager
                                        ? m_manager->serverHealth(m_languageId)
                                        : ServerHealthStatus::Unknown;
  const QString lastError =
      m_manager ? m_manager->lastServerError(m_languageId) : QString();
  const QString command = m_commandEdit->text().trimmed();
  const QString resolvedCommand =
      command.isEmpty() ? QString() : QStandardPaths::findExecutable(command);
  const QString displayName =
      LanguageCatalog::displayName(m_languageId).isEmpty()
          ? m_languageId
          : LanguageCatalog::displayName(m_languageId);
  const QString selectedAssociation =
      m_languageCombo->currentData().toString().trimmed();
  const QString selectedAssociationDisplay =
      selectedAssociation.isEmpty()
          ? tr("Auto Detect")
          : displayValue(LanguageCatalog::displayName(selectedAssociation),
                         selectedAssociation);

  QString bannerBg = m_theme.surfaceColor.name();
  QString bannerBorder = m_theme.borderColor.name();
  QString bannerText = QString("Status: %1").arg(healthLabel(health));
  if (health == ServerHealthStatus::Running) {
    bannerBg =
        QColor(
            (m_theme.backgroundColor.red() * 9 + m_theme.successColor.red()) /
                10,
            (m_theme.backgroundColor.green() * 9 +
             m_theme.successColor.green()) /
                10,
            (m_theme.backgroundColor.blue() * 9 + m_theme.successColor.blue()) /
                10)
            .name();
    bannerBorder = m_theme.successColor.name();
  } else if (health == ServerHealthStatus::Starting) {
    bannerBg =
        QColor(
            (m_theme.backgroundColor.red() * 9 + m_theme.warningColor.red()) /
                10,
            (m_theme.backgroundColor.green() * 9 +
             m_theme.warningColor.green()) /
                10,
            (m_theme.backgroundColor.blue() * 9 + m_theme.warningColor.blue()) /
                10)
            .name();
    bannerBorder = m_theme.warningColor.name();
  } else if (health == ServerHealthStatus::Error) {
    bannerBg =
        QColor(
            (m_theme.backgroundColor.red() * 9 + m_theme.errorColor.red()) / 10,
            (m_theme.backgroundColor.green() * 9 + m_theme.errorColor.green()) /
                10,
            (m_theme.backgroundColor.blue() * 9 + m_theme.errorColor.blue()) /
                10)
            .name();
    bannerBorder = m_theme.errorColor.name();
  }

  QString bannerHtml =
      QString("<div style='padding:8px;'><b>%1</b><br><span "
              "style='font-size:12px;'>%2</span></div>")
          .arg(
              displayName.toHtmlEscaped(),
              displayValue(lastError, tr("Click Save and Retry after adjusting "
                                         "the command or arguments."))
                  .toHtmlEscaped());
  m_statusBanner->setStyleSheet(
      QString("QLabel { background: %1; border: 1px solid %2; border-radius: "
              "6px; color: %3; }")
          .arg(bannerBg, bannerBorder, m_theme.foregroundColor.name()));
  m_statusBanner->setText(bannerHtml);

  QStringList lines;
  lines
      << QString("Language: %1 (%2)").arg(displayName, m_languageId)
      << QString("Health: %1").arg(healthLabel(health))
      << QString("Enabled: %1").arg(m_enabledCheck->isChecked() ? "yes" : "no")
      << QString("File language association: %1")
             .arg(selectedAssociationDisplay)
      << QString("Current effective language: %1")
             .arg(displayValue(
                 LanguageCatalog::displayName(m_effectiveLanguageId),
                 m_effectiveLanguageId))
      << QString("Command: %1").arg(displayValue(command))
      << QString("Resolved executable: %1")
             .arg(displayValue(QDir::toNativeSeparators(resolvedCommand),
                               "(not found on PATH)"))
      << QString("Arguments: %1")
             .arg(displayValue(m_argumentsEdit->text().trimmed(), "(none)"))
      << QString("Workspace: %1")
             .arg(displayValue(QDir::toNativeSeparators(m_workspaceFolder)))
      << QString("Current file: %1")
             .arg(displayValue(QDir::toNativeSeparators(m_filePath)))
      << QString("Detected project root: %1")
             .arg(displayValue(QDir::toNativeSeparators(
                 LanguageFeatureManager::detectProjectRoot(m_filePath))))
      << QString("Last error: %1").arg(displayValue(lastError, "(none)"));

  if (m_languageId == "py" && !m_filePath.trimmed().isEmpty()) {
    RunTemplateManager &manager = RunTemplateManager::instance();
    const FileTemplateAssignment assignment =
        manager.getAssignmentForFile(m_filePath);
    PythonEnvironmentPreference preference;
    preference.mode = assignment.pythonMode;
    preference.customInterpreter = assignment.pythonInterpreter;
    preference.venvPath = assignment.pythonVenvPath;
    preference.requirementsFile = assignment.pythonRequirementsFile;
    const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
        preference, m_workspaceFolder, m_filePath,
        QFileInfo(m_filePath).absolutePath());

    lines << "" << "Python environment for this file:"
          << QString("  Interpreter: %1")
                 .arg(displayValue(QDir::toNativeSeparators(info.interpreter),
                                   "(not resolved)"))
          << QString("  Virtualenv: %1")
                 .arg(displayValue(QDir::toNativeSeparators(info.venvPath),
                                   "(not resolved)"))
          << QString("  Requirements: %1")
                 .arg(displayValue(
                     QDir::toNativeSeparators(info.requirementsFile),
                     "(not found)"))
          << QString("  Suggested install: %1")
                 .arg(
                     info.interpreter.isEmpty()
                         ? QString("Resolve a Python interpreter, then install "
                                   "python-lsp-server into it.")
                         : QString("%1 -m pip install python-lsp-server")
                               .arg(info.interpreter));
  }

  lines << ""
        << "PATH:" << QProcessEnvironment::systemEnvironment().value("PATH");
  m_detailsEdit->setPlainText(lines.join('\n'));
}

void LanguageServerStatusDialog::saveAndRetry() {
  SettingsManager &settings = SettingsManager::instance();
  const QString prefix = QString("languageServers.%1").arg(m_languageId);

  settings.setValue(prefix + ".enabled", m_enabledCheck->isChecked());
  settings.setValue(prefix + ".command", m_commandEdit->text().trimmed());
  settings.setValue(prefix + ".arguments",
                    QProcess::splitCommand(m_argumentsEdit->text().trimmed()));

  if (!settings.saveSettings()) {
    ThemedMessageBox::warning(this, tr("Language Server"),
                              tr("Failed to save language server settings."));
    return;
  }

  if (m_manager) {
    m_manager->loadSettingsOverrides();
    m_manager->restartServer(m_languageId);
  }

  emit configurationApplied(m_languageCombo->currentData().toString());
  accept();
}
